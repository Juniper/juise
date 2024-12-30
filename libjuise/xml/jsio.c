/*
 * $Id$
 *
 * Copyright (c) 2006-2008, 2011, 2017, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * Session-based interface to JUNOScript/netconf sessions
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <string.h>
#include <signal.h>
#include <paths.h>
#include <pwd.h>

#include <libxml/xpathInternals.h>
#include <libxml/parserInternals.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <libxslt/extensions.h>

#include "juiseconfig.h"
#include <libpsu/psucommon.h>
#include <libpsu/psustring.h>
#include <libpsu/psulog.h>
#include <libjuise/common/aux_types.h>
#include <libjuise/string/strextra.h>
#include <libjuise/common/allocadup.h>
#include <libjuise/common/bits.h>
#include <libjuise/data/patricia.h>
#include <libjuise/io/dbgpr.h>
#include <libjuise/io/logging.h>
#include <libjuise/io/pid_lock.h>
#include <libjuise/io/fbuf.h>
#include <libjuise/env/env_paths.h>
#include <libjuise/xml/xmlrpc.h>
#include <libjuise/xml/client.h>
#include <libjuise/xml/xmlutil.h>
#include <libjuise/xml/libxml.h>
#include <libjuise/xml/jsio.h>
#include <libjuise/xml/extensions.h>

#include <libslax/slax.h>
#include <libslax/xmlsoft.h>

static const char xml_parser_reset[] = XML_PARSER_RESET;
const int xml_parser_reset_len = sizeof(xml_parser_reset) - 1;

static const char xml_trailer[] = "</" XMLRPC_APINAME ">\n";
const int xml_trailer_len = sizeof(xml_trailer);

static const char xmldec[] = "<?xml version=\"1.0\"?>\n" ;
static const char fake_creds[] = "<?xml version=\"1.0\"?>\n<" 
			XMLRPC_APINAME " version=\"" XMLRPC_VERSION "\">\n";

#undef CS_TRC_RPC
#define CS_TRC_RPC TRACE_ALL

/*
 * Patricia tree for sessions;  a patricia tree is overkill
 * for most cases, but we need to plan on someone making a zillion
 * node test case.
 */
static patroot js_session_root;
static char *js_default_server;
static char *js_default_user;
static char *js_mixer;
static session_type_t js_default_stype = ST_JUNOSCRIPT;
static int js_auth_muxer_id;
static int js_auth_websocket_id;
static char *js_auth_div_id;

static unsigned jsio_flags;
static char js_netconf_ns_attr[] = "xmlns=\"" XNM_NETCONF_NS "\"";
static int js_max = 345;	/* Max read buffer size */
static char jsio_askpass_socket_path[BUFSIZ];
static int jsio_askpass_socket;

trace_file_t *trace_file;	/* Common trace file */

typedef struct mx_header_s {
    char mh_pound;              /* Leader: pound sign */
    char mh_version[2];         /* MX_HEADER_VERSION */
    char mh_dot1;               /* Separator: period */
    char mh_len[8];             /* Total data length (including header) */
    char mh_dot2;               /* Separator: period */
    char mh_operation[8];       /* Operation name */
    char mh_dot3;               /* Separator: period */
    char mh_muxid[8];           /* Muxer ID */
    char mh_dot4;               /* Separator: period */
    char mh_trailer[];
} mx_header_t;

static unsigned long
strntoul (const char *buf, size_t bufsiz)
{
    unsigned long val = 0;

    for ( ; bufsiz > 0; buf++, bufsiz--) {
	if (isdigit((int) *buf)) {
	    val = val * 10 + (*buf - '0');
	}
    }

    return val;
}

#define JS_MX_DEFAULT_BUFFER_SIZE	(1024 * 5)

static js_mx_buffer_t *
js_mx_buffer_create (void)
{
    js_mx_buffer_t *jmbp = calloc(sizeof(js_mx_buffer_t)
	    + JS_MX_DEFAULT_BUFFER_SIZE, 1);
    
    if (jmbp == NULL) {
	return NULL;
    }

    jmbp->jmb_size = JS_MX_DEFAULT_BUFFER_SIZE;
    
    return jmbp;
}

const char *
jsio_session_type_name (session_type_t stype)
{
    if (stype == ST_DEFAULT)
	stype = js_default_stype;

    if (stype == ST_JUNOSCRIPT)
	return "junoscript";

    if (stype == ST_NETCONF)
	return "netconf";

    if (stype == ST_JUNOS_NETCONF)
	return "junos-netconf";

    if (stype == ST_SHELL)
	return "shell";

    if (stype == ST_MIXER)
	return "mixer";

    return NULL;
}

session_type_t
jsio_session_type (const char *name)
{
    if (streq(name, "junoscript"))
	return ST_JUNOSCRIPT;

    if (streq(name, "netconf"))
	return ST_NETCONF;

    if (streq(name, "junos-netconf"))
	return ST_JUNOS_NETCONF;

    if (streq(name, "shell"))
	return ST_SHELL;

    if (streq(name, "mixer"))
	return ST_MIXER;

    return ST_MAX;
}

session_type_t
jsio_set_default_session_type (session_type_t stype)
{
    session_type_t old = js_default_stype;
    js_default_stype = stype;
    return old;
}

void 
jsio_set_auth_muxer_id (char *muxerid)
{
    if (!muxerid) {
	return;
    }

    js_auth_muxer_id = strtol(muxerid, NULL, 10);
}

void 
jsio_set_auth_websocket_id (char *websocketid)
{
    if (!websocketid) {
	return;
    }

    js_auth_websocket_id = strtol(websocketid, NULL, 10);
}

void 
jsio_set_auth_div_id (char *divid)
{
    if (!divid) {
	return;
    }

    js_auth_div_id = strdup(divid);
}
	
static void
jsio_trace(const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);
    tracev(trace_file, TRACE_ALL, fmt, vap);
    va_end(vap);
}

static void
js_mixer_header_format_int (char *buf, int blen, unsigned value)
{
    char *cp = buf + blen - 1;

    for ( ; cp >= buf; cp--) {
	*cp = (value % 10) + '0';
	value /= 10;
    }
}

static void
js_mixer_header_format_string (char *buf, int blen, const char *value)
{
    int vlen = strlen(value);

    if (vlen > blen) {
	vlen = blen;
    }
    memcpy(buf, value, vlen);
    if (vlen < blen) {
	memset(buf + vlen, ' ', blen - vlen);
    }
}

static void
js_mixer_header_build (mx_header_t *mhp, int len, const char *operation,
	unsigned muxid)
{
    mhp->mh_pound = '#';
    mhp->mh_version[0] = MX_HEADER_VERSION_0;
    mhp->mh_version[1] = MX_HEADER_VERSION_1;
    mhp->mh_dot1 = mhp->mh_dot2 = mhp->mh_dot3 = mhp->mh_dot4 = '.';
    js_mixer_header_format_int(mhp->mh_len, sizeof(mhp->mh_len), len);
    js_mixer_header_format_string(mhp->mh_operation,
	    sizeof(mhp->mh_operation), operation);
    js_mixer_header_format_int(mhp->mh_muxid, sizeof(mhp->mh_muxid), muxid);
}

static int
js_mixer_send_simple (js_session_t *jsp, const char *opname, const char *attrs,
	const char *data)
{
    int dlen = data ? strlen(data) : 0;   /* length of data */
    int hlen = sizeof(mx_header_t);       /* length of header */
    int alen = attrs ? strlen(attrs) : 0; /* length of attrs */
    int len = dlen + hlen + alen + 1;     /* length of all plus \n delimiter */
    char buf[len + 1];
    
    jsio_trace("send mixer op '%s' with attrs: '%s', data: '%s'", 
	    opname ?: "<null>", attrs ?: "<null>", data ?: "<null>");

    /* Sanity check our arguments */
    if (opname == NULL || attrs == NULL || data == NULL)
        return -1;

    mx_header_t *mhp = (mx_header_t *) buf;
    js_mixer_header_build(mhp, len, opname, js_auth_muxer_id);

    if (attrs) {
	memcpy(buf + hlen, attrs, alen);
    }

    buf[hlen + alen] = '\n';
    memcpy(buf + hlen + alen + 1, data, dlen + 1);

    return write(jsp->js_stdout, buf, len) > 0;
}

static void
jsio_askpass_make_socket (void)
{
    struct passwd *pwent = getpwuid(getuid());
    struct sockaddr_un addr;

    snprintf(jsio_askpass_socket_path, sizeof(jsio_askpass_socket_path),
	     "%s/.ssh/juise.askpass.%d", pwent ? pwent->pw_dir : ".",
	     (int) getpid());

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
	return;

    memset(&addr, '\0', sizeof(addr));
    addr.sun_family = AF_UNIX;
#ifdef HAVE_SUN_LEN
    addr.sun_len = sizeof(addr);
#endif /* HAVE_SUN_LEN */

    memcpy(addr.sun_path, jsio_askpass_socket_path, sizeof(addr.sun_path));

    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
	return;

    if (listen(sock, 1) < 0)
	return;

    setenv("SSH_ASKPASS", JUISE_LIBEXECDIR "/juise-askpass", 1);
    setenv("SSH_ASKPASS_SOCKET", jsio_askpass_socket_path, 1);
    setenv("DISPLAY", "ThisMustBeSetForSshAskPassToWork", 1);

    jsio_askpass_socket = sock;

    jsio_trace("jsio_askpass: socket %d, path %s",
	       sock, jsio_askpass_socket_path);
}

