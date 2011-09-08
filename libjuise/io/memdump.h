/*
 * $Id$
 *
 * Copyright (c) 1997-2008, Juniper Networks, Inc.
 * All rights reserved.  See ../Copyright for additional information.
 */

#ifndef LIBJUISE_IO_MEMDUMP_H
#define LIBJUISE_IO_MEMDUMP_H

/*
 * memdump(): dump memory contents in hex/ascii
0         1         2         3         4         5         6         7
0123456789012345678901234567890123456789012345678901234567890123456789012345
XX XX XX XX  XX XX XX XX - XX XX XX XX  XX XX XX XX abcdefghijklmnop
 */
void
memdump (FILE *fp, const char *title, const char *data,
         size_t len, const char *tag, int indent);

#endif /* LIBJUISE_IO_MEMDUMP_H */
    
