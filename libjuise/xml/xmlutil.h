/*
 * $Id: xmlutil.h 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Copyright (c) 2000-2007, Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef __JNX_XMLUTIL_H__
#define __JNX_XMLUTIL_H__

#include <sys/types.h>
#include <libjuise/common/aux_types.h>

#define XMLRPC_ABORT		"abort"
#define XML_PARSER_RESET	"]]>]]>"
#define XML_ESCAPE_MAXLEN       8

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

