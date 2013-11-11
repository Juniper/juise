/*
 * $Id$
 *
 * Copyright (c) 2012-2013, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

/* Types of sockets (for ms_type) */
#define MST_NONE	0	/* None/unknown */
#define MST_LISTENER	1	/* Listen for incoming connections */
#define MST_FORWARDER	2	/* Forward data to/from an ssh channel */
#define MST_SESSION	3	/* An ssh session */
#define MST_CONSOLE	4	/* Debug console shell */
#define MST_WEBSOCKET	5	/* Websocket client (in a browser) */

#define MST_MAX		5	/* max(MST_*) */

/* State values (for ms_state) */
#define MSS_NORMAL	0	/* Normal/okay/ignore */
#define MSS_FAILED	1	/* Failed; needs to be closed */
#define MSS_ERROR	2	/* Errors that complete RPC but aren't fatal */
#define MSS_INPUT	3	/* Needs to read input */
#define MSS_OUTPUT	4	/* Needs to write output */
#define MSS_HOSTKEY	5	/* Asking about hostkey */
#define MSS_PASSPHRASE	6	/* Asking about passphrase */
#define MSS_PASSWORD	7	/* Asking about password */
#define MSS_ESTABLISHED	8	/* Connection is established */
#define MSS_RPC_INITIAL	9	/* Ready for a fresh RPC */
#define MSS_RPC_IDLE	10	/* RPC in progress, but doing nothing */
#define MSS_RPC_READ_RPC 11	/* Reading <rpc> from client (websock) */
#define MSS_RPC_WRITE_RPC 12	/* Writing <rpc> to server (JUNOS) */
#define MSS_RPC_READ_REPLY 13	/* Reading <rpc-reply from server */
#define MSS_RPC_WRITE_REPLY 14	/* Writing <rpc-reply> to client (ws) */
#define MSS_RPC_COMPLETE 15	/* Reply is complete (end-of-frame seen) */

#define DEFINE_BIT_FUNCTIONS(_test, _set, _clear, _type, _field, _bit)	\
    static inline unsigned _test (_type *ptr) { \
        return ((ptr->_field & _bit) != 0); \
    } \
    static inline void _set (_type *ptr) { \
	ptr->_field |= _bit;		       \
    } \
    static inline void _clear (_type *ptr) { \
	ptr->_field &= ~(_bit);	     \
    }

typedef short poll_event_t;
typedef unsigned mx_type_t;	/* Type for MST_* */

struct mx_sock_s;		    /* Forward declarations */
struct mx_sock_forwarder_s;
struct mx_sock_session_s;

typedef TAILQ_ENTRY(mx_sock_s) mx_sock_link_t;
typedef TAILQ_HEAD(mx_sock_list_s, mx_sock_s) mx_sock_list_t;

typedef unsigned long mx_offset_t; /* Offset into a buffer */
typedef int mx_boolean_t; /* Simple boolean (TRUE/FALSE) */

typedef struct mx_buffer_s {
    struct mx_buffer_s *mb_next; /* Next buffer */
    mx_offset_t mb_start;	/* Offset of first data byte */
    mx_offset_t mb_len;	/* Number of bytes in use */
    mx_offset_t mb_size;	/* Number of bytes in the buffer */
    char mb_data[0];
} mx_buffer_t;

struct mx_channel_s;		    /* Forward declaration */
typedef TAILQ_ENTRY(mx_channel_s) mx_channel_link_t;
typedef TAILQ_HEAD(mx_channel_list_s, mx_channel_s) mx_channel_list_t;

typedef struct mx_channel_s {
    mx_channel_link_t mc_link;	/* List of channels */
    unsigned mc_id;		/* Identifier for this channel */
    unsigned mc_state;		/* Current state (MSS_*) */
    unsigned mc_flags;		/* MCF_* */
    mx_offset_t mc_marker_seen;	/* Number of bytes of end-of-frame seen */
    struct mx_sock_session_s *mc_session; /* Session for this channel */
    LIBSSH2_CHANNEL *mc_channel; /* Our libssh2 channel */
    struct mx_request_s *mc_request;	 /* Current request (in progress) */
    struct mx_sock_s *mc_client; /* Our client (peer) socket */
    mx_buffer_t *mc_rbufp;	/* Read buffer */
} mx_channel_t;

#define MCF_HOLD_CHANNEL	(1<<0) /* Hold the channel after rpc complete */
#define MCF_SEEN_EOFRAME	(1<<1) /* Have seen the end-of-frame marker */

DEFINE_BIT_FUNCTIONS(mcf_is_hold_channel, mcf_set_hold_channel,
		     mcf_clear_hold_channel, mx_channel_t,
		     mc_flags, MCF_HOLD_CHANNEL);

DEFINE_BIT_FUNCTIONS(mcf_is_seen_eoframe, mcf_set_seen_eoframe,
		     mcf_clear_seen_eoframe, mx_channel_t,
		     mc_flags, MCF_SEEN_EOFRAME);

