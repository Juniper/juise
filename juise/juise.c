/*
 * $Id$
 *
 * Copyright (c) 2010-2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * juise -- driver for libjuise, allowing remote access for scripting
 */

#include <err.h>
#include <time.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <pwd.h>
#include <sys/socket.h>

#include <libxml/tree.h>
#include <libxml/dict.h>
#include <libxml/uri.h>
#include <libxml/xpathInternals.h>
#include <libxml/parserInternals.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxslt/transform.h>
#include <libxml/HTMLparser.h>
#include <libxml/xmlsave.h>
#include <libexslt/exslt.h>
#include <libxslt/xsltutils.h>
#include <libslax/slax.h>
#include <libslax/slaxdata.h>
#include <libslax/xmlsoft.h>

#include "juiseconfig.h"
#include <libjuise/string/strextra.h>
#include <libjuise/time/timestr.h>
#include <libjuise/xml/libxml.h>
#include <libjuise/io/trace.h>
#include <libjuise/io/jtrace.h>
#include <libjuise/xml/jsio.h>
#include <libjuise/xml/extensions.h>
#include <libjuise/xml/xmlrpc.h>
#include <libjuise/xml/juisenames.h>

#include "juise.h"

#define JM_NONE		0	/* Normal mode */
#define JM_CGI		1	/* CGI mode */
#define JM_CSCRIPT	2	/* Commit script mode */

static slax_data_list_t plist;
static int nbparams;

int dump_all;

static char **opt_args;
int opt_debugger;		/* Invoke the debugger */
int opt_indent;			/* Pretty-print XML output */
static int opt_load;		/* Under-implemented */
static int opt_local;		/* Run a local (server-based) script */
static int opt_user_info_on_stdin;/* User info is sent on stdin to execute 
				     RPC on localhost */
const char *opt_output_format = "compare";
char *opt_username;
char *opt_target;
char *opt_auth_socket;

static rpc_media_type_map_t mtypemap[] = {
    { MEDIA_TYPE_TEXT_HTML, RPC_FORMAT_HTML },
    { MEDIA_TYPE_TEXT_PLAIN, RPC_FORMAT_TEXT },
    { MEDIA_TYPE_APPLICATION_XML, RPC_FORMAT_XML },
    { MEDIA_TYPE_APPLICATION_JSON, RPC_FORMAT_JSON }
};

static void
juise_make_param (const char *pname, const char *pvalue, int quoted_string)
{
    slax_data_node_t *dnp;
    int isname = 1;

    if (pname == NULL || pvalue == NULL)
	errx(1, "missing parameter value");

    SLAXDATALIST_FOREACH(dnp, &plist) {
	if (isname) {
	    if (streq(pname, dnp->dn_data)) {
		trace(trace_file, TRACE_ALL,
		      "param: ignoring dup: '%s'", pname);
		return;
	    }
	}
	isname ^= 1;
    }

    if (quoted_string) {
	int plen = strlen(pvalue);
	char *tvalue = alloca(plen + 3);
	char quote = strrchr(pvalue, '\"') ? '\'' : '\"';

	tvalue[0] = quote;
	memcpy(tvalue + 1, pvalue, plen);
	tvalue[plen + 1] = quote;
	tvalue[plen + 2] = '\0';
	pvalue = tvalue;
    }

    nbparams += 1;
    slaxDataListAddNul(&plist, pname);
    slaxDataListAddNul(&plist, pvalue);

    trace(trace_file, TRACE_ALL, "param: '%s' -> '%s'", pname, pvalue);
}

static lx_node_t *
juise_add_node (lx_document_t *docp, lx_node_t *parent,
		const char *tag, const char *content)
{
    static char tag_ok[] =
	"abcdefghijklmnopqrstuwxyzABCDEFGHIJKLMNOPQRSTUWXYZ-_";
    lx_node_t *nodep;
    
    if (strspn(tag, tag_ok) != strlen(tag)) {
	trace(trace_file, TRACE_ALL,
	      "add_node: invalid node name; skipping: '%s'/'%s'",
	      tag, content ?: "");
	return NULL;
    }

    nodep = xmlNewDocNode(docp, NULL,
			  (const xmlChar *) tag, (const xmlChar *) content);
    if (nodep == NULL)
	return NULL;

    xmlAddChild(parent, nodep);

    return nodep;
}

static lx_document_t *
juise_build_input (const char *root_name, lx_node_t *nodep,
		   lx_nodeset_t *nodesetp)
{
    lx_document_t *docp;
    lx_node_t *input, *childp;
    char *value;
    struct passwd *pwd;
    char hostname[MAXHOSTNAMELEN];
    time_t now;
    int i;

    docp = xmlNewDoc((const xmlChar *) XML_DEFAULT_VERSION);
    if (docp == NULL)
	goto fail;

    docp->standalone = 1;
    docp->dict = xmlDictCreate();

    input = xmlNewDocNode(docp, NULL, (const xmlChar *) root_name, NULL);

    while (input) {		/* Not _really_ a loop, but.... */
	xmlDocSetRootElement(docp, input);

	/* Add the junos namespace */
	xmlNewNs(input, (const xmlChar *) JUNOS_FULL_NS,
		 (const xmlChar *) JUNOS_NS);

	if (nodep)
	    xmlAddChild(input, nodep);

	nodep = xmlNewDocNode(docp, NULL,
			      (const xmlChar *) ELT_JUNOS_CONTEXT, NULL);
	if (nodep == NULL)
	    break;
	xmlAddChild(input, nodep);

	/* Hostname */
	if (gethostname(hostname, sizeof(hostname)) == 0)
	    juise_add_node(docp, nodep, ELT_HOST_NAME, hostname);

	juise_add_node(docp, nodep, ELT_PRODUCT, "juise");

	/* Time */
	time(&now);
	value = strndup(ctime(&now), 24);
	if (value) {
	    juise_add_node(docp, nodep, ELT_LOCALTIME, value);
	    free(value);
	}
	juise_add_node(docp, nodep, ELT_LOCALTIME_ISO, time_isostr(&now));

	juise_add_node(docp, nodep, ELT_SCRIPT_TYPE, "op");

	childp = xmlNewDocNode(docp, NULL,
			       (const xmlChar *) ELT_USER_CONTEXT, NULL);
	if (childp == NULL)
	    break;
	xmlAddChild(nodep, childp);

	if (opt_target)
	    juise_add_node(docp, childp, ELT_HOST_NAME, opt_target);

	if (opt_username)
	    juise_add_node(docp, childp, ELT_USER, opt_username);
	else {
	    pwd = getpwuid(getuid());
	    if (pwd) {
		char nbuf[10];
		juise_add_node(docp, childp, ELT_USER, pwd->pw_name);

#if 0
#ifdef HAVE_PWD_CLASS
		juise_add_node(docp, childp, ELT_CLASS_NAME, pwd->pw_class);
#endif
#endif

		snprintf(nbuf, sizeof(nbuf), "%d", (int) pwd->pw_uid);
		juise_add_node(docp, childp, ELT_UID, nbuf);
	    }
	}

	juise_add_node(docp, childp, ELT_OP_CONTEXT, "");

	if (opt_args) {
	    char **av = opt_args;

	    lx_node_t *anp = juise_add_node(docp, childp, ELT_ARGUMENTS, NULL);
	    for ( ; *av; av++)
		juise_add_node(docp, anp, ELT_ARGUMENT, *av);
	}

	/* Now we need to add the contents of the node set argument */
	lx_node_t *rootp = xmlDocGetRootElement(docp);

	if (nodesetp) {
	    for (i = 0; i < nodesetp->nodeNr; i++) {
		nodep = nodesetp->nodeTab[i];
		if (nodep == NULL)
		    continue;

		xmlNodePtr newp = xmlDocCopyNode(nodep, docp, 1);
		if (newp)
		    xmlAddChild(rootp, newp);
	    }
	}

	break;			/* Not really a loop */
    }

 fail:
    return docp;
}

