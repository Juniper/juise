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

#include <libslax/slax.h>
#include <libpsu/psubase64.h>
#include <libxml/uri.h>

#undef UNUSED /* lighttpd has it's own definition */

/* These include files are from lighttpd */
#include "base_decls.h"
#include "fdevent_impl.h"
#include "fdlog.h"
#include "first.h"
#include "network_write.h"
#include "base.h"
#include "stat_cache.h"
#include "keyvalue.h"
#include "log.h"
#include "connections.h"
#include "http_chunk.h"
#include "http_header.h"
#include "response.h"
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
#include <stdarg.h>
#include <string.h>
#include <fdevent.h>
#include <signal.h>
#include <ctype.h>
#include <assert.h>
#include "burl.h"

#include <stdio.h>
#include <fcntl.h>

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#include <pwd.h>
#include "mod_juise.h"

#ifndef PACKAGE_DESC
#define PACKAGE_DESC "juise/clira server"
#endif


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
    const array *cgi;
    unsigned short execute_x_only;
    unsigned short require_auth;
    const buffer *mixer;
} mod_juise_plugin_config;

typedef struct {
    PLUGIN_DATA;
    buffer_pid_t cgi_pid;
    buffer *tmp_buf;
    buffer *parse_response;
    mod_juise_plugin_config defaults;
    mod_juise_plugin_config conf;
} mod_juise_plugin_data;

typedef struct {
    pid_t pid;
    int fd;
    fdnode *fdn; /* fdnode for fd-event */
    connection *remote_conn;  /* dumb pointer */
    mod_juise_plugin_data *plugin_data; /* dumb pointer */
    buffer *response;
    buffer *response_header;
} mod_juise_handler_context;

static int
mod_juise_create_env (request_st *r,
		  mod_juise_plugin_data *p, buffer *cgi_handler);

static int
mod_juise_env_add (char_array *env, const char *key,
	     size_t key_len, const char *val, size_t val_len);

static int
juise_network_write_file_chunk_mmap(const int fd, chunkqueue * const cq,
	   log_error_st * const errh);

static handler_t
mod_juise_connection_close (request_st *r, mod_juise_handler_context *hctx);

static int
mod_juise_demux_response (server *srv, mod_juise_handler_context *hctx);

static ssize_t network_write_data_len(int fd, const char *data, off_t len);

/* JUNOS Begin */
/**
 *
 * increase the internal buffer (if neccessary) to append another 'size' byte
 * ->used isn't changed
 *
 */

static int http_response_append_mem(request_st * const r, const char * const mem, uint32_t len) {
    if (r->resp_decode_chunked)
	 return http_chunk_decode_append_mem(r, mem, len);

    if (r->resp_body_scratchpad > 0) {
	 r->resp_body_scratchpad -= (off_t)len;
	 if (r->resp_body_scratchpad <= 0) {
		 r->resp_body_finished = 1;
		 if (__builtin_expect( (r->resp_body_scratchpad < 0), 0)) {
		 /*(silently truncate if data exceeds Content-Length)*/
			 len = (size_t)(r->resp_body_scratchpad + (off_t)len);
			 r->resp_body_scratchpad = 0;
		 }
	 }
    }
    else if (0 == r->resp_body_scratchpad) {
	 /*(silently truncate if data exceeds Content-Length)*/
	 return 0;
    }
    return http_chunk_append_mem(r, mem, len);
}

#define BUFFER_PIECE_SIZE 64
static int buffer_prepare_append(buffer *b, size_t size) {
	if (!b) return -1;

	if (0 == b->size) {
		b->size = size;

		/* always allocate a multiply of BUFFER_PIECE_SIZE */
		b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

		b->ptr = malloc(b->size);
		b->used = 0;
		force_assert(b->ptr);
	} else if (b->used + size > b->size) {
		b->size += size;

		/* always allocate a multiply of BUFFER_PIECE_SIZE */
		b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

		b->ptr = realloc(b->ptr, b->size);
		force_assert(b->ptr);
	}
	return 0;
}
/* JUNOS End */

static off_t
network_write_setjmp_write_cb (void *fd, const void *data, off_t len)
{
    return network_write_data_len((int)(uintptr_t)fd, data, len);
}

inline
static ssize_t network_write_data_len(int fd, const char *data, off_t len) {
  #ifdef _WIN32
    return send(fd, data, len, 0);
  #else
    return write(fd, data, len);
  #endif
}

__attribute_cold__
static int network_remove_finished_chunks(chunkqueue * const cq, const off_t len) {
    force_assert(len >= 0);
    chunkqueue_remove_finished_chunks(cq);
    return 0;
}

__attribute_cold__
static int network_write_error(int fd, log_error_st *errh) {
  #ifdef _WIN32
    switch (WSAGetLastError()) {
      case WSAEINTR:
      case WSAEWOULDBLOCK:
        return -3;
      case WSAECONNRESET:
      case WSAETIMEDOUT:
      case WSAECONNABORTED:
        return -2;
      default:
        log_serror(errh, __FILE__, __LINE__, "send() %d", fd);
        return -1;
    }
  #else
    switch (errno) {
      case EAGAIN:
     #ifdef EWOULDBLOCK
     #if EWOULDBLOCK != EAGAIN
      case EWOULDBLOCK:
     #endif
     #endif
      case EINTR:
        return -3;
      case EPIPE:
      case ECONNRESET:
        return -2;
      default:
        log_perror(errh, __FILE__, __LINE__, "write() %d", fd);
        return -1;
    }
  #endif
}

static data_unset *array_get_unused_element(array * const a, const data_type_t t) {
    /* After initial startup and config, most array usage is of homogeneous types
     * and arrays are cleared once per request, so check only the first unused
     * element to see if it can be reused */
  #if 1
    data_unset * const du = (a->used < a->size) ? a->data[a->used] : NULL;
    if (NULL != du && du->type == t) {
        a->data[a->used] = NULL;/* make empty slot at a->used for next insert */
        return du;
    }
    return NULL;
  #else
     data_unset ** const data = a->data;
     for (uint32_t i = a->used, sz = a->size; i < sz; ++i) {
          if (data[i] && data[i]->type == t) {
               data_unset * const ds = data[i];

               /* make empty slot at a->used for next insert */
               data[i] = data[a->used];
               data[a->used] = NULL;

               return ds;
          }
     }

     return NULL;
  #endif
}