static void
jsio_askpass_clean_socket (void)
{
    jsio_trace("jsio_askpass: clean");

    if (jsio_askpass_socket_path[0])
	unlink(jsio_askpass_socket_path);
    close(jsio_askpass_socket);
    jsio_askpass_socket = 0;
}

/*
 * Write the buffer to trace file
 */
static void
jsio_buffer_trace (const char *title, char *buf, int bufsiz)
{
    jsio_trace("buffer trace: %s: %p (%d/0x%x)", title, buf, bufsiz, bufsiz);

    char *cp = buf;
    int len = bufsiz;

    while (len > 0) {
	char *ep = memchr(cp, '\n', len);
	int width = ep ? ep - cp : len;

	jsio_trace("buffer: {{{%*.*s}}}", width, width, cp);

	if (ep == NULL)
	    break;

	cp += width + 1;
	len -= width + 1;
    }
}

static int
js_buffer_read_data (js_session_t *jsp, char *bp, int blen)
{
    int rc;

    if (js_max && blen > js_max)
	blen = js_max;

    if (jsp->js_rbuf) {
	int rlen = jsp->js_rlen - jsp->js_off;

	rc = MIN(rlen, blen);
	memcpy(bp, jsp->js_rbuf + jsp->js_roff, rc);
	if (rc <= rlen) {
	    /* Done with js_rbuf */
	    free(jsp->js_rbuf);
	    jsp->js_rbuf = NULL;
	    jsp->js_rlen = jsp->js_roff = 0;
	} else {
	    jsp->js_roff += blen;
	}

    } else {
	rc = read(jsp->js_stdin, bp, blen);
	if ((jsio_flags & JSIO_MEMDUMP) && rc > 0)
	    psu_mem_dump("jsio: read", bp, rc, ">", 0);
    }

    return rc;
}

static int
js_buffer_find_reset_dangling (js_session_t *jsp, char *bp, int *plen)
{
    /*
     * Now we need to see if the data has a trailing xml_parser_reset
     * string.  If so, we emit the closing tag and return end-of-file.
     */
    int len = xml_parser_reset_len - jsp->js_len;
    const char *cp = xml_parser_reset + jsp->js_len;
    int blen = *plen;

    /*
     * So we _might_ have seen the start of a reset string, but ran
     * out of room.  See what we've got now, and decide if we're
     * really seeing reset or not.
     */
    if (blen >= len && memcmp(bp, cp, len) == 0) {
	/* Looking at reset */
	jsp->js_state = JSS_TRAILER;
	jsp->js_len = 0;
	
	/* If there's extra left in the buffer, save it for later */
	if (blen > len && !(blen == len + 1 && bp[len] == '\n')) {
	    int left = blen - len;
	    jsp->js_rbuf = malloc(left);
	    if (jsp->js_rbuf == NULL)
		return -1;

	    memcpy(jsp->js_rbuf, bp + len, left);
	    jsp->js_roff = 0;
	    jsp->js_rlen = left;
	}

	return TRUE;
    }

    /*
     * Okay, bad news.  We got suckered into thinking we saw the
     * beginning of the reset string, but we didn't.  So shift the
     * data and reinsert the sucker bits.  Note that we left room for
     * this by setting blen to bufsiz - the length of the reset
     * string.
     */
    len = jsp->js_len;
    memmove(bp + len, bp, blen);
    memcpy(bp, xml_parser_reset, len);
    *plen += len;
    jsp->js_len = 0;

    return FALSE;
}

/*
 * Now we need to see if the data has a trailing xml_parser_reset
 * string.  If so, we emit the closing tag and return end-of-file.
 */
static int
js_buffer_find_reset (js_session_t *jsp, char *bp, int blen)
{
    /* If there was a dangling reset string, look for the rest */
    if (jsp->js_len && js_buffer_find_reset_dangling(jsp, bp, &blen))
	return 0;

    int reset_len = xml_parser_reset_len;
    const char *reset_value = xml_parser_reset;

    char *cp;
    char *ep = bp + blen;
    int rc = blen;

    /* Look for the reset string */
    for (cp = bp; cp < ep; cp++) {
	if (*cp != *reset_value)
	    continue;

	int left = ep - cp;
	jsio_trace("find_reset: left %d: %p:%p (%d)", left, bp, cp, reset_len);
	
	if (reset_len <= left) {
	    /* We've got enough data to find the entire reset token */
	    if (memcmp(cp + 1, reset_value + 1, reset_len - 1) != 0)
		continue;
	    /* fallthru */

	} else {
	    /*
	     * There's not enough data in the input buffer to do a
	     * full compare, so we compare what we've got.  If it's
	     * a match, we have to save record the length of the
	     * dangling piece.
	     */
	    if (memcmp(cp + 1, reset_value + 1, left - 1) != 0)
		continue;

	    /*
	     * At this point, the end of our input buffer has
	     * the start of a reset string and we won't be
	     * able to determine if this is a really a reset
	     * until we do the next read.  So we record what
	     * we've got and fake a return.
	     */
	    if (jsp->js_rbuf) {
		jsp->js_roff -= left;
		cp -= left;
		return cp - bp;

	    } else {
		jsp->js_len = left;
		return cp - bp;
	    }
	}

	/*
	 * We've seen a reset string.  Trim it, save anything
	 * after it and return the rest.
	 */
	rc = cp - bp;
	cp += reset_len;
	left -= reset_len;

#if 0
	while (left > 1 && isspace((int) cp[0])) {
	    left -= 1;
	    cp += 1;
	}
#endif

	if (left > 0) {
	    jsp->js_rbuf = malloc(left);
	    if (jsp->js_rbuf == NULL)
		return -1;

	    jsp->js_rlen = left;
	    jsp->js_roff = 0;
	    memcpy(jsp->js_rbuf, cp, left);
	}

	jsp->js_state = JSS_TRAILER;
	return rc;
    }

    return rc;
}

/*
 * This function assumes that the buffer is populated with at least one
 * completely framed mixer message.  Decode the first one and bubble it back
 * to libxml.
 */
static int
js_mixer_message_parse (js_session_t *jsp, char *buf, int bufsiz)
{
    mx_header_t *mhp;
    js_mx_buffer_t *jmbp = jsp->js_mx_buffer;
    char *cp = jmbp->jmb_data + jmbp->jmb_start;
    char *ep = jmbp->jmb_data + jmbp->jmb_start + jmbp->jmb_len;

    mhp = (mx_header_t *) cp;

    if (mhp->mh_pound != '#' || mhp->mh_version[0] != MX_HEADER_VERSION_0
	    || mhp->mh_version[1] != MX_HEADER_VERSION_1
	    || mhp->mh_dot1 != '.' || mhp->mh_dot2 != '.'
	    || mhp->mh_dot3 != '.' || mhp->mh_dot4 != '.') {
	jsio_trace("mixer parse request fails (%c)", *cp);
	goto fatal;
    }

    unsigned long len = strntoul(mhp->mh_len, sizeof(mhp->mh_len));
    unsigned long muxid = strntoul(mhp->mh_muxid, sizeof(mhp->mh_muxid));
    char *operation = mhp->mh_operation;
    for (cp = operation + sizeof(mhp->mh_operation) - 1;
	    cp >= operation; cp--) {
	if (*cp != ' ') {
	    break;
	}
    }
    *++cp = '\0';

    if (jmbp->jmb_len < len) {
	goto fatal;
    }

    cp = mhp->mh_trailer;
    for (; cp < ep; cp++) {
	if (*cp == '\n') {
	    break;
	}
    }
    if (cp >= ep) {
	goto fatal;
    }
    *cp++ = '\0';		/* Skip over '\n' */

    /*
     * Mark the header data as consumed.  The rest of the payload
     * may be used during the request.
     */
    int delta = cp - (jmbp->jmb_data + jmbp->jmb_start);
    jmbp->jmb_start += delta; 
    jmbp->jmb_len -= delta;
    len -= delta;

    size_t sent_size = 0;
    if (len > (unsigned long)bufsiz) {
	char *np = strndup(jmbp->jmb_data + jmbp->jmb_start + bufsiz,
		len - bufsiz);

	/*
	 * We have more raw RPC data than we can send back right now.  Put it
	 * in leftovers and move the buffer pointer to the beginning of the
	 * first header.
	 */
	strncpy(buf, jmbp->jmb_data + jmbp->jmb_start, bufsiz);
	jmbp->jmb_leftover = np;
	jmbp->jmb_start += len;
	jmbp->jmb_len -= len;
	sent_size = bufsiz;
    } else {
	strncpy(buf, jmbp->jmb_data + jmbp->jmb_start, len);
	sent_size = len;
    
	jmbp->jmb_start += sent_size;
	jmbp->jmb_len -= sent_size;
    }

    if (jmbp->jmb_len == 0) {
	jmbp->jmb_start = 0;
    }
    
    jsio_trace("mixer received op: '%s', data: '%s', muxid: %lu",
	    operation, buf, muxid);

    if (streq(operation, MX_OP_COMPLETE)) {
	jsp->js_state = JSS_CLOSE;
    } else if (streq(operation, MX_OP_ERROR)) {
	jsp->js_state = JSS_CLOSE;
	return -1;
    }

    return sent_size;

fatal:
    jsio_trace("fatal error parsing mixer reply");
    jmbp->jmb_start = jmbp->jmb_len = 0;

    return -1;
}

/*
 * The data from mixer is framed using our own rolled framing protocol.  We
 * need to read a single message from mixer, de-framize it, and then pass it
 * into the libxml buffer.  We don't need to mess with authentication because
 * mixer handles that for us - we already have a raw netconf connection at
 * this point.
 */
