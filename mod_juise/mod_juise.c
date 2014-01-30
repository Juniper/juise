/*
 * $Id$
 *
 * Copyright (c) 2006-2013, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * This code is based on mod_cgi.c in the lighttpd package, which is:
 *	Copyright (c) 2004, Jan Kneschke, incremental
 *	All rights reserved.
 * See lighttpd/COPYING for additional info (a newbsd copyright).
 */

#include "libjuise/juiseconfig.h"

/* We need to undefine the autoheader-based #defines */
#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION

/* These include files are from lighttpd */
#include "server.h"
#include "stat_cache.h"
#include "keyvalue.h"
#include "log.h"
#include "connections.h"
#include "joblist.h"
#include "http_chunk.h"

#include "plugin.h"

#include <sys/types.h>

#ifdef __WIN32
# include <winsock2.h>
#else
# include <sys/socket.h>
# include <sys/wait.h>
# include <sys/mman.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fdevent.h>
#include <signal.h>
#include <ctype.h>
#include <assert.h>

#include <stdio.h>
#include <fcntl.h>

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#include "version.h"

#include <libslax/slax.h>
#include "mod_juise.h"

#define LOGERR(_fmt...) \
    log_error_write(srv, __FILE__, __LINE__, _fmt)

enum { EOL_UNSET, EOL_N, EOL_RN };

typedef struct {
    char **ptr;
    size_t size;
    size_t used;
} char_array;

typedef struct {
    pid_t *ptr;
    size_t used;
    size_t size;
} buffer_pid_t;

typedef struct {
    array *cgi;
    unsigned short execute_x_only;
    unsigned short require_auth;
} mod_juise_plugin_config;

typedef struct {
    PLUGIN_DATA;
    buffer_pid_t cgi_pid;
    buffer *tmp_buf;
    buffer *parse_response;
    mod_juise_plugin_config **config_storage;
    mod_juise_plugin_config conf;
} mod_juise_plugin_data;

typedef struct {
    pid_t pid;
    int fd;
    int fde_ndx; /* index into the fd-event buffer */
    connection *remote_conn;  /* dumb pointer */
    mod_juise_plugin_data *plugin_data; /* dumb pointer */
    buffer *response;
    buffer *response_header;
} mod_juise_handler_context;

static mod_juise_handler_context *
mod_juise_handler_ctx_init (void)
{
    mod_juise_handler_context *hctx = calloc(1, sizeof(*hctx));

    assert(hctx);

    hctx->response = buffer_init();
    hctx->response_header = buffer_init();

    return hctx;
}

static void
mod_juise_handler_ctx_free (mod_juise_handler_context *hctx)
{
    buffer_free(hctx->response);
    buffer_free(hctx->response_header);

    free(hctx);
}

enum {
    FDEVENT_HANDLED_UNSET,
    FDEVENT_HANDLED_FINISHED,
    FDEVENT_HANDLED_NOT_FINISHED,
    FDEVENT_HANDLED_ERROR
};

INIT_FUNC(mod_juise_init)
{
    mod_juise_plugin_data *p;

    p = calloc(1, sizeof(*p));

    assert(p);

    p->tmp_buf = buffer_init();
    p->parse_response = buffer_init();

    return p;
}

FREE_FUNC(mod_juise_cleanup)
{
    mod_juise_plugin_data *p = p_d;
    buffer_pid_t *r = &p->cgi_pid;

    UNUSED(srv);

    if (p->config_storage) {
	size_t i;
	for (i = 0; i < srv->config_context->used; i++) {
	    mod_juise_plugin_config *s = p->config_storage[i];

	    array_free(s->cgi);

	    free(s);
	}
	free(p->config_storage);
    }

    if (r->ptr)
	free(r->ptr);

    buffer_free(p->tmp_buf);
    buffer_free(p->parse_response);

    free(p);

    return HANDLER_GO_ON;
}

