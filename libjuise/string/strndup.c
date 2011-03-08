/*
 * $Id: strndup.c 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Copyright (c) 2000-2006, Juniper Networks, Inc.
 * All rights reserved.
 *
 * (Originally part of libjuniper/strextra.c)
 */

#include <sys/types.h>
#include <string.h>
#include <libjuise/string/strextra.h>

/*
 * strndup(): strdup() meets strncat(); return a duplicate string
 * of upto count characters of str. Always NUL terminate.
 */
char *
strndup (const char *str, size_t count)
{
    if (str == NULL) return NULL;
    else {
	size_t slen = strlen(str);
	size_t len = (count < slen) ? count : slen;
	char *cp = (char *) malloc(len + 1);

	if (cp) {
	    if (str) memcpy(cp, str, len);
	    cp[ len ] = 0;
	}
	return cp;
    }
}

