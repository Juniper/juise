/*
 * $Id: jsio.h 413295 2010-12-01 06:39:06Z rsankar $
 *
 * Copyright (c) 2006-2008, Juniper Networks, Inc.
 * All rights reserved.
 *
 * Session-based interface to JUNOScript process
 */

#ifndef _JSIO_H_
#define _JSIO_H_

#include <libjuise/data/patricia.h>
#include <libjuise/io/fbuf.h>

/*
 * We support both junoscript and netconf sessions
 */
typedef enum session_type_s {
    ST_DEFAULT = 0,
    ST_JUNOSCRIPT,
    ST_NETCONF,
    ST_JUNOS_NETCONF,
    ST_MAX,
} session_type_t;

#define DEFAULT_NETCONF_PORT    830

#define JS_READ_TIMEOUT		120 /* seconds of silence before breaking */
#define JS_READ_QUICK		100 /* Microseconds of open delay */

typedef struct js_skey_s {
    session_type_t jss_type;	/* Session type (junoscript or netconf ?) */
    char jss_name[0];		/* Session name (remote host name or NULL) */
} js_skey_t;

typedef struct js_session_s {
    patnode js_node;		/* Node inside parent patricia tree */
    struct js_session_s *js_next; /* Next in linked list */
    int js_pid;			/* Child pid */
    int js_stdin;		/* Child's stdin (socket) */
    int js_stdout;		/* Child's stdout (socket) */
    int js_stderr;		/* Child's stderr (socket) */
    int js_askpassfd;		/* -oAskPassFd to communicate with ssh */
    FILE *js_fpout;		/* File pointer for writing to child */
    fbuf_t *js_fbuf;		/* Input buffer for stdin */
    char *js_creds;		/* Header of xml doc/JUNOScript credentials */
    unsigned js_state;		/* State of the reader */
    unsigned js_len;		/* How many bytes (of various things)? */
    xmlNodePtr js_hello;	/* hello packet recvd from netconf server */
    unsigned js_msgid;		/* netconf message id */
    js_boolean_t js_isjunos;	/* True when the device is junos */
    int js_off;			/* Token offset (reset, leader, or trailer) */
    char *js_rbuf;		/* Read buffer */
    int js_roff;		/* Read buffer offset */
    int js_rlen;		/* Read buffer length */
    char *js_passphrase;	/* Passphrase */

    /* NOTICE: js_key _MUST_ _BE_ _LAST_ */
    js_skey_t js_key;		/* js_session key (MUST BE LAST) */
} js_session_t;

#define SESSION_NAME_DELTA	\
	    (offsetof(struct js_session_s, js_key) \
		- (offsetof(struct js_session_s, js_node) + \
		   sizeof(((struct js_session_s *) NULL)->js_node)))

#define JSS_INIT	0	/* Just starting */
#define JSS_HEADER	1	/* Emitting creds */
#define JSS_NORMAL	2	/* Normal */
#define JSS_TRAILER	3	/* Emitting close rpc tag */
#define JSS_CLOSE	4	/* Ready for close */
#define JSS_DEAD	5	/* Bad news all around */
#define JSS_DISCARD	6	/* Discard xml declaration */

/*
 * Opens a JUNOScript session for the give host_name, username, passphrase
 */
js_session_t *
js_session_open (const char *host_name, const char *username, 
		 const char *passphrase, int flags,
		 uint port, session_type_t stype);


/*
 * Execute the give RPC in the given host_name's JUNOScript session.
 */
lx_nodeset_t *
js_session_execute (xmlXPathParserContext *ctxt, const char *host_name, 
		    lx_node_t *rpc_node, const xmlChar *rpc_name, 
		    session_type_t stype);

/*
 * Close the given host's given session.
 */
void js_session_close (const char *remote_name, session_type_t stype);
void js_session_close1 (js_session_t *jsp);

/*
 * Return the hello packet of the given session
 */
xmlNodePtr
js_gethello (const char *host_name, session_type_t stype);

