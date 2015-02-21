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

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/param.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include "juiseconfig.h"
#include <libjuise/common/aux_types.h>
#include <libjuise/string/strextra.h>
#include <libjuise/memory/memory.h>
#include <libjuise/memory/dmalloc.h>

#include "delta.h"

static void
print_help (const char *opt)
{
    if (opt)
	fprintf(stderr, "invalid option: %s\n\n", opt);

    fprintf(stderr,
	    "Usage: delta [options]\n\n"
	    "\t--help: display this message\n"
	    "\n");

    exit(1);
}

static void
print_version (void)
{
    printf("libjuice version %s\n", LIBJUISE_VERSION);
}

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
main (int argc UNUSED, char **argv UNUSED)
{
    char *cp;
    int rc = 0;

    for (argv++; *argv; argv++) {
	cp = *argv;
	if (*cp != '-')
	    break;

	if (streq(cp, "--help")) {
	    print_help(NULL);
	} else if (streq(cp, "--version") || streq(cp, "-V")) {
	    print_version();
	    exit(0);

	} else {
	    print_help(cp);
	}
    }

    dbm_memory_t *dbmp;
    size_t init_size;
    void *pointers[8<<10];
    unsigned round, idx;
    unsigned flags = DBMF_CREATE | DBMF_WRITE;

    init_size = 32<<20;

#if 0
    dbmp->dm_size = init_size;
    dbmp->dm_top = (sizeof(*dbmp) + DBM_PAGE_SIZE - 1) & ~DBM_PAGE_MASK;
#else
    dbmp = dbm_open("test.db", (caddr_t) DBM_COMPAT_ADDR,
		    10, 10 * DBM_PAGE_SIZE, &flags);
    if (dbmp == 0)
	err(1, "bad dbmp");
#endif

    dbm_malloc_init(dbmp);

    memset(pointers, 0, sizeof(pointers));

    printf("memory: %p\n", dbmp);

    for (round = 0; round < ROUNDS; round++) {
	idx = random() % (sizeof(pointers) / sizeof(*pointers));

	if (pointers[idx]) {
	    dbm_free(dbmp, pointers[idx]);
	    pointers[idx] = NULL;
	} else {
	    cp = pointers[idx] = dbm_malloc(dbmp, idx);
	    if (cp)
		memset(cp, 0x55, idx);
	    dbm_offset_t off = dbm_offset_of(dbmp, cp);
	    char *cp2 = dbm_pointer_of(dbmp, off);
	    if (cp != cp2)
		printf("%d: wrong: %p -> %lu ->  %p\n",
		       round, cp, (unsigned long) off, cp2);
	}
    }

    return rc;
}
