/*
 * $Id$
 *
 * Copyright (c) 1997-2008, Juniper Networks, Inc.
 * All rights reserved.  See ../Copyright for additional information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libjuise/io/memdump.h>

/*
 * memdump(): dump memory contents in hex/ascii
0         1         2         3         4         5         6         7
0123456789012345678901234567890123456789012345678901234567890123456789012345
XX XX XX XX  XX XX XX XX - XX XX XX XX  XX XX XX XX abcdefghijklmnop
 */
void
memdump (FILE *fp, const char *title, const char *data,
         size_t len, const char *tag, int indent)
{
    enum { MAX_PER_LINE = 16 };
    char buf[ 80 ];
    char text[ 80 ];
    char *bp, *tp;
    size_t i;
#if 0
    static const int ends[ MAX_PER_LINE ] = { 2, 5, 8, 11, 15, 18, 21, 24,
                                              29, 32, 35, 38, 42, 45, 48, 51 };
#endif

    if (fp == NULL) fp = stdout;

    fprintf(fp, "%*s[%s] @ %p (%lx/%lu)\n", indent + 1, tag,
            title, data, (unsigned long) len, (unsigned long) len);

    while (len > 0) {
        bp = buf;
        tp = text;

        for (i = 0; i < MAX_PER_LINE && i < len; i++) {
            if (i && (i % 4) == 0) *bp++ = ' ';
            if (i == 8) {
                *bp++ = '-';
                *bp++ = ' ';
            }
            sprintf(bp, "%02x ", (unsigned char) *data);
            bp += strlen(bp);
            *tp++ = (isprint(*data) && *data >= ' ') ? *data : '.';
            data += 1;
        }

        *tp = 0;
        *bp = 0;
        fprintf(fp, "%*s%-54s%s\n", indent + 1, tag, buf, text);
        len -= i;
    }
}
