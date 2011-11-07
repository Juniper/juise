/*
 * $Id: extensions.c 413295 2010-12-01 06:39:06Z rsankar $
 *
 * Copyright (c) 2005-2008, Juniper Networks, Inc.
 * All rights reserved.
 *
 * Extension functions for commit scripts
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <resolv.h>
#include <regex.h>
#include <pwd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <dirent.h>
#include <paths.h>

#include <libxml/xpathInternals.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <libxslt/extensions.h>
#include <libexslt/exslt.h>
#include <libslax/slax.h>

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

/* The definition in <libxslt/xslt.h> lacks the 'const' and gives gcc fits */
#define XSLT_NAMESPACE_CONST \
    ((const xmlChar *) "http://www.w3.org/1999/XSL/Transform")

#ifdef O_EXLOCK
#define OPEN_FLAGS (O_CREAT | O_RDWR | O_EXLOCK)
#else
#define OPEN_FLAGS (O_CREAT | O_RDWR)
#endif
#define OPEN_PERMS (S_IRWXU | S_IRWXG | S_IRWXO)

extern char *source_daemon_name;

js_boolean_t
ext_jcs_fix_namespaces (lx_node_t *node)
{
    /*
     * Free the current namespace, meaning the one in which
     * this element is defined.  We do this because dealing
     * with namespaces is an immense pain.  We do need to
     * keep the "xnm" prefix.
     */
    trace(trace_file, CS_TRC_RPC, "ns fixup for '%s'", node->name);
    if (node->ns) {
	if (node->ns->prefix == NULL
	    || !streq((const char *) node->ns->prefix, XNM_QUALIFIER))
	    node->ns = NULL;
    }

    /*
     * Now we look for the "junos" namespace, and replace the
     * current definition with a "well-known" one.  The namespace
     * string returned by JUNOS commands/daemons/etc contains has
     * a version component, which makes writing scripts immensely
     * painful.
     */
    if (node->nsDef) {
	xmlNs *nsp;
	for (nsp = node->nsDef; nsp != NULL; nsp = nsp->next) {
	    trace(trace_file, CS_TRC_RPC, "nsDef: '%s' -> '%s'",
		  nsp->prefix ?: (const xmlChar *) "",
		  nsp->href ?: (const xmlChar *) "");
	    if (nsp->prefix && streq((const char *) nsp->prefix, JUNOS_NS)) {
		if (nsp->href != NULL)
		    xmlFree((char *) const_drop(nsp->href));
		nsp->href = xmlStrdup((const xmlChar *) JUNOS_FULL_NS);
	    }
	}
    }

    /*
     * Now we just recursively fix our children.
     */
    lx_node_t *child;
    for (child = lx_node_children(node); child; child = lx_node_next(child))
	ext_jcs_fix_namespaces(child);

    return FALSE;
}

#ifdef XXX_UNUSED
static char *ext_jcs_auth_info;

/*
 * Record authentication information to pass to sub-processes
 */
char *
ext_jcs_set_auth_info (char *str)
{
    char *old = ext_jcs_auth_info;
    ext_jcs_auth_info = str;
    return old;
}

/*
 * Get authentication information 
 */
char *
ext_jcs_get_auth_info (void)
{
    return ext_jcs_auth_info;
}

/*
 * Get the authentication information for a particular parameter
 */
void
ext_jcs_extract_authinfo (const char *key, char *value, size_t size)
{
    const char *auth_info = ext_jcs_get_auth_info();
    char *cp;

    if (auth_info && (cp = strstr(auth_info, key))) {
        const char ws[] = " \t";
        size_t len;

	if (cp) {
	    cp = cp + strcspn(cp, ws); /* skip a word */
	    cp = strtrimws(cp); /* skip white space */
	    len = strcspn(cp, ws);
	    strlcpy(value, cp, (len < size) ? (len + 1) : size);
	}
    }
}
#endif /* XXX_UNUSED */

/*
 * Build a node containing a text node
 */
static xmlNode *
ext_jcs_make_text_node (xmlNs *nsp, const xmlChar *name,
		    const xmlChar *content, int len)
{
    xmlNode *newp = xmlNewNode(nsp, name);

    if (newp == NULL)
	return NULL;
    
    xmlNode *tp = xmlNewTextLen(content, len);
    if (tp == NULL) {
	xmlFreeNode(newp);
	return NULL;
    }

    xmlAddChildList(newp, tp);
    return newp;
}

/*
 * lx_ext_jcs_rpc: turns an xnm:rpc element into the results of that RPC.
 * Open a child process for "cli xml-mode" and pass it the RPC,
 * wrapped in a JUNOScript session.  Read the results of the RPC
 * and lift out the rpc-reply element, which we return.
 */
