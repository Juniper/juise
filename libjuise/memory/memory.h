/*
 * $Id: memory.h 561086 2013-01-17 19:11:21Z ib-builder $
 *
 * Copyright (c) 1998-2007, Juniper Network, Inc.
 *
 * Original Author: Mark Moraes <moraes@csri.toronto.edu>
 *
 * Copyright University of Toronto 1988, 1989, 1993.
 * Written by Mark Moraes
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author and the University of Toronto are not responsible
 * for the consequences of use of this software, no matter how awful,
 * even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 * explicit claim or by omission. Since few users ever read sources,
 * credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 * misrepresented as being the original software. Since few users
 * ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 *
 */

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>

#define DBM_VERSION_SIZE	256 /* Size of version string we embed */

/* Values for dm_endian */
#define DBM_BIG_ENDIAN		0xBE /* Flag for big endian machines */
#define DBM_LITTLE_ENDIAN	0x1E /* Flag for little endian machines */

#ifdef __powerpc__
#define DBM_MAX_ADDRESS	0x70000000 /* Avoid hitting the stack */
#define DBM_SCHEMA_ADDR	0x6EA00000
#define DBM_COMPAT_ADDR	0x60000000

#define DBM_MAX_ADDRESS_MX80    0x9F000000 /* Avoid hitting the stack */
#define DBM_SCHEMA_ADDR_MX80    0x9DA00000
#define DBM_COMPAT_ADDR_MX80    0x8F000000

#elif __mips__                     /* Running mgd on SPU/XLR/SRX  */
#ifdef _MIPS_ARCH_OCTEON
#define DBM_MAX_ADDRESS 0x3F000000 /* Avoid hitting the stack */
#define DBM_SCHEMA_ADDR 0x3DA00000
#define DBM_COMPAT_ADDR 0x30000000
#else
#define DBM_MAX_ADDRESS 0x70000000 /* Avoid hitting the stack */
#define DBM_SCHEMA_ADDR 0x6EA00000
#define DBM_COMPAT_ADDR 0x60000000
#endif
#elif __arm__
#define DBM_MAX_ADDRESS 0x70000000
#define DBM_SCHEMA_ADDR 0x6EA00000
#define DBM_COMPAT_ADDR 0x60000000
#elif defined(__amd64__)
/*
 * These were computed by hand, subtracting MAXSSIZ from USRSTACK, and
 * using the same sizes for each region as on the i386.
 */
#define DBM_MAX_ADDRESS 0x00007fffe1fdf000L
#define DBM_SCHEMA_ADDR 0x00007fffdefdf000L
#define DBM_COMPAT_ADDR 0x00007fffb43e1000L
#else
#define DBM_MAX_ADDRESS 0xBBBFE000 /* Avoid hitting the stack */
#define DBM_SCHEMA_ADDR 0xBA3FE000
#define DBM_COMPAT_ADDR 0x90000000
#endif

#ifdef __i386__
/*
 * When running for i386 the JUNOS kernel may be
 * either 32 or 64-bit.  If it is a 64-bit kernel
 * we are running in a 32-bit compatibility mode
 * which makes an additional 1GB of memory available
 * so we adjust our addresses to take advantage.
 */
#define DBM_MAX_ADDRESS_32 (DBM_MAX_ADDRESS + (1 << 30))
#define DBM_SCHEMA_ADDR_32 (DBM_SCHEMA_ADDR + (1 << 30))
#define DBM_COMPAT_ADDR_32 (DBM_COMPAT_ADDR + (1 << 30))
#endif

typedef uint32_t dbm_offset_t;
#define DBM_OFFSET_NULL		((dbm_offset_t) ~0)

/*
 * dbm_memory_t: The core structure for the dbm memory system. This
 * structure lives at the start of the shared memory segment and
 * controls all the allocation and freeing of memory. We also add
 * some control fields for versioning and such.
 */

#define DBM_PAGE_SHIFT		12
#define DBM_PAGE_SIZE		(1<<DBM_PAGE_SHIFT)
#define DBM_PAGE_MASK		(DBM_PAGE_SIZE - 1)


#define DBM_MEMORY_PAGE_COUNT	(1 << (30 - DBM_PAGE_SHIFT))	/* 1GB */
#define DBM_POW2_FRAG_COUNT	(DBM_PAGE_SHIFT - 3)		/* PAGE_SIZE / 4 */
#define DBM_MAX_CONTIG_PAGES	(4 << (20 - DBM_PAGE_SHIFT))	/* 4MB */
#define DBM_SLOT_MIN_SIZE	16	/* must be >= sizeof(struct dbm_free_slot_s) */

typedef u_int32_t dbm_mask_t;

enum dbm_known_frag_e {
    DBM_KNOWN_FRAG_DDL_OBJECT,
    DBM_KNOWN_FRAG_DDL_OBJECT_PLUS4,
    DBM_KNOWN_FRAG_DDL_OBJECT_PLUS8,
    DBM_KNOWN_FRAG_COUNT,
};

typedef struct dbm_free_page_s {
    dbm_offset_t dfp_prev, dfp_next;
} dbm_free_page_t;

typedef struct dbm_page_header_s {
    dbm_mask_t dph_usage[DBM_PAGE_SIZE / DBM_SLOT_MIN_SIZE / sizeof(dbm_mask_t) / NBBY];
} dbm_page_header_t;

typedef struct dbm_free_slot_s {
    dbm_offset_t dfs_prev, dfs_next;
} dbm_free_slot_t;

