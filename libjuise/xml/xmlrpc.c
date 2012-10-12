/*
 * $Id$
 *
 * Copyright (c) 2005-2008, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * Functions for handling xml rpc
 *
 */

#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <stdarg.h>
#include <sys/queue.h>

#include "juiseconfig.h"
#include <libjuise/io/fbuf.h>
#include <libjuise/time/timestr.h>
#include <libjuise/common/aux_types.h>
#include <libjuise/common/bits.h>
#include <libjuise/string/strextra.h>
#include <libjuise/io/dbgpr.h>
#include <libjuise/xml/xmlutil.h>
#include <libjuise/xml/xmlrpc.h>
#include <libjuise/xml/client.h>
#include <libjuise/common/allocadup.h>

#if defined(HOSTPROG) && !defined(va_copy)
#define va_copy(dest, src) ((dest) = (src))
#endif

const char *xml_formats[] = {
    "", /* XTT_UNKNOWN */
    "%d", /* XTT_INTEGER */
    "%u", /* XTT_UNSIGNED */
    "%s", /* XTT_STRING */
    "%qd", /* XTT_INT64 */
    "%qu", /* XTT_UINT64 */
    NULL
};

static xml_send_method_t xml_send_method;
static xml_eof_method_t  xml_eof_method;
static xml_vsnprintf_method_t xml_vsnprintf_method;
static xml_flush_method_t xml_flush_method;

/*
 * The xml namespace attribute
 */
#define XMLNS_ATTR              "xmlns"

xml_send_method_t
xml_set_send_method (xml_send_method_t func)
{
    xml_send_method_t old_func = xml_send_method;
    xml_send_method = func;
    return old_func;
}

xml_eof_method_t
xml_set_eof_method (xml_eof_method_t func)
{
    xml_eof_method_t old_func = xml_eof_method;
    xml_eof_method = func;
    return old_func;
}

void
xml_set_vsnprintf_method (xml_vsnprintf_method_t func)
{
    xml_vsnprintf_method = func;
}

void
xml_set_flush_method (xml_flush_method_t func)
{
    xml_flush_method = func;
}

xml_flush_method_t
xml_get_flush_method(void)
{
    return xml_flush_method;
}

static void
xml_abort_cb (void)
{
    js_client_set_aborting(TRUE);
}

boolean
xml_input_match (void *peer, int type_to_match, int *typep,
		 const char **tagp, char **restp, unsigned *flagsp)
{
    return xml_input_match2(peer, type_to_match, typep, tagp, restp, flagsp,
                            xml_abort_cb);
}

/*
 * xml_sendv: sends data over mgmt peer connection
 * Returns FALSE if no errors occurred.
 */
boolean
xml_sendv (void *peer UNUSED, unsigned flags, const char *fmt, va_list vap)
{
    char buf[ BUFSIZ ];
    char *outbuf, *xmlbuf;
    size_t buflen;
    xml_vsnprintf_method_t xml_vsnprintf = xml_vsnprintf_method ?: vsnprintf;
    va_list newvap;
    
    va_copy(newvap, vap);
    buflen = xml_vsnprintf(buf, sizeof(buf), fmt, newvap);
    va_end(newvap);

    /*
     * Was buf too small for output?  Try again if so...
     * Leave room for the (optional) terminating newline.
     */
    if (buflen + 2 >= sizeof(buf)) { /* trailing newline + NUL */
	outbuf = alloca(buflen + 2);
        va_copy(newvap, vap);
	xml_vsnprintf(outbuf, buflen + 1, fmt, newvap);
        va_end(newvap);
    } else {
	outbuf = buf;
    }

    if (flags & XSF_ESCAPE) {
	buflen = xml_escaped_size(outbuf, FALSE, 0);
	/* Do we need to call xml_escape? */
	if (buflen > 0) {
	    xmlbuf = alloca(buflen + 2);
	    xml_escape(xmlbuf, buflen + 1, outbuf, FALSE, 0);
	} else {
	    xmlbuf = outbuf;
	}
    } else {
	xmlbuf = outbuf;
    }

    if (flags & XSF_NL) {
	/* Add a terminating newline */
	char *cp = xmlbuf + strlen(xmlbuf);

	if (cp == xmlbuf || (cp > xmlbuf && cp[ -1 ] != '\n')) {
	    *cp++ = '\n';
	    *cp = 0;
	}
    }
    
    if (xml_send_method) 
	(*xml_send_method)(peer, "%s", xmlbuf);
    else {
	fputs(xmlbuf, stdout);
	fflush(stdout);
    }

    return FALSE;
}

/*
 * xml_send: sends data over mgmt peer connection
 * Returns FALSE if no errors occurred.
 */
boolean
xml_send (void *peer, unsigned flags, const char *fmt, ...)
{
    boolean rc;
    va_list vap;

    va_start(vap, fmt);
    rc = xml_sendv(peer, flags, fmt, vap);
    va_end(vap);

    return rc;
}