/* Flags for js_session_open (and js_flags) */
#define JSF_AS_ROOT     (1<<0)  /* Run the command as root (commit scripts) */
#define JSF_FBUF_TRACE  (1<<1)  /* Trace fbuf content */
#define JSF_AS_USER     (1<<2)  /* Run the command as user */
#define JSF_JUNOS_NETCONF (1<<3) /* Open Junos proprietary netconf over SSHV1 */
/* Trace flags */
#define CS_TRC_ALL      0x100   /* Turn on all trace bits */
#define CS_TRC_XSLT     0x200   /* Trace XSLT operations */
#define CS_TRC_INPUT    0x300   /* Trace input document */
#define CS_TRC_OUTPUT   0x400   /* Trace output documents */
#define CS_TRC_EVENTS   0x500   /* Trace major events */
#define CS_TRC_DEBUG    0x600   /* Trace debug events */
#define CS_TRC_OFFLINE  0x700   /* Log input in file for offline use */
#define CS_TRC_ARGUMENTS 0x800  /* Trace our arguments */
#define CS_TRC_RPC      0x900   /* Trace any RPCs */
#define CS_TRC_RUSAGE   0xa00   /* Trace resource usage */

extern trace_file_t *trace_file;

#define CS_ELT_FILE     "file"
#define CS_ELT_NAME     "name"
#define CS_ELT_OPTIONAL "optional"
#define CS_ELT_STRICT   "strict"
#define CS_ELT_DESCRIPTION      "description"
#define CS_ELT_OP_SCRIPT        "op"
#define CS_ELT_COMMAND  "command"
#define CS_ELT_MD5_CHECKSUM "checksum/md5"
#define CS_ELT_SHA1_CHECKSUM "checksum/sha1"
#define CS_ELT_SHA256_CHECKSUM "checksum/sha-256"

/* XPath expressions */
#define SELECT_ERRORS_OR_WARNINGS "xnm:error | xnm:warning"
#define SELECT_COMMIT_SCRIPTS "configuration/system/scripts/commit \
          | configuration/system/undocumented/scripts/commit"
#define SELECT_ARGUMENTS "xsl:variable[@name='arguments']/argument"
#define SELECT_EVENT_DEFINITION "xsl:variable[@name='event-definition']/event-options"
#define SELECT_JUNOS_CAPABILITY "capabilities/capability[. = 'http://xml.juniper.net/netconf/junos/1.0']"

#define JCS_FULL_NS "http://xml.juniper.net/junos/commit-scripts/1.0"

/* Element names */
#define CS_ELT_CHANGE   "change"
#define CS_ELT_CLI      "cli"
#define CS_ELT_ERROR    "error"

#define CS_ELT_INPUT        "commit-script-input"
#define CS_ELT_EVENT_INPUT  "event-script-input"
#define CS_ELT_OP_INPUT     "op-script-input"

#define CS_ELT_MESSAGE  "message"
#define CS_ELT_OUTPUT   "configuration"
#define CS_ELT_PROGRESS "progress-indicator"
#define CS_ELT_RESULTS  "commit-script-results"
#define CS_ELT_OP_RESULTS "op-script-results"
#define CS_ELT_EVENT_RESULTS "event-script-results"
#define CS_ELT_COMPLETIONS "op-script-completions"
#define CS_ELT_SLAXIFY_RESULTS "slaxify-results"
#define CS_ELT_UNSLAXIFY_RESULTS "unslaxify-results"
#define CS_ELT_OUTPUT_FILENAME "output-filename"

#define CS_ELT_SUCCESS  "success"
#define CS_ELT_SYSLOG   "syslog"
#define CS_ELT_TRANSIENT "transient-change"
#define CS_ELT_WARNING  "warning"

/*
 * Input type (i.e what type of input it is ? whether the input is normal
 * input or secret input)
 */
typedef enum csi_type_s {
    CSI_NORMAL_INPUT = 0,
    CSI_SECRET_INPUT,
} csi_type_t;

/*
 * Set the default server to the one provided
 */
void
jsio_set_default_server (const char *server);

void
jsio_set_default_user (const char *user);

void
jsio_set_ssh_options (const char *opts);

void
jsio_init (unsigned flags);

void
jsio_cleanup (void);
void
jsio_restart (void);

js_session_t *
js_session_open_server (int fdin, int fdout, session_type_t stype, int flags);

int
js_session_init (js_session_t *jsp);

int
js_session_init_netconf (js_session_t *jsp);

lx_document_t *
js_rpc_get_request (js_session_t *jsp);

const char *
js_rpc_get_name (lx_document_t *rpc);

void
js_rpc_free (lx_document_t *rpc);

/*
 * Kill the child process associated with this session.
 */
void
js_session_terminate (js_session_t *jsp);

const char *jsio_session_type_name(session_type_t stype);
session_type_t jsio_session_type(const char *name);
session_type_t jsio_set_default_session_type (session_type_t stype);

#endif /* _JSIO_H_ */
