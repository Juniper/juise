/*
 * $Id$
 *
 * Copyright (c) 2010, Juniper Networks, Inc.
 * All rights reserved.
 * See ../Copyright for the status of this software
 *
 * juise -- driver for libjuise, allowing remote access for scripting
 */

#include <err.h>
#include <time.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <pwd.h>

#include <libxml/tree.h>
#include <libxml/dict.h>
#include <libxslt/transform.h>
#include <libxml/HTMLparser.h>
#include <libxml/xmlsave.h>
#include <libexslt/exslt.h>
#include <libxslt/xsltutils.h>

#if 0
#include <libxml/globals.h>
#endif

#include "config.h"
#include <libslax/slax.h>
#include <libslax/slaxconfig.h>

#include <libjuise/string/strextra.h>
#include <libjuise/time/timestr.h>
#include <libjuise/xml/libxml.h>
#include <libjuise/io/trace.h>
#include <libjuise/io/jsio.h>
#include <libjuise/xml/extensions.h>
#include <libjuise/xml/juisenames.h>
#include <libjuise/juiseconfig.h>

#define MAX_PARAMETERS 64
#define MAX_PATHS 64

static const char *params[MAX_PARAMETERS + 1];
static int nbparams;
static int use_debugger;
static int indent;

trace_file_t *trace_file;

static void
juise_trace (void *vfp, xmlNodePtr nodep, const char *fmt, ...)
{
    trace_file_t *tp = vfp;
    FILE *fp = tp ? trace_fileptr(tp) : stderr;
    va_list vap;

    va_start(vap, fmt);

    if (nodep) {
	xmlSaveCtxt *handle;

	fprintf(fp, "XML Content (%d)\n", nodep->type);
	fflush(fp);
	handle = xmlSaveToFd(fileno(fp), NULL,
			     XML_SAVE_FORMAT | XML_SAVE_NO_DECL);
	if (handle) {
	    xmlSaveTree(handle, nodep);
	    xmlSaveFlush(handle);
	    xmlSaveClose(handle);
	    fflush(fp);
	}

    } else if (tp) {
	tracev(tp, TRACE_ALL, fmt, vap);
    } else {
	vfprintf(fp, fmt, vap);
    }

    va_end(vap);
}

static void
juise_log (void *vfp, const char *fmt, va_list vap)
{
    trace_file_t *tp = vfp;
    FILE *fp = tp ? trace_fileptr(tp) : stderr;

    if (tp) {
	tracev(tp, TRACE_ALL, fmt, vap);
    } else {
	vfprintf(fp, fmt, vap);
	fprintf(stderr, "\n");
	fflush(stderr);
    }
}

static void
juise_make_param (const char *pname, const char *pvalue)
{
    char *tvalue;
    char quote;
    int plen;

    if (pname == NULL || pvalue == NULL)
	errx(1, "missing parameter value");

    plen = strlen(pvalue);
    tvalue = xmlMalloc(plen + 3);
    if (tvalue == NULL)
	errx(1, "out of memory");

    quote = strrchr(pvalue, '\"') ? '\'' : '\"';
    tvalue[0] = quote;
    memcpy(tvalue + 1, pvalue, plen);
    tvalue[plen + 1] = quote;
    tvalue[plen + 2] = '\0';

    int pnum = nbparams++;
    params[pnum++] = pname;
    params[pnum] = tvalue;
}

static inline int
is_filename_std (const char *filename)
{
    return (filename == NULL || (filename[0] == '-' && filename[1] == '\0'));
}

static xmlNodePtr
juise_add_node (xmlNodePtr parent, const char *tag, const char *content)
{
    xmlNodePtr nodep, childp;
    
    childp = xmlNewText((const xmlChar *) content);
    if (childp == NULL)
	return NULL;

    nodep = xmlNewNode(NULL, (const xmlChar *) tag);
    if (nodep == NULL)
	return NULL;

    xmlAddChild(nodep, childp);
    xmlAddChild(parent, nodep);

    return nodep;
}