static int
js_mixer_read (void *context, char *buf, int bufsiz)
{
    js_session_t *jsp = context;
    js_mx_buffer_t *jmbp = jsp->js_mx_buffer;
    int size_to_read = jmbp->jmb_size - (jmbp->jmb_start + jmbp->jmb_len);
    mx_header_t *mhp = (mx_header_t *)(jmbp->jmb_data + jmbp->jmb_start);
    js_boolean_t need_more = FALSE;

    /*
     * If we have leftover RPC data to send back to libxml, do that before we
     * continue reading/parsing.
     */
    if (jmbp->jmb_leftover) {
	size_t leftover_size = 0;
	if (strlen(jmbp->jmb_leftover) > (unsigned long)bufsiz) {
	    char *np = strdup(jmbp->jmb_leftover + bufsiz);

	    /*
	     * Still too big to send in one block to libxml, ugh.
	     */
	    strncpy(buf, jmbp->jmb_leftover, bufsiz);
	    free(jmbp->jmb_leftover);
	    jmbp->jmb_leftover = np;
	    leftover_size = bufsiz;
	} else {
	    leftover_size = strlen(jmbp->jmb_leftover);
	    strncpy(buf, jmbp->jmb_leftover, leftover_size);
	    free(jmbp->jmb_leftover);
	    jmbp->jmb_leftover = NULL;
	}
	return leftover_size;
    }

    /*
     * If we have an incomplete block of framed mixer data in our buffer, or
     * if we have no mixer data, read some more.
     */
    if (jmbp->jmb_len == 0) {
	need_more = TRUE;
    } else {
	if (jmbp->jmb_len < sizeof(*mhp)) {
	    need_more = TRUE;
	} else if (mhp->mh_pound != '#' || mhp->mh_version[0] != MX_HEADER_VERSION_0
		|| mhp->mh_version[1] != MX_HEADER_VERSION_1
		|| mhp->mh_dot1 != '.' || mhp->mh_dot2 != '.'
		|| mhp->mh_dot3 != '.' || mhp->mh_dot4 != '.') {
	    jsio_trace("mixer parse request failed");
	    return -1;
	}

	unsigned long len = strntoul(mhp->mh_len, sizeof(mhp->mh_len));

	if (jmbp->jmb_len < len) {
	    need_more = TRUE;
	}
    }

    if (need_more) {
	/*
	 * Read some more data from mixer
	 */
	int recvlen = recv(jsp->js_stdin, jmbp->jmb_data + jmbp->jmb_start,
		size_to_read, 0);
	if (recvlen < 0) {
	    jsio_trace("reading from mixer failed");
	    return -1;
	} else if (recvlen == 0) {
	    jsio_trace("unexpected disconnect from mixer");
	    return 0;
	}
    
	jmbp->jmb_len += recvlen;
    }

    return js_mixer_message_parse(jsp, buf, bufsiz);
}

/*
 * Read the data from the server into buffer. Takes care of emitting 
 * credentials in the begining and xml trailer (</junoscript>) in the end.
 */
static int
js_buffer_read (void *context, char *buf, int bufsiz)
{
    js_session_t *jsp = context;
    int rc, len = 0, blen = bufsiz - xml_parser_reset_len;
    const char *cp;
    char *bp = buf;
    int leading_blanks = 0;

    /*
     * If we're in CLOSE or DEAD state, we should fail reads
     */
    if (jsp->js_state == JSS_CLOSE || jsp->js_state == JSS_DEAD) {
	jsio_trace("buffer read EOF (%d)", jsp->js_state);
	return -1;
    }

    /*
     * If we're in TRAILER state, we need to return the trailer
     * data and then end-of-file.
     */
    if (jsp->js_state == JSS_TRAILER) {

    emit_trailer:
	len = xml_trailer_len - jsp->js_len;
	cp = xml_trailer + jsp->js_len;

	/* Copy what we can and remember if we can't do the whole thing */
	rc = MIN(len, blen);
	memcpy(bp, cp, rc);
	if (rc == len) {
	    if (jsp->js_state != JSS_DEAD)
		jsp->js_state = JSS_CLOSE;
	    jsp->js_len = 0;
	} else {
	    jsp->js_len += rc;
	}

	bp += rc;

	/* If we know there are leading blanks, remove them */ 
	if (leading_blanks) {
	    jsio_trace("trimming leading blanks: %p..%p %d %d",
		       buf, bp, bp - buf, leading_blanks);
	    len = bp - buf;
	    memmove(buf, buf + leading_blanks, len - leading_blanks);
	    bp -= leading_blanks;
	}

	rc = bp - buf;
	jsio_buffer_trace("emit trailer", buf, rc);
	return rc;
    }

    /*
     * If we're in INIT or HEADER state, we're dealing with the
     * credentials (the xml declaration and open tag for junoscript).
     * We stuff what we can into the callers buffer, returning if
     * that all that will fit.
     */
    if (jsp->js_state <= JSS_HEADER) {
	cp = jsp->js_creds;
	len = strlen(cp);
	if (jsp->js_state == JSS_HEADER) {
	    /* js_len is the amount we've already returned */
	    len -= jsp->js_len;
	    cp += jsp->js_len;
	}

	rc = MIN(blen, len);
	memcpy(bp, cp, rc);
	if (rc < len) {
	    /* Not enough room?  Return what we can */
	    jsp->js_len += len;
	    jsp->js_state = JSS_HEADER;
	    jsio_buffer_trace("short header", buf, rc);
	    return rc;
	}

	blen -= len;
	bp += len;
	jsp->js_state = JSS_NORMAL;
	jsp->js_len = 0;
    }

    rc = js_buffer_read_data(jsp, bp, blen);
    if (rc < 0)
	xmlGenericError(NULL, "rpc read: %s", strerror(errno));
    if (rc <= 0) {
    dead:
	jsp->js_state = JSS_DEAD;
	jsp->js_len = 0;
	goto emit_trailer;
    }

    /*
     * Reply from cisco netconf session will have xml declaration. 
     * Since js_creds has the xml declaration and it is already passed to the 
     * input stream, discard the xml declaration received from server.
     */
    if (rc >= 2 && bp[0] == '<' && bp[1] == '?')
	jsp->js_state = JSS_DISCARD;

    if (jsp->js_state == JSS_DISCARD) {
	leading_blanks = 0;

	/*
	 * Replace the xml declaration with spaces
	 */
	while (bp[leading_blanks] != '>' && leading_blanks < rc)
	    bp[leading_blanks++] = ' ';

	if (bp[leading_blanks] == '>') {
	    bp[leading_blanks++] = ' ';
	    jsp->js_state = JSS_NORMAL;
	    if (bp[leading_blanks] == '\n')
		bp[leading_blanks++] = ' ';
	}
    }

    rc = js_buffer_find_reset(jsp, bp, rc);
    if (rc < 0)
	goto dead;

    if (jsp->js_state == JSS_TRAILER) {
	bp += rc;
	blen -= rc;
	goto emit_trailer;
    }

    rc += bp - buf;
    jsio_buffer_trace("normal", buf, rc);
    if (rc == 0)
	goto emit_trailer;

    return rc;
}

/*
 * Close the buffer and reinitialize the buffer state.
 */
static int
js_buffer_close (void *context)
{
    js_session_t *jsp = context;

    if (jsp->js_state == JSS_DEAD)
	return 0;

    if (jsp->js_state != JSS_CLOSE) {
	jsio_trace("session close called but not in close state");
    }

    jsp->js_state = JSS_INIT;
    jsp->js_len = 0;

    return 0;
}

/*
 * Create buffer for progressive parsing
 */
static xmlParserInputBufferPtr
js_buffer_create (js_session_t *jsp, xmlCharEncoding enc)
{
    xmlParserInputBufferPtr ret;

    if (jsp == NULL)
	return NULL;

    ret = xmlAllocParserInputBuffer(enc);
    if (ret != NULL) {
        ret->context = (void *) jsp;
	if (jsp->js_key.jss_type == ST_MIXER) {
	    jsp->js_mx_buffer = js_mx_buffer_create();
	    ret->readcallback = js_mixer_read;
	} else {
	    ret->readcallback = js_buffer_read;
	}
	ret->closecallback = js_buffer_close;
    }

    return ret;
}

/*
 * Read xmlDocument from server
 */
static xmlDoc *
js_document_read (xmlParserCtxtPtr ctxt, js_session_t *jsp,
			const char *url, const char *encoding, int options)
{
    xmlParserInputBufferPtr input;
    xmlParserInputPtr stream;
    xmlDoc *docp;

    if (jsp == NULL || ctxt == NULL || jsp->js_state == JSS_DEAD)
        return NULL;

    xmlCtxtReset(ctxt);

    input = js_buffer_create(jsp, XML_CHAR_ENCODING_NONE);
    if (input == NULL)
        return NULL;

    stream = xmlNewIOInputStream(ctxt, input, XML_CHAR_ENCODING_NONE);
    if (stream == NULL) {
        xmlFreeParserInputBuffer(input);
        return NULL;
    }

    inputPush(ctxt, stream);
    xmlCtxtUseOptions(ctxt, options);

    xmlCharEncodingHandlerPtr hdlr;
    if (encoding && ((hdlr = xmlFindCharEncodingHandler(encoding)) != NULL))
	xmlSwitchToEncoding(ctxt, hdlr);

    if (url != NULL && ctxt->input != NULL && ctxt->input->filename == NULL)
        ctxt->input->filename = (char *) xmlStrdup((const xmlChar *) url);

    /*
     * All right.  The stage is set, time to open the curtain and let
     * the show begin.
     */
    xmlParseDocument(ctxt);

    docp = ctxt->myDoc;
    ctxt->myDoc = NULL;

    if (docp && !ctxt->wellFormed) {
	xmlFreeDoc(docp);
        docp = NULL;
    }

    if (jsp->js_mx_buffer) {
	free(jsp->js_mx_buffer);
	jsp->js_mx_buffer = NULL;
    }

    return docp;
}