static lx_nodeset_t *
ext_jcs_rpc (xmlXPathParserContext *ctxt, lx_node_t *rpc_node,
	     const xmlChar *rpc_name)
{
    js_session_t *jsp;
    lx_nodeset_t *results = NULL;

    jsp = js_session_open(NULL, NULL, NULL, 0, 0, 0);
    if (jsp == NULL)
	return NULL;

    /*
     * Invoked with a simple string argument?  Handle it.
     */
    if (rpc_name)
	results = js_session_execute(ctxt, NULL, NULL, rpc_name, ST_DEFAULT);

    /*
     * Extract the RPC, which is well hidden in fancy libxml2
     * data structures.  Error checking and structure walking
     * code here is hopefully flexible enough to handle it.
     */
    if (rpc_node)
	results = js_session_execute(ctxt, NULL, rpc_node, NULL, ST_DEFAULT);

    js_session_close(NULL, 0);

    return results;
}

/*
 * Invoke a JUNOScript RPC, given in an argument and "return"
 * the results.
 *
 * Usage: var $out = jcs:invoke('get-chassis-inventory');
 */
static void
ext_jcs_invoke (xmlXPathParserContext *ctxt, int nargs)
{
    xsltTransformContextPtr tctxt;
    xmlXPathObjectPtr ret;

    if (nargs != 1) {
	LX_ERR("xnm:invoke: argument error (need exactly one)\n");
	return;
    }

    xmlXPathObject *xop = valuePop(ctxt);
    if (xop == NULL) {
	LX_ERR("xnm:invoke: null argument\n");
	return;
    }

    /*
     * Invoked with a simple string argument?  Handle it.
     */
    if (xop->stringval) {
	lx_nodeset_t *results = ext_jcs_rpc(ctxt, NULL, xop->stringval);

	xmlXPathFreeObject(xop);

	if (results == NULL)
	    return;

	tctxt = xsltXPathGetTransformContext(ctxt);
	ret = xmlXPathNewNodeSetList(results);
	slaxSetPreserveFlag(tctxt, ret);
	valuePush(ctxt, ret);
	xmlXPathFreeNodeSet(results);
	return;
    }

    /*
     * Extract the RPC, which is well hidden in fancy libxml2
     * data structures.  Error checking and structure walking
     * code here is hopefully flexible enough to handle it.
     */
    if (xop->nodesetval) {
	int i = 0;
	lx_node_t *nop, *cop;

	if (xop->nodesetval->nodeNr == 0) {
	    LX_ERR("xnm:invoke: empty input nodeset\n");
	    xmlXPathFreeObject(xop);
	    return;
	}

	for (i = 0; i < xop->nodesetval->nodeNr; i++) {
	    nop = xop->nodesetval->nodeTab[i];
	    if (nop == NULL)
		goto invalid;

	    if (nop->children == NULL)
		continue;

	    /*
	     * Whiffle thru the children looking for an "rpc" node
	     */
	    for (cop = nop->children; cop; cop = cop->next) {
		if (cop->type != XML_ELEMENT_NODE)
		    continue;

		/*
		 * Okay, so now we've found the tag.  Do the RPC and
		 * process the results.
		 */
		lx_nodeset_t *results = ext_jcs_rpc(ctxt, cop, NULL);
		if (results == NULL) {
		    xmlXPathFreeObject(xop);
		    return;
		}

		tctxt = xsltXPathGetTransformContext(ctxt);
		ret = xmlXPathNewNodeSetList(results);
		slaxSetPreserveFlag(tctxt, ret);
		valuePush(ctxt, ret);
		xmlXPathFreeNodeSet(results);
		xmlXPathFreeObject(xop);
		return;
	    }
	}
    }

 invalid:
    LX_ERR("xnm:invoke: invalid argument\n");
}

/*
 * Extract the information from the xml passed as second argument to jcs:open
 * function
 */
static void 
ext_jcs_extract_second_arg (xmlNodeSetPtr nodeset, xmlChar **username, 
			xmlChar **passphrase, session_type_t *stype, uint *port)
{
    lx_node_t *nop, *cop;
    const char *value, *key;
    int i;

    *stype = ST_DEFAULT;    /* Default session */

    for (i = 0; i < nodeset->nodeNr; i++) {
	nop = nodeset->nodeTab[i];

	if (nop->children == NULL)
	    continue;

	for (cop = nop->children; cop; cop = cop->next) {
	    if (cop->type != XML_ELEMENT_NODE)
		continue;

	    key = lx_node_name(cop);
	    if (!key)
		continue;
	    value = lx_node_value(cop);

	    if (streq(key, "username")) {
		*username = xmlStrdup((const xmlChar *) value);
		continue;
	    } 

	    if (streq(key, "port")) {
		*port = atoi(value);
		continue;
	    } 

	    if (streq(key, "passphrase") || streq(key, "password")) {
		*passphrase = xmlStrdup((const xmlChar *) value);
		continue;
	    } 
	    
	    if (streq(key,  "method")) {
		*stype = jsio_session_type(value);
		continue;
	    }
	}
    }
}

