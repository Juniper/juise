/*
 * $Id$
 *
 * Copyright (c) 2011, Juniper Networks, Inc.
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
#include <unistd.h>
#include <sys/param.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <string.h>
#include <signal.h>

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

#include "juiseconfig.h"
#include <libslax/slax.h>
#include <libpsu/psustring.h>
#include <libslax/slaxdata.h>

#include <libjuise/string/strextra.h>
#include <libjuise/time/timestr.h>
#include <libjuise/xml/libxml.h>
#include <libjuise/io/trace.h>
#include <libjuise/xml/jsio.h>
#include <libjuise/xml/extensions.h>
#include <libjuise/xml/juisenames.h>
#include <libjuise/common/allocadup.h>

#include "juise.h"

static int script_count;

static char *juise_dir;
static slax_data_list_t srv_includes;
static int srv_includes_inited;

/*
 * Add a directory to the list of directories searched for files
 */
void
srv_add_dir (const char *dir)
{
    if (!srv_includes_inited) {
	srv_includes_inited = TRUE;
	slaxDataListInit(&srv_includes);
    }

    slaxDataListAddNul(&srv_includes, dir);
}

/*
 * Add a set of directories to the list of directories searched for files
 */
void
srv_add_path (const char *dir)
{
    char *buf = NULL;
    int buflen = 0;
    const char *cp;

    while (dir && *dir) {
	cp = strchr(dir, ':');
	if (cp == NULL) {
	    srv_add_dir(dir);
	    break;
	}

	if (cp - dir > 1) {
	    if (buflen < cp - dir + 1) {
		buflen = cp - dir + 1 + BUFSIZ;
		buf = alloca(buflen);
	    }

	    memcpy(buf, dir, cp - dir);
	    buf[cp - dir] = '\0';

	    srv_add_dir(buf);
	}

	if (*cp == '\0')
	    break;
	dir = cp + 1;
    }
}

static lx_document_t *
srv_build_input_doc (lx_document_t *input)
{
    return input;
}

typedef struct script_info_s script_info_t;

#define SCRIPT_INFO_ARGS						\
        js_session_t *jsp UNUSED, script_info_t *sip UNUSED,	\
	FILE *scriptfile UNUSED,					\
	const char *scriptname UNUSED, const char *full_name UNUSED,	\
	lx_document_t *input UNUSED

typedef int (*script_info_func_t)(SCRIPT_INFO_ARGS);

struct script_info_s {
    const char *si_extension;	/* file name extension */
    const char *si_exec;	/* Path to run the script */
    script_info_func_t si_func;	/* Function to run the script */
};

static int
run_slax_script (SCRIPT_INFO_ARGS)
{
    lx_document_t *scriptdoc;
    lx_document_t *indoc;
    lx_stylesheet_t *script;
    lx_document_t *res = NULL;

    scriptdoc = slaxLoadFile(scriptname, scriptfile, NULL, 0);
    if (scriptdoc == NULL)
	errx(1, "cannot parse: '%s'", scriptname);

    if (scriptfile != stdin)
	fclose(scriptfile);

    script = xsltParseStylesheetDoc(scriptdoc);
    if (script == NULL || script->errors != 0)
	errx(1, "%d errors parsing script: '%s'",
	     script ? script->errors : 1, scriptname);

    indoc = srv_build_input_doc(input);
    if (indoc == NULL)
	errx(1, "unable to build input document");

    if (opt_indent)
	script->indent = 1;

    if (opt_debugger) {
	slaxDebugInit();
	slaxDebugSetStylesheet(script);
	res = slaxDebugApplyStylesheet(scriptname, script, "input",
				 indoc, NULL);
    } else {
	res = xsltApplyStylesheet(script, indoc, NULL);

	xsltSaveResultToFile(jsp->js_fpout, res, script);
    }

    if (res)
	xmlFreeDoc(res);

    if (indoc != input)
	xmlFreeDoc(indoc);
    xsltFreeStylesheet(script);

    return FALSE;
}

static int
run_exec_script (SCRIPT_INFO_ARGS)
{
    int sv[2];
    char *argv[3] = { ALLOCADUP(sip->si_exec), ALLOCADUP(full_name), NULL };
    sigset_t sigblocked, sigblocked_old;
    pid_t pid;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
	trace(trace_file, TRACE_ALL, "socketpair failed: %m");
        return TRUE;
    }

    sigemptyset(&sigblocked);
    sigaddset(&sigblocked, SIGCHLD);
    sigprocmask(SIG_BLOCK, &sigblocked, &sigblocked_old);

    if ((pid = fork()) == 0) {	/* Child process */
        sigprocmask(SIG_SETMASK, &sigblocked_old, NULL);

        close(sv[1]);
	dup2(sv[0], 0);
	dup2(sv[0], 1);

	int i;
        for (i = 3; i < 64; ++i) {
	    close(i);
	}

        execv(argv[0], argv);
        _exit(1);
    }

    close(sv[0]);		/* Close our side of the other side's socket */

    /* Write our output to the child process */
    slaxDumpToFd(sv[1], input, FALSE);

    /* Close the "write" side of the socket so they see EOF */
    shutdown(sv[1], SHUT_WR);

    /* Read the results */
    xmlDocPtr res = xmlReadFd(sv[1], "script.output",
			      "UTF-8", XSLT_PARSE_OPTIONS);
    if (res) {
	trace(trace_file, TRACE_ALL, "no script output");
	slaxDumpToFd(fileno(jsp->js_fpout), res, FALSE);
	xmlFreeDoc(res);
    }

    close(sv[1]);
    close(sv[0]);

    return FALSE;
}