struct mx_request_s;
typedef TAILQ_ENTRY(mx_request_s) mx_request_link_t;
typedef TAILQ_HEAD(mx_request_list_s, mx_request_s) mx_request_list_t;

typedef unsigned long mx_muxid_t;

/*
 * An request is an active request from a WebSocket client.  We need
 * to cache information during the request to allow us to properly send
 * the results to the client.  In addition, we need to handle the user
 * interaction needed during ssh session establishment for prompting
 * the user re: known-hosts and passwords.
 */
typedef struct mx_request_s {
    mx_request_link_t mr_link;
    unsigned mr_id;		/* Request ID (our ID) */
    unsigned mr_state;		/* State of this request */
    mx_muxid_t mr_muxid;	/* Muxer ID (client's ID) */
    char *mr_name;		/* Request name (tag) */
    char *mr_target;		/* Target name (could be alias) */
    char *mr_fulltarget;        /* Full target name (user@host:port) */
    char *mr_hostname;          /* Target hostname */
    unsigned mr_port;           /* Remote target port */
    char *mr_user;		/* User/login name */
    char *mr_password;		/* Password (if needed) */
    char *mr_passphrase;	/* Passphrase (if needed) */
    char *mr_hostkey;		/* Response from hostkey question */
    char *mr_desthost;		/* Destination host */
    unsigned mr_destport;	/* Destination port */
    struct mx_sock_s *mr_client; /* Our client websocket */
    struct mx_sock_session_s *mr_session; /* Our SSH session */
    struct mx_channel_s *mr_channel; /* Our SSH channel */
    mx_buffer_t *mr_rpc;	     /* The RPC we're attempting */
} mx_request_t;

typedef struct mx_sock_s {
    mx_sock_link_t ms_link;	/* List of all open sockets */
    unsigned ms_id;		/* Socket identifier */
    mx_type_t ms_type;		/* MST_* type */
    unsigned ms_sock;		/* Underlaying sock */
    unsigned ms_state;		/* Type-specific state */
    struct sockaddr_in ms_sin;	/* Address of peer */
} mx_sock_t;

typedef struct mx_sock_listener_s {
    mx_sock_t msl_base;		/* Base sock info */
    unsigned msl_port;		/* Port being listened on */
    mx_type_t msl_spawns;	/* Type of socket spawned */
    mx_request_t *msl_request;	/* Target information */
} mx_sock_listener_t;

typedef struct mx_sock_forwarder_s {
    mx_sock_t msf_base;
    struct mx_sock_session_s *msf_session; /* Master session */
    mx_channel_t *msf_channel;	   /* Our channel */
    mx_buffer_t *msf_rbufp;	   /* Read buffer */
} mx_sock_forwarder_t;

typedef struct mx_sock_session_s {
    mx_sock_t mss_base;
    char *mss_target;		  /* Remote host name (target) */
    char *mss_canonname;	  /* Canonical name (from getaddrinfo) */
    LIBSSH2_SESSION *mss_session; /* libssh2 info */
    mx_channel_list_t mss_channels; /* Set of channels */
    mx_channel_list_t mss_released; /* Set of channels free to use */
    int mss_pwfail;		    /* Number of password failures */
    int mss_keepalive_next;	    /* Number of seconds til next keepalive */
} mx_sock_session_t;

typedef struct mx_sock_websocket_s {
    mx_sock_t msw_base;
    mx_buffer_t *msw_rbufp;	   /* Read buffer */
} mx_sock_websocket_t;

typedef struct mx_password_s {
    struct mx_password_s *mp_next; /* Linked list */
    char *mp_target;		   /* Key: Target hostname */
    char *mp_user;		   /* Key: Username */
    char *mp_password;		   /* Saved password */
    time_t mp_laststamp;	   /* Last time this password was used */
} mx_password_t;

mx_sock_list_t mx_sock_list;	/* List of all sockets */
int mx_sock_count;		/* Number of mx_sock_t in mx_sock_list */

#define MX_TYPE_PRINT_ARGS \
    mx_sock_t *msp UNUSED, int indent UNUSED, const char *prefix UNUSED
typedef void (*mx_type_print_func_t)(MX_TYPE_PRINT_ARGS);

#define MX_TYPE_PREP_ARGS \
    mx_sock_t *msp UNUSED, struct pollfd *pollp UNUSED, int *timeout UNUSED
typedef int (*mx_type_prep_func_t)(MX_TYPE_PREP_ARGS);

#define MX_TYPE_POLLER_ARGS \
    mx_sock_t *msp UNUSED, struct pollfd *pollp UNUSED
typedef int (*mx_type_poller_func_t)(MX_TYPE_POLLER_ARGS);

#define MX_TYPE_SPAWN_ARGS \
    mx_sock_listener_t *mslp UNUSED, \
    int sock UNUSED, struct sockaddr_in *sin UNUSED, socklen_t sinlen UNUSED