/*
 * Extract server name and method information from session cookie
 */
static void 
ext_jcs_extract_scookie (xmlNodeSetPtr nodeset, xmlChar **server, 
		     session_type_t *stype)
{
    lx_node_t *nop, *cop;
    const char *value, *key;
    int i;

    *stype = ST_DEFAULT;    /* Default session */

    if (nodeset == NULL)
	return;

    for (i = 0; i < nodeset->nodeNr; i++) {
	nop = nodeset->nodeTab[i];

	if (nop->children == NULL)
	    continue;

	for (cop = nop->children; cop; cop = cop->next) {
	    if (cop->type != XML_ELEMENT_NODE)
		continue;

	    key = lx_node_name(cop);
	    value = lx_node_value(cop);

	    if (streq(key, "server")) {
		*server = xmlStrdup((const xmlChar *) value);
	    } else if (streq(key, "method")) {
		if (value && streq(value, "netconf")) {
		    *stype = ST_NETCONF;
		} else if (value && streq(value, "junos-netconf")) {
		    *stype = ST_JUNOS_NETCONF;
		} else if (value && streq(value, "junoscript")) {
		    *stype = ST_JUNOSCRIPT;
		}
	    }
	}
    }
}

/*
 * Usage:
 *    var $connection = jcs:open();  
 *    var $connection = jcs:open($server);  
 *    var $connection = jcs:open($server, $username, $passphrase);  
 *    var $connection = jcs:open($server, $blob);
 *             where $blob := {
 *                       <method> "junoscript" | "netconf";
 *                       <username> $username;
 *                       <passphrase> $passphrase;
 *                  }
 *
 * Opens junoscript or netconf connection with the local device or a remote 
 * device. Returns a connection handle which can be used to execute multiple 
 * rpcs and finally  close the connection using jcs:close()  
 *
 * When the method is not specified (in the first three form of usage), the
 * default connection is junoscript.
 *
 * When the $server is not passed opens a junoscript connection with local 
 * device. 
 *
 * e.g) $connection = jcs:open() -> Opens local junoscript connection 
 *
 * When $server is passed opens the specified $method connection with 
 * the server.
 *     $connection = jcs:open("fivestar") -> open junoscript connection with 
 *                                           'fivestar' router. 
 *
 */
static void
ext_jcs_open (xmlXPathParserContext *ctxt, int nargs)
{
    xsltTransformContextPtr tctxt;
    xmlXPathObjectPtr ret;
    xmlDocPtr container;
    xmlChar *server = NULL;
    xmlChar *passphrase = NULL;
    xmlChar *username = NULL;
    js_session_t *jsp = NULL;
    xmlNode *nodep, *serverp, *methodp;
    xmlXPathObject *xop = NULL;
    session_type_t stype = ST_DEFAULT; /* Default session */
    const char *sname = "junoscript";
    uint port = DEFAULT_NETCONF_PORT;

    if (nargs == 0) {
	server = NULL;

    } else if (nargs == 1) {
	server =  xmlXPathPopString(ctxt);

    } else if (nargs == 2) {
	xop = valuePop(ctxt);
	server = xmlXPathPopString(ctxt);

	if (!xop->nodesetval || !xop->nodesetval->nodeNr) {
	    LX_ERR("jcs:open invalid second parameter\n");
	    xmlXPathFreeObject(xop);
	    xmlFree(server);
	    valuePush(ctxt, xmlXPathNewNodeSet(NULL));
	    return;
	}

	ext_jcs_extract_second_arg(xop->nodesetval, &username, &passphrase, 
			       &stype, &port);
	xmlXPathFreeObject(xop);

    } else if (nargs == 3) {
	passphrase = xmlXPathPopString(ctxt);
	username = xmlXPathPopString(ctxt);
	server = xmlXPathPopString(ctxt);

    } else {
	/* Error: too many args */
	xmlXPathSetArityError(ctxt);
	return;
    }

    jsp = js_session_open((const char *) server, (const char *) username,
			  (const char *) passphrase, 0, 0, stype);
    if (jsp == NULL) {
	trace(trace_file, TRACE_ALL,
	      "Error in creating the session with \"%s\" server",
	      (char *) server ?: "local");

	xmlFreeAndEasy(passphrase);
	xmlFreeAndEasy(username);

	valuePush(ctxt, xmlXPathNewNodeSet(NULL));
	return;
    }
	
    /*
     * Create session cookie
     */
    nodep = xmlNewNode(NULL, (const xmlChar *) "cookie");

    if (!server)
	server = xmlStrdup((const xmlChar *) "");

    serverp = ext_jcs_make_text_node(NULL, (const xmlChar *) "server",
				 server, xmlStrlen(server));
    xmlAddChild(nodep, serverp);

    sname = jsio_session_type_name(stype);
    methodp = ext_jcs_make_text_node(NULL, (const xmlChar *) "method",
				 (const xmlChar *) sname, strlen(sname));

    xmlAddSibling(serverp, methodp);
    xmlAddChild(nodep, methodp);

    xmlFreeAndEasy(passphrase);
    xmlFreeAndEasy(username);
    xmlFreeAndEasy(server);

    /*
     * Create a Result Value Tree container, and register it with RVT garbage 
     * collector. 
     */
    tctxt = xsltXPathGetTransformContext(ctxt);
    container = xsltCreateRVT(tctxt);
    xsltRegisterLocalRVT(tctxt, container);

    xmlAddChild((xmlNodePtr) container, nodep);
 
    ret = xmlXPathNewNodeSet(nodep);
    slaxSetPreserveFlag(tctxt, ret);

    valuePush(ctxt, ret);
    return;
}