static int
js_ssh_askpass_accept (js_session_t *jsp)
{
    int sock = accept(jsio_askpass_socket, NULL, 0);

    jsio_trace("jsio_askpass: accept %d", sock);
    jsp->js_askpassfd = sock;

    return sock;
}

/*
 * Read the prompt from ssh and send it to cli using jcs:input() infra, read 
 * the response back from cli and send it to ssh
 */
static void
js_ssh_askpass (js_session_t *jsp, int fd)
{
    static char newline_str[] = "\n";
    unsigned int len = 0;
    js_boolean_t secret = TRUE;
    char buf[BUFSIZ], *ep, *bp, *msg;
    struct iovec iov[2];

    jsio_trace("jsio_askpass: reading %d", fd);

    len = read(fd, buf, sizeof(buf));
    if (len <= 0) {
	jsio_trace("jsio_askpass: bad read: %m");
	return;
    }

    jsio_buffer_trace("jsio_askpass", buf, len);

    bp = strchr(buf, ' ');
    if (bp == NULL)
	return;

    *bp++ = '\0';
    secret = streq(buf, "secret");

    ep = buf + len - 1;
    if (*ep == '\n')
	ep -= 1;
    *ep-- = '\0';

    if (secret && jsp->js_passphrase) {
	msg = jsp->js_passphrase; /* Use the recorded passphrase */
    } else {
	msg = slaxInput(bp, secret ? SIF_SECRET : 0);
    }

    iov[0].iov_base = msg ?: buf; /* Use buf as null message (zero length) */
    iov[0].iov_len = msg ? strlen(msg) : 0;
    iov[1].iov_base = newline_str;
    iov[1].iov_len = 1;

    if (writev(fd, iov, 2) < 0)
	jsio_trace("error writing to askpass: %m");

    close(fd);
    jsp->js_askpassfd = 0;

    if (msg && msg != jsp->js_passphrase)
	free(msg);
}

/*
 * Called initially after the process is forked.
 * This handles the initial error from process being emitted to stderr and
 * also handles the password prompt from ssh.
 */
static int
js_initial_read (js_session_t *jsp, time_t secs, long usecs)
{
    struct timeval tmo = { secs, usecs };
    fd_set rfds, xfds;
    int sin = jsp->js_stdin, serr = jsp->js_stderr, smax, rc;
    int askpassfd = jsp->js_askpassfd ?: jsio_askpass_socket;

    do {
	smax = MAX(sin, serr);

	if (askpassfd >= 0)
	    smax = MAX(smax, askpassfd);

	FD_ZERO(&rfds);
	FD_SET(sin, &rfds);
	if (serr >= 0)
	    FD_SET(serr, &rfds);

	if (askpassfd >= 0)
	    FD_SET(askpassfd, &rfds);

	FD_COPY(&rfds, &xfds);

	rc = select(smax + 1, &rfds, NULL, &xfds, &tmo);
	if (rc < 0) {
	    if (errno == EINTR)
		continue;
	    jsio_trace("error from rpc session: %m");
	    return -1;
	}

	if (rc == 0) {
	    if (secs)
		jsio_trace("timeout from rpc session");
	    return -1;
	}

	if (serr >= 0 && (FD_ISSET(serr, &rfds) || FD_ISSET(serr, &xfds))) {
	    char buf[BUFSIZ];

	    rc = read(serr, buf, sizeof(buf) - 1);
	    if (rc > 0) {
		buf[sizeof(buf) - 1] = '\0';
		jsio_trace("error from rpc session: %s", buf);
	    }
	}

	if (askpassfd >= 0 && FD_ISSET(askpassfd, &rfds)) {
	    if (askpassfd == jsio_askpass_socket) {
		askpassfd = js_ssh_askpass_accept(jsp);
	    } else {
		js_ssh_askpass(jsp, askpassfd);
		askpassfd = jsio_askpass_socket;
	    }
	}

    } while (!FD_ISSET(sin, &rfds));

    return 0;
}
	    
/*
 * Read a string with timeout
 */
static char *
js_gets_timed (js_session_t *jsp, time_t secs, long usecs)
{
    char *str;

    if (!fbuf_has_buffered(jsp->js_fbuf)) {
	if (js_initial_read(jsp, secs, usecs))
	    return NULL;
    }

    str = fbuf_gets(jsp->js_fbuf);
    if (str)
	jsio_buffer_trace("gets", str, strlen(str));

    return str;
}

/*
 * Dup an existing fd into the slot of a target fd
 */
static void
js_dup (int target, int existing)
{
    if (target != existing) {
	close(target);
	dup2(existing, target);
    }
}

/*
 * Creates and fills in js session object with appropriate data
 */
static js_session_t *
js_session_create_internal (const char *session_name, int pid, int in,
			    int out, int err, int stype, int flags)
{
    js_session_t *jsp;

    FILE *fp = fdopen(out, "w");
    if (fp == NULL) {
	jsio_trace("jsio: fdopen failed: %m");
	goto fail2;
    }

    /* A NULL session name means the localhost session */
    const char *name = session_name ?: "";
    size_t namelen = strlen(name) + 1;
    jsp = malloc(sizeof(*jsp) + namelen);
    if (jsp == NULL)
	goto fail3;

    bzero(jsp, sizeof(*jsp));
    jsp->js_pid = pid;
    jsp->js_stdin = in;
    jsp->js_stdout = out;
    jsp->js_stderr = err;
    jsp->js_fpout = fp;
    jsp->js_msgid = 0;
    jsp->js_hello = NULL;
    jsp->js_isjunos = FALSE;
    jsp->js_target = strdup(name);

    jsp->js_key.jss_type = stype;
    memcpy(jsp->js_key.jss_name, name, namelen);
    patricia_node_init_length(&jsp->js_node, sizeof(js_skey_t) + namelen);

    jsp->js_fbuf = fbuf_fdopen(out, 0);

    if (flags & JSF_FBUF_TRACE)
	fbuf_trace_tagged(jsp->js_fbuf, stdout, "jsp-read");

    return jsp;

fail3:
    fclose(fp);
fail2:
    if (in)
	close(in);
    if (out)
	close(out);
    if (err)
	close(err);
    return NULL;
}

/*
 * Create a session object and connect it as appropriate
 */
static js_session_t *
js_session_create (const char *session_name, char **argv,
		   int flags, session_type_t stype)
{
    int sv[2], ev[2];
    int pid = 0, i;
    sigset_t sigblocked, sigblocked_old;
    js_session_t *jsp;
    char buf[128];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
        return NULL;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, ev) < 0) {
	close(sv[0]);
	close(sv[1]);
        return NULL;
    }

    sigemptyset(&sigblocked);
    sigaddset(&sigblocked, SIGCHLD);
    sigprocmask(SIG_BLOCK, &sigblocked, &sigblocked_old);

    if ((pid = fork()) == 0) {	/* Child process */

        sigprocmask(SIG_SETMASK, &sigblocked_old, NULL);

        close(sv[1]);
        close(ev[1]);
	js_dup(0, sv[0]);
	js_dup(1, sv[0]);
	js_dup(2, ev[0]);

        for (i = 3; i < 64; ++i) {
	    close(i);
	}

#if 0
	/*
	 * We have to set the LD_LIBRARY_PATH environment variable in
	 * order for the loader to find our libraries.  Normally it
	 * points to /usr/lib, but if we've set a prefix, then it's
	 * probably pointing to the object side of our build sandbox.
	 */
	setenv("LD_LIBRARY_PATH", path_juniper_usr_lib(), TRUE);
#endif

	/*
	 * We need to disassociate ourselves with the controlling TTY
	 * to prevent ssh from prompting for data on that TTY.  This
	 * involves a double fork() with a setsid() call.
	 */
	if (fork() != 0)
	    exit(0);
	setsid();
	if (fork() != 0)
	    exit(0);

	/*
	 * We've double forked, but our parent needs our pid to
	 * be able to kill us when the time's right (e.g. SIGINT).
	 * So our first line of output is our pid.
	 */
	snprintf(buf, sizeof(buf), "%d\n", getpid());
	if (write(1, buf, strlen(buf)) < 0)
	    _exit(1);

        execv(argv[0], argv);
        _exit(1);

    } else if (pid < 0) {
	jsio_trace("could not run script xml-mode: %m");
	close(sv[0]);
	close(ev[0]);

	goto fail2;
    }

    close(sv[0]);
    close(ev[0]);

    /* See comment above re: returning child pid */
    for (i = 0; i < (int) sizeof(buf); i++) {
	if (read(sv[1], &buf[i], 1) < 0)
	    goto fail2;
	if (buf[i] == '\n')
	    break;
    }
    if (i == sizeof(buf))
	goto fail2;

    /* buf now has the pid, so we just turn it back into an integer */
    buf[i] = '\0';
    pid = atoi(buf);
    if (pid <= 1)
	goto fail2;

    if (stype == ST_DEFAULT)
	stype = js_default_stype;

    jsp = js_session_create_internal(session_name, pid, sv[1], sv[1], ev[1], 
				     stype, flags);

    return jsp;

 fail2:
    close(ev[1]);
    close(sv[1]);
    return NULL;
}