typedef struct dbm_memory_s {
    u_int8_t dm_endian;	/* Tell us if we have the right endian-ness */
    u_int8_t dm_major;	/* Major version number */
    u_int16_t dm_minor;	/* Minor version number */
    u_int32_t dm_size;	/* Size in bytes of this memory segment */
    u_int32_t dm_top;	/* Top of allocated memory */
    u_int32_t dm_header;	/* Size of the upper layer's header */
    char dm_version[ DBM_VERSION_SIZE ]; /* Version string of MGD */
    u_int32_t dm_flags;	/* Flags (DMF_*) */

    struct {
	int32_t tv_sec;
	int32_t tv_usec;
    } dm_changed;			/* Time when last change occurred */

    /* malloc.c related members */
    dbm_offset_t dm_free_pages[DBM_MAX_CONTIG_PAGES];

    u_int32_t dm_size_info[DBM_MEMORY_PAGE_COUNT];	/* size information per page */

    dbm_offset_t dm_pow2_frags[DBM_POW2_FRAG_COUNT];	/* 4, 8, ... DBM_PAGE_SIZE / 4 */

    dbm_offset_t dm_known_frags[DBM_KNOWN_FRAG_COUNT];
} dbm_memory_t;

/* Flags for dm_flags: */
#define DMF_DEAD	(1<<0)	/* Database is dead (client should refresh) */
#define DMF_READONLY	(1<<1)	/* Readonly access */
#define DMF_CHANGED	(1<<2)	/* Database has been changed */
#define DMF_NOMEM	(1<<3)	/* Size limit exceeded */

#define DMF_AUTOROLLBACK (1<<4)	/* Database rollback automatically at close */
#define DMF_ALL_CHANGED (1<<5)	/* Consider all objects changed */
#define DMF_PENDING_CHANGE (1<<6) /* Expecting changes (load etc) */
#define DMF_PRIVATE     (1<<7)	/* private database */

#define DMF_JUNOS_DEFAULTS_NEEDED (1<<8) /* Need to load junos-defaults */
#define DMF_EXPAND_GROUPS (1<<9) /* Need to expand groups */
#define DMF_TRANSIENTS	(1<<10)	/* Last committed database had transients */
#define DMF_LOADING	(1<<11)	/* Loading lots of changes */
#define DMF_IGNORE_PRIVATE  (1<<12)	/* Ignore private users */
#define DMF_COMMIT_CHECK (1<<13) /* db is accessed for only commit-check */
#define DMF_COND_GROUPS (1<<14) /* Setting change bits while commit internal*/ 
#define DMF_BATCH	(1<<15) /* Batch mode */

#define DMF_BIT_TEST(fn, bit)	\
static inline ddl_boolean_t	\
fn (dbm_memory_t *dbmp) {	\
    if (dbmp) return (dbmp->dm_flags & bit); \
    return FALSE;		\
}

#define DMF_BIT_SET(fn, bit)	\
static inline void		\
fn (dbm_memory_t *dbmp) {	\
    if (dbmp) dbmp->dm_flags |= bit; \
}

#define DMF_BIT_CLEAR(fn, bit)	\
static inline void		\
fn (dbm_memory_t *dbmp) {	\
    if (dbmp) dbmp->dm_flags &= ~bit; \
}

#define DMF_BIT_FUNCS(name, bit)	\
  DMF_BIT_TEST(dmf_is_ ## name, bit)	\
  DMF_BIT_SET(dmf_set_ ## name, bit)	\
  DMF_BIT_CLEAR(dmf_clear_ ## name, bit)

/* some of these get used a lot */
DMF_BIT_FUNCS(dead, DMF_DEAD);
DMF_BIT_FUNCS(readonly, DMF_READONLY);
DMF_BIT_FUNCS(changed, DMF_CHANGED);

DMF_BIT_FUNCS(nomem, DMF_NOMEM);
DMF_BIT_FUNCS(autorollback, DMF_AUTOROLLBACK);
DMF_BIT_FUNCS(all_changed, DMF_ALL_CHANGED);
DMF_BIT_FUNCS(pending_change, DMF_PENDING_CHANGE);
DMF_BIT_FUNCS(private, DMF_PRIVATE);

DMF_BIT_FUNCS(junos_defaults_needed, DMF_JUNOS_DEFAULTS_NEEDED);
DMF_BIT_FUNCS(expand_groups, DMF_EXPAND_GROUPS);
DMF_BIT_FUNCS(transients, DMF_TRANSIENTS);
DMF_BIT_FUNCS(commit_check, DMF_COMMIT_CHECK);
DMF_BIT_FUNCS(cond_groups, DMF_COND_GROUPS);

void *dbm_mmap (dbm_memory_t *dbmp, size_t nbytes);

extern void (*malloc_error_func)(const char *fmt, ... );
const char * dbm_filename(dbm_memory_t *dbmp);
int dbm_fd(dbm_memory_t *dbmp);
void *dbm_header(dbm_memory_t *dbmp);
size_t dbm_max_size (dbm_memory_t *dbmp);
int dbm_is_readonly (dbm_memory_t *dbmp);

extern caddr_t dbm_max_address(void);
extern caddr_t dbm_schema_address(void);
extern caddr_t dbm_compat_address(void);

static inline dbm_offset_t
dbm_offset (dbm_memory_t *dbmp, void *ptr)
{
    if (ptr == NULL)
	return DBM_OFFSET_NULL;

    dbm_offset_t off = (caddr_t) ptr - (caddr_t) dbmp;
    if (off >= dbmp->dm_size)
	return DBM_OFFSET_NULL;

    return off;
}

static inline void *
dbm_pointer (dbm_memory_t *dbmp, dbm_offset_t off)
{
    if (off == DBM_OFFSET_NULL || off >= dbmp->dm_size)
	return NULL;

    caddr_t cp = (caddr_t) dbmp;
    cp += off;

    return cp;
}

#endif /* __GLOBALS_H__ */ /* Do not add anything after this line */
