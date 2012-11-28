/*
 * $Id: xml_escaped_size.c,v 1.3 2007/08/02 08:13:12 builder Exp $
 *
 * Copyright (c) 2000-2007, Juniper Networks, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "juiseconfig.h"
#include <libjuise/string/strextra.h>
#include <libjuise/xml/xmlutil.h>
#include <libjuise/io/logging.h>

/*
 * xml_escaped_size: calculate the size needed if xml_escape were called on buf
 * 		     Boolean attribute dictates whether the input is an
 *		     attribute, which needs full escaping.
 *		     If attribute is true, the ' and " characters are escaped.
 */
size_t
xml_escaped_size (const char *buf, boolean attribute, unsigned flags)
{
    const char *cp;
    size_t size;
    boolean hit = FALSE;

    if (buf == NULL) return 0;
    
    for (cp = buf, size = 0; *cp; cp++) {
	switch (*cp) {

	case '>':
	    size += sizeof(XML_ESCAPE_GT) - 1;
	    hit = TRUE;
	    break;

	case '<':
	    size += sizeof(XML_ESCAPE_LT) - 1;
	    hit = TRUE;
	    break;

	case '&':
	    size += sizeof(XML_ESCAPE_AMP) - 1;
	    hit = TRUE;
	    break;

	case '"':
	    if (attribute) {
		size += sizeof(XML_ESCAPE_QUOT) - 1;
		hit = TRUE;
		break;
	    }

	case '\'':
	    if (attribute) {
		size += sizeof(XML_ESCAPE_APOS) - 1;
		hit = TRUE;
		break;
	    }

	default:
	    if (XML_IS_BINARY(*cp)) {

		/*
		 * If XML_ESCAPE_SPEC is specified then calculate the size
		 * for the vaild binary characters and ignore the other
		 */
		if ((flags & XML_ESCAPE_SPEC) && (!xmlIsCharQ(*cp & 0xff)))
		    continue;

		size += XML_ESCAPE_BINARY_SIZE;
		if (*cp < 10)
		    size -= 2;
		else if (*cp < 100)
		    size -= 1;
		hit = TRUE;
	    } else {
		size += 1;
	    }
	}
    }

    return hit ? size : 0;
}

