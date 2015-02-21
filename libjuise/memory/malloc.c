/*
 * Copyright (c) 2008, Juniper Networks, Inc.
 * All rights reserved.
 *
 * Simple power of two allocator.
 *
 *
 * The allocated memory is split up in pages (DBM_PAGE_SIZE).
 *
 *
 * Memory chunks that are > DBM_PAGE_SIZE / 4 occupy at least
 * one page, or possibly more. No partial pages are allocated.
 *
 * The pool of free pages is maintained in dbmp->dm_free_pages,
 * indexed by the number of contiguous pages available.
 *
 *
 * Memory chunks that are <= DBM_PAGE_SIZE / 4 share a page
 * that is split up into equally sized chunks allocated to the next
 * power of two (function get_frag_idx).
 *
 * E.g. 16 -> 16
 *      17 -> 32
 *      31 -> 32
 *
 * Each of those pages containing equally sized chunks has
 * a bitmap at the very beginning "dbm_page_header_t", which is used
 * to keep track of when the page can be returned back to the
 * page pool.
 *
 * A linked list of free chunks sharing pages is stored in dbmp->dm_pow2_frags.
 *
 *
 * Memory chunks that are
 * len >= sizeof(ddl_object_t) && len <= sizeof(ddl_object_t) + 8
 * live in pages that are split up into the corresponding
 * word-aligned size.
 *
 * A linked list of free chunks of this particular size sharing pages
 * is stored in dbmp->dm_known_frags.
 *
 */

#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <string.h>

#if 0
#include <machine/cpufunc.h>
#endif

#include "juiseconfig.h"
#include <libjuise/common/aux_types.h>
#include <libjuise/memory/memory.h>
#include <libjuise/memory/dmalloc.h>

#if defined(__i386__) || defined(__amd64__)
static inline u_int32_t
bsrl32 (u_int32_t mask)
{
    u_int32_t   result;

    __asm __volatile("bsrl %1,%0" : "=r" (result) : "rm" (mask));

    return result;
}
#endif /* defined(__i386__) || defined(__amd64__) */

/*
 * get_frag_idx
 *
 * returns the fragmentation index (log2(size))
 *
 */
static inline u_int32_t
get_frag_idx (u_int32_t size)
{
    u_int32_t mask, idx;

    if (size > DBM_SLOT_MIN_SIZE)
	mask = size - 1;
    else
	mask = DBM_SLOT_MIN_SIZE - 1;

#if defined(__i386__) || defined(__amd64__)
    idx = bsrl32(mask) + 1;
#else
    for (idx = 0; mask; idx++)
	mask >>= 1;
#endif

    return idx > 2 ? idx - 2 : 0;
}

/*
 * get_frag_offset_p
 *
 * retrieve fragmentation information
 *
 */
static inline dbm_offset_t *
get_frag_offset_p (dbm_memory_t *dbmp, u_int32_t size, u_int32_t *recorded_size_p)
{
    u_int32_t frag_idx;
    dbm_offset_t *frag_offset_p;
    u_int32_t recorded_size;

#ifdef DO_NOT_SWEAT_DDL_OBJECT_SIZE
    if (size >= sizeof(ddl_object_t) &&
	size <=  sizeof(ddl_object_t) + 8) {
	if (size > sizeof(ddl_object_t) + 4) {
	    frag_offset_p = dbmp->dm_known_frags + DBM_KNOWN_FRAG_DDL_OBJECT_PLUS8;
	    recorded_size = sizeof(ddl_object_t) + 8;
	} else if (size > sizeof(ddl_object_t)) {
	    frag_offset_p = dbmp->dm_known_frags + DBM_KNOWN_FRAG_DDL_OBJECT_PLUS4;
	    recorded_size = sizeof(ddl_object_t) + 4;
	} else {
	    frag_offset_p = dbmp->dm_known_frags + DBM_KNOWN_FRAG_DDL_OBJECT;
	    recorded_size = sizeof(ddl_object_t);
	}

	/* return this recorded_size if it is not a power of two */
	if (recorded_size & (recorded_size - 1)) {
	    if (recorded_size_p)
		*recorded_size_p = recorded_size;

	    return frag_offset_p;
	}
    }
#endif /* DO_NOT_SWEAT_DDL_OBJECT_SIZE */

    frag_idx = get_frag_idx(size);
    if (frag_idx < DBM_POW2_FRAG_COUNT) {
	frag_offset_p = dbmp->dm_pow2_frags + frag_idx;
	recorded_size = 4 << frag_idx;
    } else {
	frag_offset_p = NULL;
	recorded_size = (size + DBM_PAGE_SIZE - 1) & ~DBM_PAGE_MASK;
    }

    if (recorded_size_p)
	*recorded_size_p = recorded_size;

    return frag_offset_p;
}