/*
 * Emit the results for a CGI script.  If the top-level element
 * is <cgi>, then we lift out the attributes and turn them
 * into header fields and use the rest of the document as the result.
 */
static int
do_write_cgi_results (lx_document_t *res, xsltStylesheetPtr script)
{
    lx_node_t *root = lx_document_root(res);

    if (streq(xmlNodeName(root), ELT_CGI)) {
	xmlSaveCtxt *handle;
	lx_node_t *nodep;
	xmlAttrPtr attr;
	char *value;

	for (attr = root->properties; attr; attr = attr->next) {
	    if (attr->name[0] == 'x' && attr->name[0] == 'm'
			&& attr->name[0] == 'l')
		continue;

	    value = (char *) xmlGetProp(root, attr->name);
	    if (value == NULL)
		continue;

	    printf("%s: %s\n", attr->name, value);
	    xmlFree(value);
	}

	printf("\n");

	for (nodep = lx_node_children(root); nodep;
	     nodep = lx_node_next(nodep)) {
	    fflush(stdout);
	    handle = xmlSaveToFd(fileno(stdout), NULL,
				 XML_SAVE_FORMAT | XML_SAVE_NO_DECL);
	    if (handle) {
		xmlSaveTree(handle, nodep);
		xmlSaveFlush(handle);
		xmlSaveClose(handle);
		fflush(stdout);
	    }
	}

	return 0;
    }

    xsltSaveResultToFile(stdout, res, script);

    return 0;
}

static xsltStylesheetPtr
read_script (const char *scriptname)
{
    lx_document_t *scriptdoc;
    FILE *scriptfile;
    xsltStylesheetPtr script;

    if (scriptname == NULL)
	errx(1, "missing script name");

    scriptfile = fopen(scriptname, "r");
    if (scriptfile == NULL)
	err(1, "file open failed for '%s'", scriptname);

    scriptdoc = slaxLoadFile(scriptname, scriptfile, NULL, 0);
    if (scriptdoc == NULL)
	errx(1, "cannot parse: '%s'", scriptname);

    if (scriptfile != stdin)
	fclose(scriptfile);

    script = xsltParseStylesheetDoc(scriptdoc);
    if (script == NULL || script->errors != 0)
	errx(1, "%d errors parsing script: '%s'",
	     script ? script->errors : 1, scriptname);

    return script;
}

static lx_document_t *
run_script (xsltStylesheetPtr script, const char *scriptname,
	    lx_document_t *indoc, const char **params, int mode)
{
    lx_document_t *res = NULL;

    if (opt_indent)
	script->indent = 1;

    if (opt_debugger) {
	slaxDebugInit();
	slaxDebugSetStylesheet(script);
	res = slaxDebugApplyStylesheet(scriptname, script, "input",
				       indoc, params);
    } else {

	res = xsltApplyStylesheet(script, indoc, params);
	if (res) {
	    if (mode == JM_CGI)
		do_write_cgi_results(res, script);
	    else if (mode == JM_NONE)
		xsltSaveResultToFile(stdout, res, script);

	    if (dump_all)
		xsltSaveResultToFile(stderr, res, script);
	}
    }

    return res;
}

static int
do_run_op_common (const char *scriptname, const char *input,
		  char **argv UNUSED, lx_node_t *nodep, int cgi_mode)
{
    lx_document_t *indoc, *res = NULL;
    xsltStylesheetPtr script;
    slax_data_node_t *dnp;
    int i = 0;
    const char **params;

    params = alloca(nbparams * 2 * sizeof(*params) + 1);
    SLAXDATALIST_FOREACH(dnp, &plist) {
	params[i++] = dnp->dn_data;
    }

    params[i] = NULL;

    script = read_script(scriptname);
    if (script == NULL)
	return -1;

    if (input) {
	FILE *infile;

	infile = streq(input, "-") ? stdin : fopen(input, "r");
	if (infile == NULL)
	    err(1, "file open failed for '%s'", input);

	indoc = xmlReadFile(input, NULL, XSLT_PARSE_OPTIONS);
	if (indoc == NULL)
	    errx(1, "unable to read input document: %s", input);

	if (infile != stdin)
	    fclose(infile);

    } else {
	indoc = juise_build_input(ELT_OP_SCRIPT_INPUT, nodep, NULL);
	if (indoc == NULL)
	    errx(1, "unable to build input document");
    }

    res = run_script(script, scriptname, indoc, params, cgi_mode);
    if (res)
	xmlFreeDoc(res);

    xmlFreeDoc(indoc);
    xsltFreeStylesheet(script);

    return 0;
}

static int
do_run_op (const char *scriptname, const char *input, char **argv)
{
    return do_run_op_common(scriptname, input, argv, NULL, JM_NONE);
}

static int
do_emit_xml (const char *scriptname, const char *input, char **argv)
{
    xmlDocPtr docp;

    input = scriptname ?: input ?: *argv ? *argv++ : "/dev/stdin";

    docp = xmlReadFile(input, NULL, XSLT_PARSE_OPTIONS);
    if (docp == NULL) {
	errx(1, "cannot parse file: '%s'", input);
        return -1;
    }

    if (docp && docp->children) {
	lx_node_t *childp, *nextp;

	for (childp = docp->children; childp; childp = nextp) {
	    nextp = childp->next;
	    if (childp->type == XML_DTD_NODE
		|| childp->type == XML_ENTITY_DECL
		|| childp->type == XML_COMMENT_NODE) {
		xmlUnlinkNode(childp);
		xmlFreeNode(childp);
	    }
	}
    }

    slaxDumpToFd(fileno(stdout), docp, 0);

    xmlFreeDoc(docp);

    return 0;
}

static lx_node_t *
juise_build_get_configuration_rpc (lx_document_t **docpp, int post,
				   const char *format)
{
    char rpc_text[BUFSIZ];
    lx_document_t *xmlp;
    lx_node_t *rootp;

    snprintf(rpc_text, sizeof(rpc_text),
	     "<rpc><get-configuration%s%s%s/></rpc>",
	     post ? "" : " commit-scripts=\"view\"",
	     streq(format, "text") ? " format=\"text\"" : "",
	     streq(format, "compare") ? " compare=\"\"" : "");

    *docpp = xmlp = xmlReadMemory(rpc_text, strlen(rpc_text), "rpc_text",
				  NULL, XML_PARSE_NOENT);
    if (xmlp == NULL)
	return NULL;

    rootp = xmlDocGetRootElement(xmlp);
    if (rootp)
	ext_jcs_fix_namespaces(rootp);

    return rootp;
}

static lx_document_t *
juise_build_input_commit (lx_nodeset_t *config_data)
{
    lx_document_t *docp;
    lx_node_t *rootp, *nodep;
    int i;

    docp = juise_build_input(ELT_COMMIT_SCRIPT_INPUT, NULL, config_data);
    if (docp == NULL)
	return NULL;

    rootp = xmlDocGetRootElement(docp);

    for (i = 0; i < config_data->nodeNr; i++) {
	nodep = config_data->nodeTab[i];
	if (nodep == NULL)
	    continue;

	xmlNodePtr newp = xmlDocCopyNode(nodep, docp, 1);
	if (newp)
	    xmlAddChild(rootp, newp);
    }

    return docp;
}

static void
output_node (const char *title, lx_node_t *nodep)
{
    lx_output_t *handle = lx_output_open_fd(1);

    if (handle == NULL)
	return;

    printf("\n%s:\n", title);
    fflush(stdout);

    lx_output_node(handle, nodep);
    lx_output_close(handle);

    printf("\n");
    fflush(stdout);
}

