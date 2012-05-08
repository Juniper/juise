/*
 * $Id$
 *
 * Copyright (c) 2012, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * The mixer daemon will accept requests for SSH channels and will
 * service those requests using existing SSH connections where
 * possible, opening new connections as needed.  Incoming requests
 * can use the WebSockets format.
 */

#include "base.h"
#include <sys/un.h>
#include "local.h"

int
mixer_init_afunix (const char *path)
{
    struct sockaddr_un addr;
    int sock;
    
    memset(&addr, '\0', sizeof(addr));
    addr.sun_family = AF_UNIX;
#ifdef HAVE_SUN_LEN
    addr.sun_len = sizeof(addr);
#endif /* HAVE_SUN_LEN */

    int len = strlen(path);
    if (len > (int) sizeof(addr.sun_path)) {
        trace(trace_file, TRACE_ALL, "askpass: path too long: %s", path);
        return -1;
    }

    memcpy(addr.sun_path, path, len);

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        trace(trace_file, TRACE_ALL, "mixer: afunix: socket failed: %s",
              strerror(errno));
        return -1;
    }

    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        trace(trace_file, TRACE_ALL, "mixer: afunix: connect failed: %s",
              strerror(errno));
        return -1;
    }

    if (listen(sock, 1) < 0) {
        trace(trace_file, TRACE_ALL, "mixer: afunix: connect failed: %s",
              strerror(errno));
        return -1;
    }

    mixer_listen_t *mlp = mixer_peer_create(MPT_LISTEN, sizeof(*mlp));
    if (mlp == NULL)
	return -1;

    mlp->ml_mp.mp_socket = sock;

    return 0;
}
