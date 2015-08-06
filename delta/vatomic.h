/*
 * $Id$
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef VATOMIC_H
#define VATOMIC_H

#ifdef __macosx__
#include <libkern/OSAtomic.h>
#define HAVE_ATOMIC_STUFF

OSAtomicAdd32

OSAtomicCompareAndSwap64Barrier

static inline uint64_t
vatomic_fetch64 (uint64_t *ptr)
{
    
}

static inline uint32_t
vatomic_fetch_add32 (uint32_t *ptr, uint32_t inc)
{
    return OSAtomicAdd32Barrier(inc, ptr);
}

static inline int
vatomic_compare_and_swap64 (uint64_t *ptr, uint64_t cur, uint64_t next)
{
}


#endif /* __macosx__ */

#ifndef HAVE_ATOMIC_STUFF

static inline uint64_t
vatomic_fetch64 (uint64_t *ptr)
{
}

static inline uint32_t
vatomic_fetch_add32 (uint32_t *ptr)
{
}

static inline int
vatomic_compare_and_swap64 (uint64_t *ptr, uint64_t cur, uint64_t next)
{
}

#endif /* !HAVE_ATOMIC_STUFF */

#endif /* VATOMIC_H */