/*
 * xml_send_open: sends data over mgmt peer connection
 * Returns FALSE if no errors occurred.
 */

boolean
xml_send_open (void *peer, const xml_tag_t *tag, unsigned flags,
	       const char *fmt, ...)
{
    va_list vap;
    boolean rc;
    char buf[ BUFSIZ ];
    const char *endp = "";
    
    if ((flags & XSF_EMPTY) || (tag->xt_flags & XTF_EMPTY))
	endp = "/";

    snprintf(buf, sizeof(buf), "<%s%s%s%s>", tag->xt_name,
	     fmt ? " " : "", fmt ?: "", endp);
    va_start(vap, fmt);
    rc = xml_sendv(peer, flags & ~XSF_ESCAPE, buf, vap); 
    va_end(vap);
    return rc;
}

boolean
xml_send_close (void *peer, const xml_tag_t *tag, unsigned flags)
{
      return xml_send(peer, flags, "</%s>", tag->xt_name);
}

boolean
xml_send_comment (void *peer, unsigned flags, const char *fmt, ...)
{
    va_list vap, newvap;
    char buf[ BUFSIZ ], *outbuf, *xmlbuf;
    size_t buflen;

    va_start(vap, fmt);

    va_copy(newvap, vap);
    if (xml_vsnprintf_method)
        buflen = xml_vsnprintf_method(buf, sizeof(buf), fmt, newvap);
    else
        buflen = vsnprintf(buf, sizeof(buf), fmt, newvap);
    va_end(newvap);

    /*
     * Was buf too small for output?  Try again if so...
     * Leave room for the (optional) terminating newline.
     */
    if (buflen + 1 >= sizeof(buf)) {
	outbuf = alloca(buflen + 1);
        va_copy(newvap, vap);
        if (xml_vsnprintf_method)
            xml_vsnprintf_method(outbuf, buflen, fmt, newvap);
        else
            vsnprintf(outbuf, buflen, fmt, newvap);
        va_end(newvap);
    } else {
	outbuf = buf;
    }

    va_end(vap);

    buflen = xml_escaped_size(outbuf, FALSE, 0);
    /* Do we need to call xml_escape? */
    if (buflen > 0) {
	xmlbuf = alloca(buflen + 1);
	xml_escape(xmlbuf, buflen + 1, outbuf, FALSE, 0);
    } else {
	xmlbuf = outbuf;
    }

    return xml_send(peer, flags & ~XSF_ESCAPE, "<!-- %s -->", xmlbuf);
}

static boolean
xml_send_rpc_content (void *peer, int flags, const char *outer,
		      const char *major, const char *minor,
		      const char *fmt, va_list vap)
{
    char *buf, *buf_open, *buf_content, *buf_close, test_char, *endp;
    size_t len_open, len_content, len_close, len_escaped;
    static const char rpc_open[] = "<%s>\n";
    static const char rpc_close[] = "</%s>\n";
    va_list newvap;

    /*
     * First send the XMLRPC. We dont' allow many options here, but...
     */
    len_open = 0;
    if (outer) len_open += sizeof(rpc_open) - 1 + strlen(outer);
    if (major) len_open += sizeof(rpc_open) - 1 + strlen(major);
    if (minor) len_open += sizeof(rpc_open) - 1 + strlen(minor);
    buf = buf_open = alloca(len_open + 1);
    endp = buf + len_open + 1;
    if (outer) buf += snprintf(buf, endp - buf, rpc_open, outer);
    if (major) buf += snprintf(buf, endp - buf, rpc_open, major);
    if (minor) buf += snprintf(buf, endp - buf, rpc_open, minor);
    len_open = buf - buf_open;

    len_close = len_open + 3;
    buf = buf_close = alloca(len_close + 1);
    endp = buf + len_close + 1;
    if (minor) buf += snprintf(buf, endp - buf, rpc_close, minor);
    if (major) buf += snprintf(buf, endp - buf, rpc_close, major);
    if (outer) buf += snprintf(buf, endp - buf, rpc_close, outer);
    len_close = buf - buf_close;

    va_copy(newvap, vap);
    if (xml_vsnprintf_method)
        len_content = xml_vsnprintf_method(&test_char, 1, fmt ?: "", newvap);
    else
        len_content = vsnprintf(&test_char, 1, fmt ?: "", newvap);
    va_end(newvap);

    buf_content = alloca(len_content + 2);
    va_copy(newvap, vap);
    if (xml_vsnprintf_method)
        xml_vsnprintf_method(buf_content, len_content + 1, fmt ?: "", newvap);
    else
        vsnprintf(buf_content, len_content + 1, fmt ?: "", newvap);
    va_end(newvap);

    /* Add terminating newline */
    buf = buf_content + (len_content ? len_content - 1 : 0);
    if (*buf && *buf != '\n') {
	*++buf = '\n';
	*++buf = 0;
	len_content += 1;
    }

    len_escaped = xml_escaped_size(buf_content, FALSE, 0);
    if (len_escaped > len_content) {
	buf = alloca(len_escaped + 1);
	xml_escape(buf, len_escaped + 1, buf_content, FALSE, 0);
	buf_content = buf;
	len_content = len_escaped;
    }

    /* Combine the buffer; replace w/ mgmt_sock_writev() */
    buf = alloca(len_open + len_content + len_close + 1);
    memcpy(buf, buf_open, len_open);
    memcpy(buf + len_open, buf_content, len_content);
    memcpy(buf + len_open + len_content, buf_close, len_close + 1);

    if (xml_send(peer, flags & ~XSF_ESCAPE, "%s", buf)) {
	DEBUGIF(flags & XIMF_TRACE, dbgpr("xml_send_rpc: send failed"));
	return TRUE;
    }

    return FALSE;
}

