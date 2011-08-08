/*
 * $Id$
 *
 * Copyright (c) 2005-2008, Juniper Networks, Inc.
 * All rights reserved.  See ../Copyright for additional information.
 */

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <sys/queue.h>
#include <curl/curl.h>

#include <libxml/xpathInternals.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <libxslt/extensions.h>

#include "config.h"

#include <libjuise/time/time_const.h>
#include <libjuise/io/pid_lock.h>
#include <libjuise/io/trace.h>
#include <libjuise/data/parse_ip.h>
#include <libjuise/common/allocadup.h>
#include <libjuise/string/strextra.h>
#include <libjuise/xml/xmlrpc.h>
#include <libjuise/xml/client.h>
#include <libjuise/xml/libxml.h>
#include <libjuise/xml/jsio.h>
#include <libjuise/xml/extensions.h>

#include "ext_curl.h"

#define CURL_FULL_NS "http://xml.juniper.net/curl"
#define CURL_NAME_SIZE 8

/*
 * Used to chain growing lists of data, such as incoming headers and data
 */
typedef struct curl_data_s {
    TAILQ_ENTRY(curl_data_s) cd_link; /* Next session */
    int cd_len; 		/* Length of the chunk of data */
    char cd_data[0];		/* Data follows this header */
} curl_data_t;

/*
 * Defines the set of options we which to control the next request.
 * Options can be temporarily given on the curl:perform() call
 */
typedef struct curl_opts_s {
    char *co_url;		/* URL to operate on */
    char *co_method;		/* HTTP method (GET,POST,etc) */
    u_int8_t co_upload;		/* Are we uploading (aka FTP PUT)? */
    u_int8_t co_fail_on_error;	/* Fail explicitly if HTTP code >= 400 */
    char *co_username;		/* Value for CURLOPT_USERNAME */
    char *co_password;		/* Value for CURLOPT_PASSWORD */
    char *co_postfields;	/* Value for CURLOPT_POSTFIELDS */
} curl_opts_t;

typedef TAILQ_HEAD(curl_data_chain_s, curl_data_s) curl_data_chain_t;

/*
 * This is the main curl datastructure, a handle that is maintained
 * between curl function calls.  They are named using a small integer,
 * which is passed back to the extension functions to find the handle.
 * All other information is hung off this handle, including the underlaying
 * libcurl handle.
 */
typedef struct curl_handle_s {
    TAILQ_ENTRY(curl_handle_s) ch_link; /* Next session */
    curl_data_chain_t ch_reply_data;
    curl_data_chain_t ch_reply_headers;
    char ch_name[CURL_NAME_SIZE]; /* Unique ID for this handle */
    CURL *ch_handle;		/* libcurl "easy" handle */
    curl_opts_t ch_opts;	/* Options set for this handle */
    char ch_error[CURL_ERROR_SIZE]; /* Error buffer for CURLOPT_ERRORBUFFER */
} curl_handle_t;

TAILQ_HEAD(curl_session_s, curl_handle_s) ext_curl_sessions;

/*
 * Discard any transient data in the handle, particularly data
 * read from the peer.
 */
static void
ext_curl_chain_clean (curl_data_chain_t *chainp)
{
    curl_data_t *cdp;

    for (;;) {
	cdp = TAILQ_FIRST(chainp);
        if (cdp == NULL)
            break;
        TAILQ_REMOVE(chainp, cdp, cd_link);
    }
}

/*
 * Discard any transient data in the handle, particularly data
 * read from the peer.
 */
static void
ext_curl_handle_clean (curl_handle_t *curlp)
{
    ext_curl_chain_clean(&curlp->ch_reply_data);
    ext_curl_chain_clean(&curlp->ch_reply_headers);
}

/*
 * Release a set of options. The curl_opts_t is not freed, only its
 * contents.
 */