/*
 * Usage:
 *    expr jcs:close($connection); 
 *
 *  Closes the given connection.
 */
static void
ext_jcs_close (xmlXPathParserContext *ctxt, int nargs)
{
    xmlXPathObject *xop = NULL;
    xmlChar *server = NULL;
    session_type_t stype = ST_DEFAULT; /* Default session */

    if (nargs != 1) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    xop = valuePop(ctxt);
    if (xop == NULL) {
	LX_ERR("xnm:invoke: null argument\n");
	return;
    }
    ext_jcs_extract_scookie(xop->nodesetval, &server, &stype);

    js_session_close((char *) server, stype);

    xmlXPathFreeObject(xop);
    xmlFree(server);

    valuePush(ctxt, xmlXPathNewNodeSet(NULL));
    return;
}

/*
 * Usage:
 *     var $results = jcs:execute($connection,  $rpc);
 *
 * Takes the connection handle,  execute the given rpc in the connection
 * context and returns the results.  Multiple RPCs can be executed in the
 * given connection context till the connection is closed.
 *
 * e.g) $results = jcs:execute($connection, 'get-software-version');
 *
 * var $rpc = <get-interface-information> {
 *                <terse>;
 *            }
 * $results = jcs:execute($connection, $rpc);
 */
static void
ext_jcs_execute (xmlXPathParserContext *ctxt, int nargs)
{
    xsltTransformContextPtr tctxt;
    xmlXPathObjectPtr ret;
    xmlChar *server = NULL;
    session_type_t stype = ST_DEFAULT; /* Default session */

    if (nargs != 2) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    xmlXPathObject *xop = valuePop(ctxt);
    if (xop == NULL) {
	LX_ERR("jcs:execute: null argument\n");
	return;
    }

    xmlXPathObject *sop = valuePop(ctxt);
    if (sop == NULL) {
	xmlXPathFreeObject(xop);
	LX_ERR("jcs:execute: null argument\n");
	return;
    }
    ext_jcs_extract_scookie(sop->nodesetval, &server, &stype);
    if (server == NULL) {
	xmlXPathFreeObject(xop);
	xmlXPathFreeObject(sop);
	LX_ERR("jcs:execute: null argument\n");
	return;
	
    }

    /*
     * Invoked with a simple string argument?  Handle it.
     */
    if (xop->stringval) {
	lx_nodeset_t *results = 
	    js_session_execute(ctxt, (char *) server, NULL,
			       xop->stringval, stype);

	xmlXPathFreeObject(xop);
	xmlXPathFreeObject(sop);
	xmlFree(server);

	if (results == NULL)
	    return;

	tctxt = xsltXPathGetTransformContext(ctxt);
	ret = xmlXPathNewNodeSetList(results);
	slaxSetPreserveFlag(tctxt, ret);
	valuePush(ctxt, ret);
	xmlXPathFreeNodeSet(results);
	return;
    }

    /*
     * Extract the RPC, which is well hidden in fancy libxml2
     * data structures.  Error checking and structure walking
     * code here is hopefully flexible enough to handle it.
     */
    if (xop->nodesetval) {
	int i = 0;
	lx_node_t *nop, *cop;

	if (xop->nodesetval->nodeNr == 0) {
	    LX_ERR("xnm:invoke: empty input nodeset\n");
	    xmlXPathFreeObject(xop);
	    xmlXPathFreeObject(sop);
	    xmlFree(server);
	    return;
	}

	for (i = 0; i < xop->nodesetval->nodeNr; i++) {
	    nop = xop->nodesetval->nodeTab[i];
	    if (nop == NULL)
		goto invalid;

	    if (nop->children == NULL)
		continue;

	    /*
	     * Whiffle thru the children looking for an "rpc" node
	     */
	    for (cop = nop->children; cop; cop = cop->next) {
		if (cop->type != XML_ELEMENT_NODE)
		    continue;

		/*
		 * Okay, so now we've found the tag.  Do the RPC and
		 * process the results.
		 */
		lx_nodeset_t *results;

		results = js_session_execute(ctxt, (char *) server,
					     cop, NULL, stype);
		xmlXPathFreeObject(xop);
		xmlXPathFreeObject(sop);
		xmlFree(server);

		if (results == NULL)
		    return;

		tctxt = xsltXPathGetTransformContext(ctxt);
		ret = xmlXPathNewNodeSetList(results);
		slaxSetPreserveFlag(tctxt, ret);
		valuePush(ctxt, ret);
		xmlXPathFreeNodeSet(results);
		return;
	    }
	}
    }

 invalid:

    xmlXPathFreeObject(xop);
    xmlXPathFreeObject(sop);
    xmlFree(server);

    LX_ERR("xnm:invoke: invalid argument\n");
    return;
}

