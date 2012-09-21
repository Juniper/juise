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

mx_sock_t *
mx_console_start (void)
{
    mx_sock_t *msp = malloc(sizeof(*msp));
    if (msp == NULL)
	return NULL;

    bzero(msp, sizeof(*msp));
    msp->ms_id = ++mx_sock_id;
    msp->ms_type = MST_CONSOLE;
    msp->ms_sock = 0;		/* fileno(stdin) */

    TAILQ_INSERT_HEAD(&mx_sock_list, msp, ms_link);
    mx_sock_count += 1;

    fprintf(stderr, ">> ");
    fflush(stderr);

    return msp;
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
mx_console_list (const char **argv UNUSED)
{
    mx_sock_t *msp;
    const char *shost = NULL;
    unsigned int sport = 0;
    int i = 0;
    int indent = INDENT;

    mx_log("%d open sockets:", mx_sock_count);

    TAILQ_FOREACH(msp, &mx_sock_list, ms_link) {

	mx_log("%*sS%u: %s, fd %u, state %u", indent, "",
		msp->ms_id, mx_sock_type(msp), msp->ms_sock, msp->ms_state);
	if (msp->ms_sin.sin_port) {
	    shost = inet_ntoa(msp->ms_sin.sin_addr);
	    sport = ntohs(msp->ms_sin.sin_port);
	    mx_log("\t(%s .. %d)", shost, sport);
	}

	if (mx_mti(msp)->mti_print)
	    mx_mti(msp)->mti_print(msp, indent + INDENT, "");

	i += 1;
    }
}

static int
mx_console_poller (MX_TYPE_POLLER_ARGS)
{
    if (pollp && pollp->revents & POLLIN) {
	char buf[BUFSIZ];

	if (fgets(buf, sizeof(buf), stdin)) {
	    const char *argv[MAX_ARGS], *cp;

	    mx_console_split_args(buf, argv, MAX_ARGS);
	    cp = argv[0];
	    if (cp && *cp) {
		if (strabbrev("quit", cp)) {
		    exit(1);

		} else if (strabbrev("list", cp) || strabbrev("ls", cp)) {
		    mx_console_list(argv);

		} else if (strabbrev("don", cp)) {
		    mx_debug_flags(TRUE, argv[1]);

		} else if (strabbrev("doff", cp)) {
		    mx_debug_flags(FALSE, argv[1]);

		} else {
		    mx_log("%s: command not found", cp);
		}
	    }

	    fprintf(stderr, ">> ");
	    fflush(stderr);
	}
    }

    return FALSE;
}

void
mx_console_init (void)
{
    static mx_type_info_t mti = {
    mti_type: MST_CONSOLE,
    mti_name: "console",
    mti_poller: mx_console_poller,
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}