/*
 * Initialize SHELL session.
 */
int
js_shell_session_init (js_session_t *jsp)
{
    /*
     * Set the "credentials" to NULL, the SHELL session type has no use for this information
     */
    jsp->js_creds = NULL;

    return FALSE;
}

/*
 * Initialize JUNOScript session.
 * Pass the credentials to sever and read the credentials back from server and
 * store for later use.
 */
int
js_session_init (js_session_t *jsp)
{
    static const char *cred1 = "<?xml ";
    static const char *cred2 = "<" XMLRPC_APINAME " ";
    fbuf_t *fbp = jsp->js_fbuf;

    /*
     * Start by sending our side of the credentials
     */
    fprintf(jsp->js_fpout, "<?xml version=\"1.0\"?>\n<"
	    XMLRPC_APINAME " version=\"" XMLRPC_VERSION "\">\n");
    fflush(jsp->js_fpout);

    /*
     * Read their credentials and store them away for later use
     */
    char *line1 = js_gets_timed(jsp, JS_READ_TIMEOUT, 0);
    line1 = ALLOCADUPX(line1);

    char *line2 = js_gets_timed(jsp, JS_READ_TIMEOUT, 0);
    line2 = ALLOCADUPX(line2);

    if (line1 == NULL || line2 == NULL || fbuf_eof(fbp)
		|| strncmp(line1, cred1, strlen(cred1)) != 0
		|| strncmp(line2, cred2, strlen(cred2)) != 0) {
	jsio_trace("error opening connection (reading credentials)");
	return TRUE;
    }

    int len1 = strlen(line1);
    int len2 = strlen(line2);
    int len = len1 + len2;
    char *creds = malloc(len + 3);

    if (creds == NULL) {
	jsio_trace("malloc failed");
	return TRUE;
    }

    memcpy(creds, line1, len1);
    memcpy(creds + len1 + 1, line2, len2 + 1);
    creds[len1] = '\n';
    creds[len1 + len2 + 1] = '\n';
    creds[len1 + len2 + 2] = '\0';

    /*
     * Save the credentials into the session so that we can use them
     * as the header on each RPC.
     *
     * The idea is that we need to present to libxml2 an XML document,
     * but JUNOScript gives us only partial documents (<rpc-reply>
     * elements) inside a session document.  So we fake the
     * <rpc-reply> into a full document by inserting the header and a
     * trailer ("</junoscript>") into the input stream.
     */
    jsp->js_creds = creds;

    /*
     * Drain any remaining input.  Our server emits some
     * XML comments, so we can read and discard those.
     * Anything else is probably simple broken-ness.
     */
    for (;;) {
	char *cp = js_gets_timed(jsp, 0, JS_READ_QUICK);
	if (cp == NULL)
	    break;

	if (cp[0] == '<' && cp[1] == '!' && cp[2] == '-' && cp[3] == '-') {
	    /* ignore comments */
	} else {
	    jsio_trace("ignoring noise: %s", cp);
	}
    }

    return FALSE;
}

/*
 * Add the session details to patricia tree
 */
static js_boolean_t
js_session_add (js_session_t *jsp)
{
    if (!patricia_add(&js_session_root, &jsp->js_node)) {
	jsio_trace("could not add session node to root tree");
	return FALSE;
    }

    return TRUE;
}

/*
 * Delete the session details from patricia tree
 */
static js_boolean_t
js_session_delete (js_session_t *jsp)
{
    if (!patricia_delete(&js_session_root, &jsp->js_node)) {
	jsio_trace("could not delete session node from root tree");
	return FALSE;
    }

    return TRUE;
}

/*
 * Find the session for the given session_name.
 *
 * Returns NULL if the session is not found.
 */
static js_session_t *
js_session_find (const char *session_name, session_type_t stype)
{
    js_session_t *jsp;
    js_skey_t *key;
    int keylen;

    /* A NULL session name means the localhost session */
    const char *name = session_name ?: "";
    u_int16_t len = strlen(name) + 1;

    keylen = len + sizeof(js_skey_t);
    key = alloca(keylen);
    if (!key) {
	jsio_trace("could not allocate memory");
	return NULL;
    }

    if (stype == ST_DEFAULT)
	stype = js_default_stype;

    key->jss_type = stype;
    memcpy(&key->jss_name, name, len);

    patnode_t *pnp = patricia_get(&js_session_root, keylen, key);
    if (pnp) {
	jsp = (js_session_t *) pnp;
	return jsp;
    } else {
	return NULL;
    }
}

/*
 * Kill the child process associated with this session.
 */
static void
js_session_kill (js_session_t *jsp)
{
    pid_t pid = jsp->js_pid;
    int count = 0, rc, sig, status;
    js_boolean_t exited = FALSE;

    if (pid > 0) {

	sig = SIGTERM; /* First try graceful kill */

	do {

	    /* If it aign't working, bail; the process is likely dead */
	    if (kill(pid, sig) < 0)
		break;

	    /* Give some time for process to exit. */
	    usleep(1000);

	    do {
		rc = waitpid(pid, &status, WNOHANG);
	    } while (rc < 0 && errno == EINTR);

	    /* 
	     * Check whether the process is still around.
	     *
	     * If the process is around try sending SIGKILL
	     */
	    if (kill(pid, 0)) {
		exited = TRUE; 
		break;
	    }

	    count++;
	    sig = SIGKILL;

	} while (count < 2);

	if (!exited) {
	    /* not able to kill the child process, trace it */
	    jsio_trace("not able to kill child process %d", pid);
	} else {
	    /* trace whether the process exited succesfully or not */
	    jsio_trace("CHLD process %d terminated %s", pid,
		  (WIFEXITED(status) && !WEXITSTATUS(status)) ? 
		  "successfully" : "with failure");
	}
    }
}

static void
js_session_free (js_session_t *jsp)
{
    if (jsp == NULL)
	return;

    if (jsp->js_askpassfd > 0)
	close(jsp->js_askpassfd);

    fbuf_close(jsp->js_fbuf);

    if (jsp->js_passphrase)
	free(jsp->js_passphrase);

    close(jsp->js_stdin);
    close(jsp->js_stdout);
    close(jsp->js_stderr);
    fclose(jsp->js_fpout);

    if (jsp->js_hello)
	xmlFreeNode(jsp->js_hello);

    free(jsp->js_target);
    free(jsp->js_creds);
    free(jsp);
}

/*
 * Release a session that has not been added to the name tree, principally
 * because the pat_add call failed (so we can't call js_session_terminate).
 */
static void
js_session_release (js_session_t *jsp)
{
    js_session_kill(jsp);
    js_session_free(jsp);
}

/*
 * Terminate a session with extreme prejudice.  Release any and resources.
 */
void
js_session_terminate (js_session_t *jsp)
{
    js_session_delete(jsp);	/* Remove from patricia tree */
    js_session_kill(jsp);
    js_session_free(jsp);
}

/*
 * Close a specific session
 */
void
js_session_close1 (js_session_t *jsp)
{
    js_session_terminate(jsp);
}

/*
 * js_initialize: one-time/start-up procedures. Initialize the patricia root.
 */
static void
js_initialize (void)
{
    static int done_init;

    if (done_init)
	return;

    patricia_root_init(&js_session_root, FALSE,
		       PAT_MAXKEY, SESSION_NAME_DELTA);

    done_init = TRUE;
}

/*
 * Send the simple string RPC name to server
 */
static int
js_rpc_send_simple (js_session_t *jsp, const char *rpc_name)
{
    FILE *fp = jsp->js_fpout;
    int is_mixer = (jsp->js_key.jss_type == ST_MIXER);
    char buf[1024], rpc[128];

    jsio_trace("rpc name: %s", rpc_name);

    switch (jsp->js_key.jss_type) {
    case ST_JUNOSCRIPT:
	fprintf(fp, "<xnm:rpc xmlns=\"\"><%s/></xnm:rpc>\n" 
		/**/ XML_PARSER_RESET /**/ "\n", rpc_name);
	break;
	    
    case ST_NETCONF:
    case ST_JUNOS_NETCONF:
	/*
	 * Pure Hack:
	 *
	 * According to Netconf standard rpc request should have xmlns. But
	 * Junos has a bug. When the rpc is called with xmlns, the return
	 * rpc-reply returned by Junos has two xmlns and parsing fails. 
	 * This is fixed now.
	 *
	 * But for us to work with the older Junos version which has this
	 * bug, we should not be emitting the xmlns to Junos devices.
	 */
	fprintf(fp, "%s\n", xmldec);
	fprintf(fp, "<rpc %s message-id=\"%d\"><%s/></rpc>\n" 
		/**/ XML_PARSER_RESET /**/ "\n",
		jsp->js_isjunos ? "" : js_netconf_ns_attr,
		++jsp->js_msgid, rpc_name);
	break;

    case ST_SHELL:
        /* send the string */
	fprintf(fp, "%s", rpc_name); 
        break;

    case ST_MIXER:
	snprintf(buf, sizeof(buf), "target=\"%s\" "
		"authmuxid=\"%d\" authwsid=\"%d\" authdivid=\"%s\"",
		jsp->js_target, js_auth_muxer_id, js_auth_websocket_id,
		js_auth_div_id);
	snprintf(rpc, sizeof(rpc), "<%s/>", rpc_name);
	js_mixer_send_simple(jsp, MX_OP_RPC, buf, rpc);
	break;

    case ST_DEFAULT:		/* Avoid compiler errors */
    case ST_MAX:
	break;
    }

    if (!is_mixer) {
	fflush(fp);
    }

    return 0;
}

