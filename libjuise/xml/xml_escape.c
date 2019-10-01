/*
 * $Id: xml_escape.c,v 1.3 2007/08/02 08:13:12 builder Exp $
 *
 * Copyright (c) 2000-2007, Juniper Networks, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "juiseconfig.h"
#include <libpsu/psucommon.h>
#include <libpsu/psustring.h>
#include <libjuise/string/strextra.h>
#include <libjuise/xml/xmlutil.h>
#include <libjuise/io/logging.h>

/*
 * xml_escape: 	rewrite str into buf, replacing xml problem characters.
 * 		Boolean attribute dictates whether the input is an attribute,
 *		which needs full escaping.
 *		If attribute is true, the ' and " characters are escaped.
 *		
 */
boolean
xml_escape (char *buf, size_t size, const char *str, boolean attribute, 
	    unsigned flags)
{
    char binary[ XML_ESCAPE_BINARY_SIZE + 1 ];
    const char *cp;
    char *np = buf;
    long avail = size;
    size_t elen = 0;
    const char *entity = NULL;

    for (cp = str; *cp; cp++) {
	switch (*cp) {
	case '&':
	    entity = XML_ESCAPE_AMP;
	    elen = sizeof(XML_ESCAPE_AMP) - 1;
	    break;

	case '>':
	    entity = XML_ESCAPE_GT;
	    elen = sizeof(XML_ESCAPE_GT) - 1;
	    break;

	case '<':
	    entity = XML_ESCAPE_LT;
	    elen = sizeof(XML_ESCAPE_LT) - 1;
	    break;

	case '"':
	    if (attribute) {
		entity = XML_ESCAPE_QUOT;
		elen = sizeof(XML_ESCAPE_QUOT) - 1;
		break;
	    }
	    goto common;

	case '\'':
	    if (attribute) {
		entity = XML_ESCAPE_APOS;
		elen = sizeof(XML_ESCAPE_APOS) - 1;
		break;
	    }
	    goto common;

	default:
	common:
	    if (XML_IS_BINARY(*cp)) {

		/*
		 * If XML_ESCAPE_SPEC is specified then escape only the vaild 
		 * binary characters and ignore the others.
		 *
		 * According to xml specification the following are 
		 * valid characters.
		 *
		 * (tab, carriage return, line feed, and the legal characters 
		 * of Unicode and  ISO/IEC 10646)
		 *
		 * Char ::= #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | 
		 *                                            [#x10000-#x10FFFF]
		 */
		if ((flags & XML_ESCAPE_SPEC) && (!xmlIsCharQ(*cp & 0xff)))
		    continue;

		elen = snprintf(binary, sizeof(binary),
				XML_ESCAPE_BINARY, (*cp & 0xff));
		entity = binary;
		binary[XML_ESCAPE_BINARY_SIZE] = '\0'; /* to be sure */
		binary[XML_ESCAPE_BINARY_SIZE - 1] = ';'; /* ditto! */
	    } else {
		if (--avail <= 0) break;
		*np++ = *cp;
		continue;
	    }
	}

	avail -= elen;
	if (avail <= 0) break;
	memcpy(np, entity, elen);
	np += elen;
    }

    if (avail <= 0) return TRUE;

    *np = 0;
    return FALSE;
}

