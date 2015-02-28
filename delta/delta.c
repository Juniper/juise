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
#include "vatricia.h"

#include "delta.h"

#define ROUNDS 10000000

int opt_verbose;
const char *opt_database;

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

/* ---------------------------------------------------------------------- */

typedef struct test_node_s {
    int t_dummy1;			/* Test worst case */
    unsigned long t_key;		/* Our key */
    int t_dummy2;			/* More worst case */
} test_node_t;

VATNODE_TO_STRUCT(vat_to_test, test_node_t, t_vatricia);

static int
test_vatricia (void)
{
    dbm_memory_t *dbmp = NULL;

    vat_root_t *root = vatricia_root_init(dbmp, sizeof(unsigned long),
					  offsetof(test_node_t, t_key));
    unsigned long key = 1;
    int i, j;
    int misses, hits;

    for (j = 0; j < 5; j++) {
	printf("pass: %d\n", j + 1);
	hits = misses = 0;
	key = 1;

	for (i = 0; i < ROUNDS; i++) {
	    test_node_t *node = calloc(1, sizeof(test_node_t));
	    if (node == NULL)
		break;

	    key += i * 3241 + 12;
	    key %= ROUNDS * 2;

	    node->t_key = htonl(key);

#if 0
	    printf("%d: %lx\n", i, key);
#endif

	    if (!vatricia_add(dbmp, root, node)) {
		node->t_key = htonl(++key);
		if (!vatricia_add(dbmp, root, node)) {
		    misses += 1;
		    if (opt_verbose)
			printf("%d: %lx failed\n", i, key);
		    free(node);
		}
	    }
	}

	vat_node_t *pp;

	for (pp = vatricia_find_next(dbmp, root, NULL), i = 0; pp;
	     pp = vatricia_find_next(dbmp, root, pp), i++) {

	    test_node_t *node = vat_to_test(dbmp, pp);
	    if (opt_verbose)
		printf("%d: %lx\n", i, (unsigned long) ntohl(node->t_key));
	    hits += 1;
	}

	printf("hits:  %d\nmisses: %d\n", hits, misses);

	for (pp = vatricia_find_next(dbmp, root, NULL), i = 0; pp;
	     pp = vatricia_find_next(dbmp, root, NULL)) {
	    test_node_t *node = vat_to_test(dbmp, pp);
	    vatricia_delete(dbmp, root, pp);
	    free(node);
	}

	printf("empty: %s\n", vatricia_isempty(root) ? "true" : "false");

    }

    return 0;
}
/* ---------------------------------------------------------------------- */


#if 0
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
#endif

static int
test_dbm (void)
{
    char *cp;
    dbm_memory_t *dbmp;
    size_t init_size;
    void *pointers[8<<10];
    unsigned round, idx;
    unsigned flags = DBMF_CREATE | DBMF_WRITE;

    init_size = 32<<20;

    char *waste = malloc(480*1024);
    waste += 1;

#if 0
    dbmp->dm_size = init_size;
    dbmp->dm_top = (sizeof(*dbmp) + DBM_PAGE_SIZE - 1) & ~DBM_PAGE_MASK;
#else
    dbmp = dbm_open(opt_database, (caddr_t) DBM_COMPAT_ADDR,
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
	    dbm_offset_t off = dbm_offset(dbmp, cp);
	    char *cp2 = dbm_pointer(dbmp, off);
	    if (cp != cp2)
		printf("%d: wrong: %p -> %lu ->  %p\n",
		       round, cp, (unsigned long) off, cp2);
	}
    }

    return 0;
}

int
main (int argc UNUSED, char **argv UNUSED)
{
    char *cp;
    int rc = 0;

    malloc_error_func_set(NULL);

    for (argv++; *argv; argv++) {
	cp = *argv;
	if (*cp != '-')
	    break;

	if (streq(cp, "--database")) {
	    opt_database = *++argv;

	} else if (streq(cp, "--help")) {
	    print_help(NULL);

	} else if (streq(cp, "--verbose") || streq(cp, "-v")) {
	    opt_verbose = TRUE;

	} else if (streq(cp, "--version") || streq(cp, "-V")) {
	    print_version();
	    exit(0);

	} else {
	    print_help(cp);
	}

	if (*argv == NULL) {
	    /*
	     * The only way we could have a null argv is if we said
	     * "xxx = *++argv" off the end of argv.  Bail.
	     */
	    fprintf(stderr, "missing option value: %s\n", cp);
	    print_help(NULL);
	    return 1;
	}
    }

    if (opt_database == NULL)
	opt_database = "test.db";

    test_dbm();
    test_vatricia();

    return rc;
}