static void
ext_curl_options_release (curl_opts_t *opts)
{
    xmlFreeAndEasy(opts->co_url);
    xmlFreeAndEasy(opts->co_method);
    xmlFreeAndEasy(opts->co_username);
    xmlFreeAndEasy(opts->co_password);
    xmlFreeAndEasy(opts->co_postfields);

    bzero(opts, sizeof(*opts));
}

#define COPY_STRING(_name) top->_name = xmlStrdup2(fromp->_name);
#define COPY_FIELD(_name) top->_name = fromp->_name;

/*
 * Copy a set of options from one curl_opts_t to another.
 */
static void
ext_curl_options_copy (curl_opts_t *top, curl_opts_t *fromp)
{
    bzero(top, sizeof(*top));

    COPY_STRING(co_url);
    COPY_STRING(co_method);
    COPY_FIELD(co_upload);
    COPY_FIELD(co_fail_on_error);
    COPY_STRING(co_username);
    COPY_STRING(co_password);
    COPY_STRING(co_postfields);
}

/*
 * Free a curl handle
 */
static void
ext_curl_handle_free (curl_handle_t *curlp)
{
    ext_curl_handle_clean(curlp);

    if (curlp->ch_handle)
	curl_easy_cleanup(curlp->ch_handle);
    ext_curl_options_release(&curlp->ch_opts);

    TAILQ_REMOVE(&ext_curl_sessions, curlp, ch_link);
    xmlFree(curlp);
}

/*
 * Given the name of a handle, return it.
 */
static curl_handle_t *
ext_curl_handle_find (const char *name)
{
    curl_handle_t *curlp;

    TAILQ_FOREACH(curlp, &ext_curl_sessions, ch_link) {
	if (streq(name, curlp->ch_name))
	    return curlp;
    }

    return NULL;
}

#define CURL_SET_STRING(_v) \
    do { \
	if (_v) \
	    xmlFree(_v); \
	_v = xmlStrdup2(lx_node_value(nodep)); \
    } while (0)

/*
 * Parse any options from an input XML node.
 */
static void
ext_curl_parse_node (curl_opts_t *cop, xmlNodePtr nodep)
{
    const char *key;

    key = lx_node_name(nodep);
    if (key == NULL)
	return;

    if (streq(key, "url"))
	CURL_SET_STRING(cop->co_url);
    else if (streq(key, "method"))
	CURL_SET_STRING(cop->co_method);
    else if (streq(key, "username"))
	CURL_SET_STRING(cop->co_username);
    else if (streq(key, "password"))
	CURL_SET_STRING(cop->co_password);
    else if (streq(key, "upload"))
	cop->co_upload = TRUE;
    else if (streq(key, "fail-on-error"))
	cop->co_fail_on_error = TRUE;
    else if (streq(key, "field")) {
	
    } else if (streq(key, "param")) {
    }
}

/*
 * Record data from a libcurl callback into our curl handle.
 */
static size_t
ext_curl_record_data (curl_handle_t *curlp UNUSED, void *buf, size_t bufsiz,
		      curl_data_chain_t *chainp)
{
    curl_data_t *cdp;

    /*
     * We allocate an extra byte to allow us to NUL terminate it.  The
     * data is opaque to us, but will likely be a string, so we want
     * to allow this option.
     */
    cdp  = xmlMalloc(sizeof(*cdp) + bufsiz + 1);
    if (cdp == NULL)
	return 0;

    bzero(cdp, sizeof(*cdp));
    cdp->cd_len = bufsiz;
    memcpy(cdp->cd_data, buf, bufsiz);
    cdp->cd_data[bufsiz] = '\0';

    TAILQ_INSERT_TAIL(chainp, cdp, cd_link);

    return bufsiz;
}

/*
 * The callback we give libcurl to write data that has been received from
 * a transfer request.
 */
