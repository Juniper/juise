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

static int opt_verbose;
static int opt_create;
static int opt_exit_on_miss;
static const char *opt_database;
static unsigned long opt_rounds = ROUNDS;
static unsigned long opt_count = 8 << 10;
static unsigned long opt_size = 10 * DBM_PAGE_SIZE;
static unsigned long opt_passes = 5;
static unsigned long opt_key_length = sizeof(unsigned long);

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

static dbm_memory_t *
open_db (void)
{
    unsigned flags = DBMF_CREATE | DBMF_WRITE;

    dbm_memory_t *dbmp = dbm_open(opt_database, (caddr_t) DBM_COMPAT_ADDR,
				  sizeof(delta_header_t), opt_size, &flags);
    if (dbmp == 0)
	err(1, "bad dbmp");

    dbm_malloc_init(dbmp);

    delta_check_header(dbmp);

    return dbmp;
}

static void
close_db (dbm_memory_t *dbmp)
{
    dbm_close(dbmp);
}

/* ---------------------------------------------------------------------- */

static int
test_dbm (void)
{
    char *cp;
    dbm_memory_t *dbmp;
    void **pointers;
    unsigned round, idx;

    /* opt_size = 32<<20; */

    pointers = calloc(sizeof(pointers[0]), opt_count);
    if (pointers == NULL)
	return -1;

    dbmp = open_db();
    if (dbmp == NULL)
	errx(1, "open db failed");

    printf("memory: %p; rounds %lu, count %lu, size %lu\n",
	   dbmp, opt_rounds, opt_count, opt_size);

    for (round = 0; round < opt_rounds; round++) {
	idx = random() % opt_count;

	if (pointers[idx]) {
	    if (opt_verbose)
		printf("free %d: %d -> %p\n", round, idx, pointers[idx]);

	    dbm_free(dbmp, pointers[idx]);
	    pointers[idx] = NULL;
	} else {
	    cp = pointers[idx] = dbm_malloc(dbmp, idx);
	    if (opt_verbose)
		printf("allo %d: %d -> %p\n", round, idx, cp);

	    if (cp)
		memset(cp, 0x55, idx);
	    dbm_offset_t off = dbm_offset(dbmp, cp);
	    char *cp2 = dbm_pointer(dbmp, off);
	    if (cp != cp2)
		printf("%d: wrong: %p -> %lu ->  %p\n",
		       round, cp, (unsigned long) off, cp2);
	}
    }

    free(pointers);
    close_db(dbmp);

    return 0;
}

/* ---------------------------------------------------------------------- */

typedef struct test_node_s {
    int t_dummy1;			/* Test worst case */
    unsigned long t_key[3];		/* Our key */
    int t_dummy2;			/* More worst case */
} test_node_t;

VATNODE_TO_STRUCT(vat_to_test, test_node_t);