static void mod_juise_merge_config_cpv(mod_juise_plugin_config * const pconf, const config_plugin_value_t * const cpv) {
    switch (cpv->k_id) { /* index into static config_plugin_keys_t cpk[] */
      case 0: /* juise.assign */
        pconf->cgi = cpv->v.a;
        break;
      case 1: /* juise.execute-x-only */
        pconf->execute_x_only = (unsigned short)cpv->v.u;
        break;
      case 2: /* juise.require-auth */
        pconf->require_auth = (unsigned short)cpv->v.u;
        break;
      case 3: /* juise.mixer */
        pconf->mixer = cpv->v.b;
        break;
      default:/* should not happen */
        return;
    }
}


static void mod_juise_merge_config(mod_juise_plugin_config * const pconf, const config_plugin_value_t *cpv) {
    do {
        mod_juise_merge_config_cpv(pconf, cpv);
    } while ((++cpv)->k_id != -1);
}

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
    log_error_st *errh = NULL;
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

    if (r->ptr)
	free(r->ptr);

    buffer_free(p->tmp_buf);
    buffer_free(p->parse_response);

    free(p);
}

static void
mod_juise_patch_config (request_st * const r, mod_juise_plugin_data * const p) {
    server *srv = r->con->srv;
    p->conf = p->defaults; /* copy small struct instead of memcpy() */
    /*memcpy(&p->conf, &p->defaults, sizeof(plugin_config));*/
    for (int i = 1, used = p->nconfig; i < used; ++i) {
	 if (config_check_cond(r, (uint32_t)p->cvlist[i].k_id))
		 mod_juise_merge_config(&p->conf, p->cvlist + p->cvlist[i].v.u2[0]);
    }
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

URIHANDLER_FUNC(mod_juise_handle_subrequest_start)
{
    size_t k, s_len;
    mod_juise_plugin_data *p = p_d;
    connection *con = r->con;
    server *srv = con->srv;
    buffer *fn = &r->physical.path;
    buffer *uri = &r->uri.path;
    stat_cache_entry *sce = NULL;

    log_error(srv->errh, __FILE__, __LINE__, "mod_juise: start: looking at %s", fn->ptr);

    if (NULL != r->handler_module)
	return HANDLER_GO_ON;

    if (fn->used == 0)
	return HANDLER_GO_ON;

    mod_juise_patch_config(r, p);

    sce = stat_cache_get_entry(&r->physical.path);
    if (NULL == sce) return HANDLER_GO_ON;

    if (!S_ISREG(sce->st.st_mode))
	return HANDLER_GO_ON;

    if (p->conf.execute_x_only == 1
		&& (sce->st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0)
	return HANDLER_GO_ON;

    s_len = fn->used - 1;

    for (k = 0; k < p->conf.cgi->used; k++) {
	data_string *ds = (data_string *) p->conf.cgi->data[k];
	size_t ct_len = ds->key.used - 1;

	if (ds->key.used == 0)
	    continue;
	if (s_len < ct_len)
	    continue;

	if (uri->ptr == strstr(uri->ptr, ds->key.ptr)) {
	    /* Prompt for authorization information if required by config */
	    if (p->conf.require_auth == 1
		&& http_header_request_get(r, HTTP_HEADER_AUTHORIZATION, CONST_STR_LEN("Authorization")) == NULL) {
		r->http_status = 401;
		r->handler_module = NULL;
		buffer_reset(p->tmp_buf);
		buffer_append_string_len(p->tmp_buf,
					 CONST_STR_LEN("Basic realm=\"Need "
						       "basic auth header\""));
          if(NULL != p->tmp_buf)
          http_header_response_set(r, HTTP_HEADER_WWW_AUTHENTICATE,
                                 CONST_STR_LEN("WWW-Authenticate"),
                                 BUF_PTR_LEN(p->tmp_buf));
		log_error(srv->errh, __FILE__, __LINE__, "Redirecting as mandatory authentication header "
		       "missing");
		return HANDLER_FINISHED;
	    }
	}
	if (ds->key.ptr[0] == '/') {
	    if (strncmp(uri->ptr, ds->key.ptr, ct_len) == 0) {
		if (mod_juise_create_env(r, p, &ds->value)) {
		    r->handler_module = NULL;
		    r->http_status = 500;

		    buffer_reset(&r->physical.path);
		    return HANDLER_FINISHED;
		}
		/* one handler is enough for the request */
		break;
	    }
	} else {
	    if (strncmp(fn->ptr + s_len - ct_len, ds->key.ptr, ct_len) == 0) {
		if (mod_juise_create_env(r, p, &ds->value)) {
		    r->handler_module = NULL;
		    r->http_status = 500;

		    buffer_reset(&r->physical.path);
		    return HANDLER_FINISHED;
		}
		/* one handler is enough for the request */
		break;
	    }
	}
    }

    return HANDLER_GO_ON;
}

static handler_t
mod_juise_handle_fdevent (void *ctx, int revents)
{
    mod_juise_handler_context *hctx = ctx;
    connection  *con  = hctx->remote_conn;
    request_st *r = &con->request;
    server *srv = con->srv;
    joblist_append(con);

    if (hctx->fd == -1) {
        log_error(srv->errh, __FILE__, __LINE__,"%d %d", con->fd, hctx->fd);
	   return HANDLER_ERROR;
    }

    if (revents & FDEVENT_IN) {
	switch (mod_juise_demux_response(srv, hctx)) {
	case FDEVENT_HANDLED_NOT_FINISHED:
	    break;

	case FDEVENT_HANDLED_FINISHED:
	    /* we are done */

#if 0
         log_error(srv->errh, __FILE__, __LINE__,"dd", con->fd, hctx->fd);
#endif
	    mod_juise_connection_close(r, hctx);

	    /*
	     * If we get a IN|HUP and have read everything don't exec
	     * the close twice
	     */
	    return HANDLER_FINISHED;

	case FDEVENT_HANDLED_ERROR:
	    /* Send an error if we haven't sent any data yet */
	    if (r->resp_body_started == 0) {
		request_set_state(r, CON_STATE_HANDLE_REQUEST);
		r->http_status = 500;
		r->handler_module = NULL;
	    } else {
		r->resp_body_started = 1;
	    }

	    log_error(srv->errh, __FILE__, __LINE__, "demuxer failed: ");
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
	if (r->resp_body_started == 0 && !buffer_string_is_empty(hctx->response_header)) {
	    r->resp_body_started = 1;
	    http_chunk_append_buffer(r, hctx->response_header);
	}

	if (r->resp_body_finished == 0) {
	    http_chunk_close(r);
	}

	r->resp_body_finished = 1;
	joblist_append(con);
#if 0
	LOGERR("sddd", "got HUP from cgi", con->fd, hctx->fd, revents);
#endif

	/* rtsigs didn't liked the close */
	mod_juise_connection_close(r, hctx);

    } else if (revents & FDEVENT_ERR) {
	r->resp_body_finished = 1;

	/* kill all connections to the cgi process */
	mod_juise_connection_close(r, hctx);
#if 1
	log_error(srv->errh, __FILE__, __LINE__, "juise-FDEVENT_ERR");
#endif
	return HANDLER_ERROR;
    }

    return HANDLER_FINISHED;
}

static int
mod_juise_create_env (request_st *r,
		  mod_juise_plugin_data *p, buffer *cgi_handler)
{
    pid_t pid;

#ifdef HAVE_IPV6
    char b2[INET6_ADDRSTRLEN + 1];
#endif

    server *srv = r->con->srv;
    connection *con = r->con;
    int to_cgi_fds[2];
    int from_cgi_fds[2];
    struct stat st;
    char **argv;
    int argc;
    int i = 0;
    char *sdup = NULL, *cp;
    char *http_auth = NULL, *user = NULL, *pass = NULL;
    const buffer * ds1;
    const data_string *ds;
    char cbuf[256];
    handler_t hr;

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

    if (!buffer_string_is_empty(p->conf.mixer)) {
	struct passwd *pwd = getpwuid(getuid());
	char userbuf[BUFSIZ];

	bzero(userbuf, sizeof(userbuf));
	if (pwd && pwd->pw_name) {
	    snprintf(userbuf, sizeof(userbuf), " --user %s --client", pwd->pw_name);
	}

	argv[i++] = strdup("--mixer");
	snprintf(cbuf, sizeof(cbuf), "%s%s", p->conf.mixer->ptr,
		userbuf[0] ? userbuf : "");
	argv[i++] = cbuf;
    }

    if (r->physical.path.used > 1)
	argv[i++] = r->physical.path.ptr;

    argv[i] = NULL;
    argc = i;

    for (i = 0; i < argc; i++) {
	log_error(srv->errh, __FILE__, __LINE__, "juise: argv: %s", argv[i]);
    }

    if (cgi_handler->used > 1) {
	/* stat the exec file */
	if (stat(argv[0], &st) < 0) {
	    log_error(srv->errh, __FILE__, __LINE__, "stat for cgi-handler %s", argv[0],
		   "failed: %d", strerror(errno));
	    if (argv)
		free(argv);
            if (sdup)
                free(sdup);
	    return -1;
	}
    }

    if (pipe(to_cgi_fds)) {
	log_error(srv->errh, __FILE__, __LINE__, "pipe failed:%d", strerror(errno));
	if (argv)
	    free(argv);
        if (sdup)
            free(sdup);
	return -1;
    }

    if (pipe(from_cgi_fds)) {
	close(to_cgi_fds[0]);
	close(to_cgi_fds[1]);
	log_error(srv->errh, __FILE__, __LINE__, "pipe failed:%d", strerror(errno));
	if (argv)
	    free(argv);
        if (sdup)
            free(sdup);
	return -1;
    }

    /* Get Authorization header */
    ds1 = http_header_request_get(r, HTTP_HEADER_AUTHORIZATION, CONST_STR_LEN("Authorization"));
    if (ds1) {
	char *auth_realm_decoded, *auth_realm = NULL;
	size_t dlen;

	http_auth = ds1->ptr;

	if (http_auth) {
	    auth_realm = strchr(http_auth, ' ');
	}

	if (auth_realm) {
	    int auth_type_len = auth_realm - http_auth;

	    if (auth_type_len == 5 
		&& strncasecmp(http_auth, "Basic", auth_type_len) == 0 
		&& auth_realm + 1) {
		size_t hash_len = strlen(auth_realm + 1);

		auth_realm_decoded = psu_base64_decode(auth_realm + 1,
						       hash_len, &dlen);
		if (!auth_realm_decoded) {
		    log_error(srv->errh, __FILE__, __LINE__, "Failed to decode auth header");
		    r->http_status = 400;
		    if (argv)
			free(argv);
                    if (sdup)
                        free(sdup);
                    close(from_cgi_fds[0]);
                    close(from_cgi_fds[1]);
                    close(to_cgi_fds[0]);
                    close(to_cgi_fds[1]);
		    return 0;
		}

		pass = strchr(auth_realm_decoded, ':');
		if (!pass) {
		    log_error(srv->errh, __FILE__, __LINE__, "Invalid authorization format");
		    r->http_status = 400;
		    free(auth_realm_decoded);
		    if (argv)
			free(argv);
                    if (sdup)
                        free(sdup);
                    close(from_cgi_fds[0]);
                    close(from_cgi_fds[1]);
                    close(to_cgi_fds[0]);
                    close(to_cgi_fds[1]);
		    return 0;
		}

		user = auth_realm_decoded;
		*pass = '\0';
		pass++;
	    } else {
		log_error(srv->errh, __FILE__, __LINE__, "Unrecognized authorization format");
		r->http_status = 400;
		if (argv)
		    free(argv);
                if (sdup)
                    free(sdup);
                close(from_cgi_fds[0]);
                close(from_cgi_fds[1]);
                close(to_cgi_fds[0]);
                close(to_cgi_fds[1]);
		return 0;
	    }
	}
    }

    /* fork, execve */
    switch (pid = fork()) {
    case 0:
	{
	    /* child */
	    char buf[LI_ITOSTRING_LENGTH] = {0};
	    size_t n;
	    char_array env;
	    char *c;
	    const char *s;
	    server_socket *srv_sock = con->srv_socket;
         const size_t tlen = buffer_clen(srv_sock->srv_token);

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

	    if (buffer_string_is_empty(r->conf.server_tag)) {
		mod_juise_env_add(&env, CONST_STR_LEN("SERVER_SOFTWARE"),
			      CONST_STR_LEN(PACKAGE_DESC));
	    } else {
		mod_juise_env_add(&env, CONST_STR_LEN("SERVER_SOFTWARE"),
			      CONST_BUF_LEN(r->conf.server_tag));
	    }

	    if (!buffer_string_is_empty(r->conf.server_name)) {
		size_t len = r->conf.server_name->used - 1;
		char *colon = strchr(r->conf.server_name->ptr, ':');
		if (colon)
		    len = colon - r->conf.server_name->ptr;

		mod_juise_env_add(&env, CONST_STR_LEN("SERVER_NAME"),
			      r->conf.server_name->ptr, len);
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

	    //s = http_version_buf(r->http_version);
         const buffer * const v = http_version_buf(r->http_version);

	    mod_juise_env_add(&env, CONST_STR_LEN("SERVER_PROTOCOL"),BUF_PTR_LEN(v));

	    size_t len = li_utostrn(buf,sizeof(buf),
#ifdef HAVE_IPV6
		      ntohs(srv_sock->addr.plain.sa_family == AF_INET6
			    ? srv_sock->addr.ipv6.sin6_port
			    : srv_sock->addr.ipv4.sin_port)
#else
		      ntohs(srv_sock->addr.ipv4.sin_port)
#endif
		      );
	    mod_juise_env_add(&env, CONST_STR_LEN("SERVER_PORT"), buf, len);

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

	    //s = get_http_method_name(con->request.http_method);
         const buffer * const m = http_method_buf(r->http_method);
         //s = http_method_buf(r->http_method);
	    mod_juise_env_add(&env, CONST_STR_LEN("REQUEST_METHOD"), BUF_PTR_LEN(m));

	    if (!buffer_is_empty(&r->pathinfo)) {
		mod_juise_env_add(&env, CONST_STR_LEN("PATH_INFO"),
			      CONST_BUF_LEN(&r->pathinfo));
	    }
	    mod_juise_env_add(&env, CONST_STR_LEN("REDIRECT_STATUS"),
			  CONST_STR_LEN("200"));
	    if (!buffer_string_is_empty(&r->uri.query)) {
		mod_juise_env_add(&env, CONST_STR_LEN("QUERY_STRING"),
			      CONST_BUF_LEN(&r->uri.query));
	    }
	    if (!buffer_string_is_empty(&r->target_orig)) {
		mod_juise_env_add(&env, CONST_STR_LEN("REQUEST_URI"),
			      CONST_BUF_LEN(&r->target_orig));
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

	    li_utostrn(buf,sizeof(buf),
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
         dsp = http_header_env_get(r, CONST_STR_LEN("REMOTE_USER"));
	    if (dsp != NULL && dsp->value.used > 1) {
		mod_juise_env_add(&env, CONST_STR_LEN("REMOTE_USER"),
				  CONST_BUF_LEN(&dsp->value));
	    }

#ifdef USE_OPENSSL
	    if (srv_sock->is_ssl) {
		mod_juise_env_add(&env, CONST_STR_LEN("HTTPS"), CONST_STR_LEN("on"));
	    }
#endif

	    /* request.content_length < SSIZE_MAX, see request.c */
	    li_utostrn(buf, sizeof(buf), r->reqbody_length);
	    mod_juise_env_add(&env, CONST_STR_LEN("CONTENT_LENGTH"), buf, strlen(buf));
	    mod_juise_env_add(&env, CONST_STR_LEN("SCRIPT_FILENAME"),
			  CONST_BUF_LEN(&r->physical.path));
	    mod_juise_env_add(&env, CONST_STR_LEN("SCRIPT_NAME"),
			  CONST_BUF_LEN(&r->uri.path));
	    mod_juise_env_add(&env, CONST_STR_LEN("DOCUMENT_ROOT"),
			  CONST_BUF_LEN(&r->physical.doc_root));

	    /* for valgrind */
	    if ((s = getenv("LD_PRELOAD")) != NULL) {
		mod_juise_env_add(&env, CONST_STR_LEN("LD_PRELOAD"), s, strlen(s));
	    }

	    /* needed for juise/mixer interaction on lock file */
	    if ((s = getenv("HOME")) != NULL) {
		mod_juise_env_add(&env, CONST_STR_LEN("HOME"), s, strlen(s));
	    }

	    if ((s = getenv("LD_LIBRARY_PATH")) != NULL) {
		mod_juise_env_add(&env, CONST_STR_LEN("LD_LIBRARY_PATH"), s, strlen(s));
	    }
	    if ((s = getenv("DYLD_LIBRARY_PATH")) != NULL) {
		mod_juise_env_add(&env, CONST_STR_LEN("DYLD_LIBRARY_PATH"), s, strlen(s));
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
			log_error(srv->errh, __FILE__, __LINE__, "juise: making env:%s %s", *sp, cp);
		    }
		}
	    }

	    for (n = 0; n < r->rqst_headers.used ; n++) {
		ds = (data_string *)r->rqst_headers.data[n];

		if (ds->value.used && ds->key.used) {
		    size_t j;

		    /* Don't set authorization header in env */
		    if (strcasecmp(ds->key.ptr, "AUTHORIZATION") == 0) {
			continue;
		    }

		    buffer_reset(p->tmp_buf);

		    if (strcasecmp(ds->key.ptr, "CONTENT-TYPE") != 0) {
			buffer_copy_string_len(p->tmp_buf, CONST_STR_LEN("HTTP_"));
			p->tmp_buf->used--; /* strip \0 after HTTP_ */
		    }

		    buffer_prepare_append(p->tmp_buf, ds->key.used + 2);

		    for (j = 0; j < ds->key.used - 1; j++) {
			char cr = '_';
			if (light_isalpha(ds->key.ptr[j])) {
			    /* upper-case */
			    cr = ds->key.ptr[j] & ~32;
			} else if (light_isdigit(ds->key.ptr[j])) {
			    /* copy */
			    cr = ds->key.ptr[j];
			}
			p->tmp_buf->ptr[p->tmp_buf->used++] = cr;
		    }
		    p->tmp_buf->ptr[p->tmp_buf->used++] = '\0';

		    mod_juise_env_add(&env, CONST_BUF_LEN(p->tmp_buf),
				  CONST_BUF_LEN(&ds->value));
		}
	    }

	    for (n = 0; n < r->env.used; n++) {
		ds = (data_string *)r->rqst_headers.data[n];

		if (ds->value.used && ds->key.used) {
		    size_t j;

		    buffer_reset(p->tmp_buf);

		    buffer_prepare_append(p->tmp_buf, ds->key.used + 2);

		    for (j = 0; j < ds->key.used - 1; j++) {
			char cr = '_';
			if (light_isalpha(ds->key.ptr[j])) {
			    /* upper-case */
			    cr = ds->key.ptr[j] & ~32;
			} else if (light_isdigit(ds->key.ptr[j])) {
			    /* copy */
			    cr = ds->key.ptr[j];
			}
			p->tmp_buf->ptr[p->tmp_buf->used++] = cr;
		    }
		    p->tmp_buf->ptr[p->tmp_buf->used++] = '\0';

		    mod_juise_env_add(&env, CONST_BUF_LEN(p->tmp_buf),
				  CONST_BUF_LEN(&ds->value));
		}
	    }

	    if (env.size == env.used) {
		env.size += 16;
		env.ptr = realloc(env.ptr, env.size * sizeof(*env.ptr));
	    }

	    env.ptr[env.used] = NULL;

	    /* search for the last / */
	    if ((c = strrchr(&r->physical.path.ptr, '/')) != NULL) {
		*c = '\0';

		/* change to the physical directory */
		if (chdir(&r->physical.path.ptr) == -1) {
		    log_error(srv->errh, __FILE__, __LINE__, "chdir failed: %s %s",
			   strerror(errno), &r->physical.path);
		}
		*c = '/';
	    }

	    /* we don't need the client socket */
	    for (i = 3; i < 256; i++) {
		if (i != srv->errh->fd)
		    close(i);
	    }

	    /* exec the cgi */
	    execve(argv[0], argv, env.ptr);

	    log_error(srv->errh, __FILE__, __LINE__, "CGI failed:%s %s", strerror(errno), argv[0]); 

	    /* */
	    SEGFAULT();
	    break;
	}

    case -1:
	/* error */
	log_error(srv->errh, __FILE__, __LINE__, "fork failed: %s", strerror(errno));
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
	    int ret1 = 0;
         xmlChar *ret;

	    if (p->conf.require_auth == 1 && http_auth) {
		buffer *auth = buffer_init();
		buffer_append_string_len(auth, CONST_STR_LEN("user="));
		ret = xmlURIEscapeStr((const xmlChar *)user, NULL);
		if (ret != NULL) {
		    buffer_append_string(auth, (char*)ret);
		}
		buffer_append_string_len(auth, CONST_STR_LEN("\npassword="));
		ret = xmlURIEscapeStr((const xmlChar *)pass, NULL);
		if (ret != NULL) {
		    buffer_append_string(auth, (char*)ret);
		}
		buffer_append_string_len(auth, CONST_STR_LEN("\n"));
		ret1 = write(to_cgi_fds[1], auth->ptr,
			  auth->used ? auth->used - 1 : 0);
		if (ret1 < 0) {
		    switch (errno) {
			case ENOSPC:
			    r->http_status = 507;
			    break;

			default:
			    r->http_status = 403;
			    break;
		    }
		}
		buffer_free(auth);

		if (pass) {
		    bzero(pass, strlen(pass));
		}
	    }

	    if (r->reqbody_length) {
		chunkqueue *cq = &r->reqbody_queue;
		chunk *c;
                /* JUNOS Begin */
                /*
                 * r->conf.max_request_size is in kBytes
                 */
	        log_error(srv->errh, __FILE__, __LINE__, "r->conf.max_request_size %d", r->conf.max_request_size);
                if (r->conf.max_request_size != 0 &&
                    (((off_t)r->reqbody_length >> 10 ) > r->conf.max_request_size)) {
		     log_error(r->conf.errh, __FILE__, __LINE__,
                                        "request-size too long: %d", (off_t) r->reqbody_length, "-> 413");
                     r->http_status = 413;
		     break;
                }
	        log_error(srv->errh, __FILE__, __LINE__, "chunkqueue_length(cq) %d", chunkqueue_length(cq));

                while (r->reqbody_queue.bytes_in
                       != (off_t) r->reqbody_length) {
                    con->is_readable = 1;
                    hr = r->con->reqbody_read(r);
                    if (hr == HANDLER_ERROR) {
			log_error(srv->errh, __FILE__, __LINE__, "connection_handle_read_post_state returned NULL");
		    }
                }

                /*
                 * Will add conditions to handle cases if chunkqueue length
                 * is returned empty or con->request.content_length is empty
                 * or -1 and other possible corner cases
                 */

	       log_error(srv->errh, __FILE__, __LINE__, "chunkqueue_length(cq) %d", chunkqueue_length(cq));
	        log_error(srv->errh, __FILE__, __LINE__, "(off_t)r->reqbody_length %d", (off_t)r->reqbody_length);
                /* JUNOS End */
		/* there is content to send */
		for (c = cq->first; c; c = cq->first) {

		    /* copy all chunks */
		    switch (c->type) {
		    case FILE_CHUNK:
			/* JUNOS Begin */
			/*
                         * Created a separate function to handle
			 * FILE_CHUNK as per new mod_cgi.c
			 */
			ret1 = juise_network_write_file_chunk_mmap( to_cgi_fds[1], cq,NULL);
			/* JUNOS End */
			break;

		    case MEM_CHUNK:
			ret1 = write(to_cgi_fds[1], c->mem->ptr + c->offset,
				  c->mem->used - c->offset - 1);
			if (ret1 < 0) {
			    switch (errno) {
			    case ENOSPC:
				r->http_status = 507;
				break;

			    case EINTR:
				continue;

			    default:
				r->http_status = 403;
				break;
			    }
			}
			break;
              }

		    if (ret1 > 0) {
			c->offset += ret1;
			cq->bytes_out += ret1;
		    } else {
			log_error(srv->errh, __FILE__, __LINE__, "write() failed due to: %s",
			       strerror(errno));
			r->http_status = 500;
			break;
		    }
		    chunkqueue_remove_finished_chunks(cq);
		}
	    }

	    close(to_cgi_fds[1]);

	    /* register PID and wait for them asyncronously */
	    r->handler_module = p->self;
	    buffer_reset(&r->physical.path);

	    hctx = mod_juise_handler_ctx_init();

	    hctx->remote_conn = con;
	    hctx->plugin_data = p;
	    hctx->pid = pid;
	    hctx->fd = from_cgi_fds[0];
         hctx->fdn = NULL;

	    r->plugin_ctx[p->id] = hctx;

	    hctx->fdn = fdevent_register(srv->ev, hctx->fd, mod_juise_handle_fdevent, hctx);
	    fdevent_fdnode_event_set(srv->ev, hctx->fdn, FDEVENT_IN);

	    if (fdevent_fcntl_set_nb(hctx->fd) == -1) {
		log_error(srv->errh, __FILE__, __LINE__, "fcntl failed: %s", strerror(errno));

		fdevent_fdnode_event_del(srv->ev, hctx->fdn);
		fdevent_unregister(srv->ev, hctx->fdn);

		log_error(srv->errh, __FILE__, __LINE__,"juise close: %d", hctx->fd);

		close(hctx->fd);

		mod_juise_handler_ctx_free(hctx);

		r->plugin_ctx[p->id] = NULL;

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


URIHANDLER_FUNC(mod_juise_handle_physical)
{
    size_t k, s_len;
    mod_juise_plugin_data *p = p_d;
    connection *con = r->con;
    server *srv = con->srv;
    buffer *fn = &r->physical.path;
    buffer *uri = &r->uri.path;

    log_error(srv->errh, __FILE__, __LINE__, "mod_juise: physical: fn %s", fn->ptr);
    log_error(srv->errh, __FILE__, __LINE__, "mod_juise: physical: uri %s", uri->ptr);

    s_len = buffer_clen(uri);

    mod_juise_patch_config(r, p);

    for (k = 0; k < p->conf.cgi->used; k++) {
	data_string *ds = (data_string *) p->conf.cgi->data[k];
	size_t ct_len = ds->key.used - 1;

	if (ds->key.used == 0)
	    continue;
	if (s_len < ct_len)
	    continue;

	if (ds->key.ptr[0] == '/') {
	    if (strncmp(uri->ptr, ds->key.ptr, ct_len) == 0) {
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

   if (errno == ECHILD) {
	    log_error(srv->errh, __FILE__, __LINE__, "cgi child vanished");

	    mod_juise_pid_del(srv, p, p->cgi_pid.ptr[ndx]);
	    ndx--;
	    continue;
    }

	    log_error(srv->errh, __FILE__, __LINE__,"waitpid failed: ", strerror(errno));

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
		    log_error(srv->errh, __FILE__, __LINE__, "cleaning up CGI: process died with signal %d",
			   WTERMSIG(status));
		}
	    } else {
		log_error(srv->errh, __FILE__, __LINE__, "cleaning up CGI: ended unexpectedly");
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

SUBREQUEST_FUNC(mod_juise_handle_subrequest)
{
    mod_juise_plugin_data *p = p_d;
    mod_juise_handler_context *hctx = r->plugin_ctx[p->id];
    server *srv = r->con->srv;

    if (r->handler_module != p->self) return HANDLER_GO_ON;
    if (NULL == hctx) return HANDLER_GO_ON;

    log_error(srv->errh, __FILE__, __LINE__,
              "mod_juise: handle: %s", r->physical.path.ptr);

#define JUISE_CLEANUP() \
    do { \
        if (hctx->fdn) { \
            fdevent_fdnode_event_del(srv->ev, hctx->fdn); \
            fdevent_unregister(srv->ev, hctx->fdn); \
            hctx->fdn = NULL; \
        } \
        if (hctx->fd != -1) { \
            if (close(hctx->fd)) \
                log_error(srv->errh,__FILE__,__LINE__, \
                          "juise close failed %d %s", hctx->fd, strerror(errno)); \
            hctx->fd = -1; \
        } \
        mod_juise_handler_ctx_free(hctx); \
        r->plugin_ctx[p->id] = NULL; \
        r->handler_module = NULL; \
    } while (0)

    /* If child already marked dead */
    if (hctx->pid == 0) {
	 if (!r->resp_body_started) {
		 if (r->http_status == 0) r->http_status = 500;
		 JUISE_CLEANUP();
		 return HANDLER_FINISHED;
	 }
	 if (r->resp_body_finished) {
		 JUISE_CLEANUP();
		 return HANDLER_FINISHED;
	 }
	 return HANDLER_WAIT_FOR_EVENT;
    }

#ifndef __WIN32
    for (;;) {
        int status;
        pid_t pr = waitpid(hctx->pid, &status, WNOHANG);
        if (pr == 0) {
            /* Child running */
            if (r->resp_body_finished) {
                hctx->pid = 0;
                JUISE_CLEANUP();
                return HANDLER_FINISHED;
            }
            return HANDLER_WAIT_FOR_EVENT;
        }
        if (pr == -1) {
            if (errno == EINTR) continue;
            if (errno == ECHILD || errno == ESRCH) {
                /* Already reaped elsewhere */
                hctx->pid = 0;
                if (!r->resp_body_started) {
                    if (r->http_status == 0) r->http_status = 500;
                    JUISE_CLEANUP();
                    return HANDLER_FINISHED;
                }
                if (r->resp_body_finished) {
                    JUISE_CLEANUP();
                    return HANDLER_FINISHED;
                }
                return HANDLER_WAIT_FOR_EVENT;
            }
            log_error(srv->errh,__FILE__,__LINE__,
                      "waitpid failed: %s (pid=%d)",
                      strerror(errno),(int)hctx->pid);
            hctx->pid = 0;
            if (!r->resp_body_started && r->http_status == 0)
                r->http_status = 500;
            if (!r->resp_body_started || r->resp_body_finished) {
                JUISE_CLEANUP();
                return HANDLER_FINISHED;
            }
            return HANDLER_WAIT_FOR_EVENT;
        }

        /* pr == pid: child exited */
        hctx->pid = 0;

        if (!r->resp_body_started) {
            if (r->http_status == 0) r->http_status = 500;
            JUISE_CLEANUP();
            return HANDLER_FINISHED;
        }

        if (r->resp_body_finished) {
            JUISE_CLEANUP();
            return HANDLER_FINISHED;
        }

        /* Headers started; wait for EOF from demux */
        return HANDLER_WAIT_FOR_EVENT;
    }
#else
    return HANDLER_ERROR;
#endif
}

SETDEFAULTS_FUNC(mod_juise_set_defaults)
{
    static const config_plugin_keys_t cpk[] = {
      { CONST_STR_LEN("juise.assign"),
        T_CONFIG_ARRAY_KVSTRING,
        T_CONFIG_SCOPE_CONNECTION }
     ,{ CONST_STR_LEN("juise.execute-x-only"),
        T_CONFIG_BOOL,
        T_CONFIG_SCOPE_CONNECTION }
     ,{ CONST_STR_LEN("juise.require-auth"),
        T_CONFIG_BOOL,
        T_CONFIG_SCOPE_CONNECTION }
     ,{ CONST_STR_LEN("juise.mixer"),
        T_CONFIG_STRING,
        T_CONFIG_SCOPE_CONNECTION }
     ,{ NULL, 0,
        T_CONFIG_UNSET,
        T_CONFIG_SCOPE_UNSET }
    };

    mod_juise_plugin_data * const p = p_d;
    if (!config_plugin_values_init(srv, p, cpk, "mod_juise"))
	 return HANDLER_ERROR;

    /* process and validate config directives
     * (init i to 0 if global context; to 1 to skip empty global context) */
    for (int i = !p->cvlist[0].v.u2[1]; i < p->nconfig; ++i) {
	 config_plugin_value_t *cpv = p->cvlist + p->cvlist[i].v.u2[0];
	 for (; -1 != cpv->k_id; ++cpv) {
		 /* (nothing to do here for simple config) */
	 }
    }

    /* initialize p->defaults from global config context */
    if (p->nconfig > 0 && p->cvlist->v.u2[1]) {
	 const config_plugin_value_t *cpv = p->cvlist + p->cvlist->v.u2[0];
	 if (-1 != cpv->k_id)
		 mod_juise_merge_config(&p->defaults, cpv);
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
mod_juise_response_parse (request_st *r, mod_juise_plugin_data *p, buffer *in)
{
    char *ns;
    const char *s;
    int line = 0;

    buffer_copy_buffer(p->parse_response, in);

    for (s = p->parse_response->ptr; (ns = strchr(s, '\n')); s = ns + 1, line++) {
	   const char *key = NULL, *value = NULL;
	   int key_len;

	   ns[0] = '\0';
	   if (ns > s && ns[-1] == '\r') ns[-1] = '\0';

	   if (line == 0 && strncmp(s, "HTTP/1.", 7) == 0) {
		   if ((s[7] == '1' || s[7] == '0') && s[8] == ' ') {
		   int status = strtol(s+9, NULL, 10);
		   if (status >= 100 && status < 1000)
			 r->http_status = status;
		   }
	   } else {
		  key = s;
		  value = strchr(s, ':');
		  if (!value) continue;
		  key_len = (int)(value - key);
		  value++;
		  while (*value == ' ' || *value == '\t') value++;

		  const enum http_header_e id = http_header_hkey_get(key, key_len);
		  size_t vlen = strlen(value);
		  switch (id) {
		  case HTTP_HEADER_STATUS:
			 r->http_status = strtol(value, NULL, 10);
			 break;
		  case HTTP_HEADER_CONNECTION:
			 r->keep_alive = (0 == strcasecmp(value, "keep-alive"));
			 break;
		  case HTTP_HEADER_CONTENT_LENGTH:
			 if (!r->resp_decode_chunked) {
				const char *e = value + vlen;
				while (e > value && (e[-1] == ' ' || e[-1] == '\t')) --e;
				if (e > value)
				    r->resp_body_scratchpad =
					   (off_t)li_restricted_strtoint64(value,(uint32_t)(e-value), &e);
			 }
			 break;
		  case HTTP_HEADER_TRANSFER_ENCODING:
			 if (buffer_eq_icase_ss(value, vlen, CONST_STR_LEN("chunked"))) {
				r->resp_decode_chunked = 1;
				r->gw_dechunk = ck_calloc(1, sizeof(response_dechunk));
			 } else {
				r->http_status = 502;
				return 0;
			 }
			 break;
		  default:
			 break;
		  }
		  if (vlen)
			 http_header_response_insert(r, id, key, (uint32_t)key_len, value, vlen);
	   }
    }

    /* If Location present without Status, set 302 */
    if (http_header_response_get(r, HTTP_HEADER_LOCATION,
			   CONST_STR_LEN("Location"))
&& r->http_status == 0)
	   r->http_status = 302;

    return 0;
}

static int
mod_juise_demux_response (server *srv, mod_juise_handler_context *hctx)
{
    mod_juise_plugin_data *p = hctx->plugin_data;
    connection *con = hctx->remote_conn;          /* ensure remote_conn is set */
    if (con == NULL) return FDEVENT_HANDLED_ERROR; /* defensive */
    request_st *r = &con->request;                /* lighttpd >=1.4 request */

    for (;;) {
	   int n;
	   buffer_string_prepare_copy(hctx->response, 2048);
	   n = read(hctx->fd, hctx->response->ptr, hctx->response->size - 1);
	   if (n == -1) {
		  if (errno == EAGAIN || errno == EINTR)
			 return FDEVENT_HANDLED_NOT_FINISHED;
		  log_error(srv->errh, __FILE__, __LINE__, "read error: %s", strerror(errno));
		  return FDEVENT_HANDLED_ERROR;
	   }
	   if (n == 0) {
		  r->resp_body_finished = 1;
		  http_chunk_close(r);
		  joblist_append(r->con);
		  return FDEVENT_HANDLED_FINISHED;
	   }

	   buffer_commit(hctx->response, n);

	   if (!r->resp_body_started) {
		  buffer_append_string_buffer(hctx->response_header, hctx->response);
		  size_t header_len = buffer_string_length(hctx->response_header);
		  int is_header = 0, is_header_end = 0;
		  size_t last_eol = 0;
		  size_t i;

		  if (strncmp(hctx->response_header->ptr, "HTTP/1.", 7) == 0)
			 is_header = 1;

		  for (i = 0; !is_header_end && i < header_len; i++) {
			 char c = hctx->response_header->ptr[i];
			 switch (c) {
			 case '<':
				is_header_end = 1;
				break;
			 case ':':
				is_header = 1;
				break;
			 case '\n':
				if (!is_header) { is_header_end = 1; break; }
				if (last_eol > 0 &&
				    ((i - last_eol == 1) ||
					(i - last_eol == 2 &&
					 hctx->response_header->ptr[i - 1] == '\r'))) {
				    is_header_end = 1;
				    break;
				}
				last_eol = i;
				break;
			 }
		  }

		  if (is_header_end) {
			 if (!is_header) {
				http_chunk_append_buffer(r, hctx->response_header);
			    joblist_append(r->con);
			 } else {
				size_t body_start = i ? i - 1 : i;
				if (body_start > 0 && hctx->response_header->ptr[body_start] == '\r')
				    --body_start;
				size_t body_off = i;
				const char *bstart = hctx->response_header->ptr + body_off;
				size_t blen = header_len - body_off;

				buffer_string_set_length(hctx->response_header, body_start);
				mod_juise_response_parse(r, p, hctx->response_header);
				if (blen > 0) {
				    http_chunk_append_mem(r, bstart, blen);
				   joblist_append(r->con);
				}
			 }
			 r->resp_body_started = 1;
		  }
	   } else {
		  http_chunk_append_buffer(r, hctx->response);
		  joblist_append(r->con);
	   }

	   if (n < (int)(hctx->response->size - 1))
		  break; /* drained current readiness */
    }

    return FDEVENT_HANDLED_NOT_FINISHED;
}

static handler_t
mod_juise_connection_close (request_st *r, mod_juise_handler_context *hctx)
{
    int status;
    pid_t pid;
    mod_juise_plugin_data *p;
    connection  *con;
    server *srv;

    if (hctx == NULL)
	return HANDLER_GO_ON;

    p = hctx->plugin_data;
    con = hctx->remote_conn;
    srv = con->srv;

    log_error(srv->errh, __FILE__, __LINE__, __FUNCTION__);

    if (r->handler_module != p->self)
	return HANDLER_GO_ON;

#ifndef __WIN32

    /* the connection to the browser went away, but we still have a connection
     * to the CGI script
     *
     * close cgi-connection
     */

    if (hctx->fd != -1) {
	/* close connection to the cgi-script */
	if (hctx->fdn) {
	    fdevent_fdnode_event_del(srv->ev, hctx->fdn);
	    fdevent_unregister(srv->ev, hctx->fdn);
	}

	if (close(hctx->fd)) {
	    log_error(srv->errh, __FILE__, __LINE__, "juise close failed %d %s",
			    hctx->fd, strerror(errno));
	}

	hctx->fd = -1;
     hctx->fdn = NULL;
    }

    pid = hctx->pid;
    r->plugin_ctx[p->id] = NULL;

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

	    log_error(srv->errh, __FILE__, __LINE__, "waitpid failed: %s", strerror(errno));
	    return HANDLER_ERROR;

	default:
	    /* Send an error if we haven't sent any data yet */
	    if (r->resp_body_started == 0) {
		request_set_state(r, CON_STATE_HANDLE_REQUEST);
		r->http_status = 500;
		r->handler_module = NULL;
	    } else {
		r->resp_body_started = 1;
	    }

	    if (WIFEXITED(status)) {
#if 0
		LOGERR("sd", "(debug) cgi exited fine, pid:", pid);
#endif
		return HANDLER_GO_ON;
	    } else {
		log_error(srv->errh, __FILE__, __LINE__, "juise died, pid:%d %d", pid, status);
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
mod_juise_request_reset(request_st *r, void *p_d)
{
    handler_t rc;
    mod_juise_plugin_data *p = p_d;
    connection *con = r->con;
    server *srv = con->srv;

    log_error(srv->errh, __FILE__, __LINE__, "mod_juise: request_reset: %s", r->physical.path.ptr);

    rc = mod_juise_connection_close(r, r->plugin_ctx[p->id]);
    log_error(srv->errh, __FILE__, __LINE__, "mod_juise: request_reset: done");

    return rc;
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

/* JUNOS_Begin */
/*(improved from network_write_mmap.c)*/
static off_t
mmap_align_offset (off_t start)
{
    static off_t pagemask = 0;

    if (pagemask == 0) {
	   long pagesize = sysconf(_SC_PAGESIZE);

	   if (pagesize == -1) {
		  pagesize = 4096;
	   }

	   /* pagesize always power-of-2 */
	   pagemask = ~((off_t)pagesize - 1);
    }

    return (start & pagemask);
}

static int juise_network_write_accounting(const int fd, chunkqueue * const cq,log_error_st * const errh, const ssize_t wr, const off_t toSend) { 
    if (wr >= 0) {
	   const int rc = (wr == toSend) ? 0 : -3;
	   chunkqueue_mark_written(cq, wr);
	   return rc;
    }
    else
	   return network_write_error(fd, errh);
}
//static off_t network_write_setjmp_write_cb (void *fd, const void *data, off_t len);
/* next chunk must be FILE_CHUNK. send mmap()ed file with write() */
static int juise_network_write_file_chunk_mmap(const int fd, chunkqueue * const cq, log_error_st * const errh) {
    chunk * const restrict c = cq->first;
    const chunk_file_view * const restrict cfv = (!c->file.is_temp)
      ? chunkqueue_chunk_file_view(cq->first, 0, errh)/*use default 512k block*/
      : NULL;
    //if (NULL == cfv)
    //    return network_write_file_chunk_no_mmap(fd, cq, p_max_bytes, errh);

    off_t toSend = c->file.length - c->offset;
    if (toSend <= 0) return network_remove_finished_chunks(cq, toSend);

    const off_t mmap_avail = chunk_file_view_dlen(cfv, c->offset);
    const char * const data = chunk_file_view_dptr(cfv, c->offset);
    if (toSend > mmap_avail) toSend = mmap_avail;
    off_t wr = sys_setjmp_eval3(network_write_setjmp_write_cb,
                                (void *)(uintptr_t)fd, data, toSend);
    return juise_network_write_accounting(fd,cq,NULL,(ssize_t)wr,toSend);
}
/* JUNOS End */
int mod_juise_plugin_init(plugin *p);
int
mod_juise_plugin_init (plugin *p)
{
    p->version = LIGHTTPD_VERSION_ID;
    p->name = "juise";

    p->handle_request_reset = mod_juise_request_reset;
    p->handle_subrequest_start = mod_juise_handle_subrequest_start;
    p->handle_subrequest = mod_juise_handle_subrequest;
#if 0
    p->handle_fdevent = mod_juise_handle_fdevent;
#endif
    /* JUNOS Begin */
    p->handle_trigger = mod_juise_handle_trigger;
    /* JUNOS End */
    p->init = mod_juise_init;
    p->cleanup = mod_juise_cleanup;
    p->set_defaults = mod_juise_set_defaults;
    p->handle_physical = mod_juise_handle_physical;

    p->data = NULL;

    return 0;
}