static size_t
ext_curl_write_data (void *buf, size_t membsize, size_t nmemb, void *userp)
{
    curl_handle_t *curlp = userp;
    size_t bufsiz = membsize * nmemb;

    ext_curl_record_data(curlp, buf, bufsiz, &curlp->ch_reply_data);

    return bufsiz;
}

/*
 * The callback we give libcurl to catch header data that has been received
 * from a server.
 */
static size_t 
ext_curl_header_data (void *buf, size_t membsize, size_t nmemb, void *userp)
{
    curl_handle_t *curlp = userp;
    size_t bufsiz = membsize * nmemb;

    ext_curl_record_data(curlp, buf, bufsiz, &curlp->ch_reply_headers);

    return bufsiz;
}

/*
 * The callback we give libcurl to fetch data when it is ready to write
 * to the server.
 */
static size_t
ext_curl_read_data (char *buf UNUSED, size_t isize, size_t nitems, void *userp)
{
    curl_handle_t *curlp UNUSED = userp;
    size_t bufsiz UNUSED = isize * nitems;

    /* XXX Fake our own death for now */
    return -1;
}

#define CURL_SET(_n, _v) curl_easy_setopt(curlp->ch_handle, _n, _v)

/*
 * Allocate a curl handle and populate it with reasonable values.  In
 * truth, we wrap the real libcurl handle inside our own structure, so
 * this is a "half us and half them" thing.
 */
static curl_handle_t *
ext_curl_handle_alloc (void)
{
    curl_handle_t *curlp = xmlMalloc(sizeof(*curlp));
    static unsigned seed = 1618; /* Non-zero starting number (why not phi?) */

    if (curlp) {
	bzero(curlp, sizeof(*curlp));
	/* Give it a unique number as a name */
	snprintf(curlp->ch_name, sizeof(curlp->ch_name), "%u", seed++);
	TAILQ_INIT(&curlp->ch_reply_data);
	TAILQ_INIT(&curlp->ch_reply_headers);

	/* Create and populate the real libcurl handle */
	curlp->ch_handle = curl_easy_init();

	CURL_SET(CURLOPT_ERRORBUFFER, curlp->ch_error); /* Get real errors */
	CURL_SET(CURLOPT_NETRC, CURL_NETRC_OPTIONAL); /* Allow .netrc */

	/* Register callbacks */
	CURL_SET(CURLOPT_WRITEFUNCTION, ext_curl_write_data);
	CURL_SET(CURLOPT_WRITEDATA, curlp);
	CURL_SET(CURLOPT_HEADERFUNCTION, ext_curl_header_data);
	CURL_SET(CURLOPT_WRITEHEADER, curlp);

	/* Add it to the list of curl handles */
	TAILQ_INSERT_TAIL(&ext_curl_sessions, curlp, ch_link);
    }

    return curlp;
}

static void
ext_curl_open (xmlXPathParserContext *ctxt, int nargs)
{
    curl_handle_t *curlp;

    if (nargs != 0) {
	/* Error: too many args */
	xmlXPathSetArityError(ctxt);
	return;
    }

    curlp = ext_curl_handle_alloc();

    /* Return session cookie */
    xmlXPathReturnString(ctxt, xmlStrdup((const xmlChar *) curlp->ch_name));
}

/*
 * Usage:
 *    expr curl:close($handle); 
 *
 *  Closes the given handle.
 */
static void
ext_curl_close (xmlXPathParserContext *ctxt, int nargs)
{
    char *name = NULL;
    curl_handle_t *curlp;

    if (nargs != 1) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    name = (char *) xmlXPathPopString(ctxt);
    if (name == NULL) {
	LX_ERR("curl:close: no argument\n");
	return;
    }

    curlp = ext_curl_handle_find(name);
    if (curlp)
	ext_curl_handle_free(curlp);

    xmlFree(name);

    valuePush(ctxt, xmlXPathNewNodeSet(NULL));
}

