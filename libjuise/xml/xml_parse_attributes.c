/*
 * $Id: xml_parse_attributes.c 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Copyright (c) 2000-2006, 2012, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include <libjuise/string/strextra.h>
#include <libjuise/xml/xmlutil.h>

boolean
xml_parse_attributes (const char **cpp, unsigned max, char *attrs)
{
    boolean rc = TRUE;
    unsigned num;
    char *cp, *ep, *np;
    char quote;

    for (num = 0; attrs && *attrs && num < max; cp = attrs) {
	cp = strtrimws(attrs);
	if (*cp == 0) {
	    rc = FALSE;
	    break;
	}

	ep = strchr(cp, '=');
	if (ep == NULL) break;

	/* Strike over trailing space */
	np = strtrimtailws(ep - 1, cp);
	np[ 1 ] = 0;

	ep += 1;		/* Step over '=' */
	ep = strtrimws(ep); /* Step over leading ws */
	if (*ep == '\'' || *ep == '"') quote = *ep;
	else break;		/* Possible end-of-input */

	ep += 1;		/* Skip over quote */

	np = strchr(ep, quote);
	if (np == NULL) break;

	attrs = np[ 1 ] ? np + 1 : NULL;
	*np = 0;

	if (xml_unescape(ep, np - ep + 1, ep, TRUE)) break;

	cpp[ num++ ] = cp;
	cpp[ num++ ] = ep;

	if (attrs == NULL) {
	    rc = FALSE;
	    break;
	}
    }

    if (num < max) cpp[ num ] = NULL;
    return rc;
}