SETDEFAULTS_FUNC(mod_juise_set_defaults)
{
    mod_juise_plugin_data *p = p_d;
    size_t i = 0;

    config_values_t cv[] = {
	{ "juise.assign", NULL,
	  T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
	{ "juise.execute-x-only", NULL,
	  T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },     /* 1 */
	{ "juise.require-auth", NULL,
	  T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },     /* 2 */
	{ NULL, NULL,
	  T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET}
    };

    if (!p)
	return HANDLER_ERROR;

    p->config_storage = calloc(srv->config_context->used,
			       sizeof(*p->config_storage));

    for (i = 0; i < srv->config_context->used; i++) {
	mod_juise_plugin_config *s;

	s = calloc(1, sizeof(*s));
	assert(s);

	s->cgi = array_init();
	s->execute_x_only = 0;
	s->require_auth = 0;

	cv[0].destination = s->cgi;
	cv[1].destination = &s->execute_x_only;
	cv[2].destination = &s->require_auth;

	p->config_storage[i] = s;

	if (config_insert_values_global(srv,
		((data_config *) srv->config_context->data[i])->value, cv)) {
	    return HANDLER_ERROR;
	}
    }

    return HANDLER_GO_ON;
}

static int
mod_juise_pid_add (server *srv, mod_juise_plugin_data *p, pid_t pid)
{
    int m = -1;
    size_t i;
    buffer_pid_t *r = &p->cgi_pid;

    UNUSED(srv);

    for (i = 0; i < r->used; i++) {
	if (r->ptr[i] > m)
	    m = r->ptr[i];
    }

    if (r->size == 0) {
	r->size = 16;
	r->ptr = malloc(sizeof(*r->ptr) * r->size);
    } else if (r->used == r->size) {
	r->size += 16;
	r->ptr = realloc(r->ptr, sizeof(*r->ptr) * r->size);
    }

    r->ptr[r->used++] = pid;

    return m;
}

static int
mod_juise_pid_del (server *srv, mod_juise_plugin_data *p, pid_t pid)
{
    size_t i;
    buffer_pid_t *r = &(p->cgi_pid);

    UNUSED(srv);

    for (i = 0; i < r->used; i++) {
	if (r->ptr[i] == pid)
	    break;
    }

    if (i != r->used) {
	/* found */

	if (i != r->used - 1) {
	    r->ptr[i] = r->ptr[r->used - 1];
	}
	r->used--;
    }

    return 0;
}

static int
mod_juise_response_parse (server *srv, connection *con, mod_juise_plugin_data *p, buffer *in)
{
    char *ns;
    const char *s;
    int line = 0;

    UNUSED(srv);

    buffer_copy_string_buffer(p->parse_response, in);

    for (s = p->parse_response->ptr; (ns = strchr(s, '\n'));
	 	s = ns + 1, line++) {
	const char *key, *value;
	int key_len;
	data_string *ds;

	/* strip the \n */
	ns[0] = '\0';

	if (ns > s && ns[-1] == '\r')
	    ns[-1] = '\0';

	if (line == 0 && strncmp(s, "HTTP/1.", 7) == 0) {
	    /* non-parsed header ... we parse them anyway */

	    if ((s[7] == '1' || s[7] == '0') && s[8] == ' ') {
		int status;
		/* after the space should be a status code for us */

		status = strtol(s+9, NULL, 10);

		if (status >= 100 && status < 1000) {
		    /* we expected 3 digits and didn't got them */
		    con->parsed_response |= HTTP_STATUS;
		    con->http_status = status;
		}
	    }
	} else {
	    /* parse the headers */
	    key = s;
	    if (NULL == (value = strchr(s, ':'))) {
		/* we expect: "<key>: <value>\r\n" */
		continue;
	    }

	    key_len = value - key;
	    value += 1;

	    /* skip LWS */
	    while (*value == ' ' || *value == '\t') value++;

	    ds = (data_string *)
		array_get_unused_element(con->response.headers, TYPE_STRING);
	    if (ds == NULL)
		ds = data_response_init();

	    buffer_copy_string_len(ds->key, key, key_len);
	    buffer_copy_string(ds->value, value);

	    array_insert_unique(con->response.headers, (data_unset *) ds);

	    switch (key_len) {
	    case 4:
		if (strncasecmp(key, "Date", key_len) == 0) {
		    con->parsed_response |= HTTP_DATE;
		}
		break;

	    case 6:
		if (strncasecmp(key, "Status", key_len) == 0) {
		    con->http_status = strtol(value, NULL, 10);
		    con->parsed_response |= HTTP_STATUS;
		}
		break;

	    case 8:
		if (strncasecmp(key, "Location", key_len) == 0) {
		    con->parsed_response |= HTTP_LOCATION;
		}
		break;

	    case 10:
		if (strncasecmp(key, "Connection", key_len) == 0) {
		    con->response.keep_alive
			= (strcasecmp(value, "Keep-Alive") == 0) ? 1 : 0;
		    con->parsed_response |= HTTP_CONNECTION;
		}
		break;

	    case 12:
		if (strncasecmp(key, "Content-Type", key_len) == 0) {
		    response_header_overwrite(srv, con, 
					      CONST_STR_LEN("Content-Type"), 
					      CONST_BUF_LEN(ds->value));
		}
		break;

	    case 14:
		if (strncasecmp(key, "Content-Length", key_len) == 0) {
		    con->response.content_length = strtol(value, NULL, 10);
		    con->parsed_response |= HTTP_CONTENT_LENGTH;
		}
		break;

	    default:
		break;
	    }
	}
    }

    /* CGI/1.1 rev 03 - 7.2.1.2 */
    if ((con->parsed_response & HTTP_LOCATION)
	&& !(con->parsed_response & HTTP_STATUS)) {
	con->http_status = 302;
    }

    return 0;
}

static int
mod_juise_demux_response (server *srv, mod_juise_handler_context *hctx)
{
    mod_juise_plugin_data *p = hctx->plugin_data;
    connection *con = hctx->remote_conn;

    for (;;) {
	int n;

	buffer_prepare_copy(hctx->response, 1024);
	n = read(hctx->fd, hctx->response->ptr, hctx->response->size - 1);
	if (n == -1) {
	    if (errno == EAGAIN || errno == EINTR) {
		/* would block, wait for signal */
		return FDEVENT_HANDLED_NOT_FINISHED;
	    }
	    /* error */
	    LOGERR("sdd", strerror(errno), con->fd, hctx->fd);
	    return FDEVENT_HANDLED_ERROR;
	}

	if (n == 0) {
	    /* read finished */

	    con->file_finished = 1;

	    /* send final chunk */
	    http_chunk_append_mem(srv, con, NULL, 0);
	    joblist_append(srv, con);

	    return FDEVENT_HANDLED_FINISHED;
	}

	hctx->response->ptr[n] = '\0';
	hctx->response->used = n+1;

	/* split header from body */

	if (con->file_started == 0) {
	    int is_header = 0;
	    int is_header_end = 0;
	    size_t last_eol = 0;
	    size_t i;

	    buffer_append_string_buffer(hctx->response_header, hctx->response);

	    /**
	     * we have to handle a few cases:
	     *
	     * nph:
	     * 
	     *   HTTP/1.0 200 Ok\n
	     *   Header: Value\n
	     *   \n
	     *
	     * CGI:
	     *   Header: Value\n
	     *   Status: 200\n
	     *   \n
	     *
	     * and different mixes of \n and \r\n combinations
	     * 
	     * Some users also forget about CGI and just send a
	     * response and hope we handle it. No headers, no
	     * header-content seperator
	     * 
	     */
			
	    /* nph (non-parsed headers) */
	    if (strncmp(hctx->response_header->ptr, "HTTP/1.", 7) == 0)
		is_header = 1;
				
	    for (i = 0; !is_header_end && i < hctx->response_header->used - 1;
		 i++) {
		char c = hctx->response_header->ptr[i];

		switch (c) {
		case '<':
		    is_header_end = 1;
		    break;

		case ':':
		    /*
		     * We found a colon
		     * looks like we have a normal header 
		     */
		    is_header = 1;
		    break;

		case '\n':
		    /* EOL */
		    if (is_header == 0) {
			/*
			 * We got a EOL but we don't seem to got a
			 * HTTP header
			 */
			is_header_end = 1;

			break;
		    }

		    /*
		     * check if we saw a \n(\r)?\n sequence 
		     */
		    if (last_eol > 0 && 
			((i - last_eol == 1) || 
			 (i - last_eol == 2
			  && hctx->response_header->ptr[i - 1] == '\r'))) {
			is_header_end = 1;
			break;
		    }

		    last_eol = i;

		    break;
		}
	    }

	    if (is_header_end) {
		if (!is_header) {
		    /* no header, but a body */

		    if (con->request.http_version == HTTP_VERSION_1_1) {
			con->response.transfer_encoding
			    = HTTP_TRANSFER_ENCODING_CHUNKED;
		    }

		    http_chunk_append_mem(srv, con, hctx->response_header->ptr,
					  hctx->response_header->used);
		    joblist_append(srv, con);
		} else {
		    const char *bstart;
		    size_t blen;
					
		    /*
		     * i still points to the char after the terminating EOL.
		     * put it on the last \n again
		     */
		    i--;
					
		    /* the body starts after the EOL */
		    bstart = hctx->response_header->ptr + (i + 1);
		    blen = (hctx->response_header->used - 1) - (i + 1);
					
		    /* string the last \r?\n */
		    if (i > 0 && hctx->response_header->ptr[i - 1] == '\r')
			i -= 1;

		    hctx->response_header->ptr[i] = '\0';
		    hctx->response_header->used = i + 1; /* the string + \0 */
					
		    /* parse the response header */
		    mod_juise_response_parse(srv, con, p, hctx->response_header);

		    /* enable chunked-transfer-encoding */
		    if (con->request.http_version == HTTP_VERSION_1_1
			    && !(con->parsed_response & HTTP_CONTENT_LENGTH)) {
			con->response.transfer_encoding
			    = HTTP_TRANSFER_ENCODING_CHUNKED;
		    }

		    if (blen > 0) {
			http_chunk_append_mem(srv, con, bstart, blen + 1);
			joblist_append(srv, con);
		    }
		}

		con->file_started = 1;
	    }
	} else {
	    http_chunk_append_mem(srv, con, hctx->response->ptr,
				  hctx->response->used);
	    joblist_append(srv, con);
	}

#if 0
	LOGERR("ddss", con->fd, hctx->fd,
			connection_get_state(con->state), b->ptr);
#endif
    }

    return FDEVENT_HANDLED_NOT_FINISHED;
}

static handler_t
mod_juise_connection_close (server *srv, mod_juise_handler_context *hctx)
{
    int status;
    pid_t pid;
    mod_juise_plugin_data *p;
    connection  *con;

    if (hctx == NULL)
	return HANDLER_GO_ON;

    p = hctx->plugin_data;
    con = hctx->remote_conn;

    if (con->mode != p->id)
	return HANDLER_GO_ON;

#ifndef __WIN32

    /* the connection to the browser went away, but we still have a connection
     * to the CGI script
     *
     * close cgi-connection
     */

    if (hctx->fd != -1) {
	/* close connection to the cgi-script */
	fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
	fdevent_unregister(srv->ev, hctx->fd);

	if (close(hctx->fd)) {
	    LOGERR("sds", "juise close failed ",
			    hctx->fd, strerror(errno));
	}

	hctx->fd = -1;
	hctx->fde_ndx = -1;
    }

    pid = hctx->pid;

    con->plugin_ctx[p->id] = NULL;

    /* is this a good idea ? */
    mod_juise_handler_ctx_free(hctx);

    /* if waitpid hasn't been called by response.c yet, do it here */
    if (pid) {
	/* check if the CGI-script is already gone */
	switch (waitpid(pid, &status, WNOHANG)) {
	case 0:
	    /* not finished yet */
#if 0
	    LOGERR("sd", "(debug) child isn't done yet, pid:", pid);
#endif
	    break;

	case -1:
	    /* */
	    if (errno == EINTR)
		break;

	    /*
	     * errno == ECHILD happens if _subrequest catches the
	     * process-status before we have read the response of the
	     * cgi process
	     *
	     * -> catch status
	     * -> WAIT_FOR_EVENT
	     * -> read response
	     * -> we get here with waitpid == ECHILD
	     *
	     */
	    if (errno == ECHILD)
		return HANDLER_GO_ON;

	    LOGERR("ss", "waitpid failed: ", strerror(errno));
	    return HANDLER_ERROR;

	default:
	    /* Send an error if we haven't sent any data yet */
	    if (con->file_started == 0) {
		connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
		con->http_status = 500;
		con->mode = DIRECT;
	    } else {
		con->file_finished = 1;
	    }

	    if (WIFEXITED(status)) {
#if 0
		LOGERR("sd", "(debug) cgi exited fine, pid:", pid);
#endif
		return HANDLER_GO_ON;
	    } else {
		LOGERR("sdd", "juise died, pid:", pid, status);
		return HANDLER_GO_ON;
	    }
	}

	kill(pid, SIGTERM);

	/* cgi-script is still alive, queue the PID for removal */
	mod_juise_pid_add(srv, p, pid);
    }
#endif
    return HANDLER_GO_ON;
}

static handler_t
mod_juise_connection_reset (server *srv, connection *con, void *p_d)
{
    handler_t rc;
    mod_juise_plugin_data *p = p_d;

    LOGERR("ss", "mod_juise: connection_reset: ", con->physical.path->ptr);

    rc = mod_juise_connection_close(srv, con->plugin_ctx[p->id]);
    LOGERR("s", "mod_juise: connection_reset: done");

    return rc;
}

static handler_t
mod_juise_handle_fdevent (server *srv, void *ctx, int revents)
{
    mod_juise_handler_context *hctx = ctx;
    connection  *con  = hctx->remote_conn;

    joblist_append(srv, con);

    if (hctx->fd == -1) {
	LOGERR("ddss", con->fd, hctx->fd,
	       connection_get_state(con->state), "invalid cgi-fd");

	return HANDLER_ERROR;
    }

    if (revents & FDEVENT_IN) {
	switch (mod_juise_demux_response(srv, hctx)) {
	case FDEVENT_HANDLED_NOT_FINISHED:
	    break;

	case FDEVENT_HANDLED_FINISHED:
	    /* we are done */

#if 0
	    LOGERR("ddss", con->fd, hctx->fd,
		   connection_get_state(con->state), "finished");
#endif
	    mod_juise_connection_close(srv, hctx);

	    /*
	     * If we get a IN|HUP and have read everything don't exec
	     * the close twice
	     */
	    return HANDLER_FINISHED;

	case FDEVENT_HANDLED_ERROR:
	    /* Send an error if we haven't sent any data yet */
	    if (con->file_started == 0) {
		connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
		con->http_status = 500;
		con->mode = DIRECT;
	    } else {
		con->file_finished = 1;
	    }

	    LOGERR("s", "demuxer failed: ");
	    break;
	}
    }

    if (revents & FDEVENT_OUT) {
	/* nothing to do */
    }

    /* perhaps this issue is already handled */
    if (revents & FDEVENT_HUP) {
	/*
	 * check if we still have a unfinished header package which is
	 * a body in reality
	 */
	if (con->file_started == 0 && hctx->response_header->used) {
	    con->file_started = 1;
	    http_chunk_append_mem(srv, con, hctx->response_header->ptr,
				  hctx->response_header->used);
	    joblist_append(srv, con);
	}

	if (con->file_finished == 0) {
	    http_chunk_append_mem(srv, con, NULL, 0);
	    joblist_append(srv, con);
	}

	con->file_finished = 1;

	if (chunkqueue_is_empty(con->write_queue)) {
	    /* there is nothing left to write */
	    connection_set_state(srv, con, CON_STATE_RESPONSE_END);
	} else {
	    /* used the write-handler to finish the request on demand */

	}

#if 0
	LOGERR("sddd", "got HUP from cgi", con->fd, hctx->fd, revents);
#endif

	/* rtsigs didn't liked the close */
	mod_juise_connection_close(srv, hctx);

    } else if (revents & FDEVENT_ERR) {
	con->file_finished = 1;

	/* kill all connections to the cgi process */
	mod_juise_connection_close(srv, hctx);
#if 1
	LOGERR("s", "juise-FDEVENT_ERR");
#endif
	return HANDLER_ERROR;
    }

    return HANDLER_FINISHED;
}

static int
mod_juise_env_add (char_array *env, const char *key,
	     size_t key_len, const char *val, size_t val_len)
{
    char *dst;

    if (!key || !val)
	return -1;

    dst = malloc(key_len + val_len + 2);
    memcpy(dst, key, key_len);
    dst[key_len] = '=';
    memcpy(dst + key_len + 1, val, val_len);
    dst[key_len + 1 + val_len] = '\0';

    if (env->size == 0) {
	env->size = 16;
	env->ptr = malloc(env->size * sizeof(*env->ptr));
    } else if (env->size == env->used) {
	env->size += 16;
	env->ptr = realloc(env->ptr, env->size * sizeof(*env->ptr));
    }

    env->ptr[env->used++] = dst;

    return 0;
}

static int
mod_juise_create_env (server *srv, connection *con,
		  mod_juise_plugin_data *p, buffer *cgi_handler)
{
    pid_t pid;

#ifdef HAVE_IPV6
    char b2[INET6_ADDRSTRLEN + 1];
#endif

    int to_cgi_fds[2];
    int from_cgi_fds[2];
    struct stat st;
    char **argv;
    int argc;
    int i = 0;
    char *sdup = NULL, *cp;
    char *http_auth = NULL, *user = NULL, *pass = NULL;
    data_string *ds;

#ifndef __WIN32
    /* set up args */
    argc = 32;
    argv = malloc(sizeof(*argv) * argc);
    i = 0;

    if (cgi_handler->used > 1) {
	sdup = cp = strdup(cgi_handler->ptr);
	while (*cp) {
	    argv[i++] = cp;
	    if (i == argc - 2) /* Save two slots */
		break;
	    for (; *cp; cp++) {
		if (*cp == '\\' && cp[1] != '\0')
		    cp += 1;
		else if (isspace((int) *cp)) {
		    *cp++ = '\0';
		    while (isspace((int) *cp))
			cp += 1;
		    break;
		}
	    }
	}
    }

    if (con->physical.path->used > 1)
	argv[i++] = con->physical.path->ptr;
    argv[i] = NULL;
    argc = i;

    for (i = 0; i < argc; i++) {
	LOGERR("ss", "juise: argv: ", argv[i]);
    }

    if (cgi_handler->used > 1) {
	/* stat the exec file */
	if (stat(argv[0], &st) < 0) {
	    LOGERR("sbss", "stat for cgi-handler", argv[0],
		   "failed:", strerror(errno));
	    if (argv)
		free(argv);
	    return -1;
	}
    }

    if (pipe(to_cgi_fds)) {
	LOGERR("ss", "pipe failed:", strerror(errno));
	if (argv)
	    free(argv);
	return -1;
    }

    if (pipe(from_cgi_fds)) {
	close(to_cgi_fds[0]);
	close(to_cgi_fds[1]);
	LOGERR("ss", "pipe failed:", strerror(errno));
	if (argv)
	    free(argv);
	return -1;
    }

    /* Get Authorization header */
    ds = (data_string *) array_get_element(con->request.headers, 
					   "Authorization");
    if (ds) {
	char *auth_realm_decoded, *auth_realm = NULL; 
	size_t dlen;

	http_auth = ds->value->ptr;

	if (http_auth) {
	    auth_realm = strchr(http_auth, ' ');
	}

	if (auth_realm) {
	    int auth_type_len = auth_realm - http_auth;

	    if (auth_type_len == 5 
		&& strncasecmp(http_auth, "Basic", auth_type_len) == 0 
		&& auth_realm + 1) {
		size_t hash_len = strlen(auth_realm + 1);

		auth_realm_decoded = (char *) slaxBase64Decode(auth_realm + 1, 
							      hash_len, &dlen);
		if (!auth_realm_decoded) {
		    LOGERR("s", "Failed to decode auth header");
		    con->http_status = 400;
		    if (argv)
			free(argv);
		    return 0;
		}

		pass = strchr(auth_realm_decoded, ':');
		if (!pass) {
		    LOGERR("s", "Invalid authorization format");
		    con->http_status = 400;
		    free(auth_realm_decoded);
		    if (argv)
			free(argv);
		    return 0;
		}

		user = auth_realm_decoded;
		*pass = '\0';
		pass++;
	    } else {
		LOGERR("s", "Unrecognized authorization format");
		con->http_status = 400;
		return 0;
	    }
	}
    }

    /* fork, execve */
    switch (pid = fork()) {
    case 0:
	{
	    /* child */
	    char buf[32];
	    size_t n;
	    char_array env;
	    char *c;
	    const char *s;
	    server_socket *srv_sock = con->srv_socket;

	    /* move stdout to from_cgi_fd[1] */
	    close(STDOUT_FILENO);
	    dup2(from_cgi_fds[1], STDOUT_FILENO);
	    close(from_cgi_fds[1]);
	    /* not needed */
	    close(from_cgi_fds[0]);

	    /* move the stdin to to_cgi_fd[0] */
	    close(STDIN_FILENO);
	    dup2(to_cgi_fds[0], STDIN_FILENO);
	    close(to_cgi_fds[0]);
	    /* not needed */
	    close(to_cgi_fds[1]);

	    /* create environment */
	    env.ptr = NULL;
	    env.size = 0;
	    env.used = 0;

	    if (buffer_is_empty(con->conf.server_tag)) {
		mod_juise_env_add(&env, CONST_STR_LEN("SERVER_SOFTWARE"),
			      CONST_STR_LEN(PACKAGE_DESC));
	    } else {
		mod_juise_env_add(&env, CONST_STR_LEN("SERVER_SOFTWARE"),
			      CONST_BUF_LEN(con->conf.server_tag));
	    }

	    if (!buffer_is_empty(con->server_name)) {
		size_t len = con->server_name->used - 1;
		char *colon = strchr(con->server_name->ptr, ':');
		if (colon)
		    len = colon - con->server_name->ptr;

		mod_juise_env_add(&env, CONST_STR_LEN("SERVER_NAME"),
			      con->server_name->ptr, len);
	    } else {
#ifdef HAVE_IPV6
		s = inet_ntop(srv_sock->addr.plain.sa_family,
			      srv_sock->addr.plain.sa_family == AF_INET6 ?
			      (const void *) &(srv_sock->addr.ipv6.sin6_addr) :
			      (const void *) &(srv_sock->addr.ipv4.sin_addr),
			      b2, sizeof(b2)-1);
#else
		s = inet_ntoa(srv_sock->addr.ipv4.sin_addr);
#endif
		mod_juise_env_add(&env, CONST_STR_LEN("SERVER_NAME"), s, strlen(s));
	    }
	    mod_juise_env_add(&env, CONST_STR_LEN("GATEWAY_INTERFACE"),
			  CONST_STR_LEN("CGI/1.1"));

	    s = get_http_version_name(con->request.http_version);

	    mod_juise_env_add(&env, CONST_STR_LEN("SERVER_PROTOCOL"), s, strlen(s));

	    LI_ltostr(buf,
#ifdef HAVE_IPV6
		      ntohs(srv_sock->addr.plain.sa_family == AF_INET6
			    ? srv_sock->addr.ipv6.sin6_port
			    : srv_sock->addr.ipv4.sin_port)
#else
		      ntohs(srv_sock->addr.ipv4.sin_port)
#endif
		      );
	    mod_juise_env_add(&env, CONST_STR_LEN("SERVER_PORT"), buf, strlen(buf));

	    switch (srv_sock->addr.plain.sa_family) {
#ifdef HAVE_IPV6
	    case AF_INET6:
		s = inet_ntop(srv_sock->addr.plain.sa_family,
			      (const void *) &(srv_sock->addr.ipv6.sin6_addr),
			      b2, sizeof(b2)-1);
		break;

	    case AF_INET:
		s = inet_ntop(srv_sock->addr.plain.sa_family,
			      (const void *) &(srv_sock->addr.ipv4.sin_addr),
			      b2, sizeof(b2)-1);
		break;
#else
	    case AF_INET:
		s = inet_ntoa(srv_sock->addr.ipv4.sin_addr);
		break;
#endif
	    default:
		s = "";
		break;
	    }
	    mod_juise_env_add(&env, CONST_STR_LEN("SERVER_ADDR"), s, strlen(s));

	    s = get_http_method_name(con->request.http_method);
	    mod_juise_env_add(&env, CONST_STR_LEN("REQUEST_METHOD"), s, strlen(s));

	    if (!buffer_is_empty(con->request.pathinfo)) {
		mod_juise_env_add(&env, CONST_STR_LEN("PATH_INFO"),
			      CONST_BUF_LEN(con->request.pathinfo));
	    }
	    mod_juise_env_add(&env, CONST_STR_LEN("REDIRECT_STATUS"),
			  CONST_STR_LEN("200"));
	    if (!buffer_is_empty(con->uri.query)) {
		mod_juise_env_add(&env, CONST_STR_LEN("QUERY_STRING"),
			      CONST_BUF_LEN(con->uri.query));
	    }
	    if (!buffer_is_empty(con->request.orig_uri)) {
		mod_juise_env_add(&env, CONST_STR_LEN("REQUEST_URI"),
			      CONST_BUF_LEN(con->request.orig_uri));
	    }


	    switch (con->dst_addr.plain.sa_family) {
#ifdef HAVE_IPV6
	    case AF_INET6:
		s = inet_ntop(con->dst_addr.plain.sa_family,
			      (const void *) &(con->dst_addr.ipv6.sin6_addr),
			      b2, sizeof(b2)-1);
		break;

	    case AF_INET:
		s = inet_ntop(con->dst_addr.plain.sa_family,
			      (const void *) &(con->dst_addr.ipv4.sin_addr),
			      b2, sizeof(b2)-1);
		break;
#else
	    case AF_INET:
		s = inet_ntoa(con->dst_addr.ipv4.sin_addr);
		break;
#endif
	    default:
		s = "";
	    }

	    mod_juise_env_add(&env, CONST_STR_LEN("REMOTE_ADDR"), s, strlen(s));

	    LI_ltostr(buf,
#ifdef HAVE_IPV6
		      ntohs(con->dst_addr.plain.sa_family == AF_INET6
			    ? con->dst_addr.ipv6.sin6_port
			    : con->dst_addr.ipv4.sin_port)
#else
		      ntohs(con->dst_addr.ipv4.sin_port)
#endif
		      );
	    mod_juise_env_add(&env, CONST_STR_LEN("REMOTE_PORT"), buf, strlen(buf));

	    data_string *dsp;
	    dsp = (data_string *) array_get_element(con->environment,
						    "REMOTE_USER");
	    if (dsp != NULL && dsp->value->used > 1) {
		mod_juise_env_add(&env, CONST_STR_LEN("REMOTE_USER"),
				  CONST_BUF_LEN(dsp->value));
	    }

#ifdef USE_OPENSSL
	    if (srv_sock->is_ssl) {
		mod_juise_env_add(&env, CONST_STR_LEN("HTTPS"), CONST_STR_LEN("on"));
	    }
#endif

	    /* request.content_length < SSIZE_MAX, see request.c */
	    LI_ltostr(buf, con->request.content_length);
	    mod_juise_env_add(&env, CONST_STR_LEN("CONTENT_LENGTH"), buf, strlen(buf));
	    mod_juise_env_add(&env, CONST_STR_LEN("SCRIPT_FILENAME"),
			  CONST_BUF_LEN(con->physical.path));
	    mod_juise_env_add(&env, CONST_STR_LEN("SCRIPT_NAME"),
			  CONST_BUF_LEN(con->uri.path));
	    mod_juise_env_add(&env, CONST_STR_LEN("DOCUMENT_ROOT"),
			  CONST_BUF_LEN(con->physical.doc_root));

	    /* for valgrind */
	    if ((s = getenv("LD_PRELOAD")) != NULL) {
		mod_juise_env_add(&env, CONST_STR_LEN("LD_PRELOAD"), s, strlen(s));
	    }

	    if ((s = getenv("LD_LIBRARY_PATH")) != NULL) {
		mod_juise_env_add(&env, CONST_STR_LEN("LD_LIBRARY_PATH"), s, strlen(s));
	    }
#ifdef __CYGWIN__
	    /* CYGWIN needs SYSTEMROOT */
	    if ((s = getenv("SYSTEMROOT")) != NULL) {
		mod_juise_env_add(&env, CONST_STR_LEN("SYSTEMROOT"), s, strlen(s));
	    }
#endif

	    /*
	     * We want to pass the SSH environment variables to our
	     * child so it can access ssh-agent to get our ssh keys.
	     */
	    {
		const char *vars[] = {
		    "SSH_AGENT_PID",
		    "SSH_AUTH_SOCK",
		    "JUISE_OPTIONS",
		    "JUISE_TRACE_FILE",
		    "JUISE_WAIT",
		    "PATH",
		    NULL
		};
		const char **sp;

		for (sp = vars ; *sp; sp++) {
		    cp = getenv(*sp);
		    if (cp) {
			mod_juise_env_add(&env, *sp, strlen(*sp), cp, strlen(cp));
			LOGERR("sss", "juise: making env:", *sp, cp);
		    }
		}
	    }

	    for (n = 0; n < con->request.headers->used; n++) {
		ds = (data_string *)con->request.headers->data[n];

		if (ds->value->used && ds->key->used) {
		    size_t j;

		    /* Don't set authorization header in env */
		    if (strcasecmp(ds->key->ptr, "AUTHORIZATION") == 0) {
			continue;
		    }

		    buffer_reset(p->tmp_buf);

		    if (strcasecmp(ds->key->ptr, "CONTENT-TYPE") != 0) {
			buffer_copy_string_len(p->tmp_buf, CONST_STR_LEN("HTTP_"));
			p->tmp_buf->used--; /* strip \0 after HTTP_ */
		    }

		    buffer_prepare_append(p->tmp_buf, ds->key->used + 2);

		    for (j = 0; j < ds->key->used - 1; j++) {
			char cr = '_';
			if (light_isalpha(ds->key->ptr[j])) {
			    /* upper-case */
			    cr = ds->key->ptr[j] & ~32;
			} else if (light_isdigit(ds->key->ptr[j])) {
			    /* copy */
			    cr = ds->key->ptr[j];
			}
			p->tmp_buf->ptr[p->tmp_buf->used++] = cr;
		    }
		    p->tmp_buf->ptr[p->tmp_buf->used++] = '\0';

		    mod_juise_env_add(&env, CONST_BUF_LEN(p->tmp_buf),
				  CONST_BUF_LEN(ds->value));
		}
	    }

	    for (n = 0; n < con->environment->used; n++) {
		ds = (data_string *)con->environment->data[n];

		if (ds->value->used && ds->key->used) {
		    size_t j;

		    buffer_reset(p->tmp_buf);

		    buffer_prepare_append(p->tmp_buf, ds->key->used + 2);

		    for (j = 0; j < ds->key->used - 1; j++) {
			char cr = '_';
			if (light_isalpha(ds->key->ptr[j])) {
			    /* upper-case */
			    cr = ds->key->ptr[j] & ~32;
			} else if (light_isdigit(ds->key->ptr[j])) {
			    /* copy */
			    cr = ds->key->ptr[j];
			}
			p->tmp_buf->ptr[p->tmp_buf->used++] = cr;
		    }
		    p->tmp_buf->ptr[p->tmp_buf->used++] = '\0';

		    mod_juise_env_add(&env, CONST_BUF_LEN(p->tmp_buf),
				  CONST_BUF_LEN(ds->value));
		}
	    }

	    if (env.size == env.used) {
		env.size += 16;
		env.ptr = realloc(env.ptr, env.size * sizeof(*env.ptr));
	    }

	    env.ptr[env.used] = NULL;

	    /* search for the last / */
	    if ((c = strrchr(con->physical.path->ptr, '/')) != NULL) {
		*c = '\0';

		/* change to the physical directory */
		if (chdir(con->physical.path->ptr) == -1) {
		    LOGERR("ssb", "chdir failed:",
			   strerror(errno), con->physical.path);
		}
		*c = '/';
	    }

	    /* we don't need the client socket */
	    for (i = 3; i < 256; i++) {
		if (i != srv->errorlog_fd)
		    close(i);
	    }

	    /* exec the cgi */
	    execve(argv[0], argv, env.ptr);

	    /* LOGERR("sss", "CGI failed:", strerror(errno), argv[0]); */

	    /* */
	    SEGFAULT();
	    break;
	}

    case -1:
	/* error */
	LOGERR("ss", "fork failed:", strerror(errno));
	close(from_cgi_fds[0]);
	close(from_cgi_fds[1]);
	close(to_cgi_fds[0]);
	close(to_cgi_fds[1]);
	if (sdup)
	    free(sdup);
	if (argv)
	    free(argv);
	return -1;

    default:
	{
	    mod_juise_handler_context *hctx;
	    /* father */

	    close(from_cgi_fds[1]);
	    close(to_cgi_fds[0]);
		    
	    int r = 0;

	    if (p->conf.require_auth == 1 && http_auth) {
		buffer *auth = buffer_init();
		buffer_append_string_len(auth, CONST_STR_LEN("user="));
		buffer_append_string(auth, user);
		buffer_append_string_len(auth, CONST_STR_LEN("\npassword="));
		buffer_append_string(auth, pass);
		buffer_append_string_len(auth, CONST_STR_LEN("\n"));
		r = write(to_cgi_fds[1], auth->ptr, 
			  auth->used ? auth->used - 1 : 0);
		if (r < 0) {
		    switch (errno) {
			case ENOSPC:
			    con->http_status = 507;
			    break;

			default:
			    con->http_status = 403;
			    break;
		    }
		}
		buffer_free(auth);

		if (pass) {
		    bzero(pass, strlen(pass));
		}
	    }

	    if (con->request.content_length) {
		chunkqueue *cq = con->request_content_queue;
		chunk *c;

		assert(chunkqueue_length(cq)
		       == (off_t)con->request.content_length);

		/* there is content to send */
		for (c = cq->first; c; c = cq->first) {

		    /* copy all chunks */
		    switch (c->type) {
		    case FILE_CHUNK:

			if (c->file.mmap.start == MAP_FAILED) {
			    /* Open the file if not already open */
			    if (c->file.fd == -1
				&& (c->file.fd
				    = open(c->file.name->ptr, O_RDONLY)) == -1) {
				LOGERR("ss", "open failed: ",
				       strerror(errno));

				close(from_cgi_fds[0]);
				close(to_cgi_fds[1]);
				if (sdup)
				    free(sdup);
				if (argv)
				    free(argv);
				return -1;
			    }

			    c->file.mmap.length = c->file.length;

			    c->file.mmap.start = mmap(0,  c->file.mmap.length,
						      PROT_READ, MAP_SHARED,
						      c->file.fd, 0);
			    if (c->file.mmap.start == MAP_FAILED) {
				LOGERR("ssbd", "mmap failed: ",
				       strerror(errno), c->file.name,
				       c->file.fd);

				close(from_cgi_fds[0]);
				close(to_cgi_fds[1]);
				if (sdup)
				    free(sdup);
				return -1;
			    }

			    close(c->file.fd);
			    c->file.fd = -1;

			    /*
			     * chunk_reset() or chunk_free() will
			     * cleanup for us
			     */
			}

			r = write(to_cgi_fds[1],
				  c->file.mmap.start + c->offset,
				  c->file.length - c->offset);
			if (r < 0) {
			    switch (errno) {
			    case ENOSPC:
				con->http_status = 507;
				break;

			    case EINTR:
				continue;

			    default:
				con->http_status = 403;
				break;
			    }
			}
			break;

		    case MEM_CHUNK:
			r = write(to_cgi_fds[1], c->mem->ptr + c->offset,
				  c->mem->used - c->offset - 1);
			if (r < 0) {
			    switch (errno) {
			    case ENOSPC:
				con->http_status = 507;
				break;

			    case EINTR:
				continue;

			    default:
				con->http_status = 403;
				break;
			    }
			}
			break;

		    case UNUSED_CHUNK:
			break;
		    }

		    if (r > 0) {
			c->offset += r;
			cq->bytes_out += r;
		    } else {
			LOGERR("ss", "write() failed due to: ",
			       strerror(errno)); 
			con->http_status = 500;
			break;
		    }
		    chunkqueue_remove_finished_chunks(cq);
		}
	    }

	    close(to_cgi_fds[1]);

	    /* register PID and wait for them asyncronously */
	    con->mode = p->id;
	    buffer_reset(con->physical.path);

	    hctx = mod_juise_handler_ctx_init();

	    hctx->remote_conn = con;
	    hctx->plugin_data = p;
	    hctx->pid = pid;
	    hctx->fd = from_cgi_fds[0];
	    hctx->fde_ndx = -1;

	    con->plugin_ctx[p->id] = hctx;

	    fdevent_register(srv->ev, hctx->fd, mod_juise_handle_fdevent, hctx);
	    fdevent_event_set(srv->ev, &(hctx->fde_ndx), hctx->fd, FDEVENT_IN);

	    if (fdevent_fcntl_set(srv->ev, hctx->fd) == -1) {
		LOGERR("ss", "fcntl failed: ", strerror(errno));

		fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
		fdevent_unregister(srv->ev, hctx->fd);

		LOGERR("sd", "juise close:", hctx->fd);

		close(hctx->fd);

		mod_juise_handler_ctx_free(hctx);

		con->plugin_ctx[p->id] = NULL;

		if (sdup)
		    free(sdup);
		if (argv)
		    free(argv);
		return -1;
	    }
	}
    }

    if (sdup)
	free(sdup);
    if (argv)
	free(argv);
    return 0;
#else
    if (sdup)
	free(sdup);
    if (argv)
	free(argv);
    return -1;
#endif
}

#define PATCH(x) p->conf.x = s->x

static int
mod_juise_patch_connection (server *srv, connection *con, mod_juise_plugin_data *p)
{
    size_t i, j;
    mod_juise_plugin_config *s = p->config_storage[0];

    PATCH(cgi);
    PATCH(execute_x_only);
    PATCH(require_auth);

    /* skip the first, the global context */
    for (i = 1; i < srv->config_context->used; i++) {
	data_config *dc = (data_config *) srv->config_context->data[i];
	s = p->config_storage[i];

	/* condition didn't match */
	if (!config_check_cond(srv, con, dc))
	    continue;

	/* merge config */
	for (j = 0; j < dc->value->used; j++) {
	    data_unset *du = dc->value->data[j];

	    if (buffer_is_equal_string(du->key, 
				       CONST_STR_LEN("juise.assign"))) {
		PATCH(cgi);
	    } else if (buffer_is_equal_string(du->key,
				      CONST_STR_LEN("juise.execute-x-only"))) {
		PATCH(execute_x_only);
	    } else if (buffer_is_equal_string(du->key, 
				CONST_STR_LEN("juise.require-auth"))) {
		PATCH(require_auth);
	    }
	}
    }

    return 0;
}
#undef PATCH

URIHANDLER_FUNC(mod_juise_handle_subrequest_start)
{
    size_t k, s_len;
    mod_juise_plugin_data *p = p_d;
    buffer *fn = con->physical.path;
    buffer *uri = con->uri.path;
    stat_cache_entry *sce = NULL;

    LOGERR("ss", "mod_juise: start: looking at ", fn->ptr);

    if (con->mode != DIRECT)
	return HANDLER_GO_ON;

    if (fn->used == 0)
	return HANDLER_GO_ON;

    mod_juise_patch_connection(srv, con, p);

    if (stat_cache_get_entry(srv, con, con->physical.path, &sce)
		== HANDLER_ERROR)
	return HANDLER_GO_ON;

    if (!S_ISREG(sce->st.st_mode))
	return HANDLER_GO_ON;

    if (p->conf.execute_x_only == 1
		&& (sce->st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0)
	return HANDLER_GO_ON;

    /* Prompt for authorization information if required by config */
    if (p->conf.require_auth == 1 
	&& array_get_element(con->request.headers, "Authorization") == NULL) {
	con->http_status = 401;
	con->mode = DIRECT;
	buffer_reset(p->tmp_buf);
	buffer_append_string_len(p->tmp_buf, 
			CONST_STR_LEN("Basic realm=\"Need basic auth header\""));
	response_header_insert(srv, con, CONST_STR_LEN("WWW-Authenticate"), 
			       CONST_BUF_LEN(p->tmp_buf));
	LOGERR("s", "Redirecting as mandatory authentication header missing");
	return HANDLER_FINISHED;
    }

    s_len = fn->used - 1;

    for (k = 0; k < p->conf.cgi->used; k++) {
	data_string *ds = (data_string *) p->conf.cgi->data[k];
	size_t ct_len = ds->key->used - 1;

	if (ds->key->used == 0)
	    continue;
	if (s_len < ct_len)
	    continue;

	if (ds->key->ptr[0] == '/') {
	    if (strncmp(uri->ptr, ds->key->ptr, ct_len) == 0) {
		if (mod_juise_create_env(srv, con, p, ds->value)) {
		    con->mode = DIRECT;
		    con->http_status = 500;

		    buffer_reset(con->physical.path);
		    return HANDLER_FINISHED;
		}
		/* one handler is enough for the request */
		break;
	    }
	} else {
	    if (strncmp(fn->ptr + s_len - ct_len, ds->key->ptr, ct_len) == 0) {
		if (mod_juise_create_env(srv, con, p, ds->value)) {
		    con->mode = DIRECT;
		    con->http_status = 500;

		    buffer_reset(con->physical.path);
		    return HANDLER_FINISHED;
		}
		/* one handler is enough for the request */
		break;
	    }
	}
    }

    return HANDLER_GO_ON;
}

URIHANDLER_FUNC(mod_juise_handle_physical)
{
    size_t k, s_len;
    mod_juise_plugin_data *p = p_d;
    buffer *fn = con->physical.path;
    buffer *uri = con->uri.path;

    LOGERR("ss", "mod_juise: physical: fn ", fn->ptr);
    LOGERR("ss", "mod_juise: physical: uri ", uri->ptr);

    s_len = uri->used - 1;

    mod_juise_patch_connection(srv, con, p);

    for (k = 0; k < p->conf.cgi->used; k++) {
	data_string *ds = (data_string *) p->conf.cgi->data[k];
	size_t ct_len = ds->key->used - 1;

	if (ds->key->used == 0)
	    continue;
	if (s_len < ct_len)
	    continue;

	if (ds->key->ptr[0] == '/') {
	    if (strncmp(uri->ptr, ds->key->ptr, ct_len) == 0) {
		buffer_copy_string(fn, PATH_JUISE);
		break;
	    }
	}
    }
    return HANDLER_GO_ON;
}

TRIGGER_FUNC(mod_juise_handle_trigger)
{
    mod_juise_plugin_data *p = p_d;
    size_t ndx;
    /* the trigger handle only cares about lonely PID which we have to wait for */

#ifndef __WIN32

    for (ndx = 0; ndx < p->cgi_pid.used; ndx++) {
	int status;

	switch (waitpid(p->cgi_pid.ptr[ndx], &status, WNOHANG)) {
	case 0:
	    /* not finished yet */
#if 0
	    LOGERR("sd", "(debug) child isn't done yet, pid:",
		   p->cgi_pid.ptr[ndx]);
#endif
	    break;

	case -1:
	    LOGERR("ss", "waitpid failed: ", strerror(errno));

	    return HANDLER_ERROR;

	default:
	    if (WIFEXITED(status)) {
#if 0
		LOGERR("sd", "(debug) cgi exited fine, pid:",
		       p->cgi_pid.ptr[ndx]);
#endif
	    } else if (WIFSIGNALED(status)) {
		/*
		 * FIXME: what if we killed the CGI script with a
		 * kill(..., SIGTERM) ?
		 */
		if (WTERMSIG(status) != SIGTERM) {
		    LOGERR("sd", "cleaning up CGI: process died with signal",
			   WTERMSIG(status));
		}
	    } else {
		LOGERR("s", "cleaning up CGI: ended unexpectedly");
	    }

	    mod_juise_pid_del(srv, p, p->cgi_pid.ptr[ndx]);

	    /*
	     * del modified the buffer structure
	     * and copies the last entry to the current one
	     * -> recheck the current index
	     */
	    ndx -= 1;
	}
    }
#endif
    return HANDLER_GO_ON;
}

/*
 * - HANDLER_GO_ON : not our job
 * - HANDLER_FINISHED: got response header
 * - HANDLER_WAIT_FOR_EVENT: waiting for response header
 */
SUBREQUEST_FUNC(mod_juise_handle_subrequest)
{
    int status;
    mod_juise_plugin_data *p = p_d;
    mod_juise_handler_context *hctx = con->plugin_ctx[p->id];

    LOGERR("ss", "mod_juise: handle: ", con->physical.path->ptr);

    if (con->mode != p->id)
	return HANDLER_GO_ON;

    if (NULL == hctx)
	return HANDLER_GO_ON;

#if 0
    LOGERR("sdd", "subrequest, pid =", hctx, hctx->pid);
#endif

    if (hctx->pid == 0) {
	/* cgi already dead */
	if (!con->file_started)
	    return HANDLER_WAIT_FOR_EVENT;
	return HANDLER_FINISHED;
    }

#ifndef __WIN32
    switch (waitpid(hctx->pid, &status, WNOHANG)) {
    case 0:
	/* we only have for events here if we don't have the header yet,
	 * otherwise the event-handler will send us the incoming data */
	if (con->file_started)
	    return HANDLER_FINISHED;

	return HANDLER_WAIT_FOR_EVENT;

    case -1:
	if (errno == EINTR)
	    return HANDLER_WAIT_FOR_EVENT;

	if (errno == ECHILD && con->file_started == 0) {
	    /*
	     * second round but still not response
	     */
	    return HANDLER_WAIT_FOR_EVENT;
	}

	LOGERR("ss", "waitpid failed: ", strerror(errno));
	con->mode = DIRECT;
	con->http_status = 500;

	hctx->pid = 0;

	fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
	fdevent_unregister(srv->ev, hctx->fd);

	if (close(hctx->fd)) {
	    LOGERR("sds", "juise close failed ", hctx->fd, strerror(errno));
	}

	mod_juise_handler_ctx_free(hctx);

	con->plugin_ctx[p->id] = NULL;

	return HANDLER_FINISHED;

    default:
	/* cgi process exited */
	hctx->pid = 0;

	/* we already have response headers? just continue */
	if (con->file_started)
	    return HANDLER_FINISHED;

	if (WIFEXITED(status)) {
	    /* clean exit - just continue */
	    return HANDLER_WAIT_FOR_EVENT;
	}

	/*
	 * cgi proc died, and we didn't get any data yet - send error
	 * message and close cgi con
	 */
	LOGERR("s", "juise died ?");

	con->http_status = 500;
	con->mode = DIRECT;

	fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
	fdevent_unregister(srv->ev, hctx->fd);

	if (close(hctx->fd)) {
	    LOGERR("sds", "juise close failed ", hctx->fd, strerror(errno));
	}

	mod_juise_handler_ctx_free(hctx);

	con->plugin_ctx[p->id] = NULL;
	return HANDLER_FINISHED;
    }
#else
    return HANDLER_ERROR;
#endif
}

int mod_juise_plugin_init(plugin *p);
int
mod_juise_plugin_init (plugin *p)
{
    p->version = LIGHTTPD_VERSION_ID;
    p->name = buffer_init_string("juise");

    p->connection_reset = mod_juise_connection_reset;
    p->handle_subrequest_start = mod_juise_handle_subrequest_start;
    p->handle_subrequest = mod_juise_handle_subrequest;
#if 0
    p->handle_fdevent = mod_juise_handle_fdevent;
#endif
    p->handle_trigger = mod_juise_handle_trigger;
    p->init = mod_juise_init;
    p->cleanup = mod_juise_cleanup;
    p->set_defaults = mod_juise_set_defaults;
    p->handle_physical = mod_juise_handle_physical;

    p->data = NULL;

    return 0;
}