/*
 * Perform a libcurl transfer but setting up all the options
 * which we've been provided with, and then calling curl_easy_perform().
 * Since options are _very_ sticky to libcurl, we have to explicitly
 * set all values to the ones we've been asked for.
 */
static CURLcode
ext_curl_do_perform (curl_handle_t *curlp, curl_opts_t *opts)
{
    CURLcode success;
    long putv = 0, postv = 0, getv = 0;

    if (opts->co_url == NULL) {
	LX_ERR("curl: missing URL\n");
	return FALSE;
    }

    curl_easy_setopt(curlp->ch_handle, CURLOPT_URL, opts->co_url);

    /* Upload file via FTP */
    if (opts->co_upload) {
	curl_easy_setopt(curlp->ch_handle, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curlp->ch_handle, CURLOPT_READFUNCTION,
			 ext_curl_read_data);
	curl_easy_setopt(curlp->ch_handle, CURLOPT_READDATA, curlp);
    }
    
    CURL_SET(CURLOPT_USERNAME, opts->co_username);
    CURL_SET(CURLOPT_PASSWORD, opts->co_password);

    if (opts->co_method) {
	if (streq(opts->co_method, "get"))
	    getv = 1;
	else if (streq(opts->co_method, "put"))
	    putv = 1;
	else if (streq(opts->co_method, "post"))
	    postv = 1;
    }
    if (getv + postv + putv == 0)
	getv = 1;		/* Default method */

    CURL_SET(CURLOPT_HTTPGET, getv);
    CURL_SET(CURLOPT_PUT, putv);
    CURL_SET(CURLOPT_POST, postv);

    if (postv) {
	CURL_SET(CURLOPT_POSTFIELDS, opts->co_postfields);
	if (opts->co_postfields == NULL)
	    CURL_SET(CURLOPT_POSTFIELDSIZE, 0L);
    }

    if (opts->co_fail_on_error)
	CURL_SET(CURLOPT_FAILONERROR, 1L); /* Fail if HTTP code >= 400 */

    success = curl_easy_perform(curlp->ch_handle);

    return success;
}

/*
 * Turn a chain of data strings into XML content
 */
static void
ext_curl_build_data (curl_handle_t *curlp UNUSED, lx_document_t *docp,
		     xmlNodePtr nodep, curl_data_chain_t *chainp,
		     const char *name)
{
    char *buf, *cp;
    size_t bufsiz = 0;
    curl_data_t *cdp;
    xmlNodePtr tp, xp;

    bufsiz = 0;
    TAILQ_FOREACH(cdp, chainp, cd_link) {
	bufsiz += cdp->cd_len;
    }

    if (bufsiz == 0)		/* No data */
	return;

    tp = xmlNewText(NULL);
    if (tp == NULL)
	return;

    cp = buf = xmlMalloc(bufsiz + 1);
    if (buf == NULL)
	return;

    /* Populate buf with the chain of data */
    TAILQ_FOREACH(cdp, chainp, cd_link) {
	memcpy(cp, cdp->cd_data, cdp->cd_len);
	cp += cdp->cd_len;
    }
    *cp = '\0';			/* NUL terminate content */

    tp->content = (xmlChar *) buf;

    xp = xmlNewDocNode(docp, NULL, (const xmlChar *) name, NULL);
    if (xp) {
	xmlAddChild(nodep, xp);
	xmlAddChild(xp, tp);
    }
}

/*
 * Turn the raw header value into a set of real XML tags:
       <header>
        <version>HTTP/1.1</version>
        <code>404</code>
        <message>Not Found</message>
        <field name="Content-Type">text/html</field>
        <field name="Content-Length">345</field>
        <field name="Date">Mon, 08 Aug 2011 03:40:21 GMT</field>
        <field name="Server">lighttpd/1.4.28 juisebox</field>
      </header>
 */
