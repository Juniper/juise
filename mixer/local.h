/*
 * $Id$
 *
 * Copyright (c) 2012-2013, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/param.h>
#include <stdarg.h>

#include <assert.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/queue.h>

#include <libslax/slax.h>

#include <libssh2.h>

#include "juiseconfig.h"
#include <libjuise/common/aux_types.h>
#include <libjuise/string/strextra.h>
#include <libjuise/xml/xmlutil.h>

#include "debug.h"
#include "mtypes.h"
#include "buffer.h"

#ifndef INADDR_NONE
#define INADDR_NONE (in_addr_t)-1
#endif

#define LOCALHOST_ADDRESS "127.0.0.1"

#define MAX_POLL	10000	/* Largest number of open sockets to poll() */
#define MAX_ARGS	10	/* Max number of args to carve up */
#define MAX_XML_ATTR	10	/* Max number of attributes on rpc tag */
#define MAX_PWFAIL	3	/* Max times password can fail */

#define INDENT 		4	/* Indentation increment */
#define BUFFER_DEFAULT_SIZE (4*1024)
#define POLL_TIMEOUT	30000	/* Poll() timeout */

extern char keyfile1[], keyfile2[];

extern unsigned mx_sock_id;   /* Monotonically increasing ID number */

extern char *opt_dot_dir;	/* Directory for our dot files */
extern const char *opt_user;
extern const char *opt_password;
extern const char *opt_desthost;
extern const char *opt_db;
extern unsigned opt_destport;
extern int opt_no_db;
extern int opt_no_agent;
extern int opt_keepalive;
extern int opt_knownhosts;

static inline char *
nstrdup (const char *str)
{
    return str ? strdup(str) : NULL;
}

mx_password_t *
mx_password_find (const char *target, const char *user);

const char *
mx_password (const char *target, const char *user);

mx_password_t *
mx_password_save (const char *target, const char *user, const char *password);

void
mx_nonblocking (int sock);

void
mx_sock_close (mx_sock_t *msp);

/*
 * Looks like the _SAFE macros are missing from some linux distros
 */
#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar)                      \
        for ((var) = TAILQ_FIRST((head));                               \
            (var) && ((tvar) = TAILQ_NEXT((var), field), 1);            \
            (var) = (tvar))
#endif /* TAILQ_FOREACH_SAFE */