/*
 * Usage:
 *    expr jcs:get-hello($connection); 
 *
 * Given the connection handle will return the hello packet received from the 
 * netconf server during session establishment. 
 *
 * If the passed connection handle belongs a Junoscript session then empty
 * nodeset will be returned.
 *  
 */
static void
ext_jcs_gethello (xmlXPathParserContext *ctxt, int nargs)
{
    xsltTransformContextPtr tctxt;
    xmlDocPtr container;
    xmlXPathObject *xop = NULL, *ret;
    xmlChar *server = NULL;
    session_type_t stype = ST_DEFAULT; /* Default session */
    lx_node_t *hellop, *newhellop;

    if (nargs != 1) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    xop = valuePop(ctxt);
    if (xop == NULL) {
	LX_ERR("xnm:invoke: null argument\n");
	return;
    }
    ext_jcs_extract_scookie(xop->nodesetval, &server, &stype);

    hellop = js_gethello((char *) server, stype);

    if (hellop) {
	/*
	* Create a Result Value Tree container, and register it with RVT 
	* garbage collector. 
	*
	* Take copy of hello packet and attach that with RVT container, so
	* that the copied node will be freed when the container is freed.
	*/
	tctxt = xsltXPathGetTransformContext(ctxt);
	container = xsltCreateRVT(tctxt);
	xsltRegisterLocalRVT(tctxt, container);

	newhellop = xmlCopyNode(hellop, 1);
	xmlAddChild((xmlNodePtr) container, newhellop);
 
	ret = xmlXPathNewNodeSet(newhellop);
	slaxSetPreserveFlag(tctxt, ret);
	valuePush(ctxt, ret);
    } else {
	valuePush(ctxt, xmlXPathNewNodeSet(NULL));
    }

    xmlXPathFreeObject(xop);
    xmlFree(server);
}

static void
ext_jcs_getprotocol (xmlXPathParserContext *ctxt, int nargs)
{
    xmlXPathObject *xop = NULL;
    xmlNodeSetPtr nodeset;
    lx_node_t *nop, *cop;
    const char *key, *value;
    int i;

    if (nargs != 1) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    xop = valuePop(ctxt);
    if (xop == NULL) {
	LX_ERR("xnm:invoke: null argument\n");
	return;
    }

    nodeset = xop->nodesetval;
    for (i = 0; i < nodeset->nodeNr; i++) {
	nop = nodeset->nodeTab[i];

	if (nop->children == NULL)
	    continue;

	for (cop = nop->children; cop; cop = cop->next) {
	    if (cop->type != XML_ELEMENT_NODE)
		continue;

	    key = lx_node_name(cop);

	    if (!key)
		continue;

	    if (streq(key, "method")) {
		value = lx_node_value(cop);
		if (value) {
		    xmlXPathReturnString(ctxt,
					 xmlStrdup((const xmlChar *) value));
		} else {
		    xmlXPathReturnString(ctxt,
					 xmlStrdup((const xmlChar *) ""));
		}

		xmlXPathFreeObject(xop);
		return;
	    }
	}
    }

    xmlXPathFreeObject(xop);
    xmlXPathReturnString(ctxt, xmlStrdup((const xmlChar *) ""));
}

/*
 * Easy access to the C library function call.  Returns the
 * hostname of the given ip address.
 *
 * Usage:  var $name = jcs:hostname($address);
 */