boolean
xml_send_rpcv (void *peer, int flags, const char *tagmajor,
	       const char *tagminor, const char *fmt, va_list vap)
{
    return xml_send_rpc_content(peer, flags, XMLRPC_REQUEST,
				tagmajor, tagminor, fmt, vap);
}

boolean
xml_send_rpc (void *peer, int flags, const char *tagmajor,
	      const char *tagminor, const char *fmt, ...)
{
    va_list vap;
    boolean rc;

    va_start(vap, fmt);
    rc = xml_send_rpcv(peer, flags, tagmajor, tagminor, fmt, vap);
    va_end(vap);

    return rc;
}

boolean
xml_send_replyv (void *peer, int flags, const char *tagmajor,
		 const char *tagminor, const char *fmt, va_list vap)
{
    return xml_send_rpc_content(peer, flags, XMLRPC_REPLY,
				tagmajor, tagminor, fmt, vap);
}

boolean
xml_send_reply (void *peer, int flags, const char *tagmajor,
		const char *tagminor, const char *fmt, ...)
{
    va_list vap;
    boolean rc;

    va_start(vap, fmt);
    rc = xml_send_replyv(peer, flags, tagmajor, tagminor, fmt, vap);
    va_end(vap);

    return rc;
}

boolean
xml_send_success_tag (void *peer)
{

    return xml_send(peer, XSF_NL, XMLRPC_SUCCESS_TAG);
}

boolean
xml_eof (void *peer)
{
    INSIST(xml_eof_method != NULL);
    
    return (*xml_eof_method)(peer);
}

/*
 * A tag is characterized by: a name, a type, a namespace and attributes
 * according to fbuf_get_xml_namespace. This function is used to
 * reconstruct the tag and send it out.
 */
boolean
xml_put_namespace (void *peer, const char *name, int type,
		   const char *namespace, const char *rest)
{
    switch (type) {
    case XML_TYPE_OPEN:
	if (namespace)
	    return XML_RAW(peer, 0, rest ? "<%s:%s %s>" : "<%s:%s>",
			   namespace, name, rest);
	else
	    return XML_RAW(peer, 0, rest ? "<%s %s>" : "<%s>",
			   name, rest);
	break;

    case XML_TYPE_DATA:
	return XML_RAW(peer, 0, "%s", name);
	break;

    case XML_TYPE_CLOSE:
	if (namespace)
	    return XML_RAW(peer, 0, "</%s:%s>", namespace, name);
	else 
	    return XML_RAW(peer, 0, "</%s>", name);
	break;

    case XML_TYPE_EMPTY:
	return XML_RAW(peer, 0, "<%s/>", name);
	break;

    case XML_TYPE_COMMENT:
	return XML_COMMENT(peer, "%s", name);
	break;
    }

    dbgpr("xml_put_namespace: unknown tag type %d", type);
    return TRUE;
}

#if defined(UNIT_TEST)
const char *test[] = {
    "&\"<>=&\"<>=&\"<>=",
    "one\" & two', = three <rah>",
    "one two three",
    NULL
};

int
main (int argc UNUSED, char *argv UNUSED)
{
    int i;
    size_t len;
    char *cp, *tp;
    const char *zp;

    for (i = 0, zp = test[ i ]; zp; zp = test[ ++i ]) {
	len = xml_escaped_size(zp, FALSE, 0);
	if (len == 0) continue;

	cp = alloca(len + 1);
	tp = alloca(len + 1);

	if (xml_escape(cp, len, zp, FALSE, 0))
	    printf("failed xml_escape for '%s'\n", zp);
	else if (xml_unescape(tp, len, cp, FALSE))
	    printf("failed xml_unescape for '%s'\n", zp);
	else if (strcmp(tp, zp) != 0)
	    printf("failed compare for '%s'\n", zp);

	cp = alloca(len);

	if (xml_escape(cp, len, zp, FALSE, 0))
	    printf("failed xml_escape2 for '%s'\n", zp);
	else if (xml_unescape(cp, len, cp, FALSE))
	    printf("failed xml_unescape2 for '%s'\n", zp);
    }

    return 0;
}
#endif /* UNIT_TEST */