static void
ext_curl_build_reply_headers (curl_handle_t *curlp, lx_document_t *docp,
			      xmlNodePtr parent)
{
    if (curlp->ch_reply_headers.tqh_first == NULL)
	return;

    xmlNodePtr nodep = xmlNewDocNode(docp, NULL,
			      (const xmlChar *) "header", NULL);
    if (nodep == NULL)
	return;

    xmlAddChild(parent, nodep);

    int count = 0;
    size_t len;
    char *cp, *sp, *ep;
    curl_data_t *cdp;

    TAILQ_FOREACH(cdp, &curlp->ch_reply_headers, cd_link) {
	len = cdp->cd_len;
	if (len > 0 && cdp->cd_data[len - 1] == '\n')
	    len -= 1;		/* Drop trailing newlines */
	if (len > 0 && cdp->cd_data[len - 1] == '\r')
	    len -= 1;		  /* Drop trailing returns */
	cdp->cd_data[len] = '\0'; /* NUL terminate it */

	cp = cdp->cd_data;
	ep = cp + len;

	if (count++ == 0) {
	    /*
	     * The first "header" is the HTTP response code.  This is
	     * formatted as "HTTP/v.v xxx message" so we must pull apart
	     * the value into distinct fields.
	     */
	    sp = memchr(cp, ' ', ep - cp);
	    if (sp == NULL)
		continue;

	    *sp++ = '\0';
	    xmlAddChildContent(docp, nodep, (const xmlChar *) "version",
			       (const xmlChar *) cp);

	    cp = sp;
	    sp = memchr(cp, ' ', ep - cp);
	    if (sp == NULL)
		continue;

	    *sp++ = '\0';
	    xmlAddChildContent(docp, nodep, (const xmlChar *) "code",
			       (const xmlChar *) cp);
	    if (*sp)
		xmlAddChildContent(docp, nodep,
				   (const xmlChar *) "message",
				   (const xmlChar *) sp);

	} else {
	    /* Subsequent headers are normal "field: value" lines */
	    sp = memchr(cp, ':', ep - cp);
	    if (sp == NULL)
		continue;

	    *sp++ = '\0';
	    while (*sp == ' ')
		sp += 1;
	    xmlNodePtr xp = xmlAddChildContent(docp, nodep,
				   (const xmlChar *) "field",
				   (const xmlChar *) sp);
	    if (xp)
		xmlSetProp(xp, (const xmlChar *) "name", (const xmlChar *) cp);
	}
    }
}

/*
 * Build the results XML hierarchy, using the headers and data
 * returned from the server.
 *
 * NOTE: libcurl defines success as success at the libcurl layer, not
 * success at the protocol level.  This means that "something was
 * transfered" which doesn't really mean "success".  An error will
 * often still result in data being transfered, such as a "404 not
 * found" giving some html data to display to the user.  So you
 * typically don't want to test for success, but look at the
 * header/code value:
 *
 *    if (curl-success && header/code && header/code < 400) { ... }
 */
static xmlNodePtr 
ext_curl_build_results (lx_document_t *docp, curl_handle_t *curlp,
			curl_opts_t *opts, CURLcode success)
{
    xmlNodePtr nodep = xmlNewDocNode(docp, NULL,
			      (const xmlChar *) "results", NULL);

    xmlAddChildContent(docp, nodep, (const xmlChar *) "url",
		       (const xmlChar *) opts->co_url);

    if (success == 0) {
	xmlNodePtr xp = xmlNewDocNode(docp, NULL,
				      (const xmlChar *) "curl-success", NULL);
	if (xp)
	    xmlAddChild(nodep, xp);

	/* Add header information, raw and cooked */
	ext_curl_build_data(curlp, docp, nodep,
			    &curlp->ch_reply_headers, "raw-headers");
	ext_curl_build_reply_headers(curlp, docp, nodep);

	/* Add raw data string */
	ext_curl_build_data(curlp, docp, nodep,
			    &curlp->ch_reply_data, "raw-data");
    } else {
	xmlAddChildContent(docp, nodep, (const xmlChar *) "error",
		       (const xmlChar *) curlp->ch_error);
    }

    return nodep;
}