static void
ext_jcs_hostname (xmlXPathParserContext *ctxt, int nargs)
{
    if (nargs == 0) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    char *str = (char *) xmlXPathPopString(ctxt);
    if (str == NULL) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    int family = AF_UNSPEC;
    union {
        struct in6_addr in6;
        struct in_addr in;
    } addr;

    if (inet_pton(AF_INET, str, &addr) > 0) {
        family = AF_INET;
    } else if (inet_pton(AF_INET6, str, &addr) > 0) {
        family = AF_INET6;
    }

    xmlChar *outstr = NULL;
    struct hostent *hp = NULL;

    if (family == AF_UNSPEC) {
	hp = gethostbyname(str);

    } else {
	int retrans = _res.retrans, retry = _res.retry;

	_res.retrans = 1; /* Store new values */
	_res.retry = 1;

	int addr_size = (family == AF_INET)
	    ? sizeof(addr.in) : sizeof(addr.in6);

	hp = gethostbyaddr((char *) &addr, addr_size, family);

	_res.retrans = retrans;     /* Restore old values */
	_res.retry = retry;
    }

    if (hp && hp->h_name)
	outstr = xmlStrdup((const xmlChar *) hp->h_name);
    else
	outstr = xmlStrdup((const xmlChar *) "");

    xmlFree(str);
    xmlXPathReturnString(ctxt, outstr);
}

/*
 *  Usage:
 *  var $output = jcs:parse-ip(ipaddress-v4 or v6 address/prefix-length or 
 *                             netmask);
 * Returns:
 *      $output[1] => Hostaddress or NULL if case of error
 *      $output[2] => Address family ("inet4" for ipv4 and "inet6" for ipv6)
 *      $output[3] => Prefix length
 *      $output[4] => Network address
 *      $output[5] => Netmask for ipv4 and blank in case of ipv6 
 *
 * Example:
 *   var $results = jcs:parse-ip("10.1.2.4/25");
 *   var $results = jcs:parse-ip("10.1.2.10/255.255.255.0");
 *   var $results = jcs:parse-ip("080:0:0:0:8:800:200C:417A/100") 
 */