/*
 * alloc_pages
 *
 * allocate "page_count" contiguous pages
 *
 */
static dbm_offset_t
alloc_pages (dbm_memory_t *dbmp, u_int32_t page_count)
{
    u_int32_t page_count_probe, page_count_left;
    dbm_offset_t free_page, free_page_next;
    dbm_free_page_t *free_p, *next_free_p;
    void *new_chunk;

    if (page_count < 1 ||
	page_count > DBM_MAX_CONTIG_PAGES)
	return DBM_OFFSET_NULL;

    for (page_count_probe = page_count;
	 page_count_probe <= DBM_MAX_CONTIG_PAGES;
	 page_count_probe++) {
	free_page = dbmp->dm_free_pages[page_count_probe - 1];

	if (free_page == DBM_OFFSET_NULL)
	    continue;

	free_p = (dbm_free_page_t *)(void *)((caddr_t)dbmp + free_page);

	if (free_p->dfp_next != DBM_OFFSET_NULL) {
	    next_free_p = (dbm_free_page_t *)(void *)((caddr_t)dbmp + free_p->dfp_next);
	    next_free_p->dfp_prev = DBM_OFFSET_NULL;
	}

	dbmp->dm_free_pages[page_count_probe - 1] = free_p->dfp_next;

	if (page_count_probe > page_count) {
	    /* register leftover pages */

	    free_page_next = free_page + page_count * DBM_PAGE_SIZE;
	    page_count_left = page_count_probe - page_count;

	    free_p = (dbm_free_page_t *)(void *)((caddr_t)dbmp + free_page_next);
	    free_p->dfp_prev = DBM_OFFSET_NULL;
	    free_p->dfp_next = dbmp->dm_free_pages[page_count_left - 1];

	    if (free_p->dfp_next != DBM_OFFSET_NULL) {
		next_free_p = (dbm_free_page_t *)(void *)((caddr_t)dbmp + free_p->dfp_next);
		next_free_p->dfp_prev = free_page_next;
	    }

	    dbmp->dm_free_pages[page_count_left - 1] = free_page_next;
	}

	return free_page;
    }

    if ((dbmp->dm_top + DBM_PAGE_SIZE - 1) / DBM_PAGE_SIZE + page_count > DBM_MEMORY_PAGE_COUNT)
	return DBM_OFFSET_NULL;

    new_chunk = dbm_mmap(dbmp, page_count * DBM_PAGE_SIZE);
    if (new_chunk) {
	free_page = (caddr_t)new_chunk - (caddr_t)dbmp;

	assert(!(free_page & DBM_PAGE_MASK));
	assert(free_page / DBM_PAGE_SIZE + page_count <= DBM_MEMORY_PAGE_COUNT);

	return free_page;
    }

    return DBM_OFFSET_NULL;
}

/*
 * free_pages
 *
 * release "page_count" contiguous pages
 *
 */
static void
free_pages (dbm_memory_t *dbmp, dbm_offset_t start, u_int32_t page_count)
{
    dbm_free_page_t *free_p, *next_free_p;

    assert (!(start & DBM_PAGE_MASK) &&
	    page_count > 0 && page_count <= DBM_MAX_CONTIG_PAGES);

    free_p = (dbm_free_page_t *)(void *)((caddr_t)dbmp + start);
    free_p->dfp_prev = DBM_OFFSET_NULL;
    free_p->dfp_next = dbmp->dm_free_pages[page_count - 1];

    if (free_p->dfp_next != DBM_OFFSET_NULL) {
	next_free_p = (dbm_free_page_t *)(void *)((caddr_t)dbmp + free_p->dfp_next);
	next_free_p->dfp_prev = start;
    }

    dbmp->dm_free_pages[page_count - 1] = start;
}