/*
 * Parse a set of option values and store them in an options structure
 */
static void
ext_curl_options_parse (curl_handle_t *curlp UNUSED, curl_opts_t *opts,
			xmlXPathObject *ostack[], int nargs)
{
    int osi;

    for (osi = nargs - 1; osi >= 0; osi--) {
	xmlXPathObject *xop = ostack[osi];

	if (xop->stringval) {
	    if (opts->co_url)
		xmlFree(opts->co_url);
	    opts->co_url = (char *) xmlStrdup(xop->stringval);

	} else if (xop->nodesetval) {
	    xmlNodeSetPtr nodeset;
	    xmlNodePtr nop, cop;
	    int i;

	    nodeset = xop->nodesetval;
	    for (i = 0; i < nodeset->nodeNr; i++) {
		nop = nodeset->nodeTab[i];

		if (nop->type == XML_ELEMENT_NODE)
		    ext_curl_parse_node(opts, nop);

		if (nop->children == NULL)
		    continue;

		for (cop = nop->children; cop; cop = cop->next) {
		    if (cop->type != XML_ELEMENT_NODE)
			continue;

		    ext_curl_parse_node(opts, cop);
		}
	    }
	}
    }
}

/*
 * Set parameters for a curl handle
 * Usage:
      var $handle = curl:open();
      expr curl:set($handle, $opts, $more-opts);
 */
static void
ext_curl_set (xmlXPathParserContext *ctxt, int nargs)
{
    xmlXPathObject *ostack[nargs];	/* Stack for objects */
    curl_handle_t *curlp;
    char *name;
    int osi;

    if (nargs < 1) {
	LX_ERR("curl:execute: too few arguments error\n");
	return;
    }

    for (osi = nargs - 1; osi >= 0; osi--)
	ostack[osi] = valuePop(ctxt);

    name = (char *) xmlXPathCastToString(ostack[0]);
    if (name == NULL) {
	LX_ERR("curl:set: no argument\n");
	goto fail;
    }

    curlp = ext_curl_handle_find(name);
    if (curlp == NULL) {
	trace(trace_file, TRACE_ALL,
	      "curl:set: unknown handle: %s\n", name);
	xmlFree(name);
	goto fail;
    }

    /*
     * The zeroeth element of the ostack is the handle name, so we
     * have skip over it.
     */
    ext_curl_options_parse(curlp, &curlp->ch_opts, ostack + 1, nargs - 1);

    xmlXPathReturnString(ctxt, xmlStrdup((const xmlChar *) ""));

 fail:
    for (osi = nargs - 1; osi >= 0; osi--)
	if (ostack[osi])
	    xmlXPathFreeObject(ostack[osi]);
}

/*
 * Main curl entry point for transfering files.
 * Usage:
      var $handle = curl:open();
      var $res = curl:perform($handle, $url, $opts, $more-opts);
 */
