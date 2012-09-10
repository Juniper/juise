/*
 * $Id$
 *
 * Copyright (c) 2000-2007, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef __JNX_XMLUTIL_H__
#define __JNX_XMLUTIL_H__

#include <sys/types.h>
#include <libjuise/common/aux_types.h>

#define XMLRPC_ABORT		"abort"
#define XML_PARSER_RESET	"]]>]]>"
#define XML_ESCAPE_MAXLEN       8

#define XML_ESCAPE_GT	"&gt;"
#define XML_ESCAPE_LT	"&lt;"
#define XML_ESCAPE_APOS	"&apos;"
#define XML_ESCAPE_QUOT	"&quot;"
#define XML_ESCAPE_AMP	"&amp;"
#define XML_ESCAPE_BINARY "&#%u;"
/*
 * Unlike the others above, XML_ESCAPE_BINARY is a printf format
 * string, we need to allow for &#nnn; which is 6 chars.
 * It turns out that sizeof(XML_ESCAPE_BINARY) is correct (as long as
 * we don't -1 was we do for the others).  Use XML_ESCAPE_BINARY_SIZE
 * to avoid confusion below.
 */
#define XML_ESCAPE_BINARY_SIZE sizeof(XML_ESCAPE_BINARY)

/*
 * If we hit any invalid xmlChar (such as ^T), we need to encode this into
 * Unicode XML format "&#xYYYY;" in the Private area of E000-F8FF.  For
 * instance, ^T (0x14) becomes &#xE014;  This ensures we don't break XML
 * parsing and we can read these control characters back in.
 */
#define XML_ESCAPE_BINARY_UNICODE_BASE 0xe000
#define XML_ESCAPE_BINARY_UNICODE_END 0xe100
#define XML_ESCAPE_BINARY_UNICODE "&#x%04x;"
#define XML_ESCAPE_BINARY_UNICODE_SIZE 8

/* this is the criteria for using XML_ESCAPE_BINARY */
#define XML_IS_BINARY(c) ((c & 0x80) || (!isprint(c) && !isspace(c)))

#define XML_UNICODE_CONVERT(c) (c & 0xff)

/*
 * Macros to check the vaild XML characters. 
 *
 * According to xml specification the following are valid characters.
 * (tab, carriage return, line feed, and the legal characters of Unicode and 
 * ISO/IEC 10646)
 *
 * Char ::= #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | 
 *                                                            [#x10000-#x10FFFF]
 *
 * (Copied from contrib/libxml2/include/libxml/chvalid.h)
 */
#define xmlIsChar_ch(c) (((0x9 <= (c)) && ((c) <= 0xa)) || ((c) == 0xd) || \
			 (0x20 <= (c)))

#define xmlIsCharQ(c) (((c) < 0x100) ? xmlIsChar_ch((c)) : \
	(((0x100 <= (c)) && ((c) <= 0xd7ff)) || \
	 ((0xe000 <= (c)) && ((c) <= 0xfffd)) || \
	 ((0x10000 <= (c)) && ((c) <= 0x10ffff))))

/* 
 * If this flag is specified then xml_escape() will escape only the valid
 * binary xml data and will ignore the remaining binary xml data which 
 * are invalid according to xml specification.
 *
 * If not specified, xml_escape() will escape all the binary xml data. 
 *
 */
#define XML_ESCAPE_SPEC			(1<<0)

boolean xml_escape(char *buf, size_t size, const char *str, boolean attribute, 
		   unsigned flags);
boolean xml_unescape(char *buf, size_t size, const char *str,
		     boolean attribute);
size_t  xml_escaped_size(const char *buf, boolean attribute, unsigned flags);
boolean xml_parse_attributes (const char **cpp, unsigned max, char *attrs);

#endif /* __JNX_XMLUTIL_H__ */