void *
dbm_malloc (dbm_memory_t *dbmp, size_t size)
{
    u_int32_t slot_idx, offset;
    dbm_offset_t *frag_offset_p, new_pages, prev_offset;
    u_int32_t recorded_size, page_count;
    u_int32_t *size_info_p;
    dbm_page_header_t *header_p;
    dbm_free_slot_t *free_p, *next_free_p;

    frag_offset_p = get_frag_offset_p(dbmp, size, &recorded_size);

    if (frag_offset_p) {
	if (*frag_offset_p != DBM_OFFSET_NULL) {
	    header_p = (dbm_page_header_t *)(void *)((caddr_t)dbmp + (*frag_offset_p & ~DBM_PAGE_MASK));
	    slot_idx = ((*frag_offset_p & DBM_PAGE_MASK) - sizeof(dbm_page_header_t)) / recorded_size;
	    header_p->dph_usage[slot_idx / (sizeof(dbm_mask_t) * NBBY)] |=
		1 << (slot_idx % (sizeof(dbm_mask_t) * NBBY));

	    free_p = (dbm_free_slot_t *)(void *)((caddr_t)dbmp + *frag_offset_p);

	    if (free_p->dfs_next != DBM_OFFSET_NULL) {
		next_free_p = (dbm_free_slot_t *)(void *)((caddr_t)dbmp + free_p->dfs_next);
		next_free_p->dfs_prev = DBM_OFFSET_NULL;
	    }

	    *frag_offset_p = free_p->dfs_next;

	    return free_p;
	}

	page_count = 1;
    } else
	page_count = (size + DBM_PAGE_SIZE - 1) / DBM_PAGE_SIZE;

    new_pages = alloc_pages(dbmp, page_count);
    if (new_pages == DBM_OFFSET_NULL) {
	errno = ENOMEM;
	return NULL;
    }

    size_info_p = dbmp->dm_size_info + new_pages / DBM_PAGE_SIZE;
    while (page_count > 0)
	size_info_p[--page_count] = recorded_size;

    if (frag_offset_p) {
	header_p = (dbm_page_header_t *)(void *)((caddr_t)dbmp + new_pages);
	memset(header_p->dph_usage, 0, sizeof(header_p->dph_usage));
	header_p->dph_usage[0] = 1; /* first one will be used */
	    
	offset = sizeof(dbm_page_header_t) + recorded_size;
	*frag_offset_p = new_pages + offset;
	prev_offset = DBM_OFFSET_NULL;
	for (;;) {
	    free_p = (dbm_free_slot_t *)(void *)((caddr_t)dbmp + new_pages + offset);
	    offset += recorded_size;
	    free_p->dfs_prev = prev_offset;
	    if (offset + recorded_size > DBM_PAGE_SIZE) {
		free_p->dfs_next = DBM_OFFSET_NULL;
		break;
	    } else
		free_p->dfs_next = new_pages + offset;
	    prev_offset = new_pages + offset;
	}

	return (caddr_t)dbmp + new_pages + sizeof(dbm_page_header_t);
    } else
	return (caddr_t)dbmp + new_pages;
}

void
dbm_free (dbm_memory_t *dbmp, void *ptr)
{
    u_int32_t *size_info_p, page_count;
    dbm_offset_t *frag_offset_p, page_offset, slot_offset;
    u_int32_t slot_idx, offset, i;
    int release_page;
    dbm_mask_t *usage_p, mask;
    dbm_page_header_t *header_p;
    dbm_free_slot_t *free_p, *prev_free_p, *next_free_p;

    slot_offset = (caddr_t)ptr - (caddr_t)dbmp;
    page_offset = slot_offset & ~DBM_PAGE_MASK;
    size_info_p = dbmp->dm_size_info + slot_offset / DBM_PAGE_SIZE;

    frag_offset_p = get_frag_offset_p(dbmp, *size_info_p, NULL);

    if (frag_offset_p) {
	header_p = (dbm_page_header_t *)(void *)((caddr_t)dbmp + page_offset);
	slot_idx = ((slot_offset & DBM_PAGE_MASK) - sizeof(dbm_page_header_t)) / *size_info_p;
	usage_p = header_p->dph_usage + slot_idx / (sizeof(dbm_mask_t) * NBBY);
	mask = 1 << (slot_idx % (sizeof(dbm_mask_t) * NBBY));
	if (!(*usage_p & mask))
	    return; /* already free */

	*usage_p &= ~mask;

	/* release this slot */

	free_p = (dbm_free_slot_t *)(void *)((caddr_t)dbmp + slot_offset);
	free_p->dfs_prev = DBM_OFFSET_NULL;
	free_p->dfs_next = *frag_offset_p;

	if (free_p->dfs_next != DBM_OFFSET_NULL) {
	    next_free_p = (dbm_free_slot_t *)(void *)((caddr_t)dbmp + free_p->dfs_next);
	    next_free_p->dfs_prev = slot_offset;
	}

	*frag_offset_p = slot_offset;

	if (*usage_p)
	    return; /* can't release page yet */

	release_page = 1;
	for (i = 0;
	     i < sizeof(header_p->dph_usage) / sizeof(*header_p->dph_usage);
	     i++) {
	    if (header_p->dph_usage[i]) {
		release_page = 0;
		break;
	    }
	}

	if (!release_page)
	    return; /* can't release page yet */

	/* return page to pool */

	for (offset = sizeof(dbm_page_header_t);
	     offset + *size_info_p <= DBM_PAGE_SIZE;
	     offset += *size_info_p) {
	    free_p = (dbm_free_slot_t *)(void *)((caddr_t)dbmp + page_offset + offset);

	    if (free_p->dfs_prev != DBM_OFFSET_NULL) {
		prev_free_p = (dbm_free_slot_t *)(void *)((caddr_t)dbmp + free_p->dfs_prev);
		prev_free_p->dfs_next = free_p->dfs_next;
	    } else
		*frag_offset_p = free_p->dfs_next;

	    if (free_p->dfs_next != DBM_OFFSET_NULL) {
		next_free_p = (dbm_free_slot_t *)(void *)((caddr_t)dbmp + free_p->dfs_next);
		next_free_p->dfs_prev = free_p->dfs_prev;
	    }
	}

	free_pages(dbmp, page_offset, 1);
    } else {
	page_count = (*size_info_p + DBM_PAGE_SIZE - 1) / DBM_PAGE_SIZE;
	free_pages(dbmp, page_offset, page_count);
    }
}

