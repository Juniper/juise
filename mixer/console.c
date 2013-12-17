/*
 * $Id$
 *
 * Copyright (c) 2012, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#include "local.h"
#include "debug.h"
#include "console.h"
#include "request.h"

static FILE *console_fp;

static mx_sock_t *
mx_console_build (int state, int fd)
{
    mx_sock_t *msp = malloc(sizeof(*msp));
    if (msp == NULL)
	return NULL;

    bzero(msp, sizeof(*msp));
    msp->ms_id = ++mx_sock_id;
    msp->ms_type = MST_CONSOLE;
    msp->ms_sock = fd;
    msp->ms_state = state;

    FILE *fp = fdopen(fd, "w+");
    if (fp) {
	mx_log_file(fp);
	if (console_fp)
	    fclose(console_fp);	/* Close the previous one */
	console_fp = fp;
	setlinebuf(fp);
    }

    return msp;
}

void
mx_console_start (void)
{
    int fd = dup(2);		/* Make our own fd of stderr */
    shutdown(2, SHUT_RD);	/* Stop reading from stderr */

    mx_sock_t *msp = mx_console_build(MSS_NORMAL, fd);

    TAILQ_INSERT_HEAD(&mx_sock_list, msp, ms_link);
    mx_sock_count += 1;

    fprintf(console_fp, ">> ");
    fflush(console_fp);
}

/**
 * Split the input given by users into tokens
 * @buf buffer to split
 * @args array for args
 * @max_args length of args
 * @return number of args
 * @bug needs to handle escapes and quotes
 */
static int
mx_console_split_args (char *buf, const char **args, int max_args)
{
    static const char wsp[] = " \t\n\r";
    int i;
    char *s;

    buf += strspn(buf, wsp);

    for (i = 0; (s = strsep(&buf, wsp)) && i < max_args - 1; i++) {
	args[i] = s;

	if (buf)
	    buf += strspn(buf, wsp);
    }

    args[i] = NULL;

    return i;
}

static int
strabbrev (const char *name, const char *value)
{
    int min = 1;
    int len = strlen(value);
    return (len >= min && strncmp(name, value, len) == 0);
}

static void
mx_console_sock (mx_sock_t *msp, int indent)
{
    const char *shost = NULL;
    unsigned int sport = 0;

    mx_log("%*sS%u: %s, fd %u, state %u", indent, "",
	   msp->ms_id, mx_sock_type(msp), msp->ms_sock, msp->ms_state);
    if (msp->ms_sin.sin_port) {
	shost = inet_ntoa(msp->ms_sin.sin_addr);
	sport = ntohs(msp->ms_sin.sin_port);
	mx_log("\t(%s .. %d)", shost, sport);
    }

    if (mx_mti(msp)->mti_print)
	mx_mti(msp)->mti_print(msp, indent + INDENT, "");
}

static void
mx_console_list (const char **argv UNUSED)
{
    mx_sock_t *msp;
    int indent = INDENT;

    mx_log("%d open sockets:", mx_sock_count);

    TAILQ_FOREACH(msp, &mx_sock_list, ms_link) {
	mx_console_sock(msp, indent);
    }

    mx_request_print_all(0, "");
}

static int
mx_console_poller (MX_TYPE_POLLER_ARGS)
{
    if (pollp && pollp->revents & POLLIN) {
	char buf[BUFSIZ];

	if (console_fp == NULL) {
	    mx_log("console has null console fp");
	    mx_sock_close(msp);
	    return FALSE;
	}

	if (fgets(buf, sizeof(buf), console_fp)) {
	    const char *argv[MAX_ARGS], *cp;

	    mx_console_split_args(buf, argv, MAX_ARGS);
	    cp = argv[0];
	    if (cp && *cp) {
		if (strabbrev("quit", cp)) {
		    exit(1);

		} else if (strabbrev("list", cp) || strabbrev("ls", cp)) {
		    mx_console_list(argv);

		} else if (strabbrev("close", cp)) {
		    mx_close_byname(argv[1]);

		} else if (strabbrev("don", cp)) {
		    mx_debug_flags(TRUE, argv[1]);

		} else if (strabbrev("doff", cp)) {
		    mx_debug_flags(FALSE, argv[1]);

		} else {
		    mx_log("%s: command not found", cp);
		}
	    }

	    fprintf(console_fp, ">> ");
	    fflush(console_fp);
	} else if (feof(console_fp)) {
	    mx_log("console sees EOF");
	    fclose(console_fp);
	    console_fp = NULL;
	    mx_log_file(NULL);
	    msp->ms_state = MSS_FAILED;
	}
    }

    return FALSE;
}

static mx_sock_t *
mx_console_spawn (MX_TYPE_SPAWN_ARGS)
{
    mx_sock_t *msp = mx_console_build(MSS_NORMAL, sock);
    if (msp == NULL)
	return NULL;

    msp->ms_sun = *sun;
    return msp;
}

void
mx_console_init (void)
{
    static mx_type_info_t mti = {
    mti_type: MST_CONSOLE,
    mti_name: "console",
    mti_letter: "P",
    mti_poller: mx_console_poller,
    mti_spawn: mx_console_spawn,
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}
