/*
 * $Id$
 *
 * Copyright (c) 2000-2008, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * Functions for handling xml rpc
 */

#ifndef JUNOSCRIPT_XMLRPC_H
#define JUNOSCRIPT_XMLRPC_H

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <libjuise/common/aux_types.h>
#include <libjuise/io/logging.h>

/*
 * Use C linkage when using a C++ compiler
 */
#ifdef __cplusplus
extern "C" { 
    namespace junos {
#endif /* __cplusplus */

#define XMLRPC_OPEN(_x)		"<" _x ">"
#define XMLRPC_CLOSE(_x)	"</" _x ">"
#define XMLRPC_EMPTY(_x)	"<" _x "/>"

#define XMLRPC_REPLY 		"rpc-reply"
#define XMLRPC_REQUEST		"rpc"
#define XMLRPC_COMMAND		"command"
#define XMLRPC_EXPAND		"expand"
#define XMLRPC_EXPOUND		"expound"
#define XMLRPC_DATA		"data"
#define XMLRPC_PROMPT		"prompt"
#define XMLRPC_ERROR		"error"
#define XMLRPC_WARN		"warning"
#define XMLRPC_ABORT_ACK	"abort-acknowledgement"

#define XMLRPC_NAME		"xml"
#define XMLRPC_VERSION_ATTR	"version"
#define XMLRPC_VERSION		"1.0"
#define XMLRPC_APINAME		"junoscript"
#define XMLRPC_NS_PREFIX	"http://xml.juniper.net/junos/"

#define XMLRPC_SUCCESS		"success"
#define XMLRPC_SUCCESS_TAG	"<" XMLRPC_SUCCESS "/>"

#define XMLRPC_COMMENT		"comment"
#define XMLRPC_GET_CONFIGURATION	"get-configuration"
#define XMLRPC_LOAD_CONFIGURATION	"load-configuration"
#define XMLRPC_UNDOCUMENTED	"undocumented"

#define XMLRPC_SCHEMA_LOCATION	"schemaLocation"
#define XMLRPC_SCHEMA_LOCATION_PREFIX	"junos/"

/*
 * Define the junos namespace and qualified attributes
 */
#define JUNOS_NS		"junos"
#define JUNOS_SECONDS		"junos:seconds"
#define JUNOS_MICROSECONDS	"junos:microseconds"
#define JUNOS_CELSIUS           "junos:celsius"
#define JUNOS_STYLE		"junos:style"
#define JUNOS_FORMAT		"junos:format"
#define JUNOS_EMIT		"junos:emit"
#define JUNOS_MARKED		"junos:marked"
#define JUNOS_INDENT		"junos:indent"
#define JUNOS_MEMPAGES		"junos:memorypages"
#define JUNOS_KEY		"junos:key"
#define JUNOS_DISPLAY           "junos:display"
#define JUNOS_GROUP		"junos:group"
#define JUNOS_CHANGED		"junos:changed"
#define JUNOS_POSITION		"junos:position"
#define JUNOS_TOTAL		"junos:total"
#define JUNOS_COMMIT_SECONDS    "junos:commit-seconds"
#define JUNOS_COMMIT_LOCALTIME  "junos:commit-localtime"
#define JUNOS_CHANGED_SECONDS    "junos:changed-seconds"
#define JUNOS_CHANGED_LOCALTIME  "junos:changed-localtime"
#define JUNOS_COMMIT_USER       "junos:commit-user"

#define JUNOS_DISPLAY_TYPE_NONE "none" /* value for junos:display */

#define JUNOS_FULL_NS		"http://xml.juniper.net/junos/*/junos"

#define XNM_QUALIFIER		"xnm"
#define XNM_FULL_NS		"http://xml.juniper.net/xnm/1.1/xnm"
#define XNM_NETCONF_NS		"urn:ietf:params:xml:ns:netconf:base:1.0"

#define XSL_QUALIFIER		"xsl"
#define XSL_FULL_NS		"http://www.w3.org/1999/XSL/Transform"

#define MAX_XML_ATTRIBUTES	20
#define MAX_XML_ATTR_ARRAY	(MAX_XML_ATTRIBUTES * 2 + 1)

#define XML_TAG(_id) XML_ARRAY[ _id ].xt_name

typedef struct xml_tag_s {	/* Each XML tag is described by one of these */
    const char *xt_name;	/* Name of the tag */
    unsigned short xt_type;	/* Type of this tag */
    unsigned short xt_flags;	/* Flags for this tag */
} xml_tag_t;

/* Values for xt_type: see xml_formats[] in xmlrpc.c */
#define XTT_UNKNOWN	0	/* Unknown type */
#define XTT_INTEGER	1	/* %d */
#define XTT_UNSIGNED	2	/* %u */
#define XTT_STRING	3	/* %s */
#define XTT_INT64       4	/* %qd */
#define XTT_UINT64      5	/* %qu */

/* Values for xt_flags: */
#define XTF_LEVEL	(1<<0)	/* <foo>...</foo> */
#define XTF_DATA	(1<<1)	/* <foo>value</foo> */
#define XTF_EMPTY	(1<<2)	/* <foo/> */
#define XTF_OUTPUT	(1<<3)	/* Make output if not xml_on_board */
#define XTF_NEWLINE	(1<<4)	/* Make newline if not xml_on_board */
#define XTF_COLON	(1<<5)	/* Make a ': ' after the tag/value */
#define XTF_ATTR	(1<<6)	/* foo is an attribute */

#define NULL_XML_TAG_ENTRY \
NULL, 0, 0

boolean xml_input_match(void *peer, int type_to_match, int *typep,
			const char **tagp, char **restp, unsigned *flagsp);
boolean xml_input_match2(void *peer, int type_to_match, int *typep,
			 const char **tagp, char **restp, unsigned *flagsp,
			 void (*abort_cb)(void));

/* Flags for xml_input_match: */
#define XIMF_SKIP_COMMENTS	(1<<0) /* Ignore comments (when trivial) */
#define XIMF_ABORT_SEEN		(1<<1) /* We've seen an '<abort/>' tag */
#define XIMF_TRACE		(1<<2) /* Trace input via dbgpr() */
#define XIMF_SKIP_ABORTS	(1<<3) /* Skip <abort/> tags (but mark them) */
#define XIMF_LINEBUFD		(1<<4) /* Read one line of data at a time */
#define XIMF_DRAINRPC		(1<<5) /* Drain the full XMLRPC */
#define XIMF_INITFIRST		(1<<6) /* Start the initial handshake */
#define XIMF_ALLOW_NOOP		(1<<7) /* Return NOOPs */
#define XIMF_PRESERVE_WS	(1<<8) /* Preserve whitespaces */

/* Flags for xml_send*(): */
#define XSF_ESCAPE	(1<<0)	/* Escape content before sending */
#define XSF_EMPTY	(1<<1)	/* Empty open tag (insert trailing slash) */
#define XSF_NL          (1<<2)  /* Add a newline to the end of the output */

boolean xml_send_open (void *peer, const xml_tag_t *tag, unsigned flags,
		       const char *fmt, ...);
boolean xml_send (void *peer, unsigned flags, const char *fmt, ...) \
                                             FORMAT_CHECK(3, 4);
boolean xml_sendv (void *peer, unsigned flags, const char *fmt,
			va_list vap);
boolean xml_send_close (void *peer, const xml_tag_t *tag, unsigned flags);
boolean xml_send_all (void *peer, const xml_tag_t *tag, unsigned flags,
		      const char *fmt, ...) FORMAT_CHECK(4, 5);
boolean xml_send_comment (void *peer, unsigned flags,
			       const char *fmt, ...) FORMAT_CHECK(3, 4);
boolean xml_send_rpcv (void *peer, int flags, const char *tagmajor,
		       const char *tagminor, const char *fmt, va_list vap);
boolean xml_send_rpc (void *peer, int flags, const char *rpcmajor,
		      const char *tagminor, const char *fmt, ...) \
                                             FORMAT_CHECK(5, 6);
boolean xml_send_replyv (void *peer, int flags, const char *tagmajor,
			 const char *tagminor, const char *fmt, va_list vap);
boolean xml_send_reply (void *peer, int flags, const char *tagmajor,
			const char *tagminor, const char *fmt, ...) \
                                             FORMAT_CHECK(5, 6);

boolean xml_eof (void *peer);

boolean xml_put_namespace (void *peer, const char *name, int type,
			   const char *ns, const char *rest);
boolean xml_forward_error (void *dst, void *src, int flags);

boolean xml_input_rpc (void *peer, void *error_peer,
               unsigned flags, int *typep, char **tagp,
               char **restp, char **datap, const char *rpctag,
	       const char *rpcminor, const char *fmt, ...);

boolean xml_send_success_tag (void *peer);

/* Internal Use Only */
#define XML_OPEN_REVERSE(_peer, _flags, _id, _attribute_format...) \
    xml_send_open(_peer, &XML_ARRAY[ _id ], _flags, _attribute_format)

/*
 * XML_OPEN sends the opening xml _tag.  If _fmt is NULL, it is sent as
 * <_tag>.  A non-null _fmt contains the format and args to create the
 * tag attributes;  it is sent as <_tag ...> where ... is the result of
 * the vsnprintf call.
 */
#define XML_OPEN(_peer, _id...) \
    XML_OPEN_REVERSE(_peer, XSF_NL, _id, NULL)

#define XML_OPEN2(_peer, _id...) \
    XML_OPEN_REVERSE(_peer, 0, _id, NULL)

#define XML_EMPTY(_peer, _id...) \
    XML_OPEN_REVERSE(_peer, XSF_EMPTY | XSF_NL, _id, NULL)

/*
 * XML_CLOSE sends the closing tag, i.e. </_tag>
 */
#define XML_CLOSE(_peer, _id) \
    xml_send_close(_peer, &XML_ARRAY[ _id ], XSF_NL)

/*
 * XML_SUBTREE is a convenience function for opening a subtree.  
 * It handles the emission of the open tag and automatically 
 * emits the closing tag after the following scoped code is 
 * executed.  
 *
 * example:  
 *
 *	XML_SUBTREE(peer, ODCI_MUMBLE) {   .. opening tag is emitted
 *		...code to emit more tags and subtrees here 
 *	}                                  .. closing tag is emitted	
 * 		
 *
 * Be aware that we're hiding behind a global stack here.. 
 */

/* helper functions */

void xml_subtree_open (void *peer /* NULL OK */,
		       const xml_tag_t *tag /* !NULL */, 
		       const char* fmt /* NULL OK */, ...);
int  xml_subtree_once (void);
void xml_subtree_close (void);

/* 
 * atexit() suitable unwind function to emit all matching close 
 * tags immediately. 
 */

void xml_subtree_unwind (void);
int xml_subtree_marker(void);
void xml_subtree_unwind2(int marker);

#define XML_SUBTREE_MAXDEPTH 512 /* don't recurse more than this */

#define XML_SUBTREE(_peer, _id) 		\
    for(xml_subtree_open(_peer, &XML_ARRAY[ _id ], NULL); \
        xml_subtree_once();			\
        xml_subtree_close())			\

#define XML_SUBTREE2(_peer, _id, _fmt...)	\
    for(xml_subtree_open(_peer, &XML_ARRAY[ _id ], _fmt); \
        xml_subtree_once();			\
        xml_subtree_close())			\

#define XML_SUBTREE_OPEN(_peer, _id)            \
    xml_subtree_open(_peer, &XML_ARRAY[ _id ], NULL)

#define XML_SUBTREE_OPEN2(_peer, _id, _fmt...)  \
    xml_subtree_open(_peer, &XML_ARRAY[ _id ], _fmt) 

#define XML_SUBTREE_CLOSE(_id)                  \
do {                                            \
    xml_subtree_close();                        \
    xml_subtree_once();                         \
} while(0)

#define XML_SUBTREE_UNWIND()                    \
    xml_subtree_unwind()

#define XML_SUBTREE_MARKER()                    \
    xml_subtree_marker()

#define XML_SUBTREE_UNWIND2(marker)              \
    xml_subtree_unwind2(marker)
        
/*
 * XML_CONTENT builds the data to send using _fmt and any args, performs
 * an xmlescape on the data, and sends it to the specified peer.
 */
#define XML_CONTENT(_peer, _flags, _data_format...) \
    xml_send(_peer, XSF_ESCAPE | (_flags), _data_format)

#define XML_DATA(_peer, _data_format...) \
    xml_send(_peer, XSF_ESCAPE | XSF_NL, _data_format)

#define XML_DATAV(_peer, _data_format, _vap) \
    xml_sendv(_peer, XSF_ESCAPE | XSF_NL, _data_format, _vap)

/*
 * XML_RAW does the same thing as XML_DATA but sends the
 * raw data (no xmlescape).
 */
#define XML_RAW(_peer, _flags, _format...) \
    xml_send(_peer, _flags | XSF_NL, _format)

/*
 * XML_VALUE sends the opening tag, the xmlescaped data, and the close tag.
 * Note: no attributes are allowed with the tag.  For tags with attributes,
 * use individual calls to XML_OPEN with the attributes, XML_DATA, and
 * XML_CLOSE.
 */
#define XML_ELEMENT(_peer, _id, _flags, _data_format...) \
    xml_send_all(_peer, &XML_ARRAY[ _id ], XSF_ESCAPE | XSF_NL | (_flags), \
                 ##_data_format)

/*
 * XML_ELT must have a format statement followed by arguments.  This prevents
 * you from abusing XML_ELT with XML_ELT(peer, TAG_X, "%d") and getting
 * random output.  You must do XML_ELT(peer, TAG_X, "%s, "%d"), or better
 * yet, use XML_OUT whenever you can (format statement always chosen for you).
 */
#define XML_ELT(_peer, _id, _data_format, _args...) \
    xml_send_all(_peer, &XML_ARRAY[ _id ], XSF_ESCAPE, _data_format, ##_args)

#define XML_OUT(_peer, _id, _val) \
    xml_send_all(_peer, &XML_ARRAY[ _id ], XSF_ESCAPE, NULL, _val)

#define XML_COMMENT(_peer, _fmt...) \
    xml_send_comment(_peer, XSF_ESCAPE | XSF_NL, _fmt)

#define XML_SEND_SUCCESS_TAG(_peer) \
    xml_send_success_tag(_peer);

#define XML_ATTR(_id, _fmt...) \
    xml_attribute(&XML_ARRAY[ _id ], _fmt)

typedef void (*xml_send_method_t)(void *, const char *, ...);

xml_send_method_t
xml_set_send_method (xml_send_method_t func);

typedef char * (*xml_get_method_t)(void *, int *typep, char **restp, unsigned flags);

xml_get_method_t
xml_set_get_method (xml_get_method_t func);

typedef boolean (*xml_eof_method_t)(void *);

xml_eof_method_t
xml_set_eof_method (xml_eof_method_t func);

typedef int (*xml_vsnprintf_method_t)(char *, size_t, const char *, va_list);

void
xml_set_vsnprintf_method (xml_vsnprintf_method_t func);

typedef void (*xml_flush_method_t)(void *);

void
xml_set_flush_method (xml_flush_method_t func);

xml_flush_method_t xml_get_flush_method(void);
 
/*
 * xml_attribute:
 *	return a buffer with '<attr>="<escaped attr_val>"' as its contents.
 * 	NULL is returned if there is not enough memory to accomodate.
 *	NOTE: callers should NOT free this memory.
 * 
 */
const char *xml_attribute (const xml_tag_t *tag, const char *fmt, ...);
const char *xml_attr_seconds (time_t seconds);
const char *xml_attr_microseconds (u_int32_t microseconds);
const char *xml_attr_celsius (int degrees);
const char *xml_attr_celsius_float_1_decimal (float degrees);
const char *xml_attr_celsius_float_2_decimal (float degrees);
const char *xml_attr_style (const char *style);
const char *xml_attr_format (const char *format);
const char *xml_attr_emit (void);
const char *xml_attr_marked (void);
const char *xml_attr_indent (int indent);
const char *xml_attr_display (const char *display);
const char *xml_attr_xmlns (const char *xml_namespace);
const char *xml_attr_named_xmlns (const char *xml_namespace, const char *tag);
const char *xml_attr_schema_location (const char *xml_namespace);
const char *xml_attr_mempages (const int64_t pages);

/*
 * xml_tag_equal: return true if a tag matches in either an empty namespace
 * or one of the two well-defined ones.
 */
int xml_tag_equal(const char *input, const char *tag);

/*
 * xml_make_namespace: make the namespace string given the base name
 */
static inline void
xml_make_namespace (char *buf, size_t buflen, const char *name)
{
    snprintf(buf, buflen, "%s*/%s",
	     XMLRPC_NS_PREFIX,
             name);
}

/*
 * xml_make_schema_location: make a schemaLocation pair consisting
 * of the namespace and associated schemaLocation.
 */
static inline void
xml_make_schema_location (char *buf, size_t buflen, const char * ns)
{
    size_t slen, prefix_len = sizeof(XMLRPC_NS_PREFIX) - 1;

    INSIST(buf && ns && buflen);
    slen = strlen(ns);
    INSIST(slen >= prefix_len);
    snprintf(buf, buflen, "%s%s.xsd",
	     XMLRPC_SCHEMA_LOCATION_PREFIX, ns + prefix_len);
}


/*
 * xml_output* functions, so that programs do not need to define
 * an <output> tag to emit preformatted output.
 */
void xml_output (void *dst, const char *fmt, ...);
void xml_output_start (void *dst);
void xml_output_end (void *dst);

#ifdef __cplusplus
    }
}
#endif /* __cplusplus */

#endif /* JUNOSCRIPT_XMLRPC_H */