static void
ext_curl_perform (xmlXPathParserContext *ctxt, int nargs)
{
    xmlXPathObject *ostack[nargs];	/* Stack for objects */
    curl_handle_t *curlp;
    char *name;
    CURLcode success;
    int osi;
    xmlNodePtr nodep;
    xsltTransformContextPtr tctxt;
    xmlXPathObjectPtr ret;
    xmlDocPtr container;
    curl_opts_t co;

    if (nargs < 1) {
	LX_ERR("curl:execute: too few arguments error\n");
	return;
    }

    for (osi = nargs - 1; osi >= 0; osi--)
	ostack[osi] = valuePop(ctxt);

    name = (char *) xmlXPathCastToString(ostack[0]);
    if (name == NULL) {
	LX_ERR("curl:execute: no argument\n");
	return;
    }

    curlp = ext_curl_handle_find(name);
    if (curlp == NULL) {
	trace(trace_file, TRACE_ALL,
	      "curl:execute: unknown handle: %s\n", name);
	xmlFree(name);
	goto fail;
    }

    /*
     * We make a local copy of our options that the parameters will
     * affect, but won't be saved.
     */
    ext_curl_options_copy(&co, &curlp->ch_opts);

    ext_curl_options_parse(curlp, &co, ostack + 1, nargs - 1);

    success = ext_curl_do_perform(curlp, &co);

    /*
     * Create a Result Value Tree container, and register it with RVT garbage
     * collector.
     */
    tctxt = xsltXPathGetTransformContext(ctxt);
    container = xsltCreateRVT(tctxt);
    xsltRegisterLocalRVT(tctxt, container);

    nodep = ext_curl_build_results(container, curlp, &co, success);

    xmlAddChild((xmlNodePtr) container, nodep);
    xmlNodeSet *results = xmlXPathNodeSetCreate(NULL);
    xmlXPathNodeSetAdd(results, nodep);
    ret = xmlXPathNewNodeSetList(results);

    ext_curl_options_release(&co);

    valuePush(ctxt, ret);
    xmlXPathFreeNodeSet(results);

 fail:
    for (osi = nargs - 1; osi >= 0; osi--)
	if (ostack[osi])
	    xmlXPathFreeObject(ostack[osi]);
}

static void
ext_curl_single (xmlXPathParserContext *ctxt UNUSED, int nargs UNUSED)
{
    xmlXPathObject *ostack[nargs];	/* Stack for objects */
    curl_handle_t *curlp;
    CURLcode success;
    int osi;
    xmlNodePtr nodep;
    xsltTransformContextPtr tctxt;
    xmlXPathObjectPtr ret;
    xmlDocPtr container;

    if (nargs < 1) {
	LX_ERR("curl:execute: too few arguments error\n");
	return;
    }

    for (osi = nargs - 1; osi >= 0; osi--)
	ostack[osi] = valuePop(ctxt);

    curlp = ext_curl_handle_alloc();
    if (curlp == NULL) {
	trace(trace_file, TRACE_ALL, "curl:fetch: alloc failed");
	goto fail;
    }

    ext_curl_options_parse(curlp, &curlp->ch_opts, ostack, nargs);

    success = ext_curl_do_perform(curlp, &curlp->ch_opts);

    /*
     * Create a Result Value Tree container, and register it with RVT garbage
     * collector.
     */
    tctxt = xsltXPathGetTransformContext(ctxt);
    container = xsltCreateRVT(tctxt);
    xsltRegisterLocalRVT(tctxt, container);

    nodep = ext_curl_build_results(container, curlp, &curlp->ch_opts, success);

    xmlAddChild((xmlNodePtr) container, nodep);
    xmlNodeSet *results = xmlXPathNodeSetCreate(NULL);
    xmlXPathNodeSetAdd(results, nodep);
    ret = xmlXPathNewNodeSetList(results);

    ext_curl_handle_free(curlp);

    valuePush(ctxt, ret);
    xmlXPathFreeNodeSet(results);

 fail:
    for (osi = nargs - 1; osi >= 0; osi--)
	if (ostack[osi])
	    xmlXPathFreeObject(ostack[osi]);
}

void
ext_curl_init (void)
{
    TAILQ_INIT(&ext_curl_sessions);

    (void) lx_extension_register(CURL_FULL_NS, "close", ext_curl_close);
    (void) lx_extension_register(CURL_FULL_NS, "perform", ext_curl_perform);
    (void) lx_extension_register(CURL_FULL_NS, "single", ext_curl_single);
    (void) lx_extension_register(CURL_FULL_NS, "open", ext_curl_open);
    (void) lx_extension_register(CURL_FULL_NS, "set", ext_curl_set);
}