/*
 * Send multiple line RPC to server
 */
static int
js_rpc_send (js_session_t *jsp, lx_node_t *rpc_node)
{
    FILE *fp = jsp->js_fpout;
    lx_output_t *handle = NULL;
    char buf[BUFSIZ];
    int is_mixer = (jsp->js_key.jss_type == ST_MIXER);

    /*
     * If we're a mixer instance, we need to dump to a string so we can pass
     * it to our mixer message functions.  Otherwise dump the raw XML document
     * directly to the socket
     */
    if (is_mixer) {
	handle = lx_output_open_buffer();
    } else  {
	handle = lx_output_open_fd(fileno(fp));
    }
    if (handle == NULL) {
	jsio_trace("jsio: open buffer/fd failed");
	return -1;
    }

    if (trace_flag_is_set(trace_file, CS_TRC_RPC))
	lx_trace_node(rpc_node, "rpc node");

    int is_rpc = streq(XMLRPC_REQUEST, (const char *) rpc_node->name);
    if (!is_rpc) {
	switch (jsp->js_key.jss_type) {
	case ST_JUNOSCRIPT:
	    fprintf(fp, "<xnm:rpc xmlns=\"\">\n");
	    break;
	       	
	    case ST_NETCONF:
	    case ST_JUNOS_NETCONF:
		fprintf(fp, "%s\n", xmldec);
		fprintf(fp, "<rpc %s message-id=\"%d\">\n", 
			jsp->js_isjunos ? "" : js_netconf_ns_attr,
			++jsp->js_msgid); 
		break;

	case ST_MIXER:
	case ST_SHELL:		        /* Avoid compiler errors */
	case ST_DEFAULT:		/* Avoid compiler errors */
	case ST_MAX:
	    break;
	}
    }

    /*
     * We must flush before calling lx_output_node, since it's not
     * using our FILE pointer
     */
    if (!is_mixer) {
	fflush(fp);
    }

    /* 
     * Write the rpc data to the junoscript server (or dump to buffer if
     * mixer)
     */
    lx_output_node(handle, rpc_node);

    if (!is_rpc) {
	switch (jsp->js_key.jss_type) {
	case ST_JUNOSCRIPT:
	    fprintf(fp, "</xnm:rpc>\n");
	    break;
	    
	case ST_NETCONF:
	case ST_JUNOS_NETCONF:
	    fprintf(fp, "</rpc>\n");
	    break;
	
	case ST_MIXER:
	    snprintf(buf, sizeof(buf), "target=\"%s\" "
		    "authmuxid=\"%d\" authwsid=\"%d\" authdivid=\"%s\"",
		    jsp->js_target, js_auth_muxer_id, js_auth_websocket_id,
		    js_auth_div_id);
	    js_mixer_send_simple(jsp, MX_OP_RPC, buf,
		    lx_output_buffer(handle));
	    break;
	case ST_SHELL:		        /* Avoid compiler errors */
	case ST_DEFAULT:		/* Avoid compiler errors */
	case ST_MAX:
	    break;
	}
    }

    if (!is_mixer) {
	fputs(xml_parser_reset, fp);
	fflush(fp);
    }

    lx_output_close(handle);
    lx_output_cleanup(handle);

    return 0;
}

/*
 * Read RPC reply
 */
static lx_document_t *
js_rpc_get_document (js_session_t *jsp)
{
    lx_document_t *docp = NULL;
    xmlParserCtxt *read_ctxt = xmlNewParserCtxt();

    if (read_ctxt == NULL) {
	jsio_trace("jsio: could not make parser context");
    } else {
	docp = js_document_read(read_ctxt, jsp, "xnm:rpc results", NULL, 0);
	if (docp == NULL) {
	    jsio_trace("jsio: could not read content (null document)");
	}

	js_buffer_close(jsp);
	xmlFreeParserCtxt(read_ctxt);
    }

    return docp;
}

static lx_nodeset_t *
js_rpc_get_reply (xmlXPathParserContext *ctxt, js_session_t *jsp)
{
    lx_document_t *docp = js_rpc_get_document(jsp);

    if (docp == NULL)
	goto fail;

    lx_node_t *nop = lx_document_root(docp);
    if (nop == NULL) {
	jsio_trace("jsio: could not find document root");
	goto fail;
    }

    if (trace_flag_is_set(trace_file, CS_TRC_RPC))
	lx_trace_node(nop, "results of rpc");

    /*
     * If this is a mixer connection, our top level tag will be <rpc-reply>,
     * if it is a non-mixer connection, it will be <junoscript>
     */
    if (jsp->js_key.jss_type == ST_MIXER) {
	if (!streq(xmlNodeName(nop), XMLRPC_REPLY)) {
	    jsio_trace("jsio: could not find rpc-reply tag");
	    goto fail;
	}
    } else {
	if (!streq(xmlNodeName(nop), XMLRPC_APINAME)) {
	    jsio_trace("jsio: could not find api tag");
	    goto fail;
	}
	nop = lx_node_children(nop);
    }

    /*
     * Create a Result Value Tree container, and register it with RVT garbage 
     * collector. 
     */
    xsltTransformContextPtr tctxt = xsltXPathGetTransformContext(ctxt);
    xmlDocPtr container = xsltCreateRVT(tctxt);
    xsltRegisterLocalRVT(tctxt, container);

    for (; nop; nop = lx_node_next(nop)) {
	if (streq(xmlNodeName(nop), XMLRPC_REPLY)) {
	    lx_node_t *cop, *newp;
	    lx_nodeset_t *setp = xmlXPathNodeSetCreate(NULL);

	    if (setp == NULL) {
		jsio_trace("jsio: could not allocate set");
		goto fail;
	    }

	    ext_jcs_fix_namespaces(nop);

	    for (cop = lx_node_children(nop); cop; cop = lx_node_next(cop)) {
		if (cop->type == XML_TEXT_NODE
		    && streq((const char *) cop->content, "\n"))
		    continue;
	
		newp = xmlCopyNode(cop, 1);
		xmlXPathNodeSetAdd(setp, newp);

		/*
		 * Attach the return nodes with RVT container, so that the 
		 * nodes will be freed when the container is freed.
		 */
		xmlAddChild((lx_node_t *) container, newp);
	    }

	    lx_document_free(docp);
	    return setp;
	}
    }

 fail:
    jsio_trace("invalid reply to rpc");

    if (docp)
	lx_document_free(docp);

    return NULL;

}

lx_document_t *
js_rpc_get_request (js_session_t *jsp)
{
    return js_rpc_get_document(jsp);
}

const char *
js_rpc_get_name (lx_document_t *rpc)
{
    lx_node_t *root = lx_document_root(rpc);
    if (root && root->children) {
	lx_node_t *cop = root->children;

	for ( ; cop; cop = cop->next) {
	    if (cop->type != XML_ELEMENT_NODE)
		continue;
	    if (!streq(XMLRPC_REQUEST, (const char *) cop->name))
		return NULL;

	    for (cop = cop->children; cop; cop = cop->next) {
		if (cop->type != XML_ELEMENT_NODE)
		    continue;
		return (const char *) cop->name;
	    }

	    return NULL;
	}
    }

    return NULL;
}

/*
 * Set the default server to the one provided
 */
void
jsio_set_default_server (const char *server)
{
    if (js_default_server)
	free(js_default_server);

    js_default_server = server ? strdup(server) : NULL;
}

/*
 * Get the name, either from the argument or from the global
 */
static const char *
js_session_get_name (const char *name)
{
    if (name != NULL && *name != '\0')
	return name;

    name = js_default_server;
    if (name != NULL && *name != '\0')
	return name;

    return NULL;
}

static const char *
js_session_get_user (const char *name)
{
    if (name != NULL && *name != '\0')
	return name;

    name = js_default_user;
    if (name != NULL && *name != '\0')
	return name;

    return NULL;
}

void
jsio_set_default_user (const char *user)
{
    if (js_default_user)
	free(js_default_user);

    js_default_user = user ? strdup(user) : NULL;
}

void
jsio_set_mixer (const char *mixer)
{
    if (js_mixer)
	free(js_mixer);

    if (mixer) {
	jsio_set_default_session_type(ST_MIXER);
    }

    js_mixer = mixer ? strdup(mixer) : NULL;
}

#define JSIO_SSH_OPTIONS_MAX 16
static int jsio_ssh_options_count;
static char *jsio_ssh_options[JSIO_SSH_OPTIONS_MAX];

void
jsio_add_ssh_options (const char *opts)
{
    if (opts && jsio_ssh_options_count < JSIO_SSH_OPTIONS_MAX)
	jsio_ssh_options[jsio_ssh_options_count++] = strdup(opts);
}

/*
 * Opens a JUNOScript session on the localhost using jade for authentication
 */