static void
ext_jcs_parse_ip (xmlXPathParserContext *ctxt, int nargs)
{
    xsltTransformContextPtr tctxt;
    xmlDocPtr container;
    xmlXPathObjectPtr ret;
    char *str;
    struct in6_addr addr, mask;
    char errmsg[BUFSIZ];
    int pfxseen = 0, maskseen = 0, af = AF_UNSPEC;
    size_t pfxlen = 0;
    xmlNode *newp, *last = NULL;
    xmlNodeSet *results;
    parse_retcode_t status;
    char address[IP_ADDR_BUFLEN];
    u_int32_t *v4_net = NULL, *v4_msk = NULL;
    struct in6_addr v6_net;

    if (nargs != 1) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    str = (char *) xmlXPathPopString(ctxt);
    if (str == NULL) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    /*
     * Create a Result Value Tree container, and register it with RVT garbage 
     * collector. 
     */
    results =  xmlXPathNodeSetCreate(NULL);
    tctxt = xsltXPathGetTransformContext(ctxt);
    container = xsltCreateRVT(tctxt);
    xsltRegisterLocalRVT(tctxt, container);

    /*
     * Parse the given IP address. Given IP address can be either IPV4 or IPV6
     */
    status = parse_ipaddr(&af, str, PIF_LEN | PIF_MASK, &addr, sizeof(addr), 
			  &pfxseen, &pfxlen, &maskseen, &mask, sizeof(mask), 

			  errmsg, sizeof(errmsg));

    /*
     * Bail out if there is an error in IP parsing
     */
    if (status == PARSE_ERR || status == PARSE_ERR_RESTRICT) {
	LX_ERR("parse-ip error: %s\n", errmsg);
	goto done;
    }

    if (af == AF_INET) {                                /* IPv4 */
	v4_net = (u_int32_t *) &addr;
	v4_msk = (u_int32_t *) &mask;

	if (pfxseen) {
	    /*
	     * Prefix length specified, figure out netmask
	     */
	    if (pfxlen == 0) {
		*v4_msk = 0; 
	    } else {
		*v4_msk = htonl(~((1<<(IN_HOST_PLEN - pfxlen)) - 1));
	    }
	} 

	if (maskseen) {
	    u_int32_t tmp_msk = *v4_msk;

	    /*
	     * Netmask specified, find out prefixlen.
	     *
	     * Prefixlen = Number of bits set in netmask
	     */
	     for (pfxlen = 0; tmp_msk; tmp_msk >>= 1) {
		 pfxlen += tmp_msk & 1;
	     }
	}
    } else if (af == AF_INET6) {                        /* IPv6 */
	u_char byte, first_bits;

	/*
	 * In case of IPv6 only prefix length will be specified. Find out
	 * the network part using prefix length.
	 */
	
	memcpy(&v6_net, &addr, sizeof(v6_net));
	/*
	 * Find the last network byte
	 */
	byte = ((pfxlen +  NBBY - 1) / NBBY) - 1;

	/*
	 * If the prefix length is not exactly divisible by 8 then it means
	 * some bits in last network byte is part of host. Unset those bits 
	 */
	if (pfxlen % NBBY) {
	    first_bits = 0xff << (NBBY - (pfxlen % NBBY));
	    v6_net.s6_addr[byte] &= first_bits;
	}

	/*
	 * Assign zero to host part bytes. Host part starts from last network 
	 * byte plus one.
	 */
	for (byte = byte + 1; byte < sizeof(addr); byte++) {
	    v6_net.s6_addr[byte] = 0;
	}
    } else {
	LX_ERR("parse-ip error: Unknown address family\n");
	goto done;
    }

    /* Output: results[1] =>  Address */
    inet_ntop(af, &addr, address, sizeof(address));
    newp = ext_jcs_make_text_node(NULL, (const xmlChar *) "parse-ip",
			      (const xmlChar *) address, strlen(address));
    if (newp) {
	xmlXPathNodeSetAdd(results, newp);

	xmlAddChild((xmlNodePtr) container, newp);
	last = newp;
    }

    /* Output: results[2] => inet or inet6 */
    const char *tag = (af == AF_INET) ? "inet" : "inet6";
    newp = ext_jcs_make_text_node(NULL, (const xmlChar *) "parse-ip",
			      (const xmlChar *) tag, strlen(tag));

    if (newp) {
	xmlXPathNodeSetAdd(results, newp);

	if (last)
	    xmlAddSibling(last, newp);

	xmlAddChild((xmlNodePtr) container, newp);
	last = newp;
    }

    if (maskseen || pfxseen) {

	/* Output: results[3] => Prefix length */
	snprintf(address, sizeof(address), "%d", (int) pfxlen);
	newp = ext_jcs_make_text_node(NULL, (const xmlChar *) "parse-ip",
				  (const xmlChar *) address, strlen(address));

	if (newp) {
    	    xmlXPathNodeSetAdd(results, newp);

    	    if (last)
    		xmlAddSibling(last, newp);

	    xmlAddChild((xmlNodePtr) container, newp);
    	    last = newp;
       	}


	/* Output: results[4] => Network part */
	if (af == AF_INET) {
    	    *v4_net = (*v4_net) & (*v4_msk);
    	    inet_ntop(af, v4_net, address, sizeof(address));
	} else {
    	    inet_ntop(af, &v6_net, address, sizeof(address));
	}

	newp = ext_jcs_make_text_node(NULL, (const xmlChar *) "parse-ip",
				  (const xmlChar *) address, strlen(address));
	if (newp) {
    	    xmlXPathNodeSetAdd(results, newp);

    	    if (last)
    		xmlAddSibling(last, newp);

	    xmlAddChild((xmlNodePtr) container, newp);
    	    last = newp;
       	}

	/* Output: results[5] => Netmask only in case of ipv4 */
	if (af == AF_INET) {
	    inet_ntop(af, v4_msk, address, sizeof(address));
	    newp = ext_jcs_make_text_node(NULL, (const xmlChar *) "parse-ip",
				      (const xmlChar *) address,
				      strlen(address));

	    if (newp) {
		xmlXPathNodeSetAdd(results, newp);

		if (last)
		    xmlAddSibling(last, newp);

		xmlAddChild((xmlNodePtr) container, newp);
		last = newp;
	    }
	}
    }


 done:

    xmlFree(str);

    ret = xmlXPathNewNodeSetList(results);
    slaxSetPreserveFlag(tctxt, ret);
    valuePush(ctxt, ret);
    xmlXPathFreeNodeSet(results);
    return;
}

/*
 * This function calculates the diffrence between times 'new' and 'old'
 * by subtracting 'old' from 'new' and put the result in 'diff'.
 */
static void
time_diff (const struct timeval *new, const struct timeval *old,
	   struct timeval *diff)
{
    long sec, usec;

    if (!diff)
	return;

    if (new->tv_usec < old->tv_usec) {
	usec = new->tv_usec + 1000000;
	sec = new->tv_sec - 1;
    } else {
	usec = new->tv_usec;
	sec = new->tv_sec;
    }

    diff->tv_sec = sec - old->tv_sec;
    diff->tv_usec = usec - old->tv_usec;
}

/*
 * Usage: 
 *     var $rc = jcs:dampen($tag, max, frequency);
 *
 * dampen function returns true/false based on number of times the
 * function call made by a script. If dampen function is called less
 * than the 'max' times in last 'frequency' of minutes then it will
 * return 'true' which means a success otherwise it will return
 * 'false' which means the call is failed. The values for 'max' and
 * 'frequency' should be passed as greater than zero.
 */