static xmlDocPtr
juise_build_input_doc (void)
{
    xmlParserCtxtPtr ctxt = xmlNewParserCtxt();
    xmlDocPtr docp;
    xmlNodePtr nodep, childp;
    char *value;
    struct passwd *pwd;
    char hostname[MAXHOSTNAMELEN];
    time_t now;

    docp = xmlNewDoc((const xmlChar *) XML_DEFAULT_VERSION);
    if (docp == NULL)
	return NULL;

    docp->standalone = 1;

    nodep = xmlNewNode(NULL, (const xmlChar *) ELT_OP_SCRIPT_INPUT);
    while (nodep) {
	xmlDocSetRootElement(docp, nodep);

	childp = xmlNewNode(NULL, (const xmlChar *) ELT_JUNOS_CONTEXT);
	if (childp == NULL)
	    break;
	nodep = childp;

	/* Hostname */
	if (gethostname(hostname, sizeof(hostname)) == 0)
	    juise_add_node(nodep, ELT_HOST_NAME, hostname);

	juise_add_node(nodep, ELT_PRODUCT, "juise");

	/* Time */
	time(&now);
	value = strndup(ctime(&now), 24);
	if (value) {
	    juise_add_node(nodep, ELT_LOCALTIME, value);
	    free(value);
	}
	juise_add_node(nodep, ELT_LOCALTIME_ISO, time_isostr(&now));

	juise_add_node(nodep, ELT_SCRIPT_TYPE, "op");

	childp = xmlNewNode(NULL, (const xmlChar *) ELT_USER_CONTEXT);
	if (childp == NULL)
	    break;
	nodep = childp;

	pwd = getpwuid(getuid());
	if (pwd) {
	    char nbuf[10];
	    juise_add_node(nodep, ELT_USER, pwd->pw_name);

#ifdef HAVE_PWD_CLASS
	    juise_add_node(nodep, ELT_CLASS_NAME, pwd->pw_class);
#endif

	    snprintf(nbuf, sizeof(nbuf), "%d", pwd->pw_uid);
	    juise_add_node(nodep, ELT_UID, nbuf);
	}

	juise_add_node(nodep, ELT_OP_CONTEXT, "");

	break;			/* Not really a loop */
    }

    if (ctxt->dict) {
	docp->dict = ctxt->dict;
	xmlDictReference(docp->dict);
    }

    return docp;
}

static int
do_run_op (const char *scriptname, char **argv UNUSED)
{
    xmlDocPtr scriptdoc;
    FILE *scriptfile;
    xmlDocPtr indoc;
    xsltStylesheetPtr script;
    xmlDocPtr res = NULL;

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

    indoc = juise_build_input_doc();
    if (indoc == NULL)
	errx(1, "unable to build input document");

    if (indent)
	script->indent = 1;

    if (use_debugger) {
	slaxDebugInit();
	slaxDebugSetStylesheet(script);
	slaxDebugApplyStylesheet(scriptname, script, "input",
				 indoc, params);
    } else {
	res = xsltApplyStylesheet(script, indoc, params);

	xsltSaveResultToFile(stdout, res, script);

	if (res)
	    xmlFreeDoc(res);
    }

    xmlFreeDoc(indoc);
    xsltFreeStylesheet(script);

    return 0;
}

/*
 * An ugly attempt to seed the random number generator with the best
 * value possible.  Ugly, but localized ugliness.
 */
static void
init_randomizer (void)
{
#if defined(HAVE_SRANDDEV)
    sranddev();

#elif defined(HAVE_SRAND)
#if defined(HAVE_GETTIMEOFDAY)

    struct timeval tv;
    int seed;

    gettimeofday(&tv, NULL);
    seed = ((int) tv.tv_sec) + ((int) tv.tv_usec);
    srand(seed);

#else /* HAVE_GETTIMEOFDAY */
    srand((int) time(NULL));

#endif /* HAVE_GETTIMEOFDAY */
#else /* HAVE_SRAND */
    fprintf(stderr, "could not initialize random\n");
#endif /* HAVE_SRAND */
}