static script_info_t script_info[] = {
    { "slax", NULL, run_slax_script },
    { "xsl", NULL, run_slax_script },
    { "xslt", NULL, run_slax_script },
    { "sh", "/bin/sh", run_exec_script },
#ifdef JUISE_PATH_PERL
    { "perl", JUISE_PATH_PERL, run_exec_script },
    { "pl", JUISE_PATH_PERL, run_exec_script },
#endif /* JUISE_PATH_PERL */
#ifdef JUISE_PATH_PYTHON
    { "python", JUISE_PATH_PYTHON, run_exec_script },
    { "py", JUISE_PATH_PYTHON, run_exec_script },
    { "pyc", JUISE_PATH_PYTHON, run_exec_script },
#endif /* JUISE_PATH_PYTHON */
    { NULL, NULL, NULL }
};

static FILE *
open_script (const char *scriptname, char *full_name, int full_size,
	     script_info_t **sipp)
{
    script_info_t *sip;
    FILE *fp;
    slax_data_node_t *dnp;

    SLAXDATALIST_FOREACH(dnp, &srv_includes) {
	char *dir = dnp->dn_data;

	for (sip = script_info; sip->si_extension; sip++) {
	    snprintf(full_name, full_size, "%s/%s%s%s",
		     dir, scriptname, sip->si_extension[0] ? "." : "",
		     sip->si_extension);
	    fp = fopen(full_name, "r+");
	    if (fp) {
		trace(trace_file, TRACE_ALL,
		      "server: found script file '%s'", full_name);
		*sipp = sip;
		return fp;
	    }
	}
    }

    *sipp = NULL;
    return NULL;
}

static int
srv_run_script (js_session_t *jsp, const char *scriptname,
		lx_document_t *input)
{
    int rc;
    FILE *scriptfile;
    char full_name[MAXPATHLEN];
    script_info_t *sip = NULL;

    script_count += 1;

    if (scriptname == NULL)
	errx(1, "missing script name");

    trace(trace_file, TRACE_ALL, "server: running script '%s'", scriptname);

    scriptfile = open_script(scriptname, full_name, sizeof(full_name), &sip);
    if (scriptfile == NULL) {
	trace(trace_file, TRACE_ALL,
	      "file open failed for script '%s' (%s)",
	      scriptname, juise_dir ?: JUISE_SCRIPT_DIR);
	return TRUE;
    }

    rc = sip->si_func(jsp, sip, scriptfile, scriptname, full_name, input);

    fclose(scriptfile);

    return rc;
}

void
run_server (int fdin, int fdout, session_type_t stype)
{
    static const char rpc_reply_open[] = "<rpc-reply>\n";
    static const char rpc_reply_close[] = "</rpc-reply>]]>]]>\n";
    static const char rpc_error[]
	= "<error><message>invalid rpc</message></error>";
    js_session_t *jsp;
    lx_document_t *rpc;
    const char *name;
    int rc;

    jsp = js_session_open_server(fdin, fdout, stype, 0);
    if (jsp == NULL)
	errx(1, "could not open server");

    for (;;) {
	rpc = js_rpc_get_request(jsp);
	if (rpc == NULL) {
	    if (jsp->js_state == JSS_DEAD || jsp->js_state == JSS_CLOSE)
		break;
	    continue;
	}

	name = js_rpc_get_name(rpc);
	if (name) {
	    if (write(fdout, rpc_reply_open, sizeof(rpc_reply_open) - 1) < 0)
		trace(trace_file, TRACE_ALL, "error writing reply: %m");

	    if (srv_run_script(jsp, name, rpc)) {
		rc = write(fdout, rpc_error, sizeof(rpc_error) - 1);
                if (rc < 0)
                    trace(trace_file, TRACE_ALL, "error writing error: %m");

	    }
	    if (write(fdout, rpc_reply_close, sizeof(rpc_reply_close) - 1) < 0)
		trace(trace_file, TRACE_ALL, "error writing reply: %m");
	}

	js_rpc_free(rpc);
    }

    js_session_terminate(jsp);
}
