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

#ifndef VATRICIA_DBASE_H
#define VATRICIA_DBASE_H

#include "vatricia.h"

/*
 * The database is a set of interconnected trees with multiple roots.
 * Each root has a generation number and each node has the number of
 * the generation that made it, as well as a reference count.  When
 * a new tree is made, the tree can be built from a base generation,
 * in which base the contents of the tree is that base tree.  As nodes
 * are added, the links of the tree are slowly recreated in the new tree,
 * as needed.
 */
typedef uint16_t db_generation_t; /* Generation number */

#define DB_GENERATION_NULL	0 /* No base generation */

/*
 * The database header lies inside the shared memory database and
 * controls the contents of the database, independent of any particular
 * client.
 */
typedef struct db_header_s {
    uint32_t dh_version;	/* DB version number */
    uint32_t dh_magic;		/* DB magic number */
    db_generation_t dh_generation; /* Most recent generation number */
    db_generation_t dh_committed;  /* Committed generation number */
    uint32_t dh_flags;		/* Flags for the database */
    vat_root_list_t dh_trees;	/* Linked list of vatricia trees */
} db_header_t;

#define DB_HEADER_VERSION	0xff000001
#define DB_HEADER__MAGIC	0x00ca4ada

static inline db_header_t *
db_header (dbm_memory_t *dbmp)
{
    return (db_header_t *) dbm_header(dbmp);
}

static inline db_header_t *
db_check_header (dbm_memory_t *dbmp)
{
    db_header_t *dhp = dbm_header(dbmp);

    if (dhp->dh_version == 0) {
	dhp->dh_version = htonl(DB_HEADER_VERSION);
	dhp->dh_magic = htonl(DB_HEADER_MAGIC);
    } else {
	if (dhp->dh_version != htonl(DB_HEADER_VERSION))
	    assert(!"bad version number");
	if (dhp->dh_magic != htonl(DB_HEADER_MAGIC))
	    assert(!"bad magic number");
    }

    return dhp;
}

/*
 * This is the user-space handle for a database, used to track data that
 * cannot be placed in the shared memory segment.  It's also used as an
 * encapsulation layer to hide as many database implementation details as
 * possible.  In reality, many functions here will be inlined, requiring
 * that many header files and structures be exposed.  But callers should
 * take care to limit themselves to the official API.
 */
typedef struct db_handle_s {
    char *db_filename;		/* Current filename */
    dbm_memory_t *db_dbmp;	/* Shared memory database */
    db_header_t *db_header;	/* Header data structure */
    unsigned db_flags;		/* Flags */
} db_handle_t;

/**
 * Open a database, returning a handle that can be used for future calls.
 * param[in] filename Name of database file
 * param[in] flags Flags to control operation
 */
db_handle_t *db_open (const char *filename, unsigned flags);

/**
 * Close a database and release all associated resources.
 */
void db_close (db_handle_t *);

typedef struct db_tree_s {
    db_handle_t *dt_handle;	/* Underlaying handle */
    db_generation_t dt_generation; /* Generation number */
    vat_root_t *dt_root;	   /* Our tree's root */
} db_tree_t;

/**
 * Return a
db_tree_t db_new_tree(db_handle_t *handle, db_generation_t base);

#endif /* VATRICIA_DBASE_H */