static void
output_nodeset (const char *title, lx_nodeset_t *nsp)
{
    lx_output_t *handle = lx_output_open_fd(1);
    int i;
    lx_node_t *nodep;

    if (handle == NULL)
	return;

    printf("\nResults from %s:\n", title);
    fflush(stdout);

    for (i = 0; i < nsp->nodeNr; i++) {
	nodep = nsp->nodeTab[i];
	if (nodep == NULL || nodep->type != XML_ELEMENT_NODE)
	    continue;
	lx_output_node(handle, nodep);
    }

    lx_output_close(handle);
    printf("\n");
    fflush(stdout);
}

static int
result_has_success (lx_nodeset_t *nsp)
{
    int i;
    lx_node_t *nodep;

    for (i = 0; i < nsp->nodeNr; i++) {
	nodep = nsp->nodeTab[i];
	if (nodep == NULL || nodep->type != XML_ELEMENT_NODE)
	    continue;

	if (!streq((const char *) nodep->name,
		   ELT_LOAD_CONFIGURATION_RESULTS))
	    continue;

	for (nodep = nodep->children; nodep; nodep = nodep->next) {
	    if (nodep->type != XML_ELEMENT_NODE)
		continue;
	    if (streq((const char *) nodep->name, ELT_LOAD_SUCCESS))
		return TRUE;
	}
    }

    return FALSE;
}

static int
load_change (js_session_t *jsp UNUSED, xmlXPathParserContext *pctxt,
	     lx_node_t *changep, int transient)
{
    static char rpc_text[] = "<rpc><load-configuration \
xmlns:junos=\"http://xml.juniper.net/junos/*/junos\">\
<configuration/></load-configuration></rpc>\n";

    lx_document_t *docp;
    lx_node_t *rootp, *nodep, *confp = NULL, *childp;
    lx_nodeset_t *res;
    int rc = FALSE;

    docp = xmlReadMemory(rpc_text, strlen(rpc_text), "rpc_text",
			 NULL, XML_PARSE_NOENT);
    if (docp == NULL)
	return TRUE;

    rootp = xmlDocGetRootElement(docp);

    if (rootp && rootp->children && rootp->children->children)
	confp = rootp->children->children;

    if (confp == NULL || confp->type != XML_ELEMENT_NODE) {
	xmlFreeDoc(docp);
	return TRUE;
    }

    for (childp = changep->children; childp; childp = childp->next) {
	nodep = xmlDocCopyNode(childp, docp, 1);
	if (nodep)
	    xmlAddChild(confp, nodep);
    }

    res = js_session_execute(pctxt, NULL, rootp, NULL, ST_DEFAULT);
    if (res == NULL) {
	fprintf(stderr, "load-configuration failed");
	output_node("Failed to load the following configuration", changep);
	rc = TRUE;

    } else {
	output_nodeset(transient ? "load change-transient" : "load change",
		       res);
	if (!result_has_success(res)) {
	    output_node("Failed to load the following configuration",
			changep);
	    rc = TRUE;
	}
    }

    xmlFreeDoc(docp);
    return rc;
}

static int
invoke_rpc (js_session_t *jsp UNUSED, xmlXPathParserContext *pctxt,
	    const char *title, const char *rpc_text)
{
    lx_document_t *docp;
    lx_node_t *rootp;
    lx_nodeset_t *res;
    int rc = FALSE;

    docp = xmlReadMemory(rpc_text, strlen(rpc_text), "rpc_text",
			 NULL, XML_PARSE_NOENT);
    if (docp == NULL) {
	fprintf(stderr, "rpc failed to parse");
	return TRUE;
    }

    rootp = xmlDocGetRootElement(docp);

    res = js_session_execute(pctxt, NULL, rootp, NULL, ST_DEFAULT);
    if (res == NULL) {
	fprintf(stderr, "rpc execution failed");
	rc = TRUE;
    } else {
	output_nodeset(title, res);
	xmlXPathFreeNodeSet(res);
    }


    xmlFreeDoc(docp);

    return rc;
}

static int
run_edit_private (js_session_t *jsp UNUSED,
		  xmlXPathParserContext *pctxt UNUSED)
{
    static char rpc_text[] = "<rpc><open-configuration \
xmlns:junos=\"http://xml.juniper.net/junos/*/junos\">\
<private/></open-configuration></rpc>\n";

    return invoke_rpc(jsp, pctxt, "edit private", rpc_text);
}

static int
run_commit_check (js_session_t *jsp UNUSED,
		  xmlXPathParserContext *pctxt UNUSED)
{
    static char rpc_text[] = "<rpc><commit-configuration \
xmlns:junos=\"http://xml.juniper.net/junos/*/junos\">\
<check/></commit-configuration></rpc>\n";

    return invoke_rpc(jsp, pctxt, "commit check", rpc_text);
}

static void
report_error (const char *tag, lx_node_t *child)
{
    const char *path = lx_node_child_value(child, ELT_EDIT_PATH);
    const char *stmt = lx_node_child_value(child, ELT_STATEMENT);
    const char *msg = lx_node_child_value(child, ELT_MESSAGE);
    int indent = 0;

    if (path) {
	fprintf(stderr, "%s\n", path);
	indent += 2;
    }

    if (stmt) {
	fprintf(stderr, "%*s'%s'\n", indent, "", stmt);
	indent += 2;
    }
    
    fprintf(stderr, "%*s%s: %s\n", indent, "", tag, msg ?: "unknown");
}

static int
show_post_commit_config (js_session_t *jsp UNUSED,
			 xmlXPathParserContext *pctxt,
			 const char *format)
{
    lx_document_t *rpc = NULL;
    lx_node_t *get_config_rpc;
    lx_nodeset_t *res;

    get_config_rpc = juise_build_get_configuration_rpc(&rpc, TRUE, format);

    res = js_session_execute(pctxt, NULL, get_config_rpc,
				     NULL, ST_DEFAULT);
    if (res == NULL)
	err(0, "get-configuration (post) rpc failed");

    output_nodeset("script", res);
    xmlXPathFreeNodeSet(res);

    if (rpc)
	xmlFreeDoc(rpc);

    return FALSE;
}

