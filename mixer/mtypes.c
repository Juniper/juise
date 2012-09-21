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

mx_type_info_t mx_type_info[MST_MAX + 1];

void
mx_type_info_register (int version UNUSED, mx_type_info_t *mtip)
{
    mx_log("type: %s (%d)", mtip->mti_name, mtip->mti_type);
    
    mx_type_info[mtip->mti_type] = *mtip;
}

int
mx_sock_isreadable (int sock)
{
    struct pollfd pfd;
    int rc;

    bzero(&pfd, sizeof(pfd));
    pfd.fd = sock;
    pfd.events = POLLIN;

    for (;;) {
	rc = poll(&pfd, 1, 0);
        if (rc < 0) {
	    if (errno == EINTR)
		continue;
	    return FALSE;
	}
	return (pfd.revents & POLLIN) ? TRUE : FALSE;
    }
}

const char *
mx_sock_sin (mx_sock_t *msp)
{
    #define NUM_BUFS 3
    static unsigned buf_num;
    static char bufs[NUM_BUFS][BUFSIZ];
    const char *shost = inet_ntoa(msp->ms_sin.sin_addr);
    unsigned int sport = ntohs(msp->ms_sin.sin_port);

    if (++buf_num >= NUM_BUFS)
	buf_num = 0;

    snprintf(bufs[buf_num], BUFSIZ, "%s:%d", shost, sport);

    return bufs[buf_num];
}

const char *
mx_sock_type_number (mx_type_t type)
{
    if (type > MST_MAX)
	return "unknown";
    if (mx_type_info[type].mti_name)
	return mx_type_info[type].mti_name;
    return "unknown";
}

const char *
mx_sock_type (mx_sock_t *msp)
{
    if (msp == NULL)
	return "null";
    return mx_sock_type_number(msp->ms_type);
}
