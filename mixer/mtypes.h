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

#define MPT_ARGS_CREATE \
    mixer_peer_t *mpp UNUSED

#define MPT_ARGS_DESTROY \
    mixer_peer_t *mpp UNUSED

#define MPT_ARGS_PRE_SELECT \
    mixer_peer_t *mpp UNUSED, struct pollfd *pollp UNUSED

#define MPT_ARGS_POST_SELECT \
    mixer_peer_t *mpp UNUSED, struct pollfd *pollp UNUSED

typedef int (*mpt_func_create)(MPT_ARGS_CREATE);
typedef int (*mpt_func_destroy)(MPT_ARGS_DESTROY);
typedef int (*mpt_func_pre_select)(MPT_ARGS_PRE_SELECT);
typedef int (*mpt_func_post_select)(MPT_ARGS_POST_SELECT);

typedef struct mixer_peer_type_s {
    const char *mpt_name;	/* Name of this type */

    /* Functions */
    mpt_func_create mpt_create;
    mpt_func_destroy mpt_destroy;
    mpt_func_pre_select mpt_pre_select;
    mpt_func_post_select mpt_post_select;
} mixer_peer_type_t;

#define NUM_MIXER_PEER_TYPES 10
extern mixer_peer_type_t mixer_peer_type_table[NUM_MIXER_PEER_TYPES];

static inline
int mixer_peer_type_create (MPT_ARGS_CREATE)
{
    return mixer_peer_type_table[mpp->mp_type].mpt_create(mpp);
}

static inline
int mixer_peer_type_destroy (MPT_ARGS_DESTROY)
{
    return mixer_peer_type_table[mpp->mp_type].mpt_destroy(mpp);
}

static inline
int mixer_peer_type_pre_select (MPT_ARGS_PRE_SELECT)
{
    return mixer_peer_type_table[mpp->mp_type].mpt_pre_select(mpp, pollp);
}

static inline
int mixer_peer_type_post_select (MPT_ARGS_POST_SELECT)
{
    return mixer_peer_type_table[mpp->mp_type].mpt_post_select(mpp, pollp);
}