static int
do_test_commit_script (const char *scriptname, const char *input UNUSED,
		       char **argv UNUSED)
{
    lx_document_t *rpc = NULL, *docp, *indoc, *res = NULL;
    xmlXPathContextPtr ctxt;
    xmlXPathParserContext *pctxt;
    lx_nodeset_t *config_data;
    lx_node_t *get_config_rpc;
    xsltStylesheetPtr script;
    const char **params;
    slax_data_node_t *dnp;
    int i = 0;
    js_session_t *jsp;
    int rc = FALSE;

    params = alloca(nbparams * 2 * sizeof(*params) + 1);
    SLAXDATALIST_FOREACH(dnp, &plist) {
	params[i++] = dnp->dn_data;
    }
    params[i] = NULL;

    docp = xmlNewDoc((const xmlChar *) XML_DEFAULT_VERSION);
    ctxt = xmlXPathNewContext(docp);
    pctxt = xmlXPathNewParserContext(NULL, ctxt);

    jsp = js_session_open(NULL, 0);
    if (jsp == NULL)
	errx(1, "could not open session to target");
    
    get_config_rpc = juise_build_get_configuration_rpc(&rpc, FALSE, "xml");

    config_data = js_session_execute(pctxt, NULL, get_config_rpc,
				     NULL, ST_DEFAULT);
    if (config_data == NULL)
	err(0, "get-configuration rpc failed");

    if (rpc)
	xmlFreeDoc(rpc);

    script = read_script(scriptname);
    if (script == NULL)
	return -1;

    indoc = juise_build_input_commit(config_data);

    res = run_script(script, scriptname, indoc, params, JM_CSCRIPT);
    if (res) {
	/*
	 * We need to look through the output of a commit script,
	 * which can contain:
	 * <error> -- report and stop
	 * <warning> -- report and continue
	 * <change> -- normal configuration change
	 * <change-transient> -- transient configuration change
	 * <syslog> -- report (but don't syslog)
	 * <progress> -- report
	 */
	int seen_error = 0, seen_change = 0, seen_transient = 0;
	lx_node_t *childp;
	lx_node_t *rootp = xmlDocGetRootElement(res);

	if (rootp == NULL)
	    errx(1, "could not find document root");

	for (childp = rootp->children; childp; childp = childp->next) {
	    if (childp->type != XML_ELEMENT_NODE)
		continue;

	    if (streq((const char *) childp->name, ELT_ERROR)) {
		seen_error += 1;
		report_error("error", childp);

	    } else if (streq((const char *) childp->name, ELT_WARNING)) {
		report_error("warning", childp);

	    } else if (streq((const char *) childp->name, ELT_CHANGE)) {
		/* Wait for second pass */
		seen_change += 1;

	    } else if (streq((const char *) childp->name,
			     ELT_CHANGE_TRANSIENT)) {
		/* Wait for second pass */
		seen_transient += 1;

	    } else if (streq((const char *) childp->name, ELT_SYSLOG)) {
		const char *msg = xmlNodeValue(childp);
		fprintf(stdout, "syslog: %s\n", msg);

	    } else if (streq((const char *) childp->name, ELT_PROGRESS)) {
		const char *msg = xmlNodeValue(childp);
		fprintf(stdout, "progress-message: %s\n", msg);

	    } else {
		fprintf(stderr, "unknown tag: '%s' (ignored)\n",
			(const char *) childp->name);
	    }
	}

	if (!seen_error && (seen_change || seen_transient)) {
	    if (run_edit_private(jsp, pctxt))
		goto done;

	    for (childp = rootp->children; childp; childp = childp->next) {
		if (childp->type != XML_ELEMENT_NODE)
		    continue;

		if (streq((const char *) childp->name, ELT_CHANGE))
		    rc = load_change(jsp, pctxt, childp, FALSE);

		else if (streq((const char *) childp->name,
			       ELT_CHANGE_TRANSIENT))
		    rc = load_change(jsp, pctxt, childp, TRUE);

		if (rc)
		    break;
	    }

	    if (!rc && !run_commit_check(jsp, pctxt) && opt_output_format)
		show_post_commit_config(jsp, pctxt, opt_output_format);
	}

    done:
	xmlFreeDoc(res);
    }

    xmlFreeDoc(indoc);
    xsltFreeStylesheet(script);
    js_session_close(NULL, 0);

    return 0;
}

static int
do_run_server_on_stdin (const char *scriptname UNUSED,
			const char *input UNUSED, char **argv UNUSED)
{
    int fd = 0;

    if (input && !streq(input, "-")) {
	fd = open(input, O_RDONLY);
	if (fd < 0)
	    err(1, "could not open file '%s'", input);
    }

    run_server(fd, 1, ST_DEFAULT);
    close(fd);
    return 0;
}

static void
parse_query_string (lx_document_t *docp, lx_node_t *nodep, char *str)
{
    char *cp, *ap, *ep, *xp;

    trace(trace_file, TRACE_ALL, "querystring: %p, (%u) '%s'",
	  nodep, (unsigned) strlen(str), str);

    for (cp = str; cp && *cp; cp = ap) {
	ep = strchr(cp, '=');
	if (ep == NULL)
	    break;
	*ep++ = '\0';

	ap = strchr(ep, '&');
	if (ap != NULL)
	    *ap++ = '\0';

	/* Pluses are really spaces; pluses are URL-escaped */
	for (xp = ep; *xp; xp++)
	    if (*xp == '+')
		*xp = ' ';

	/* At this point, cp is the name and ep is the value */
	cp = xmlURIUnescapeString(cp, 0, NULL);
	ep = xmlURIUnescapeString(ep, 0, NULL);
	
	if (cp && ep) {
	    trace(trace_file, TRACE_ALL, "querystring: param: '%s' -> '%s'",
		  cp, ep);
	    if (strncmp(cp, "junos", 5) != 0 && !streq(cp, ELT_CGI))
		juise_make_param(cp, ep, TRUE);
	    juise_add_node(docp, nodep, cp, ep);
	}
	xmlFreeAndEasy(ep);	/* xmlURIUnescapeString allocated them */
	xmlFreeAndEasy(cp);
    }
}

static int
do_run_as_cgi (const char *scriptname, const char *input UNUSED, char **argv)
{
    static const char *cgi_params[] = {
	"CONTENT_LENGTH",	"content-length",
	"DOCUMENT_ROOT",	"document-root",
	"GATEWAY_INTERFACE",	"gateway-interface",
	"HTTPS",		"https",
	"LD_LIBRARY_PATH",	"ld-library-path",
	"LD_PRELOAD",		"ld-preload",
	"PATH_INFO",		"path-info",
	"QUERY_STRING",		"query-string",
	"REDIRECT_STATUS",	"redirect-status",
	"REMOTE_ADDR",		"remote-addr",
	"REMOTE_PORT",		"remote-port",
	"REMOTE_USER",		"remote-user",
	"REQUEST_METHOD",	"request-method",
	"REQUEST_URI",		"request-uri",
	"SCRIPT_FILENAME",	"script-filename",
	"SCRIPT_NAME",		"script-name",
	"SERVER_ADDR",		"server-addr",
	"SERVER_NAME",		"server-name",
	"SERVER_PORT",		"server-port",
	"SERVER_PROTOCOL",	"server-protocol",
	"SERVER_SOFTWARE",	"server-software",
	"SYSTEMROOT",		"systemroot",
	NULL,			NULL,
    };

    int len = 0;
    int i, rc;
    lx_node_t *nodep, *paramp = NULL;
    char *method = NULL, *uri = NULL;
    char *cp;
    char buf[BUFSIZ];
    slax_data_list_t lines;
    char local_scriptname[MAXPATHLEN];

    slaxDataListInit(&lines);

    rc = chdir(JUISE_WEB_DIR);
    if (rc < 0)
        trace(trace_file, TRACE_ALL, "cannot change directory to %s: %s",
              JUISE_WEB_DIR, strerror(errno));

    lx_document_t *docp = xmlNewDoc((const xmlChar *) XML_DEFAULT_VERSION);
    if (docp == NULL)
	errx(1, "op: out of memory");

    nodep = xmlNewDocNode(docp, NULL, (const xmlChar *) ELT_CGI, NULL);
    if (nodep == NULL)
	errx(1, "op: out of memory");

    /* Turn all the CGI environment variables into XSLT parameters */
    for (i = 0; cgi_params[i]; i += 2) {
	cp = getenv(cgi_params[i]);
	if (cp) {
	    juise_make_param(cgi_params[i], cp, TRUE);
	    juise_add_node(docp, nodep, cgi_params[i + 1], cp);

	    trace(trace_file, TRACE_ALL, "cgi: env: '%s' = '%s'",
		  cgi_params[i], cp);

	    if (streq("request-method", cgi_params[i + 1]))
		method = cp;
	    else if (streq("request-uri", cgi_params[i + 1]))
		uri = cp;
	}
    }

    paramp = xmlNewDocNode(docp, NULL, (const xmlChar *) ELT_PARAMETERS, NULL);
    if (paramp == NULL)
	errx(1, "juise: out of memory");
    xmlAddChild(nodep, paramp);

    cp = getenv("QUERY_STRING");
    if (cp)
	parse_query_string(docp, paramp, cp);

    while (fgets(buf, sizeof(buf), stdin) != NULL) {
	trace(trace_file, TRACE_ALL, "cgi: stdin: %s", buf);
	i = strlen(buf);
	len += i;
	slaxDataListAddLen(&lines, buf, i);
    }

    if (len) {
	slax_data_node_t *dnp;
	char *bp;

	bp = malloc(len + 1);
	if (bp) {
	    len = 0;
	    SLAXDATALIST_FOREACH(dnp, &lines) {
		memcpy(bp + len, dnp->dn_data, dnp->dn_len);
		len += dnp->dn_len;
	    }
	    bp[len] = '\0';

	    if (method && streq(method, "POST"))
		juise_add_node(docp, nodep, ELT_CONTENTS, bp);

	    parse_query_string(docp, paramp, bp);

	    slaxDataListClean(&lines);
	    free(bp);
	}
    }

    juise_make_param("cgi", "/op-script-input/cgi", FALSE);

    if (opt_local) {
	/*
	 * Invoke local scripts.  The QUERY_STRING has the format
	 *
	 *    '/' leader '/' operation '/' params
	 * leader: is anything before the second slash, typically "/rpc/"
	 * operation: the script to invoke
	 * params: optional set of param values, encoded as "name[=value]"
	 *     pairs separated by '/'
	 */
	char *operation, *params;
	if (uri == NULL)
	    errx(1, "missing REQUEST_URI");

	operation = strchr(uri + 1, '/');
	if (operation == NULL || (operation[0] == '\0'))
	    errx(1, "missing operation");
	operation += 1;

	params = strchr(operation, '/');
	if (params)
	    *params++ = '\0';

	trace(trace_file, TRACE_ALL, "rpc: op [%s] p [%s]",
	      operation, params ?: "");

	snprintf(local_scriptname, sizeof(local_scriptname),
		 "%s/%s.slax", JUISE_CGI_DIR, operation);
	scriptname = local_scriptname;

	/* Populate the operation node with our parameter values */
	if (params) {
	    char *next;

	    for (; params && *params; params = next) {
		cp = strchr(params, '=');
		next = cp ? strchr(cp, '/') : NULL;
		if (cp)
		    *cp++ = '\0';
		if (next)
		    *next++ = '\0';

		params = xmlURIUnescapeString(params, 0, NULL);
		cp = xmlURIUnescapeString(cp, 0, NULL);

		juise_make_param(params, cp, TRUE);
		juise_add_node(docp, paramp, params, cp);
		trace(trace_file, TRACE_ALL, "cgi-local: env: '%s' = '%s'",
		      params, cp);

		xmlFreeAndEasy(params);
		xmlFreeAndEasy(cp);
	    }
	}
    }

    return do_run_op_common(scriptname, input, argv, nodep, JM_CGI);
}

