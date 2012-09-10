/*
 * $Id: xml_unescape.c 387330 2010-06-30 21:28:07Z rjohnst $
 *
 * Copyright (c) 2000-2007, Juniper Networks, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include <libjuise/string/strextra.h>
#include <libjuise/xml/xmlutil.h>

#define STRCEQ(_x, _c) \
  (((_x)[ 1 ] == (_c)[ 1 ]) \
    && strncmp(&(_x)[ 2 ], &(_c)[ 2 ], sizeof(_c) - 3) == 0)

#define ERRMSG(tag...)

/*
 * xmlunescape: rewrite str into buf, unescaping xml problem characters.
 *              Unlike xml_escape, ' and " get unescaped when the
 *              input is not an attribute.  We only have to escape
 *              " and ' if the input is an attribute but we need to
 *              deal with the escaped " (&quot;) and ' (&apos;) regardless
 *              of whether the input is an attribute or text content of an
 *              element.
 */
boolean
xml_unescape (char *buf, size_t size, const char *str, boolean attribute UNUSED)
{
    const char *cp;
    char *np = buf;
    long avail = size;
    char newc = 0;

    for (cp = str; *cp; ) {
	if (*cp != '&') {
	    if (--avail < 0) break;
	    if (np != cp) *np = *cp;
	    np += 1;
	    cp += 1;
	    continue;

	} else if (cp[1] == '#') {
	    /* binary? */
	    char *ep;

	    if (cp[2] == 'x') {
		long val = strtoul(&cp[3], &ep, 16);
		if (val >= XML_ESCAPE_BINARY_UNICODE_BASE && val <=
		    XML_ESCAPE_BINARY_UNICODE_END) {
		    /*
		     * Specially encoded JUNOS control character.
		     */
		    newc = val - XML_ESCAPE_BINARY_UNICODE_BASE;
		} else {
		    /*
		     * Some other unicode value we don't support
		     */
		    newc = XML_UNICODE_CONVERT(val);
		}
	    } else {
		newc = strtoul(&cp[2], &ep, 10);
	    }
	    if (*ep == ';') { 
		cp = ++ep;
	    } else {
		static int first = 1;

		if (first) {
		    first = 0;
		    ERRMSG(LIBJNX_XML_DECODE_FAILED, LOG_ERR,
			   "Invalid XML encoding '%.6s', skipping to ';'",
			   cp);
		}
		/* eat up to (and including) the next ';' */
		do {
		    if (*cp++ == ';')
			break;
		} while (*cp) ;
	    }
	} else if (STRCEQ(cp, XML_ESCAPE_GT)) {
	    newc = '>';
	    cp += sizeof(XML_ESCAPE_GT) - 1;

	} else if (STRCEQ(cp, XML_ESCAPE_LT)) {
	    newc = '<';
	    cp += sizeof(XML_ESCAPE_LT) - 1;

	} else if (STRCEQ(cp, XML_ESCAPE_AMP)) {
	    newc = '&';
	    cp += sizeof(XML_ESCAPE_AMP) - 1;

	} else if (STRCEQ(cp, XML_ESCAPE_QUOT)) {
	    newc = '"';
	    cp += sizeof(XML_ESCAPE_QUOT) - 1;

	} else if (STRCEQ(cp, XML_ESCAPE_APOS)) {
	    newc = '\'';
	    cp += sizeof(XML_ESCAPE_APOS) - 1;

	} else {
	    if (--avail < 0) break;
	    if (np != cp) *np = *cp;
	    np += 1;
	    cp += 1;
	    continue;
	}

	if (--avail < 0) break;
	*np++ = newc;
    }

    if (avail < 0) return TRUE;

    *np = 0;
    return FALSE;
}

#ifdef UNIT_TEST
/*
 * Build with:
 *
 *	mk xmlutil.test
 *
 * Run:
 *
 *	xmlutil.test | cat -vt
 *
 * So you can see the binary chars survive the encode/decode passes.
 */

#include "xml_escape.c"

int
main (int argc __unused, char *argv[] __unused)
{
    const char *strings[] = {
	"Hello, World!",
	"<hello>World!</hello>",
	"<binary/> chars encoded as &#%d;",
	"<binary>\007\001\t\004\377\177</binary>",
	NULL,
    };
    const char *bogus = "&lt;bogus&gt;This &#9 will skip to here; is bogus&lt;/bogus&gt;";
    char encode[BUFSIZ];
    char decode[BUFSIZ];
    int i;

    openlog("xmlutil.test", LOG_PERROR, LOG_USER);
    for (i = 0; strings[i]; ++i) {
	xml_escape(encode, sizeof(encode), strings[i], FALSE, 0);
	xml_unescape(decode, sizeof(decode), encode, FALSE);
	printf("string: '%s'\nencode: '%s'\ndecode: '%s'\n\n",
	       strings[i], encode, decode);
    }
    fflush(stdout);
    xml_unescape(decode, sizeof(decode), bogus, FALSE);
    printf("bogus:  '%s'\ndecode: '%s'\n\n", bogus, decode);
    closelog();
    exit(0);
}
#endif

