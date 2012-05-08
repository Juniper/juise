/*
 * $Id$
 *
 * Copyright (c) 2012, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef JUISE_MIXER_H
#define JUISE_MIXER_H

/* Callback function prototype */
#if 0
typedef int (*mixer_read_cb_t)(struct mixer_channel_s *, void *); 
#endif

typedef struct mixer_buffer_s {
    char *mb_buf;		/* Memory for this buffer */
    unsigned mb_cur;		/* Offset to current content */
    unsigned mb_size;		/* Size of the buffer */
    unsigned mb_len;		/* Length of active data */
} mixer_buffer_t;

/*
 * The base peer type is extended as needed
 */
typedef struct mixer_peer_s {
    TAILQ_ENTRY(mixer_peer_s) mp_link; /* Next peer */
    int mp_type;                /* Type of this peer (MPT_*) */
    int mp_state;    		/* State of this peer (MPS_*) */
    struct mixer_peer_s *mp_assoc; /* Associated peer */
    int mp_socket;                /* Underlaying client socket */

    mixer_buffer_t mp_input;    /* Buffered input data */
    mixer_buffer_t mp_output;   /* Buffered output data */

    void *mp_data;		  /* Opaque data for callback */
} mixer_peer_t;

typedef TAILQ_HEAD(mixer_peer_list_s, mixer_peer_s) mixer_peer_list_t;

/* Peer types */
#define MPT_UNKNOWN	0	/* Undefined */
#define MPT_LISTEN	1	/* Listening for new requesters */
#define MPT_REQUESTER	2	/* Client requester */
#define MPT_CHANNEL	3	/* SSH channel */
#define MPT_SESSION	4	/* SSH session (which contains channels) */

/* States */
#define MPS_UNKNOWN	0	/* Initial default */
#define MPS_NORMAL	1	/* Normal active state */
#define MPS_BLKREAD	2	/* Blocked on read */
#define MPS_BLKWRITE	3	/* Blocked on write */

typedef struct mixer_channel_s {
    mixer_peer_t mc_mp;	/* Base peer fields */
    TAILQ_ENTRY(mixer_channel_s) mc_session_link; /* Next channel in session */

    LIBSSH2_CHANNEL *mc_channel;  /* SSH channel */

} mixer_channel_t;

typedef TAILQ_HEAD(mixer_channel_list_s, mixer_channel_s) mixer_channel_list_t;

typedef struct mixer_session_s {
    mixer_peer_t ms_mp;	/* Base peer fields */
    char *ms_name;                /* Name (user@host) of this session */

    LIBSSH2_SESSION *ms_session;
    mixer_channel_list_t ms_channels; /* List of channels for this session */
} mixer_session_t;

typedef TAILQ_HEAD(mixer_session_list_s, mixer_session_s) mixer_session_list_t;

/*
 * A requester represents the client side of the mixer, where clients
 * both ask for channels and then send and receive data with that channel.
 */
typedef struct mixer_requester_s {
    mixer_peer_t mr_mp;	/* Base peer fields */
} mixer_requester_t;

/*
 * The listening socket that listens for new requesters.
 */
typedef struct mixer_listen_s {
    mixer_peer_t ml_mp;	/* Base peer fields */
} mixer_listen_t;

void *mixer_peer_create (unsigned type, size_t size);
void mixer_peer_destroy (mixer_peer_t *mp);

int mixer_init_afunix (const char *sockpath);
void mixer_init_websocket (void);
void mixer_init_session (void);
void mixer_init_channel (void);

int mixer_listen_afunix (const char *path);


#endif /* JUISE_MIXER_H */