void *
dbm_realloc (dbm_memory_t *dbmp, void *ptr, size_t size)
{
    void *new_ptr;
    u_int32_t *size_info_p, page;

    if (!ptr)
	return dbm_malloc(dbmp, size);

    page = ((caddr_t)ptr - (caddr_t)dbmp) / DBM_PAGE_SIZE;
    size_info_p = dbmp->dm_size_info + page;

    if (size > *size_info_p) {
	new_ptr = dbm_malloc(dbmp, size);
	if (!new_ptr) {
	    errno = ENOMEM;
	    return NULL;
	}

	memcpy(new_ptr, ptr, *size_info_p);
	dbm_free(dbmp, ptr);

	return new_ptr;
    } else
	return ptr;
}

void *
dbm_calloc(dbm_memory_t *dbmp, size_t number, size_t size)
{
    void *rv;
    u_int32_t n;

    n = number * size;

    rv = dbm_malloc(dbmp, n);
    if (rv)
	memset(rv, 0, n);

    return rv;
}

void
dbm_malloc_init (dbm_memory_t *dbmp)
{
    unsigned i;

    for (i = 0; i < DBM_MAX_CONTIG_PAGES; i++)
	dbmp->dm_free_pages[i] = DBM_OFFSET_NULL;

    for (i = 0; i < DBM_POW2_FRAG_COUNT; i++)
	dbmp->dm_pow2_frags[i] = DBM_OFFSET_NULL;

    for (i = 0; i < DBM_KNOWN_FRAG_COUNT; i++)
	dbmp->dm_known_frags[i] = DBM_OFFSET_NULL;
}

#ifdef TEST
#include <stdio.h>

#define ROUNDS 10000000

void *
dbm_mmap (dbm_memory_t *dbmp, size_t nbytes)
{
    caddr_t old_top;
    size_t new_top;

    old_top = (caddr_t)dbmp + dbmp->dm_top;
    new_top = dbmp->dm_top + nbytes;

    if (new_top > dbmp->dm_size)
	return NULL;

    dbmp->dm_top = new_top;

    return old_top;
}

int
main(int argc, char **argv)
{
    dbm_memory_t *dbmp;
    size_t init_size;
    void *pointers[8<<10];
    unsigned round, idx;

    init_size = 32<<20;

    dbmp = calloc(1, init_size);

    dbmp->dm_size = init_size;
    dbmp->dm_top = (sizeof(*dbmp) + DBM_PAGE_SIZE - 1) & ~DBM_PAGE_MASK;

    dbm_malloc_init(dbmp);

    memset(pointers, 0, sizeof(pointers));

    for (round = 0; round < ROUNDS; round++) {
	idx = random() % (sizeof(pointers) / sizeof(*pointers));

	if (pointers[idx]) {
	    dbm_free(dbmp, pointers[idx]);
	    pointers[idx] = NULL;
	} else {
	    pointers[idx] = dbm_malloc(dbmp, idx);
	}
    }

    return 0;
}

#endif

/*
 * Local Variables:
 * compile-command: "gcc -g -O2 -DTEST -I.. -I../../libjuniper/h -I../../../shared -I../../../pan-release -o mtest malloc.c"
 * End:
 */