static int
do_run_as_fastcgi (const char *scriptname UNUSED, const char *input UNUSED,
		   char **argv UNUSED)
{
    return 0;
}

static xmlNodePtr
makeLeafNode (xmlDocPtr docp, xmlNodePtr parent,
	      const char *name, const char *value)
{
    char *uname, *uvalue;
    xmlNodePtr childp, textp;

    trace(trace_file, TRACE_ALL, "querystring: param: '%s' -> '%s'",
	  name, value ?: "");

    uname = xmlURIUnescapeString(name, 0, NULL);
    childp = xmlNewDocNode(docp, NULL, (const xmlChar *) uname, NULL);
    if (childp == NULL)
	errx(1, "could not build parameter: '%s'", name);
    xmlAddChild(parent, childp);

    xmlFreeAndEasy(uname); /* xmlURIUnescapeString allocated them */

    if (value && *value) {
	uvalue = xmlURIUnescapeString(value, 0, NULL);
	textp = xmlNewText((const xmlChar *) uvalue);
	if (textp == NULL)
	    errx(1, "could not make node: text");

	xmlAddChild(childp, textp);
	xmlFreeAndEasy(uvalue);
    }

    return childp;
}

/*
 * Parses MIME list for known media types and return corresponding rpc format
 */
static char * 
rpc_parse_mime_list (char *str)
{
    char *cp, *ap, *ep;
    char *output_format = NULL;
    size_t i;

    for (cp = str; cp && *cp; cp = ap) {
	ap = strchr(cp, ',');

	if (ap != NULL)
    	    *ap++ = '\0';

	while (*cp == ' ') cp++;

	ep = strchr(cp, ';');

	if (ep != NULL)
	    *ep = '\0';

	/* Match against supported output formats */
	for (i = 0; i < sizeof(mtypemap) / sizeof(rpc_media_type_map_t); i++) {
	    if (!strncmp(mtypemap[i].rmtm_media_type, cp, strlen(cp))) {
		output_format = strdup(mtypemap[i].rmtm_output_format);
		break;
	    }
	}

	if (i == (sizeof(mtypemap) / sizeof(rpc_media_type_map_t))) {
	    output_format = strdup( MEDIA_TYPE_APPLICATION_XML );
	}

	if (output_format != NULL)
	    break;
    }

    return output_format;
}

/*
 * Converts RPC output format to equivalent MIME representation and writes it
 * to response Content-Type header
 */
static void 
rpc_write_content_type (const char *output_format) 
{
    size_t i;

    for (i = 0; i < sizeof(mtypemap) / sizeof(rpc_media_type_map_t); i++) {
	if (streq(mtypemap[i].rmtm_output_format, output_format)) {
	    fprintf(stdout, "Content-Type: %s\n\n", 
		    mtypemap[i].rmtm_media_type);
	    fflush(stdout);
	    break;
	}
    }

    if (i == (sizeof(mtypemap) / sizeof(rpc_media_type_map_t))) {
	fprintf(stdout, "Content-Type: " MEDIA_TYPE_APPLICATION_XML "\n\n");
	fflush(stdout);
    }
}

/*
 * Build XML node out of raw string data and attach it to given node
 */
static int
rpc_build_data (lx_document_t *docp, lx_node_t *nodep, char *str)
{
    int rc = 0;
    xmlDocPtr xmlp;
    xmlNodePtr newp, cur;

    if (str == NULL)
        return 1;

    xmlp = xmlReadMemory(str, strlen(str), NULL, NULL, XML_PARSE_NOENT);

    if (xmlp == NULL)
        return 1;

    xmlNodePtr childp = xmlDocGetRootElement(xmlp);
    if (childp) {
	cur = childp->xmlChildrenNode;
	while (cur != NULL) {
	    newp = xmlDocCopyNode(cur, docp, 1);
	    if (newp) {
		xmlAddChild(nodep, newp);
	    } else {
		rc = 1;
		goto exit;
	    }
	    cur = cur->next;
	}
    } else {
	rc = 1;
	goto exit;
    }

exit:
    xmlFreeDoc(xmlp);
    return rc;
}

static void
rpc_parse_query_string (lx_document_t *docp, lx_node_t *nodep, char *str)
{
    char *cp, *ap, *ep, *xp;

    trace(trace_file, TRACE_ALL, "querystring: %p, (%u) '%s'",
	  nodep, (unsigned) strlen(str), str);

    for (cp = str; cp && *cp; cp = ap) {
	ep = strchr(cp, '=');
	if (ep == NULL)
	    break;
	*ep++ = '\0';

	ap = strchr(ep, '&');
	if (ap != NULL)
	    *ap++ = '\0';

	/* Pluses are really spaces; pluses are URL-escaped */
	for (xp = ep; *xp; xp++)
	    if (*xp == '+')
		*xp = ' ';
	
	if (cp && ep)
	    makeLeafNode(docp, nodep, cp, ep);
    }
}

/*
 * do_run_rpc is used to give REST-ish access to NETCONF RPCs.  The
 * URL passed in has five parts:
 *
 *    '/' leader '/' target '/' operation ['@' attributes] '/' params
 *
 * leader: is anything before the second slash, typically "/rpc/"
 * target: target device for the RPC
 * operation: the RPC to invoke
 * attributes: optional set of attributes for the operation
 * params: optional set of param values, encoded as "name[=value]"
 *     pairs separated by '/'
 * In addition, parameter values can be encoded using the "?n=v" HTTP url
 * or as POST variables.
 *
 * We build the RPC and let the libjuise functions to the raw RPC.
 */
