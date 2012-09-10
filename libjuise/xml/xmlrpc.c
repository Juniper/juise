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

#include <libjuise/juiseconfig.h>
#include "config.h"

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

#if 0
boolean
xml_send_elementv (void *peer, const char *name, unsigned flags,
		   const char *fmt, va_list vap)
{
    boolean rc;
    char *outbuf, *xmlbuf;
    char buf[ BUFSIZ ];
    const char *allfmt = "<%s>%s%s%s</%s>", *nl = (flags & XSF_NL) ? "\n" : "";
    xml_vsnprintf_method_t xml_vsnprintf = xml_vsnprintf_method ?: vsnprintf;
    size_t buflen;

    buflen = xml_vsnprintf(buf, sizeof(buf), fmt, vap);

    /*
     * Was buf too small for output?  Try again if so...
     * Leave room for the (optional) terminating newline.
     */
    if (buflen + 2 >= sizeof(buf)) { /* trailing newline + NUL */
	outbuf = alloca(buflen + 2);
	xml_vsnprintf(outbuf, buflen + 1, fmt, vap);
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

    rc = xml_send(peer, (flags & ~XSF_ESCAPE) | XSF_NL, allfmt, name,
		  nl, xmlbuf, nl, name);
    return rc;
}

boolean
xml_send_all (void *peer, const xml_tag_t *tag, unsigned flags,
	      const char *fmt, ...)
{
    boolean rc;
    va_list vap;

    if (fmt == NULL)
	fmt = tag ? xml_formats[ tag->xt_type ] : "";

    va_start(vap, fmt);
    rc = xml_send_elementv(peer, tag->xt_name, flags, fmt, vap);
    va_end(vap);

    return rc;
}

boolean
xml_send_element (void *peer, const char *name, unsigned flags,
		      const char *fmt, ...)
{
    boolean rc;
    va_list vap;

    va_start(vap, fmt);
    rc = xml_send_elementv(peer, name, flags, fmt, vap);
    va_end(vap);

    return rc;
}
#endif

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

#if 0
boolean
xml_forward_error (void *dst, void *src, int flags)
{
    int type;
    const char *tag;
    char *rest;
    boolean rc = FALSE;
    int send_flags;
    xml_tag_t xt;
    boolean quiet = FALSE;
    boolean hit = FALSE;

    bzero(&xt, sizeof(xt));

    XML_OPEN(dst, ODCI_XNM_ERROR, "%s %s",
	     xml_attr_xmlns(XNM_FULL_NS),
	     xml_attr_named_xmlns(XNM_FULL_NS, XNM_QUALIFIER));

    for (tag = NULL, rest = NULL; ; ) {
	if (!quiet && dst && tag) {
	    send_flags = 0;

	    switch (type) {
	    case XML_TYPE_EMPTY:
		send_flags |= XSF_EMPTY;
		/* fallthru */

	    case XML_TYPE_OPEN:
		xt.xt_name = tag;
		xml_send_open(dst, &xt, send_flags, rest ? "%s" : NULL, rest);
		hit = TRUE;
		break;

	    case XML_TYPE_CLOSE:
		xt.xt_name = tag;
		xml_send_close(dst, &xt, 0);
		break;

	    case XML_TYPE_DATA:
		xml_send(dst, XSF_NL, "%s", tag);
		break;
	    }
	}

	tag = NULL;
	xml_input_match(src, 0, &type, &tag, &rest, &flags);

	/*
	 * If we see a </rpc-reply> or EOF, we bail immediately with
	 * a return code set to TRUE. If we see a </error> we bail
	 * if the caller wants us to; otherwise we stop reporting
	 * tags on the dst peer, and just suck input up until we hit
	 * EOF or </rpc-reply>.
	 */
	if (type == XML_TYPE_CLOSE && tag) {
	    if (streq(tag, XMLRPC_REPLY)) {
		rc = TRUE;
		break;
	    }
	    if (streq(tag, XMLRPC_ERROR)) {
		if (!(flags & XIMF_DRAINRPC)) break;
		quiet = TRUE;
	    }
	}

	if (xml_eof(src)) {
	    rc = TRUE;
	    break;
	}
    }

    if (!hit)
	XML_OUT(dst, ODCI_MESSAGE,
		"unknown error condition: communication error");

    /* Always make a close tag */
    XML_CLOSE(dst, ODCI_XNM_ERROR);

    return rc;
}

/*
 * xml_input_rpc: make a simple xmlrpc, returning the simple results
 */
boolean
xml_input_rpc (void *peer, void *error_peer,
	       unsigned flags, int *typep, char **tagp,
	       char **restp, char **datap, const char *rpctag,
	       const char *rpcminor, const char *fmt, ...)
{
    int type, ws_preserve = 0;
    const char *tag;
    char *rest, *data, *operation;

    if (flags & XIMF_PRESERVE_WS)
	ws_preserve = 1;

    flags = flags & (~XIMF_PRESERVE_WS);

    flags |= XIMF_SKIP_ABORTS | XIMF_SKIP_COMMENTS;

    do {
	if (rpctag) {
	    va_list vap;

	    va_start(vap, fmt);
	    if (xml_send_rpcv(peer, flags, rpctag, rpcminor, fmt, vap)) {
		va_end(vap);
		DEBUGIF(flags & XIMF_TRACE,
			dbgpr("xml_input_rpc: send failed"));
		js_error("could not make rpc request: %s%s%s",
			 rpctag, rpcminor ? ":" : "", rpcminor ?: "");
		break;
	    }
	    va_end(vap);
	}

	tag = XMLRPC_REPLY;
	if (!xml_input_match(peer, XML_TYPE_OPEN, NULL, &tag, &rest, &flags))
	    break;

	tag = NULL;
	if (!xml_input_match(peer, XML_TYPE_OPEN, &type,
			     &tag, &rest, &flags)) {
	    if (type == XML_TYPE_CLOSE && tag && streq(tag, XMLRPC_REPLY)) {
		if (typep) *typep = (flags & XIMF_ABORT_SEEN)
			       ? XML_TYPE_ABORT : XML_TYPE_DATA;
		if (tagp) *tagp = NULL;
		if (restp) *restp = NULL;
		if (datap) *datap = NULL;
		return (flags & XIMF_ABORT_SEEN);
	    }
	    /* deal with empty data element later */
	    if (type != XML_TYPE_EMPTY)
	    break;
	}

	if (tag == NULL) break;

	/* If the input is a empty data element <data/>, returns an empty
	 * string 
 	 */
	if (type == XML_TYPE_EMPTY) {
	    data = ALLOCADUPX("");
	    operation = ALLOCADUPX(tag);
	    rest = ALLOCADUPX(rest);
	} else {

	    if (streq(tag, XMLRPC_ERROR)) {
		xml_forward_error(error_peer, peer,
			XIMF_DRAINRPC | XIMF_SKIP_ABORTS | XIMF_SKIP_COMMENTS);
		break;
	    }

	    operation = ALLOCADUPX(tag);
	    rest = ALLOCADUPX(rest);

	    tag = NULL;

	    if (ws_preserve)
		flags |= XIMF_PRESERVE_WS;
	    if (!xml_input_match(peer, XML_TYPE_DATA, NULL,
				 &tag, NULL, &flags))
		break;

	    data = ALLOCADUPX(tag);

	    tag = operation;
	    if (!xml_input_match(peer, XML_TYPE_CLOSE, NULL,
				 &tag, NULL, &flags))
		break;
	}

	tag = XMLRPC_REPLY;
	if (!xml_input_match(peer, XML_TYPE_CLOSE, NULL, &tag, NULL, &flags))
	    break;

	if (flags & XIMF_ABORT_SEEN)
	    break;

	if (typep) *typep = XML_TYPE_DATA;
	if (tagp) *tagp = strdup(operation);
	if (restp) *restp = rest ? strdup(rest) : NULL;
	if (datap) *datap = data ? strdup(data) : NULL;

	return FALSE;

    } while (0);

    if (typep)
	*typep = (flags & XIMF_ABORT_SEEN) ? XML_TYPE_ABORT : XML_TYPE_ERROR;
    if (tagp) *tagp = NULL;
    if (restp) *restp = NULL;
    if (datap) *datap = NULL;

    return TRUE;
}

/*
 * xml_attribute:
 *	return a buffer with '<attr>="<escaped attr_val>"' as its contents.
 * 	NULL is returned if there is not enough memory to accomodate.
 *	NOTE: callers must not free this memory.
 * 
 */
const char *
xml_attribute (const xml_tag_t *tag, const char *fmt, ...)
{
    char	**rb;
    char	*attr=NULL;
    size_t 	attr_size;
    size_t 	bufsize;
    int		namelen;
    char 	*new_buf;
    int 	rc;
    va_list 	vap;

    if (fmt == NULL) fmt = tag ? xml_formats[ tag->xt_type ] : "";
    
    va_start(vap, fmt);
    rc = vasprintf(&attr, fmt, vap); /* must free attr!!! */
    va_end(vap);
    if (rc < 0) {
	if (attr) free(attr);	/* paranoia won't destroy ya */
	return NULL;
    }
    
    /*
     * figure out the size of the buffer we need, including for
     * 1 '=', 2 '"' and null terminator
     */
    attr_size = xml_escaped_size(attr, TRUE, 0);
    if (attr_size == 0) attr_size = strlen(attr);
    namelen = strlen(tag->xt_name);
    bufsize = namelen + attr_size + 4;
    rb = mgmt_get_rolling_buffer();

    new_buf = realloc(*rb, bufsize);
    if (new_buf == NULL) {
	free(attr);
	return NULL;
    }
    *rb = new_buf;

    /*
     * Write '<attr>="<escaped attr_val>"' to new_buf
     * *** NOTE: if format changes, change the bufsize calculation above
     */
    sprintf(new_buf, "%s=\"", tag->xt_name);
    xml_escape(new_buf + namelen + 2, attr_size + 1, attr, TRUE, 0);
    free(attr);	/* had to free attr */
    new_buf[bufsize-2] = '"';
    new_buf[bufsize-1] = '\0';

    return new_buf;
}

const char *
xml_attr_seconds (time_t seconds)
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = JUNOS_SECONDS;
    xt.xt_type = XTT_UNSIGNED;

    return xml_attribute(&xt, NULL, seconds);
}

