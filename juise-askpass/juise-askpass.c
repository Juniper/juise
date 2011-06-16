/*
 * $Id$
 *
 * Copyright (c) 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redirect prompt requests to grandparent process
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/param.h>
#include <string.h>
#include <sys/uio.h>

#include <libjuise/juiseconfig.h>
#include <libjuise/string/strextra.h>
#include <libjuise/io/trace.h>

trace_file_t *trace_file;

/*
 * One of the issues with SSHASKPASS is the lack of an indication
 * for whether the prompt is for a secret or a yes/no prompt.  We
 * have to read the tea leaves to figure this out.
 * See https://bugzilla.mindrot.org/show_bug.cgi?id=1871 for details
 */
static int
isyesno (const char *msg)
{
    const char *cp;

    if (getenv("SSH_ASKPASS_CONFIRMATION_ONLY") != NULL)
	return 1;

    for (cp = msg; cp && *cp; cp++) {
	cp = strchr(cp, '/');
	if (cp == NULL)
	    break;
	if (cp[1] == 'n')	/* "yes/no" */
	    return 1;
    }

    return 0;
}

static char default_msg[] = "Enter value";
static char newline_str[] = "\n";
static char secret_str[] = "secret ";
static char confirm_str[] = "confirm ";

static void
print_version (void)
{
    printf("libjuice version %s\n", LIBJUISE_VERSION);
}

int
main (int argc UNUSED, char **argv)
{
    char *msg = argv[1] ?: default_msg;
    char buf[BUFSIZ];
    char *path = getenv("SSH_ASKPASS_SOCKET");
    int yesno = isyesno(msg);
    struct iovec iov[3];
    int len;
    char *cp;

    for (argv++; *argv; argv++) {
	cp = *argv;

	if (*cp != '-') {
	    break;

	} else if (streq(cp, "--version") || streq(cp, "-V")) {
	    print_version();
	    exit(0);

	} else
	    break;
    }

    const char *tfn = getenv("JUISE_ASKPASS_TRACE");
    if (tfn)
	trace_file = trace_file_open(NULL, tfn, 1000000, 10);

    trace(trace_file, TRACE_ALL, "askpass: msg: %s%s%s",
	  msg ? "\"" : "", msg ?: "null", msg ? "\"" : "");

    trace(trace_file, TRACE_ALL, "askpass: path: %s%s%s",
	  path ? "\"" : "", path ?: "null", path ? "\"" : "");

    if (path == NULL)
	return -1;

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
	trace(trace_file, TRACE_ALL, "askpass: socket failed: %s",
	      strerror(errno));
	return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, '\0', sizeof(addr));
    addr.sun_family = AF_UNIX;
#ifdef HAVE_SUN_LEN
    addr.sun_len = sizeof(addr);
#endif /* HAVE_SUN_LEN */

    memcpy(addr.sun_path, path, sizeof(addr.sun_path));

    if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
	trace(trace_file, TRACE_ALL, "askpass: connect failed: %s",
	      strerror(errno));
	return -1;
    }

    iov[0].iov_base = yesno ? confirm_str : secret_str;
    iov[0].iov_len = strlen(iov[0].iov_base);
    iov[1].iov_base = msg;
    iov[1].iov_len = strlen(msg);
    iov[2].iov_base = newline_str;
    iov[2].iov_len = 1;

    if (writev(sock, iov, 3) < 0) {
	trace(trace_file, TRACE_ALL, "askpass: write failed: %m");
	return -1;
    }

    len = read(sock, buf, sizeof(buf));
    if (write(1, buf, len) < 0)
	trace(trace_file, TRACE_ALL, "askpass: write failed: %m");
    close(sock);

    if (len > 0)
	trace(trace_file, TRACE_ALL, "askpass: result: \"%*.*s\"",
	      len, len, buf);

    return 0;
}