static int
do_run_rpc (const char *scriptname UNUSED, const char *input UNUSED,
	    char **argv UNUSED)
{
    char *cp, *uri, *target, *user = NULL, *pass = NULL;
    char *operation, *attributes, *params;
    lx_document_t *docp;
    xmlXPathContextPtr ctxt;
    xmlXPathParserContext *pctxt;
    lx_nodeset_t *reply;
    int i;
    char buf[BUFSIZ];
    xmlNodePtr rpc;
    xmlAttrPtr attr;
    slax_data_list_t lines;
    int len = 0;
    char *output_format = NULL, *content_type = NULL, *accept_header;
    char oper_delimiter = '/';

    docp = xmlNewDoc((const xmlChar *) XML_DEFAULT_VERSION);
    ctxt = xmlXPathNewContext(docp);
    pctxt = xmlXPathNewParserContext(NULL, ctxt);
    slaxDataListInit(&lines);

    uri = getenv("REQUEST_URI");
    if (uri == NULL || *uri == '\0')
	errx(1, "missing REQUEST_URI");

    target = strchr(uri + 1, '/');
    if (target == NULL || target[1] == '\0')
	errx(1, "missing target");
    target += 1;

    operation = strchr(target, '/');
    if (operation == NULL || operation[0] == '\0')
	errx(1, "missing operation");
    *operation++ = '\0';

    params = strchr(operation, oper_delimiter);

    if (!params && strchr(operation, '?')) {
        oper_delimiter = '?';
        params = strchr(operation, oper_delimiter);
    } else if (params && strchr(operation, '?')) {
        if (params > strchr(operation, '?')) {
            oper_delimiter = '?';
            params = strchr(operation, oper_delimiter);
        }
    }
    
    attributes = strchr(operation, '@');
    if (attributes && (params == NULL || attributes < params)) {
        *attributes++ = '\0';
    } else {
        attributes = NULL;
    }

    if (params)
	*params++ = '\0';

    cp = strchr(target, '@');
    if (cp) {
	if (cp == target)
	    target += 1;
	else {
	    user = target;
	    *cp++ = '\0';
	    target = cp;
	}
    }

    cp = strchr(target, ':');
    if (cp) {
	if (cp[1] != '\0') {
	    *cp++ = '\0';
	    pass = cp;
	}
    }

    trace(trace_file, TRACE_ALL, "rpc: target [%s] op [%s] attr [%s] p [%s]",
	  target, operation, attributes ?: "", params ?: "");

    rpc = xmlNewDocNode(docp, NULL, (const xmlChar *) operation, NULL);
    if (rpc == NULL)
	errx(1, "op: out of memory");

    /* If we have attributes, insert them onto the rpc node */
    if (attributes) {
	char *next;
	for (; attributes && *attributes; attributes = next) {
	    cp = strchr(attributes, '=');
	    next = cp ? strchr(cp, '@') : NULL;
	    if (cp)
		*cp++ = '\0';
	    if (next)
		*next++ = '\0';

            if (strncmp(ATT_FORMAT, attributes, strlen(attributes)) == 0) {
                output_format = strdup(cp);
            }

            attr = xmlNewProp(rpc, (const xmlChar *) attributes,
                              (const xmlChar *) cp);
	    if (attr == NULL)
		errx(1, "could not build attribute: '%s'", attributes);
	}
    }

    /* Parse Accept header and set output format */
    accept_header = getenv("HTTP_ACCEPT");

    if (!output_format) {
	output_format = rpc_parse_mime_list(accept_header);

	if (output_format == NULL) {
	    output_format = strdup("xml");
	}

	attr = xmlNewProp(rpc, (const xmlChar *) ATT_FORMAT, 
			  (const xmlChar *) output_format);

	if (attr == NULL) {
	    free(output_format);
	    errx(1, "could not build 'format' attribute");
	}
    }

    /* Populate the operation node with our parameter values */
    if (params) {
	char *next;
	char param_delimiter = (oper_delimiter == '/') ? '/' : '&';

	for (; params && *params; params = next) {
	    cp = strchr(params, '=');
	    next = cp ? strchr(cp, param_delimiter) : NULL;
	    if (cp)
		*cp++ = '\0';
	    if (next)
		*next++ = '\0';

	    makeLeafNode(docp, rpc, params, cp);
	}
    }


    cp = getenv("QUERY_STRING");
    if (cp)
	rpc_parse_query_string(docp, rpc, cp);

    /*
     * If we are running RPC on box, we receive username and password in first
     * two lines
     */
    if (opt_user_info_on_stdin) {
	if (fgets(buf, sizeof(buf), stdin) == NULL 
	    || !strlcmp("user=", buf, 0)) {
	    if (output_format)
		free(output_format);
	    err(1, "Missing Authorization header %s", buf);
	}
	user = strdup(strchr(buf, '=') + 1);

	if (fgets(buf, sizeof(buf), stdin) == NULL 
	    || !strlcmp("password=", buf, 0)) {
	    if (output_format)
		free(output_format);
	    err(1, "Missing Authorization header %s", buf);
	}
	pass = strdup(strchr(buf, '=') + 1);
    }

    if (getenv("CONTENT_TYPE")) {
	content_type = rpc_parse_mime_list(getenv("CONTENT_TYPE"));
    }

    /*
     * If xml is sent as POST data, wrap it around an outer <post> tag so we 
     * can form valid xml and process the contents later when creating rpc
     * request
     */
    if (content_type && streq(content_type, "xml")) {
	len += 6;
	slaxDataListAddLen(&lines, "<post>", 6);
    }

    while (fgets(buf, sizeof(buf), stdin) != NULL) {
	trace(trace_file, TRACE_ALL, "cgi: stdin: %s", buf);
	i = strlen(buf);
	len += i;
	slaxDataListAddLen(&lines, buf, i);
    }

    if (content_type && streq(content_type, "xml")) {
	len += 7;
	slaxDataListAddLen(&lines, "</post>", 7);
    }

    if (len) {
	slax_data_node_t *dnp;
	char *bp;

	bp = malloc(len + 1);
	if (bp) {
	    len = 0;
	    SLAXDATALIST_FOREACH(dnp, &lines) {
		memcpy(bp + len, dnp->dn_data, dnp->dn_len);
		len += dnp->dn_len;
	    }
	    bp[len] = '\0';

            if (content_type && streq(content_type, "xml")) {
                if (rpc_build_data(docp, rpc, bp)) {
                    printf("Status: 400\n\n");
		    free(bp);
		    free(content_type);
		    if (output_format)
			free(output_format);
                    errx(1, "Failed to parse XML POST data");
                }
            } else {
                rpc_parse_query_string(docp, rpc, bp);
            }

	    slaxDataListClean(&lines);
	    free(bp);
	}
    }
    free(content_type);

    if (user)
	user = xmlURIUnescapeString(user, 0, NULL);
    if (pass)
	pass = xmlURIUnescapeString(pass, 0, NULL);

    js_session_opts_t jso;
    bzero(&jso, sizeof(jso));
    jso.jso_server = target;
    jso.jso_username = user;
    jso.jso_passphrase = pass;

    js_session_t *jsp;

    if (opt_user_info_on_stdin)
        jsp = js_session_open_localhost(&jso, 0, opt_auth_socket);
    else
        jsp = js_session_open(&jso, 0);

    if (jsp == NULL) {
        printf("Status: 401\n\n");
	printf("Failed to open session to execute RPC\n");
	if (output_format)
	    free(output_format);
        errx(1, "Could not open session");
    }

    /*
     * Write Content-Type into output header
     */
    if (output_format) {
	rpc_write_content_type(output_format);
	free(output_format);
    }
   
    reply = js_session_execute(pctxt, target, rpc, NULL, ST_DEFAULT);
    if (reply == NULL)
	err(0, "rpc operation failed");

    if (rpc)
	xmlFreeNode(rpc);

    xmlSaveCtxt *handle;
    handle = xmlSaveToFd(fileno(stdout), NULL,
			 XML_SAVE_FORMAT | XML_SAVE_NO_DECL);
    if (handle) {
	for (i = 0; i < reply->nodeNr; i++)
	    xmlSaveTree(handle, reply->nodeTab[i]);
	xmlSaveFlush(handle);
	xmlSaveClose(handle);
    }

    xmlXPathFreeNodeSet(reply);
    js_session_close1(jsp);

    xmlFreeAndEasy(user);
    xmlFreeAndEasy(pass);

    return 0;
}