typedef mx_sock_t *(*mx_type_spawn_func_t)(MX_TYPE_SPAWN_ARGS);

#define MX_TYPE_WRITE_ARGS \
    mx_sock_t *msp UNUSED,  mx_channel_t *mcp UNUSED, mx_buffer_t *mbp UNUSED
typedef int (*mx_type_write_func_t)(MX_TYPE_WRITE_ARGS);

#define MX_TYPE_WRITE_COMPLETE_ARGS \
    mx_sock_t *msp UNUSED, mx_channel_t *mcp UNUSED
typedef int (*mx_type_write_complete_func_t)(MX_TYPE_WRITE_COMPLETE_ARGS);

#define MX_TYPE_SET_CHANNEL_ARGS \
    mx_sock_t *msp UNUSED, mx_sock_session_t *mssp UNUSED, \
	mx_channel_t *mcp UNUSED
typedef void (*mx_type_set_channel_func_t)(MX_TYPE_SET_CHANNEL_ARGS);

#define MX_TYPE_CLOSE_ARGS \
    mx_sock_t *msp UNUSED
typedef void (*mx_type_close_func_t)(MX_TYPE_CLOSE_ARGS);

#define MX_TYPE_CHECK_HOSTKEY_ARGS \
    mx_sock_t *client UNUSED, mx_request_t *mrp UNUSED, const char *info UNUSED
typedef int (*mx_type_check_hostkey_func_t)(MX_TYPE_CHECK_HOSTKEY_ARGS);

#define MX_TYPE_GET_PASSPHRASE_ARGS \
    mx_sock_t *client UNUSED, mx_request_t *mrp UNUSED, const char *info UNUSED
typedef int (*mx_type_get_passphrase_func_t)(MX_TYPE_GET_PASSPHRASE_ARGS);

#define MX_TYPE_GET_PASSWORD_ARGS \
    mx_sock_t *client UNUSED, mx_request_t *mrp UNUSED, const char *info UNUSED
typedef int (*mx_type_get_password_func_t)(MX_TYPE_GET_PASSWORD_ARGS);

#define MX_TYPE_IS_BUF_ARGS \
    mx_sock_t *msp UNUSED, short flags UNUSED
typedef int (*mx_type_is_buf_func_t)(MX_TYPE_IS_BUF_ARGS);

#define MX_TYPE_ERROR_ARGS \
    mx_sock_t *msp UNUSED, mx_request_t *mrp UNUSED, const char *message UNUSED
typedef void (*mx_type_error_func_t)(MX_TYPE_ERROR_ARGS);

typedef struct mx_type_info_s {
    mx_type_t mti_type;		/* MST_* */
    const char *mti_name;	/* Printable name */
    const char *mti_letter;     /* One letter name */
    mx_type_print_func_t mti_print; /* Print function */
    mx_type_prep_func_t mti_prep; /* Prepare for poll() data */
    mx_type_poller_func_t mti_poller; /* Process poll() data */
    mx_type_spawn_func_t mti_spawn; /* Spawn a new sock from a listener */
    mx_type_write_func_t mti_write; /* Write a buffer of data */
    mx_type_write_complete_func_t mti_write_complete; /* Finish a write */
    mx_type_set_channel_func_t mti_set_channel; /* Set session/channel */
    mx_type_close_func_t mti_close; /* Close the socket */
    mx_type_check_hostkey_func_t mti_check_hostkey; /* Check the hostkey */
    mx_type_get_passphrase_func_t mti_get_passphrase; /* Get a passphrase */
    mx_type_get_password_func_t mti_get_password; /* Get a password */
    mx_type_is_buf_func_t mti_is_buf; /* Has buffered i/o */
    mx_type_error_func_t mti_error; /* Report error to client */
} mx_type_info_t;

extern mx_type_info_t mx_type_info[MST_MAX + 1];

static inline mx_type_info_t *
mx_mti_number (mx_type_t type)
{
    static mx_type_info_t mti_null = { mti_name: "null/zero/nada" };

    if (type > MST_MAX)
	return &mti_null;

    return &mx_type_info[type];
}

static inline mx_type_info_t *
mx_mti (mx_sock_t *msp)
{
    return mx_mti_number(msp->ms_type);
}

#define MX_TYPE_INFO_VERSION 1
void
mx_type_info_register (int version UNUSED, mx_type_info_t *mtip);

const char *
mx_sock_sin (mx_sock_t *msp);

const char *
mx_sock_type_number (mx_type_t type);

int
mx_sock_isreadable (int sock);

const char *
mx_sock_type (mx_sock_t *msp);

const char *
mx_sock_letter (mx_sock_t *msp);

const char *
mx_sock_title  (mx_sock_t *msp);

void
mx_close_byname (const char *name);

static inline void *
mx_sock (mx_sock_t *msp, mx_type_t type)
{
    assert(msp);
    assert(msp->ms_type == type);

    return msp;
}
