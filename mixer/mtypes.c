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
mx_sock_name (mx_sock_t *msp)
{
    #define NUM_BUFS 3
    static unsigned buf_num;
    static char bufs[NUM_BUFS][BUFSIZ];

    if (++buf_num >= NUM_BUFS)
	buf_num = 0;

    if (msp->ms_sin.sin_port) {
	const char *shost = inet_ntoa(msp->ms_sin.sin_addr);
	unsigned int sport = ntohs(msp->ms_sin.sin_port);
	snprintf(bufs[buf_num], BUFSIZ, "inet: %s:%d", shost, sport);
    } else if (msp->ms_sin6.sin6_port) {
	unsigned int sport = ntohs(msp->ms_sin6.sin6_port);
	snprintf(bufs[buf_num], BUFSIZ, "inet6: %d", sport);
    } else if (msp->ms_sun.sun_path[0]) {
	snprintf(bufs[buf_num], BUFSIZ, "unix: %s", msp->ms_sun.sun_path);

    } else {
	snprintf(bufs[buf_num], BUFSIZ, "unknown");
    }

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

const char *
mx_sock_letter (mx_sock_t *msp)
{
    mx_type_t type = msp->ms_type;

    if (type > MST_MAX)
	return "unknown";
    if (mx_type_info[type].mti_letter)
	return mx_type_info[type].mti_letter;
    return "U";
}

const char *
mx_sock_title  (mx_sock_t *msp)
{
    #define TMAX 8
    static char title[TMAX][16];
    static int tnum;
    char *cp = title[tnum];

    snprintf(title[tnum], sizeof(title[tnum]), "%s%u", mx_sock_letter(msp),
	     msp ? msp->ms_id : 0);

    if (++tnum >= TMAX)
	tnum = 0;

    return cp;
}

void
mx_close_byname (const char *name)
{
    mx_sock_t *msp;
    unsigned id;

    if (*name == 's' || *name == 'S')
	name += 1;
    id = atoi(name);

    TAILQ_FOREACH(msp, &mx_sock_list, ms_link) {
	if (msp->ms_id != id)
	    continue;
	if (mx_mti(msp)->mti_close)
	    mx_mti(msp)->mti_close(msp);
    }
}