/*
 * URIs received to run on box will not have target specified. We rewrite 
 * URI to include localhost as target before calling do_run_rpc
 */
static int
do_run_rpc_on_box (const char *scriptname UNUSED, const char *input UNUSED,
        char **argv UNUSED)
{
    char *uri, *newuri, *operation;
    const char *localhost = "localhost";

    uri = getenv("REQUEST_URI");
    if (uri == NULL || *uri == '\0')
        errx(1, "missing REQUEST_URI");

    operation = strchr(uri + 1, '/');

    if (operation == NULL || operation[0] == '\0')
        errx(1, "missing operation");

    if (asprintf(&newuri, "%.*s%s%s", operation - uri + 1, uri, localhost, 
		 operation) < 0) {
	errx(1, "Failed building REQUEST_URI");
    }

    setenv("REQUEST_URI", newuri, 1);

    free(newuri);

    return do_run_rpc(scriptname, input, argv);
}

static char **
build_argv (const char *argstring)
{
    char *av[1000], **realv;
    int ac = 0;
    const char *cp, *sp;
    char *ap;
    int len;
    char quote = 0;

    if (argstring == NULL || *argstring == '\0')
	return NULL;

    for (sp = cp = argstring; ; cp++) {
	if (*cp == '\'' || *cp == '\"') {
	    if (quote == *cp) {
		quote = 0;
	    } else if (quote == 0) {
		quote = *cp;
	    }

	} else if (*cp == '\\') {
	    if (cp[0])
		cp += 1;

	} else if (quote) {
	    /* do nothing */

	} else if (*cp == ' ' || *cp == '\t' || *cp == '\0') {
	    ap = malloc(cp - sp + 1);
	    if (ap) {
		memcpy(ap, sp, cp - sp);
		ap[cp - sp] = '\0';
		av[ac++] = ap;

		if (*cp != '\0')
		    sp = cp + 1;
	    }
	}

	if (*cp == '\0')
	    break;
    }

    if (ac == 0)
	return NULL;

    av[ac] = NULL;

    len = (ac + 1) * sizeof(realv[0]);
    realv = malloc(len);
    if (realv)
	memcpy(realv, av, len);

    return realv;
}

static void
print_version (void)
{
    printf("libjuice version %s\n", LIBJUISE_VERSION);
    printf("libslax version %s\n",  LIBSLAX_VERSION);
    printf("Using libxml %s, libxslt %s and libexslt %s\n",
	   xmlParserVersion, xsltEngineVersion, exsltLibraryVersion);
    printf("juise was compiled against libxml %d, "
	   "libxslt %d and libexslt %d\n",
	   LIBXML_VERSION, LIBXSLT_VERSION, LIBEXSLT_VERSION);
    printf("libxslt %d was compiled against libxml %d\n",
	   xsltLibxsltVersion, xsltLibxmlVersion);
    printf("libexslt %d was compiled against libxml %d\n",
	   exsltLibexsltVersion, exsltLibxmlVersion);
}

static void
print_help (void)
{
    fprintf(stderr,
"Usage: juise [@target] [options] [script] [param value]*\n"
"\t--agent OR -A: enable ssh-agent forwarding\n"
"\t--auth-socket <socket-path>: for authentication when run on localhost\n"
"\t--commit-script OR -c: test a commit script\n"
"\t--debug OR -d: use the libslax debugger\n"
"\t--directory <dir> OR -D <dir>: set the directory for server scripts\n"
"\t--include <dir> OR -I <dir>: search directory for includes/imports\n"
"\t--input <file> OR -i <file>: use given file for input\n"
"\t--indent OR -g: indent output ala output-method/indent\n"
"\t--junoscript OR -J: use junoscript API protocol\n"
"\t--load OR -l: load commit script changes in test mode\n"
"\t--lib <dir> OR -L <dir>: search directory for extension libraries\n"
"\t--mixer OR -M: use mixer connection (if available)\n"
"\t--no-randomize: do not initialize the random number generator\n"
"\t--param <name> <value> OR -a <name> <value>: pass parameters\n"
"\t--protocol <name> OR -P <name>: use the given API protocol\n"
"\t--rpc-on-box: Executes RPC on localhost\n"
"\t--run-server OR -R: run in juise server mode\n"
"\t--script <name> OR -S <name>: run the given script\n"
"\t--target <name> OR -T <name>: specify the default target device\n"
"\t--trace <file> OR -t <file>: write trace data to a file\n"
"\t--user <name> OR -u <name>: specify the user name for API connections\n"
"\t--verbose OR -v: enable debugging output (slaxLog())\n"
"\t--version OR -V: show version information (and exit)\n"
"\t--wait <seconds>: wait after starting (for gdb to attach)\n"

"\nProject juise home page: https://github.com/Juniper/juise\n"
"\n");
}