const char *
xml_attr_marked (void)
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = JUNOS_MARKED;
    xt.xt_type = XTT_STRING;

    return xml_attribute(&xt, NULL, "marked");
}

const char *
xml_attr_microseconds (u_int32_t microseconds)
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = JUNOS_MICROSECONDS;
    xt.xt_type = XTT_UNSIGNED;

    return xml_attribute(&xt, NULL, microseconds);
}

const char *
xml_attr_mempages (const int64_t pages)
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = JUNOS_MEMPAGES;
    xt.xt_type = XTT_INT64;

    return xml_attribute(&xt, NULL, pages);
}

const char *
xml_attr_celsius (int degrees)
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = JUNOS_CELSIUS;
    xt.xt_type = XTT_UNSIGNED;

    return xml_attribute(&xt, NULL, degrees);
}

const char *
xml_attr_celsius_float_1_decimal (float degrees)
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = JUNOS_CELSIUS;
    xt.xt_type = XTT_UNSIGNED;

    return xml_attribute(&xt, "%.1f", degrees);
}

const char *
xml_attr_celsius_float_2_decimal (float degrees)
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = JUNOS_CELSIUS;
    xt.xt_type = XTT_UNSIGNED;

    return xml_attribute(&xt, "%.2f", degrees);
}

const char *
xml_attr_style (const char *style)
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = JUNOS_STYLE;
    xt.xt_type = XTT_STRING;

    return xml_attribute(&xt, NULL, style);
}