static void
print_version (void)
{
    printf("libjuice version %s\n", LIBJUISE_VERSION);
    printf("libslax version %s\n",  LIBSLAX_VERSION);
    printf("Using libxml %s, libxslt %s and libexslt %s\n",
	   xmlParserVersion, xsltEngineVersion, exsltLibraryVersion);
    printf("slaxproc was compiled against libxml %d, "
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
    printf("Usage: juise [mode] [options] [script] [file]\n");
    printf("\nProject juise home page: http://juise.googlecode.com\n");
}

int
main (int argc UNUSED, char **argv)
{
    char *cp;
    const char *name = NULL, *trace_file_name = NULL;
    int (*func)(const char *, char **) = NULL;
    FILE *trace_fp = NULL;
    int randomize = 1;
    int logger = FALSE;
    char *server = NULL;
    int ssh_agent_forwarding = FALSE;

    for (argv++; *argv; argv++) {
	cp = *argv;

	if (*cp != '-') {
	    break;

	} else if (streq(cp, "--version") || streq(cp, "-V")) {
	    print_version();
	    exit(0);

	} else if (streq(cp, "--op") || streq(cp, "-O")) {
	    if (func)
		errx(1, "open one action allowed");
	    func = do_run_op;

	} else if (streq(cp, "--trace") || streq(cp, "-t")) {
	    trace_file_name = *++argv;

	} else if (streq(cp, "--debug") || streq(cp, "-d")) {
	    use_debugger = TRUE;

	} else if (streq(cp, "--verbose") || streq(cp, "-v")) {
	    logger = TRUE;

	} else if (streq(cp, "--indent") || streq(cp, "-g")) {
	    indent = TRUE;

	} else if (streq(cp, "--server") || streq(cp, "-s")) {
	    server = *++argv;

	} else if (streq(cp, "--agent") || streq(cp, "-A")) {
	    ssh_agent_forwarding = TRUE;

	} else if (streq(cp, "--param") || streq(cp, "-a")) {
	    char *pname = *++argv;
	    char *pvalue = *++argv;

	    juise_make_param(pname, pvalue);

	} else if (streq(cp, "--no-randomize")) {
	    randomize = 0;

	} else {
	    fprintf(stderr, "invalid option: %s\n", cp);
	    print_help();
	    return -1;
	}
    }

    /*
     * Handle the rest of argv:
     * - @xxx -> --server xxx
     * - the first argument is the name of the script
     * - the rest of the arguments are <name> <value> parameters
     */
    for ( ; *argv; argv++) {
	cp = *argv;

	if (*cp == '@') {
	    server = cp + 1;

	} else if (name == NULL) {
	    name = cp;

	} else {
	    char *pname = *++argv;
	    char *pvalue = *++argv;

	    juise_make_param(pname, pvalue);
	}
    }

    if (func == NULL)
	func = do_run_op; /* the default action */

    /*
     * Seed the random number generator.  This is optional to allow
     * test jigs to take advantage of the default stream of generated
     * numbers.
     */
    if (randomize)
	init_randomizer();

    /*
     * Start the XML API
     */
    xmlInitParser();
    xsltInit();
    slaxEnable(SLAX_ENABLE);
    slaxIoUseStdio();

    if (logger)
	slaxLogEnable(TRUE);

    exsltRegisterAll();
    ext_register_all();

    jsio_init();

    if (trace_file_name) {
	if (is_filename_std(trace_file_name)) {
	    slaxTraceEnable(juise_trace, NULL);
	    slaxLogEnableCallback(juise_log, NULL);
	} else {
	    trace_file = trace_file_open(NULL, trace_file_name,
					 1000000, 10);
	    if (trace_file == NULL || trace_fileptr(trace_file) == NULL)
		errx(1, "could not open trace file: %s", trace_file_name);
		
	    slaxTraceEnable(juise_trace, trace_file);
	    slaxLogEnableCallback(juise_log, trace_file);
	}
    }

    if (server)
	jsio_set_default_server(server);

    if (ssh_agent_forwarding)
	jsio_set_ssh_options("-A");

    func(name, argv);

    if (trace_fp && trace_fp != stderr)
	fclose(trace_fp);

    xsltCleanupGlobals();
    xmlCleanupParser();

    jsio_cleanup();

    return 0;
}