int
main (int argc UNUSED, char **argv, char **envp)
{
    char **save_argv = argv;
    char *cp;
    const char *script = NULL, *opt_trace_file_name = NULL;
    int (*func)(const char *, const char *, char **) = NULL;
    FILE *trace_fp = NULL;
    int randomize = 1;
    int logger = FALSE;
    int ssh_agent_forwarding = FALSE;
    session_type_t stype;
    int skip_args = FALSE;
    int opt_waiting = 0;
    int i;
    char *input = NULL;
    unsigned jsio_flags = 0;
    int opt_commit_script = FALSE;
    char *env_args = getenv("JUISE_OPTIONS");
    unsigned ioflags = 0;
    int opt_ignore_arguments = FALSE;

    slaxDataListInit(&plist);

    opt_args = argv;

    cp = *argv;
    if (cp) {
	static char strxmlmode[] = "xml-mode";
	char *ep = cp + strlen(cp) + 1;

	if (streq(strxmlmode, ep - sizeof(strxmlmode))) {
	    func = do_run_server_on_stdin;
	    skip_args = TRUE;
	    jsio_set_default_session_type(ST_JUNOS_NETCONF);
	}
    }

    if (!skip_args) {
	argv += 1;		/* Skip argv[0] */
    parse_args:
	for ( ; *argv; argv++) {
	    cp = *argv;

	    if (*cp != '-') {
		break;

	    } else if (streq(cp, "--agent") || streq(cp, "-A")) {
		ssh_agent_forwarding = TRUE;

	    } else if (streq(cp, "--auth-socket")) {
		opt_auth_socket = *++argv;

	    } else if (streq(cp, "--commit-script") || streq(cp, "-c")) {
		opt_commit_script = TRUE;
		func = do_test_commit_script;

	    } else if (streq(cp, "--cgi")) {
		func = do_run_as_cgi;

	    } else if (streq(cp, "--debug") || streq(cp, "-d")) {
		opt_debugger = TRUE;

	    } else if (streq(cp, "--debug-io")) {
		jsio_flags |= JSIO_MEMDUMP;

	    } else if (streq(cp, "--directory") || streq(cp, "-D")) {
		srv_add_path(*++argv);

	    } else if (streq(cp, "--fastcgi")) {
		func = do_run_as_fastcgi;

	    } else if (streq(cp, "--help")) {
		print_help();
		return -1;

	    } else if (streq(cp, "--ignore-arguments")) {
		opt_ignore_arguments = TRUE;
		break;

	    } else if (streq(cp, "--include") || streq(cp, "-I")) {
		slaxIncludeAdd(*++argv);

	    } else if (streq(cp, "--input") || streq(cp, "-i")) {
		input = *++argv;

	    } else if (streq(cp, "--indent") || streq(cp, "-g")) {
		opt_indent = TRUE;

	    } else if (streq(cp, "--junoscript") || streq(cp, "-J")) {
		stype = ST_JUNOSCRIPT;

	    } else if (streq(cp, "--load") || streq(cp, "-l")) {
		opt_load = TRUE;

	    } else if (streq(cp, "--local")) {
		opt_local = TRUE;

	    } else if (streq(cp, "--lib") || streq(cp, "-L")) {
		slaxDynAdd(*++argv);

	    } else if (streq(cp, "--mixer") || streq(cp, "-M")) {
		jsio_set_mixer(*++argv);

	    } else if (streq(cp, "--no-randomize")) {
		randomize = 0;

	    } else if (streq(cp, "--no-tty")) {
		ioflags |= SIF_NO_TTY;

	    } else if (streq(cp, "--op") || streq(cp, "-O")) {
		if (func)
		    errx(1, "open one action allowed");
		func = do_run_op;

	    } else if (streq(cp, "--output-format")) {
		opt_output_format = *++argv;
		if (!streq(opt_output_format, "xml")
			    && !streq(opt_output_format, "none")
			    && !streq(opt_output_format, "text")
			    && !streq(opt_output_format, "compare"))
		    errx(1, "invalid output format: %s", opt_output_format);
	    
	    } else if (streq(cp, "--param") || streq(cp, "-a")) {
		char *pname = *++argv;
		char *pvalue = *++argv;

		juise_make_param(pname, pvalue, TRUE);

	    } else if (streq(cp, "--protocol") || streq(cp, "-P")) {
		cp = *++argv;
		stype = jsio_session_type(cp);
		if (stype == ST_MAX) {
		    fprintf(stderr, "invalid protocol: %s\n", cp);
		    return -1;
		}
		jsio_set_default_session_type(stype);

	    } else if (streq(cp, "--rpc")) {
		func = do_run_rpc;

	    } else if (streq(cp, "--rpc-on-box")) {
		func = do_run_rpc_on_box;
		opt_user_info_on_stdin = TRUE;

	    } else if (streq(cp, "--run-server") || streq(cp, "-R")) {
		func = do_run_server_on_stdin;

	    } else if (streq(cp, "--script") || streq(cp, "-s")) {
		script = *++argv;

	    } else if (streq(cp, "--ssh-options") || streq(cp, "-S")) {
		jsio_add_ssh_options(*++argv);

	    } else if (streq(cp, "--target") || streq(cp, "-T")) {
		opt_target = *++argv;

	    } else if (streq(cp, "--trace") || streq(cp, "-t")) {
		opt_trace_file_name = *++argv;

	    } else if (streq(cp, "--user") || streq(cp, "--username")
		       || streq(cp, "-u")) {
		opt_username = *++argv;

	    } else if (streq(cp, "--verbose") || streq(cp, "-v")) {
		logger = TRUE;

	    } else if (streq(cp, "--version") || streq(cp, "-V")) {
		print_version();
		exit(0);

	    } else if (streq(cp, "--wait")) {
		opt_waiting = atoi(*++argv);

	    } else if (streq(cp, "--xml")) {
		func = do_emit_xml;

	    } else if (streq(cp, "--auth-muxer-id")) {
		jsio_set_auth_muxer_id(cp);

	    } else if (streq(cp, "--auth-websocket-id")) {
		jsio_set_auth_websocket_id(cp);
	    
	    } else if (streq(cp, "--auth-div-id")) {
		jsio_set_auth_div_id(cp);

	    } else {
		fprintf(stderr, "invalid option: %s\n", cp);
		print_help();
		return 1;
	    }

	    if (*argv == NULL) {
		/*
		 * The only way we could have a null argv is if we said
		 * "xxx = *++argv" off the end of argv.  Bail.
		 */
		fprintf(stderr, "missing option value: %s\n", cp);
		print_help();
		return 1;
	    }
	}

	/*
	 * Handle the rest of argv:
	 * - @xxx -> --target xxx
	 * - the first argument is the name of the script
	 * - the rest of the arguments are <name> <value> parameters
	 */
	for ( ; *argv; argv++) {
	    cp = *argv;

	    if (opt_target == NULL && *cp == '@') {
		opt_target = cp + 1;

	    } else if (opt_target == NULL
		       && (opt_target = strchr(cp, '@')) != NULL) {
		opt_username = cp;
		*opt_target++ = '\0';

	    } else if (script == NULL) {
		script = cp;

	    } else {
		char *pname = cp;
		char *pvalue = *++argv;

		juise_make_param(pname, pvalue, TRUE);
	    }
	}
    }

    if (env_args) {
	argv = build_argv(env_args);
	env_args = NULL;
	if (argv)
	    goto parse_args;
    }

    if (func == NULL)
	func = do_run_op; /* the default action */

    cp = getenv("JUISEPATH");
    if (cp)
	slaxIncludeAddPath(cp);

    cp = getenv("SLAXPATH");
    if (cp)
	slaxIncludeAddPath(cp);

    /* Add the default directory and the default import directory last */
    slaxIncludeAddPath(JUISE_DIR);
    slaxIncludeAddPath(JUISE_DIR "/import");

    cp = getenv("JUISE_SCRIPT_DIR");
    if (cp)
	srv_add_path(cp);
    srv_add_dir(JUISE_SCRIPT_DIR);

    if (opt_trace_file_name == NULL)
	opt_trace_file_name = getenv("JUISE_TRACE_FILE");

    if (opt_trace_file_name) {
	dump_all = 1;

	juise_trace_init(opt_trace_file_name, &trace_file);

	if (dump_all) {
	    for (i = 0; save_argv[i]; i++)
		trace(trace_file, TRACE_ALL, "argv: '%s'", save_argv[i]);

	    for (i = 0; envp[i]; i++)
		trace(trace_file, TRACE_ALL, "envp: '%s'", envp[i]);
	}
    }

    if (!opt_waiting) {
	cp = getenv("JUISE_WAIT");
	if (cp)
	    opt_waiting = atoi(cp);
    }

    /* Waiting allows 'gdb' to attach to a spawned process */
    if (opt_waiting) {
	trace(trace_file, TRACE_ALL, "waiting %d seconds", opt_waiting);
	sleep(opt_waiting);
    }

    /*
     * Seed the random number generator.  This is optional to allow
     * test jigs to take advantage of the default stream of generated
     * numbers.
     */
    if (randomize)
	slaxInitRandomizer();

    /*
     * Start the XML API
     */
    xmlInitParser();
    xsltInit();
    slaxEnable(SLAX_ENABLE);
    slaxIoUseStdio(ioflags);

    if (logger)
	slaxLogEnable(TRUE);

    exsltRegisterAll();
    ext_jcs_register_all();

    jsio_init(jsio_flags);

    /* The target argument defines the default target */
    if (opt_target)
	jsio_set_default_server(opt_target);
    else if (opt_commit_script)
	errx(1, "target must be specified for commit script mode");

    if (opt_username)
	jsio_set_default_user(opt_username);

    if (ssh_agent_forwarding)
	jsio_add_ssh_options("-A");

    cp = getenv("HTTP_X_MIXER_AUTH_MUXER_ID");
    if (cp) {
	jsio_set_auth_muxer_id(cp);
    }

    cp = getenv("HTTP_X_MIXER_AUTH_WEBSOCKET_ID");
    if (cp) {
	jsio_set_auth_websocket_id(cp);
    }
    
    cp = getenv("HTTP_X_MIXER_AUTH_DIV_ID");
    if (cp) {
	jsio_set_auth_div_id(cp);
    }

    if (opt_ignore_arguments) {
	static char *null_args[] = { NULL };
	argv = null_args;
    }

    /* Finally we get to work: call the mode-specific function */
    func(script, input, argv);

    if (trace_fp && trace_fp != stderr)
	fclose(trace_fp);

    xsltCleanupGlobals();
    xmlCleanupParser();

    jsio_cleanup();

    exit(slaxGetExitCode());
}