static int
test_vatricia (void)
{
    dbm_memory_t *dbmp = NULL;

    dbmp = open_db();
    if (dbmp == NULL)
	errx(1, "open db failed");

    vat_root_t *root;
    test_node_t *node;

    unsigned long key = 1;
    unsigned i, j;
    unsigned long misses, hits, made = 0;

    delta_header_t *dhp = delta_header(dbmp);
    root = &dhp->dh_root;

    if (opt_create) {
	vatricia_root_init(dbmp, root, opt_key_length,
			   offsetof(test_node_t, t_key));
    }

    for (j = 0; j < opt_passes; j++) {
	printf("pass: %d\n", j + 1);
	hits = misses = 0;
	key = 1;

	for (i = 0; i < opt_rounds; i++) {
	    node = dbm_malloc(dbmp, sizeof(test_node_t));
	    if (node == NULL)
		break;

	    bzero(node, sizeof(*node));

	    key += i * 3241 + 12;
	    /* key %= opt_rounds * 2; */

	    node->t_key[0] = htonl(key);
	    node->t_key[1] = i * 4;
	    node->t_key[2] = i * 16;

	    if (opt_verbose)
		printf("%d: %lx\n", i, key);

	    if (!vatricia_add(dbmp, root, node, 0)) {
		node->t_key[0] = htonl(++key);
		if (!vatricia_add(dbmp, root, node, 0)) {
		    misses += 1;
		    if (opt_verbose)
			printf("%d: %lx failed\n", i, key);
		    dbm_free(dbmp, node);
		}
	    } else
		made += 1;
	}

	if (opt_exit_on_miss) {
	    printf("misses: %lu, made: %lu\n", misses, made);
	    exit(1);
	}

	vat_node_t *pp;

	for (pp = vatricia_find_next(dbmp, root, NULL), i = 0; pp;
	     pp = vatricia_find_next(dbmp, root, pp), i++) {

	    node = vat_to_test(dbmp, pp);
	    if (opt_verbose)
		printf("%d: %lx\n", i, (unsigned long) ntohl(node->t_key[0]));
	    hits += 1;
	}

	printf("hits:  %lu\nmisses: %lu\n", hits, misses);

	for (pp = vatricia_find_next(dbmp, root, NULL), i = 0; pp;
	     pp = vatricia_find_next(dbmp, root, NULL)) {
	    node = vat_to_test(dbmp, pp);
	    vatricia_delete(dbmp, root, pp);
	    dbm_free(dbmp, node);
	}

	printf("empty: %s\n", vatricia_isempty(root) ? "true" : "false");
    }

    close_db(dbmp);

    return 0;
}

static int
readline (char *buf, int size, FILE *input)
{
    if (fgets(buf, size, input) == NULL)
	return -1;

    int len = strlen(buf);
    if (len == 0)
	return -1;

    if (buf[len - 1] == '\n')
	buf[--len] = '\0';

    if (buf[len - 1] == '\r')
	buf[--len] = '\0';

    return len;
}

typedef struct test_node2_s {
    int t_dummy1;			/* Test worst case */
    int t_dummy2;			/* More worst case */
    char *t_data;
    char t_key[0];
} test_node2_t;

VATNODE_TO_STRUCT(vat_to_test2, test_node2_t);

static int
test_vatricia2 (const char *name)
{
    int bufsiz = BUFSIZ;
    char *keybuf = malloc(bufsiz);
    char *databuf = malloc(bufsiz);
    int keysize, datasize;
    unsigned i, j;
    FILE *fp;

    dbm_memory_t *dbmp = open_db();
    if (dbmp == NULL)
	errx(1, "open db failed");

    if (name) {
	fp = fopen(name, "r");
	if (fp == NULL)
	    err(1, "cannot open file: %s", name);
    } else {
	fp = stdin;
	opt_passes = 1;
    }

    vat_root_t *root;
    test_node2_t *node;

    unsigned long key = 1;
    unsigned long misses, hits, made = 0;


    delta_header_t *dhp = delta_header(dbmp);
    root = &dhp->dh_root;

    if (opt_create) {
	vatricia_root_init(dbmp, root, 0, offsetof(test_node2_t, t_key));
    }

    for (j = 0; j < opt_passes; j++) {
	printf("pass: %d\n", j + 1);
	hits = misses = 0;
	key = 1;

	rewind(fp);

	for (i = 0;; i++) {
	    keysize = readline(keybuf, BUFSIZ, fp);
	    if (keysize <= 0)
		break;

	    datasize = readline(databuf, BUFSIZ, fp);
	    if (datasize <= 0)
		break;

	    node = dbm_malloc(dbmp, sizeof(*node) + keysize + datasize + 2);
	    if (node == NULL)
		break;

	    bzero(node, sizeof(*node));
	    memcpy(node->t_key, keybuf, keysize + 1);
	    node->t_data = node->t_key + keysize + 1;
	    memcpy(node->t_data, databuf, datasize + 1);

	    node->t_dummy1 = i;
	    node->t_dummy2 = i;

	    if (opt_verbose)
		printf("%u: add (%p/%s) -> (%p/%s)\n", i,
		       node->t_key, node->t_key,
		       node->t_data, node->t_data);

	    if (!vatricia_add(dbmp, root, node, keysize + 1)) {
		misses += 1;
		if (opt_verbose)
		    printf("%d: %lx failed\n", i, key);
		dbm_free(dbmp, node);
	    } else
		made += 1;
	}

	if (opt_exit_on_miss) {
	    printf("misses: %lu, made: %lu\n", misses, made);
	    exit(1);
	}

	vat_node_t *pp;

	for (pp = vatricia_find_next(dbmp, root, NULL), i = 0; pp;
	     pp = vatricia_find_next(dbmp, root, pp), i++) {

	    node = vat_to_test2(dbmp, pp);
	    if (opt_verbose)
		printf("%u: get (%p/%s) -> (%p/%s)\n", i,
		       node->t_key, node->t_key,
		       node->t_data, node->t_data);
	    hits += 1;
	}

	printf("hits:  %lu\nmisses: %lu\n", hits, misses);

	for (pp = vatricia_find_next(dbmp, root, NULL), i = 0; pp;
	     pp = vatricia_find_next(dbmp, root, NULL)) {
	    node = vat_to_test2(dbmp, pp);
	    if (opt_verbose)
		printf("%u: get (%p/%s) -> (%p/%s)\n", i,
		       node->t_key, node->t_key,
		       node->t_data, node->t_data);

	    vatricia_delete(dbmp, root, pp);
	    dbm_free(dbmp, node);
	}

	printf("empty: %s\n", vatricia_isempty(root) ? "true" : "false");
    }

    close_db(dbmp);

    return 0;





    for (;;) {

	/* build node */
    }

    close_db(dbmp);

    return 0;
}