static void
ext_jcs_dampen (xmlXPathParserContext *ctxt, int nargs)
{
    char filename[MAXPATHLEN + 1];
    char new_filename[MAXPATHLEN + 1];
    char buf[BUFSIZ], *cp, *tag;
    FILE *old_fp = NULL, *new_fp = NULL;
    struct stat sb;
    struct timeval tv, rec_tv, diff;
    int fd, rc, max, freq, no_of_recs = 0;

    /* Get the time of invocation of this function */
    gettimeofday(&tv, NULL);
    
    if (nargs != 3) {
	xmlXPathSetArityError(ctxt);
	return;
    } else {
	xmlChar *max_str, *freq_str, *tag_str;

	freq_str = xmlXPathPopString(ctxt);
	freq = xmlAtoi(freq_str);
	max_str = xmlXPathPopString(ctxt);
	max = xmlAtoi(max_str);
	tag_str = xmlXPathPopString(ctxt);
	tag = (char *) tag_str;
	xmlFree(freq_str);
	xmlFree(max_str);
    }

    if (max <= 0 || freq <= 0) {
	LX_ERR("Values less than or equal to zero are not valid\n");
	return;
    }

    /*
     * Creating the absolute filename by appending '_PATH_VARRUN' and
     * the tag, and check whether this file is already present.
     */
    snprintf(filename, sizeof(filename), "%s%s", _PATH_VARRUN, tag);

    xmlFree(tag);

    rc = stat(filename, &sb);

    /* If the file is already present then open it in the read mode */
    if (rc != -1) {
	old_fp = fopen(filename, "r");
	if (!old_fp) {
	    LX_ERR("File open failed for file: %s\n", filename);
	    return;
	}
    }

    /*
     * Create the file to write the new records with the name of
     * <filename+> and associating a stream with this to write on
     * it
     */
    snprintf(new_filename, sizeof(new_filename), "%s+", filename);
    fd = open(new_filename, OPEN_FLAGS, OPEN_PERMS);
    if (fd == -1) {
	LX_ERR("File open failed for file: %s\n", new_filename);
	return;
    } else
	new_fp = fdopen(fd, "w+");

    /* If the old records are present corresponding to this tag */
    if (old_fp) {
	while (fgets(buf, sizeof(buf), old_fp)) {
	    cp = strchr(buf, '-');
	    *cp = '\0';
	    rec_tv.tv_sec = strtol(buf, NULL, 10);
	    cp++;
	    rec_tv.tv_usec = strtol(cp, NULL, 10);
	    time_diff(&tv, &rec_tv, &diff);

	    /*
	     * If the time difference between this record and current
	     * time stamp then write it into the new file
	     */
	    if (diff.tv_sec < (freq * 60) ||
		(diff.tv_sec == (freq * 60) && diff.tv_usec == 0)) {
		memset(buf, '\0', sizeof(buf));
		snprintf(buf, sizeof(buf), "%lu-%lu\n",
			 (unsigned long) rec_tv.tv_sec,
			 (unsigned long) rec_tv.tv_usec);
		if (fputs(buf, new_fp)) {
		    LX_ERR("Write operation is failed\n");
		    return;
		}
		no_of_recs++;
	    }
	}
	fclose(old_fp);
    }

    if (no_of_recs < max) {
	memset(buf, '\0', sizeof(buf));
	snprintf(buf, sizeof(buf), "%lu-%lu\n",
		 (unsigned long) tv.tv_sec, (unsigned long) tv.tv_usec);
	if (fputs(buf, new_fp)) {
	    LX_ERR("Write operation is failed\n");
	    return;
	}
	no_of_recs++;
	rc = TRUE;
    } else
	rc = FALSE;

    /*
     * Closing the <filename+>, delete the <filename> and move <filename+>
     * to <filename>
     */
    fclose(new_fp);
    unlink(filename);
    rename(new_filename, filename);


    if (rc == TRUE)
	xmlXPathReturnTrue(ctxt);
    else
	xmlXPathReturnFalse(ctxt);
}

/*
 * Register our extension functions.  We try to hide all the
 * details of the libxslt interactions here.
 */
int
ext_jcs_register_all (void)
{
    slaxExtRegisterOther (JCS_FULL_NS);

    (void) lx_extension_register(JCS_FULL_NS, "close", ext_jcs_close);
    (void) lx_extension_register(JCS_FULL_NS, "dampen", ext_jcs_dampen);
    (void) lx_extension_register(JCS_FULL_NS, "execute", ext_jcs_execute);
    (void) lx_extension_register(JCS_FULL_NS, "get-hello", ext_jcs_gethello);
    (void) lx_extension_register(JCS_FULL_NS, "get-protocol",
				 ext_jcs_getprotocol);
    (void) lx_extension_register(JCS_FULL_NS, "hostname", ext_jcs_hostname);
    (void) lx_extension_register(JCS_FULL_NS, "invoke", ext_jcs_invoke);
    (void) lx_extension_register(JCS_FULL_NS, "open", ext_jcs_open);
    (void) lx_extension_register(JCS_FULL_NS, "parse-ip", ext_jcs_parse_ip);

    return 0;
}