js_session_t *
js_session_open_localhost (js_session_opts_t *jsop, int flags, 
                           const char *auth_socket)
{
    js_session_t *jsp;
    int conn_addr_len, conn_sock;
    char auth_rpc[BUFSIZ];
    char *auth_resp;
    struct sockaddr_un conn_addr;

    if (auth_socket == NULL) {
	jsio_trace("Missing mandatory auth socket");
        return NULL;
    }

    js_initialize();

    if (jsop->jso_stype == ST_DEFAULT)
        jsop->jso_stype = js_default_stype;

    const char *name = js_session_get_name(jsop->jso_server);
    if (name == NULL)
	return NULL;

    const char *user = js_session_get_user(jsop->jso_username);
    if (user == NULL)
	return NULL;

    /*
     * Check whether the junoscript session already exists for the given 
     * hostname, if so then return that.
     */
    jsp = js_session_find(name, jsop->jso_stype);
    if (jsp)
        return jsp;

    conn_sock = socket(PF_LOCAL, SOCK_STREAM, 0);

    if (conn_sock < 0) {
        jsio_trace("Failed to create socket");
        return NULL;
    }

    /*
     * Set up connection for authentication
     */
#ifdef HAVE_SUN_LEN
    conn_addr.sun_len = sizeof(struct sockaddr_un);
#endif /* HAVE_SUN_LEN */
    conn_addr.sun_family = AF_UNIX;
    strcpy(conn_addr.sun_path, auth_socket);

#ifdef HAVE_SUN_LEN
    conn_addr_len = sizeof(conn_addr.sun_len) + sizeof( conn_addr.sun_family)
#else
    conn_addr_len = sizeof( conn_addr.sun_family)
#endif /* HAVE_SUN_LEN */
        + strlen(conn_addr.sun_path);

    /*
     * Connect on the socket to exec jade that handles authentication and
     * provides us with a Junoscript session
     */
    if (connect(conn_sock, (struct sockaddr *)&conn_addr, conn_addr_len) < 0) {
        jsio_trace("Failed to connect for authentication");
        return NULL;
    }

    jsp = js_session_create_internal(name, -1, conn_sock, 
				     conn_sock, -1, jsop->jso_stype, flags);
    if (jsp == NULL)
	return NULL;

    /*
     * We need to be able to close this infant session if we
     * have a SIGINT, so we add it to the patricia tree even
     * though we may need to remove it if it fails here.
     */
    if (!js_session_add(jsp)) {
	js_session_release(jsp);
	return NULL;
    }

    if (js_session_init(jsp)) {
	js_session_terminate(jsp);
	return NULL;
    }

    jsio_trace("Session initialized");
    snprintf_safe(auth_rpc, sizeof(auth_rpc),
		  "<rpc><request-login><username>%s</username>"
		  "<challenge-response>%s</challenge-respone>"
		  "</request-login></rpc>\n", user,
		  jsop->jso_passphrase);

    /*
     * Write authetication RPC into the socket
     */
    if (write(conn_sock, auth_rpc, strlen(auth_rpc)) < 0) {
	jsio_trace("Failed to send authentication rpc");
	bzero(auth_rpc, strlen(auth_rpc));
	bzero(jsop->jso_passphrase, strlen(jsop->jso_passphrase));
	return NULL;
    }

    bzero(auth_rpc, strlen(auth_rpc));
    bzero(jsop->jso_passphrase, strlen(jsop->jso_passphrase));

    if (!fbuf_has_buffered(jsp->js_fbuf)) {
	if (js_initial_read(jsp, JS_READ_TIMEOUT, 0)) {
	    return NULL;
	}
    }

    /*
     * Read the response and look for status in authentication reply
     */
    for (;;) {
	auth_resp = js_gets_timed(jsp, 0, JS_READ_QUICK);
	if (auth_resp == NULL)
	    break;

	if (strstr(auth_resp, "<status>fail</status>")) {
	    jsio_trace("Authentication failed");
	    break;
	}

	if (strstr(auth_resp, "<status>success</status>")) {
	    jsio_trace("Authentication successful");
	    return jsp;
	}
    }

    js_session_terminate(jsp);
    return NULL;
}

/*
 * Opens a JUNOScript session for the given options (hostname, username,
 * passphrase, etc).
 */
js_session_t *
js_session_open (js_session_opts_t *jsop, int flags)
{
    js_session_t *jsp;
    int max_argc = JSIO_SSH_OPTIONS_MAX * 2, argc = 0;
    char *argv[max_argc];
    char *port_str = NULL;
    char *timeout_str = NULL;
    char *conn_timeout_str = NULL;
    int i;
    js_session_opts_t jso;

    if (jsop == NULL) {		/* "Make life easier" */
	bzero(&jso, sizeof(jso));
	jsop = &jso;
    }

    js_initialize();

    if (jsop->jso_stype == ST_DEFAULT)
	jsop->jso_stype = js_default_stype;

    if (flags & JSF_JUNOS_NETCONF)
	jsop->jso_stype = ST_JUNOS_NETCONF;

    if (js_mixer) {
	jsop->jso_stype = ST_MIXER;
    }

    const char *name = js_session_get_name(jsop->jso_server);
    if (name == NULL)
	return NULL;

    const char *user = js_session_get_user(jsop->jso_username);

    /*
     * Check whether the junoscript session already exists for the given 
     * hostname, if so then return that.
     */
    jsp = js_session_find(name, jsop->jso_stype);
    if (jsp)
	return jsp;

    /*
     * If we are using a mixer connection, we need to fork a mixer binary to
     * handle this, rather than an SSH binary.  Mixer speaks its own framing
     * language which we have to intercept and handle before we pass the data
     * back to the normal routines.
     */
    if (js_mixer) {
	/*
	 * js_mixer likely is in the format   
	 * "<mixer binary> --user <username>" or something.  We need to parse
	 * this out to a valid argv array
	 */
	static char whitespace[] = " \t\n\r";
	const char *cp = js_mixer, *ep = js_mixer + strlen(js_mixer);
	char buf[BUFSIZ], *bp = buf;
	int bufsiz = sizeof(buf);
	char *ap;

	for (; cp < ep;) {
	    cp += strspn(cp, whitespace);
	    if (*cp == '\0') {
		break;
	    }

	    for (ap = bp; cp < ep; cp++) {
		if (ap - bp > bufsiz) {
		    char *np = alloca(bufsiz * 2);
		    memcpy(np, bp, bufsiz);
		    ap = np + (ap - bp);
		    bp = np;
		    bufsiz = bufsiz * 2;
		}

		if (*cp == '\\') {
		    *ap++ = *++cp;
		    continue;
		}

		if (index(whitespace, *cp) != NULL) {
		    break;
		}

		*ap++ = *cp;
	    }

	    *ap = '\0';
	    argv[argc++] = strdup(bp);
	}
    } else {
	argv[argc++] = ALLOCADUP(PATH_SSH);
	argv[argc++] = ALLOCADUP("-aqTx");
	argv[argc++] = ALLOCADUP("-oTCPKeepAlive=yes");

	if (jsop->jso_timeout) {
	    timeout_str = strdupf("-oServerAliveInterval=%u", jsop->jso_timeout);
	    argv[argc++] = timeout_str;
	}

	if (jsop->jso_connect_timeout) {
	    conn_timeout_str = strdupf("-oConnectTimeout=%u", jsop->jso_connect_timeout);
	    argv[argc++] = conn_timeout_str;
	}

	/* Add the global options */
	for (i = 0; i < jsio_ssh_options_count; i++)
	    argv[argc++] = jsio_ssh_options[i];

	/* Add the options from the <ssh-option> element */
	for (i = 0; i < jsop->jso_argc; i++)
	    argv[argc++] = jsop->jso_argv[i];

	if (jsop->jso_port) {
	    port_str = strdupf("-p%u", jsop->jso_port);
	    argv[argc++] = port_str;
	}

	/*
	 * If username is passed in, use that user name
	 */
	if (user != NULL) {
	    argv[argc++] = ALLOCADUP("-l");
	    argv[argc++] = ALLOCADUP(user);
	} else {
	    /*
	     * When username is not passed, ssh takes it from
	     * getlogin().  Not always login name will be the name of
	     * the user executing the script. So, instead of relying
	     * on that get the username from auth info and pass it to
	     * ssh.
	     */
	    const char *logname = getlogin();

	    if (logname) {
		argv[argc++] = ALLOCADUP("-l");
		argv[argc++] = ALLOCADUP(logname);
	    }
	}

	argv[argc++] = ALLOCADUP(name);

	if (jsop->jso_stype == ST_NETCONF) {
	    argv[argc++] = ALLOCADUP("-s");
	    argv[argc++] = ALLOCADUP("netconf");
	} else if (jsop->jso_stype == ST_JUNOS_NETCONF) {
	    argv[argc++] = ALLOCADUP("xml-mode");
	    argv[argc++] = ALLOCADUP("netconf");
	    argv[argc++] = ALLOCADUP("need-trailer");
	} else if (jsop->jso_stype == ST_SHELL) {
	    /* shell requires no options */
	} else {
	    argv[argc++] = ALLOCADUP("xml-mode");
	    argv[argc++] = ALLOCADUP("need-trailer");
	}
    }

    argv[argc] = NULL;		/* Terminate the argument list */

    INSIST(argc < max_argc);

    jsp = js_session_create(name, argv, flags, jsop->jso_stype);
    if (jsp == NULL)
	return NULL;

    /*
     * We need to be able to close this infant session if we
     * have a SIGINT, so we add it to the patricia tree even
     * though we may need to remove it if it fails here.
     */
    if (!js_session_add(jsp)) {
	js_session_release(jsp);
	return NULL;
    }

    if (jsop->jso_passphrase)
	jsp->js_passphrase = strdup(jsop->jso_passphrase);

    if (jsop->jso_stype == ST_JUNOSCRIPT) {
	if (js_session_init(jsp)) {
	    js_session_terminate(jsp);
	    return NULL;
	}
    } else if (jsop->jso_stype == ST_SHELL) {
	if (js_shell_session_init(jsp)) {
	    js_session_terminate(jsp);
	    return NULL;
	}
    } else if (jsop->jso_stype == ST_MIXER) {
	/*
	 * This is a mixer connection - we don't actually attempt to connect
	 * to the device until we issue an RPC.  Any netconf handshaking is
	 * done by mixer.  Nothing to do at this point - session is open.
	 */
    } else {
	if (js_session_init_netconf(jsp)) {
	    js_session_terminate(jsp);
	    return NULL;
	}
    }

    if (port_str)
	free(port_str);
    if (timeout_str)
	free(timeout_str);
    if (conn_timeout_str)
	free(conn_timeout_str);

    return jsp;
}
/*
 * Send the given string to the given JUNOScript session.
 */