const char *
xml_attr_format (const char *format)
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = JUNOS_FORMAT;
    xt.xt_type = XTT_STRING;

    return xml_attribute(&xt, NULL, format);
}

const char *
xml_attr_emit ()
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = JUNOS_EMIT;
    xt.xt_type = XTT_STRING;

    return xml_attribute(&xt, NULL, "emit");
}

const char *
xml_attr_indent (int indent)
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = JUNOS_INDENT;
    xt.xt_type = XTT_STRING;

    return xml_attribute(&xt, "%d", indent);
}

const char *
xml_attr_display (const char *display)
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = JUNOS_DISPLAY;
    xt.xt_type = XTT_STRING;

    return xml_attribute(&xt, NULL, display);
}

const char *
xml_attr_xmlns (const char *namespace)
{
    xml_tag_t xt;

    bzero(&xt, sizeof(xt));
    xt.xt_name = XMLNS_ATTR;
    xt.xt_type = XTT_STRING;

    return xml_attribute(&xt, NULL, namespace);
}

const char *
xml_attr_named_xmlns (const char *namespace, const char *tag)
{
    xml_tag_t xt;
    char buf[ BUFSIZ ];

    snprintf(buf, sizeof buf, "%s:%s", XMLNS_ATTR, tag);
    
    bzero(&xt, sizeof(xt));
    xt.xt_name = buf;
    xt.xt_type = XTT_STRING;

    return xml_attribute(&xt, NULL, namespace);
}

const char *
xml_attr_schema_location (const char *namespace)
{
    xml_tag_t xt;
    char buf[BUFSIZ];

    bzero(&xt, sizeof(xt));
    xt.xt_name = XMLRPC_SCHEMA_LOCATION;
    xt.xt_type = XTT_STRING;

    xml_make_schema_location(buf, sizeof(buf), namespace);
  
    return xml_attribute(&xt, "%s %s", namespace, buf);
}
#endif

const char *
xml_get_attribute (const char **cpp, const char *name)
{
    for (; *cpp; cpp += 2)
	if (streq(*cpp, name)) return cpp[ 1 ];

    return NULL;
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
