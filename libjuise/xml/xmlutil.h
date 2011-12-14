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