void
js_session_send (const char *session_name, const xmlChar *text)
{
    js_session_t *jsp;
    int rc;

    session_name = js_session_get_name(session_name);
    if (session_name == NULL)
	return;

    jsp = js_session_find(session_name, ST_SHELL);
    if (!jsp) { 
	LX_ERR("Session for server \"%s\" does not exist\n",
	   session_name ?: "local");
	return;
    }

    rc = js_rpc_send_simple(jsp, (const char *) text);
    if (rc) {
	jsio_trace("could not send request");
       	js_session_terminate(jsp);
    }
}

/*
 * Receive a string from the given JUNOScript session.
 */
char *
js_session_receive (const char *session_name, time_t secs)
{
    js_session_t *jsp;

    session_name = js_session_get_name(session_name);
    if (session_name == NULL)
	return NULL;

    jsp = js_session_find(session_name, ST_SHELL);
    if (!jsp) { 
	LX_ERR("Session for server \"%s\" does not exist\n",
	   session_name ?: "local");
	return NULL;
    }

    char *cp = js_gets_timed(jsp, secs, 0);

    return cp;
}

/*
 * Execute the given RPC over the given JUNOScript session.
 */
lx_nodeset_t *
js_session_execute (xmlXPathParserContext *ctxt, const char *session_name,
		    lx_node_t *rpc_node, const xmlChar *rpc_name, 
		    session_type_t stype)
{
    js_session_t *jsp;
    int rc;

    session_name = js_session_get_name(session_name);
    if (session_name == NULL)
	return NULL;

    if (stype == ST_DEFAULT)
	stype = js_default_stype;

    jsp = js_session_find(session_name, stype);
    if (jsp == NULL) {
	LX_ERR("Session for server \"%s\" does not exist\n",
	       session_name ?: "local");
	return NULL;
    }

    if (rpc_node) {
	rc = js_rpc_send(jsp, rpc_node);
    } else {
	rc = js_rpc_send_simple(jsp, (const char *) rpc_name);
    }

    if (rc) {
	jsio_trace("could not send request");
       	js_session_terminate(jsp);
	return NULL;
    }

    lx_nodeset_t *reply = js_rpc_get_reply(ctxt, jsp);
    if (reply == NULL) {
	jsio_trace("could not get reply");
	return NULL;
    }

    return reply;
}
    
void
js_rpc_free (lx_document_t *rpc)
{
    lx_document_free(rpc);
}

/*
 * Close the given host's given session
 */
void
js_session_close (const char *session_name, session_type_t stype)
{
    js_session_t *jsp;

    session_name = js_session_get_name(session_name);

    if (stype == ST_DEFAULT)
	stype = js_default_stype;

    jsp = js_session_find(session_name, stype);
    if (!jsp) { 
	LX_ERR("Session for server \"%s\" does not exist\n",
	       session_name ?: "local");
	return;
    }

    js_session_terminate(jsp);
}

/*
 * Send hello packet to netconf server
 */
static void
js_send_netconf_hello (js_session_t *jsp)
{
    FILE *fp = jsp->js_fpout;

    fprintf(fp, "<?xml version=\"1.0\"?>\n");
    fprintf(fp, 
	    "<hello xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">");
    fprintf(fp, "<capabilities>");
    fprintf(fp, "<capability>");
    fprintf(fp, "urn:ietf:params:netconf:base:1.0");
    fprintf(fp, "</capability>");
    fprintf(fp, "</capabilities>");
    fprintf(fp, "</hello>");
    fprintf(fp, XML_PARSER_RESET "\n");
    fflush(fp);
}

/*
 * Read hello packet from server and return the nodeset of hello packet
 */
static lx_node_t *
js_read_netconf_hello (js_session_t *jsp)
{ 
    xmlParserCtxt *read_ctxt;
    lx_node_t *nop = NULL, *rop = NULL;
    lx_document_t *docp = NULL;
    lx_nodeset_t *junos_cap;

    if (jsp->js_stderr > 0 && js_initial_read(jsp, JS_READ_TIMEOUT, 0))
	return NULL;

    /*
     * The reply from cisco box is a full xml document with xml declaration 
     * and rpc-reply as root node. But the reply from junos is not a full xml
     * document, it does not have xml declaration, it just has rpc-reply as
     * root node. 
     *
     * But while creating documents for reply from server, we need to present 
     * libxml2 an full XML document. 
     *
     * To have a generic logic for the both the devices (and for junoscript 
     * session also), the idea here is, insert a fake xml declaration and 
     * top level <junoscript> tag into input stream.
     */
    jsp->js_creds = strdup(fake_creds);

    read_ctxt = xmlNewParserCtxt();
    if (read_ctxt == NULL) {
	jsio_trace("jsio: could not make parser context");
	return NULL;
    }

    docp = js_document_read(read_ctxt, jsp, "hello packet", NULL, 0);
    if (docp == NULL) {
	jsio_trace("netconf: could not read hello");
	return NULL;
    }

    js_buffer_close(jsp);
    xmlFreeParserCtxt(read_ctxt);

    nop = lx_document_root(docp);

    if (!nop) {
	lx_document_free(docp);
	return NULL;
    }

    for (nop = lx_node_children(nop); nop; nop = lx_node_next(nop)) {
	if (streq(xmlNodeName(nop), "hello")) {
	    rop = xmlCopyNode(nop, 1);
	    break;
	}
    }

    /*
     * If the hello packet has "http://xml.juniper.net/netconf/junos/1.0" 
     * capability then we are considering the device as junos
     */
    junos_cap = lx_xpath_select(docp, rop, SELECT_JUNOS_CAPABILITY);
    if (lx_nodeset_size(junos_cap)) {
	jsp->js_isjunos = TRUE;
    }

    lx_document_free(docp);

    return rop;
}

/*
 * Initialize Netconf session.
 * Send and read the hello packet from netconf server
 */
int
js_session_init_netconf (js_session_t *jsp)
{
    lx_node_t *hello;

    /*
     * Send the hello packet
     */
    js_send_netconf_hello(jsp);

    hello = js_read_netconf_hello(jsp);
    if (!hello) {
	jsio_trace("did not receive hello packet from server");
	return TRUE;
    }

    jsp->js_hello = hello;

    return FALSE;
}

js_session_t *
js_session_open_server (int fdin, int fdout, session_type_t stype, int flags)
{
    static const char server_name[] = "#server";
    js_session_t *jsp;
    FILE *fp;

    js_initialize();

    fp = fdopen(fdout, "w");
    if (fp == NULL)
	return NULL;

    jsp = malloc(sizeof(*jsp) + sizeof(server_name));
    if (jsp == NULL) {
	fclose(fp);
	return NULL;
    }

    bzero(jsp, sizeof(*jsp));
    jsp->js_pid = -1;
    jsp->js_stdin = fdin;
    jsp->js_stdout = fdout;
    jsp->js_stderr = -1;
    jsp->js_askpassfd = 0;
    jsp->js_fpout = fp;
    jsp->js_msgid = 0;
    jsp->js_hello = NULL;
    jsp->js_isjunos = FALSE;

    if (stype == ST_DEFAULT)
	stype = js_default_stype;

    jsp->js_key.jss_type = stype;
    memcpy(jsp->js_key.jss_name, server_name, sizeof(server_name));
    patricia_node_init_length(&jsp->js_node,
			      sizeof(js_skey_t) + sizeof(server_name));

    if (!js_session_add(jsp)) {
	js_session_release(jsp);
	return NULL;
    }

    jsp->js_fbuf = fbuf_fdopen(fdin, 0);

    if (flags & JSF_FBUF_TRACE)
	fbuf_trace_tagged(jsp->js_fbuf, stdout, "jsp-read");

    if (stype == ST_NETCONF || stype == ST_JUNOS_NETCONF) {
	if (js_session_init_netconf(jsp)) {
	    js_session_terminate(jsp);
	    return NULL;
	}
    } else if (stype == ST_SHELL) {
        /* do nothing */
    } else {
	if (js_session_init(jsp)) {
	    js_session_terminate(jsp);
	    return NULL;
	}
    }

    return jsp;
}

/*
 * Return the hello packet of the given session
 */
lx_node_t *
js_gethello (const char *session_name, session_type_t stype)
{
    js_session_t *jsp;

    if (stype == ST_DEFAULT)
	stype = js_default_stype;

    /*
     * No hello packet for Junoscript session
     */
    if (stype == ST_JUNOSCRIPT)
	return NULL;

    session_name = js_session_get_name(session_name);
    if (session_name == NULL)
	return NULL;

    jsp = js_session_find(session_name, stype);
    if (jsp == NULL)
	return NULL;

    return jsp->js_hello;

}

void
jsio_init (unsigned flags UNUSED)
{
    jsio_flags = flags;
    jsio_askpass_make_socket();
}

void
jsio_restart (void)
{
    for (;;) {
	patnode_t *pnp = patricia_find_next(&js_session_root, NULL);

	if (pnp == NULL)
	    break;
	
	js_session_t *jsp = (void *) pnp;
	js_session_terminate(jsp);
    }
}

void
jsio_cleanup (void)
{
    jsio_restart();
    jsio_askpass_clean_socket();
}
