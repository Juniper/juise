/*
 * $Id$
 *
 * Copyright (c) 2000-2006, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef __JNX_DBGPR_H__
#define __JNX_DBGPR_H__

#include <libjuise/common/bits.h>
#include <libjuise/io/trace.h>

/*
 * DEBUGIF(): a simple macro to make debug code go away without littering
 * code with #ifdef's.
 */
#if defined(DEBUG)
#define DEBUGIF(c, s) do { if (c) s; } while (0)
void debug_init(const char *file);
void dbgpr(const char *fmt, ...);
void dbgprv(const char *fmt, va_list vap);
void dbgpr_set_trace_file (trace_file_t *tp);
#else
#define DEBUGIF(c, s) do { } while (0)
#define debug_init(x) do { } while (0)
#define dbgpr_set_trace_file(tp) do { } while (0)
#define dbgpr(fmt...) do { } while (0)
#define dbgprv(fmt, vap) do { } while (0)
#endif

extern flag64_t ddl_debug;		/* Debug flags */

static inline void
debug_toggle (const flag64_t bit, const int on)
{
    if (on) ddl_debug |= bit;
    else ddl_debug &= ~bit;
}

#endif /* __JNX_DBGPR_H__ */