static unsigned long
parse_arg_number (const char *cp)
{
    if (cp == NULL)
	return 0;

    char *ep = NULL;
    unsigned long rc = strtoul(cp, &ep, 10);

    if (ep && *ep) {
	switch (*ep) {
	case 'g':
	    rc *= 1024 * 1024 * 1024;
	    break;
	case 'k':
	    rc *= 1024;
	    break;
	case 'm':
	    rc *= 1024 * 1024;
	    break;
	}
    }

    return rc;
}

int
main (int argc UNUSED, char **argv UNUSED)
{
    char *cp;
    int rc = 0;
    int opt_test = 0;

    malloc_error_func_set(NULL);

    for (argv++; *argv; argv++) {
	cp = *argv;
	if (*cp != '-')
	    break;

	if (streq(cp, "--database")) {
	    opt_database = *++argv;

	} else if (streq(cp, "--help")) {
	    print_help(NULL);

	} else if (streq(cp, "--count") || streq(cp, "-c")) {
	    opt_count = parse_arg_number(*++argv);

	} else if (streq(cp, "--create") || streq(cp, "-C")) {
	    opt_create = TRUE;

	} else if (streq(cp, "--exit-on-miss") || streq(cp, "-E")) {
	    opt_exit_on_miss = TRUE;

	} else if (streq(cp, "--key-length") || streq(cp, "-k")) {
	    opt_key_length = TRUE;

	} else if (streq(cp, "--passes") || streq(cp, "-p")) {
	    opt_passes = parse_arg_number(*++argv);

	} else if (streq(cp, "--rounds") || streq(cp, "-r")) {
	    opt_rounds = parse_arg_number(*++argv);

	} else if (streq(cp, "--size") || streq(cp, "-s")) {
	    opt_size = parse_arg_number(*++argv);

	} else if (streq(cp, "--test") || streq(cp, "-t")) {
	    opt_test = parse_arg_number(*++argv);

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

    if (opt_create)
	unlink(opt_database);

    switch (opt_test) {
    case 0:
	test_dbm();
	break;

    case 1:
	test_vatricia();
	break;

    case 2:
	test_vatricia2(NULL);
	break;
    }

    return rc;
}
