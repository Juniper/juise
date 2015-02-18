/*
 * $Id: getmem.c 597246 2013-08-08 17:46:00Z ngoyal $
 *
 * Copyright 1998-2006, Juniper Network, Inc
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

/*LINTLIBRARY*/

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <syslog.h>
#include <assert.h>
#include <errno.h>
#include <machine/endian.h>

#include <junoscript/error.h>
#include <jnx/swversion.h>
#include <jnx/aux_types.h>
#include <jnx/logging.h>
#include <ddl/ddl_compat.h>
#include <ddl/memory.h>
#include <ddl/dmalloc.h>
#include <ddl/util.h>
#include <ddl/dbase.h>

#define MGD_COMPONENT_NAME	"MGD"
#define DBM_ALLOC_ROUNDUP	(512*1024)

#if BYTE_ORDER == LITTLE_ENDIAN
#define DBM_MY_ENDIAN	DBM_LITTLE_ENDIAN
#elif BYTE_ORDER == BIG_ENDIAN
#define DBM_MY_ENDIAN	DBM_BIG_ENDIAN
#else
#define DBM_MY_ENDIAN	endianness_unknown!!!!!!!
#endif

#ifdef HOSTPROG
# ifndef MAP_CORE
#   define MAP_CORE 0
# endif
# ifndef MAP_NOCOUNT
#   define MAP_NOCOUNT 0
# endif
#endif

extern u_int32_t sequence_number[]; /* From libddl */

/*
 * gets memory from the system via mmaping a file.  This was written for SunOS
 * versions greater than 4.0.  The filename is specified by the environment
 * variable CSRIMALLOC_MMAPFILE or by the call to mal_mmapset().  Using this
 * instead of sbrk() has the advantage of bypassing the swap system, allowing
 * processes to run with huge heaps even on systems configured with small swap
 * space.
 */
typedef struct dbm_mmap_s {
    char *dmm_filename;		/* Filename of the database */
    int dmm_fd;			/* File handle */
    int dmm_lock_count;		/* Number of concentric locks */
    dbm_memory_t *dmm_dbmp;	/* Pointer to the database's memory */
    char *dmm_capture;		/* Captured data (for core dumps) */
    size_t dmm_size;		/* Size of data */
    size_t dmm_max_size;	/* Max size of data */
    unsigned dmm_flags;		/* Flags for this mapping */
} dbm_mmap_t;

/* Flags for dmm_flags: */
#define DMMF_STOLEN	(1<<0)	/* Stolen address space; repair when done */
#define DMMF_READONLY	(1<<1)	/* Open/map readonly */
#define DMMF_FIXED	(1<<2)	/* Use MAP_FIXED */

#define NUM_DBM_MAP	8
dbm_mmap_t dbm_mmap_table[ NUM_DBM_MAP ];
dbm_mmap_t *dbm_mmap_top = dbm_mmap_table;

/*
 * reclaim reserved address space to keep other mmap calls
 * out of our backyard
 */
static void
dbm_reclaim_reserved_address_space (caddr_t addr)
{
    if (addr >= (caddr_t) dbm_schema_address() &&
	addr <  (caddr_t)dbm_max_address())
	mmap((void *)dbm_schema_address(), dbm_max_address() - dbm_schema_address(),
	     PROT_NONE, MAP_FIXED | MAP_ANON | MAP_NOCOUNT, -1, 0);
    else if (ddl_api_compat.reserve_address_space &&
	     addr >= (caddr_t)dbm_compat_address() &&
	     addr <  (caddr_t)dbm_schema_address())
	mmap((void *)dbm_compat_address(), dbm_schema_address() - dbm_compat_address(),
	     PROT_NONE, MAP_FIXED | MAP_ANON | MAP_NOCOUNT, -1, 0);
}

/*
 * Finds the matching dbm_mmap_t entry from top of stack
 */
static dbm_mmap_t *
dbm_find_mmap (dbm_memory_t *dbmp)
{
    dbm_mmap_t *map;

    for (map = dbm_mmap_top - 1; map >= dbm_mmap_table; map--) {
	if (map->dmm_dbmp == dbmp)
	    return map;
    }
    return NULL;
}    

/*
 * Calls the mmap() routine on an existing memory mapped db.  Adjusts
 * fields necessary to reflect the new size.
 */
static void *
dbm_re_mmap (dbm_mmap_t *map, size_t size)
{
    dbm_memory_t *dbmp;
    void *addr;
    int fixed, protect, readonly;

    INSIST(map);

    fixed = (map->dmm_flags & DMMF_FIXED);
    readonly = (map->dmm_flags & DMMF_READONLY);
    protect = PROT_READ | (readonly ? 0: PROT_WRITE);

    munmap((void *) map->dmm_dbmp,  size);
    addr = mmap(map->dmm_dbmp, size, protect,
		MAP_SHARED | (fixed ? MAP_FIXED : 0) | MAP_CORE,
		map->dmm_fd, (off_t) 0);
    if (addr == MAP_FAILED)
	return addr;

    dbmp = (dbm_memory_t *) addr;

    if (fixed && dbmp != map->dmm_dbmp) {
	munmap((void *) map->dmm_dbmp,  size);
	return MAP_FAILED;
    }

    if (size < dbmp->dm_size || size < dbmp->dm_top) {
	/*
	 * This shouldn't happen.
	 */
	ERRMSG(UI_DBASE_REMMAP_INVALID, LOG_ERR,
	       "size is smaller than expected"
	       " dm_size %u dm_top %u size %zu",
	       dbmp->dm_size, dbmp->dm_top, size);

	size = dbmp->dm_size > dbmp->dm_top ? dbmp->dm_size : dbmp->dm_top;
	return dbm_re_mmap(map, size);
    }

    map->dmm_dbmp = dbmp;
    map->dmm_size = size;
    if (!readonly)
	dbmp->dm_size = size;
    madvise(addr, size, MADV_RANDOM);

    return addr;
}

void *
dbm_mmap (dbm_memory_t *dbmp, size_t nbytes)
{
    dbm_mmap_t *map;
    size_t size;
    caddr_t addr;
    const char *estr;
    int error;

    for (map = dbm_mmap_top - 1; map >= dbm_mmap_table; map--)
	if (map->dmm_dbmp == dbmp) break;

    if (map < dbm_mmap_table) {
	if (malloc_error_func)
	    (*malloc_error_func)("database could not locate file map entry");
	ERRMSG(UI_DBASE_NOT_FOUND, LOG_ERR,
	       "Database entry not found for address %p", dbmp);
	return NULL;
    }

    size = dbmp->dm_top + nbytes;
    if (size & PAGE_MASK) {
	if (malloc_error_func)
	    (*malloc_error_func)("database must be multiple of pagesize");
	ERRMSG(UI_DBASE_EXTEND_INVALID, LOG_ERR,
	       "Database size %#zx (%#x+%#zx) would not be a multiple "
	       "of pagesize(%#x) for file '%s'",
	       size, dbmp->dm_top, nbytes, PAGE_SIZE, map->dmm_filename);
	return NULL;
    }

    /* Round up to the next memory border */
    size += DBM_ALLOC_ROUNDUP - 1;
    size &= ~(DBM_ALLOC_ROUNDUP - 1);

    if (size <= dbmp->dm_size) {
	caddr_t top = (caddr_t) dbmp + dbmp->dm_top;
	dbmp->dm_top += nbytes;
	return top;
    }
	    
    if (!((map->dmm_max_size == 0 || size < map->dmm_max_size) &&
	ftruncate(map->dmm_fd, size) == 0)) {

	/*
	 * errno is valid only if we called ftruncate
	 */
	if (size < map->dmm_max_size)
	    estr = strerror(errno);
	else
	    estr = "size exceeds limit";

	dmf_set_nomem(dbmp);
	if (malloc_error_func)
	    (*malloc_error_func)("configuration database size limit exceeded");
 	ERRMSG(UI_DBASE_EXTEND_FAILED, LOG_ERR,
	       "Unable to extend configuration database file "
	       "'%s' to size %#lx: %s",
	       map->dmm_filename, (u_long) size, estr);
	return NULL;
    }

    addr = dbm_re_mmap(map, size);

    if (addr != (caddr_t) dbmp) {
	if (malloc_error_func)
	    (*malloc_error_func)("configuration database size limit exceeded");
	ERRMSG(UI_DBASE_ACCESS_FAILED, LOG_ERR,
	       "Unable to reaccess configuration database file '%s', "
	       "address %p, size %#lx: %s",
	       map->dmm_filename, addr, (u_long) size,
	       (addr == (caddr_t) -1) ? strerror(errno) : "invalid address");
	(void) ftruncate(map->dmm_fd, dbmp->dm_size);

        /*
         * Restore mmap to last size.
         */
        dbm_re_mmap(map, dbmp->dm_size);
	return NULL;
    }

    error = dbm_set_vmbase_map(addr, size, TRUE);
    if (error) {
	if (malloc_error_func)
	    (*malloc_error_func)("configuration database size limit exceeded");
	ERRMSG(UI_DBASE_ACCESS_FAILED, LOG_ERR,
	       "Unable to reaccess configuration database file '%s', "
	       "address %p, size %#lx: %s",
	       map->dmm_filename, addr, (u_long) size,
	       strerror(error));

        /*
         * Restore mmap to last size.
         */
        dbm_re_mmap(map, dbmp->dm_size);
	return NULL;
    }

    addr += dbmp->dm_top;
    dbmp->dm_top += nbytes;

    return addr;
}

/*
 * dbm_make_reparations: fixup stolen address space. When a second
 * database just be mapped into our address space, we 'steal' the
 * address's mmap()ing and mark ourselves as the thief (DMMF_STOLEN).
 * When we release the address space, we must re-mmap() the original
 * address space, or face certain death.
 */
static void
dbm_make_reparations (dbm_mmap_t *thief, caddr_t addr)
{
    dbm_mmap_t *map;
    int error;

    if (thief) addr = (caddr_t) thief->dmm_dbmp;

    for (map = (thief ?: dbm_mmap_top) - 1; map >= dbm_mmap_table; map--)
	if ((caddr_t) map->dmm_dbmp == addr) break;

    if (map < dbm_mmap_table) {
	ERRMSG(UI_DBASE_NOT_FOUND, LOG_ERR,
	       "Database entry not found for address %p", addr);
	return;
    }

    if (thief) thief->dmm_dbmp = NULL;

    addr = dbm_re_mmap(map, map->dmm_size);

    if (addr != (caddr_t) map->dmm_dbmp) {
	ERRMSG(UI_DBASE_ACCESS_FAILED, LOG_ERR,
	       "Unable to reaccess configuration database file '%s', "
	       "address %p, size %#lx: %s",
	       map->dmm_filename, addr, (u_long) map->dmm_size,
	       (addr == (caddr_t) -1) ? strerror(errno) : "invalid address");
	abort();
	return;
    }

    error = dbm_set_vmbase_map(addr, map->dmm_size, TRUE);
    if (error) {
	ERRMSG(UI_DBASE_ACCESS_FAILED, LOG_ERR,
	       "Unable to reaccess configuration database file '%s', "
	       "address %p, size %#lx: %s",
	       map->dmm_filename, addr, (u_long) map->dmm_size,
	       strerror(error));
	abort();
	return;
    }
}

void
dbm_close (dbm_memory_t *dbmp)
{
    dbm_mmap_t *map;

    for (map = dbm_mmap_top - 1; map >= dbm_mmap_table; map--)
	if (map->dmm_dbmp == dbmp) break;

    if (map < dbm_mmap_table) {
	ERRMSG(UI_DBASE_NOT_FOUND, LOG_ERR,
	       "Database entry not found for address %p",
	       dbmp);
	return;
    }

    dbm_set_vmbase_map((void *)map->dmm_dbmp, map->dmm_dbmp->dm_size, FALSE);
    munmap((void *) map->dmm_dbmp,  map->dmm_dbmp->dm_size);

    /* keep the largest possible address space reserved */
    dbm_reclaim_reserved_address_space((caddr_t)map->dmm_dbmp);

    safe_close(map->dmm_fd);

    if (map->dmm_flags & DMMF_STOLEN)
	dbm_make_reparations(map, (caddr_t) map->dmm_dbmp);

#undef free
    free(map->dmm_filename);
    bzero((void *) map, sizeof(*map));
}    

void
dbm_sync (dbm_memory_t *dbmp)
{
    dbm_mmap_t *map;

    for (map = dbm_mmap_top - 1; map >= dbm_mmap_table; map--)
	if (map->dmm_dbmp == dbmp) break;

    if (map < dbm_mmap_table) {
	ERRMSG(UI_DBASE_NOT_FOUND, LOG_ERR,
	       "Database entry not found for address %p", dbmp);
	return;
    }

    if (map->dmm_flags & DMMF_READONLY)
	/* no need to fsync a read-only db */
	return;

    msync((void *) map->dmm_dbmp,  map->dmm_dbmp->dm_size, MS_ASYNC);
}    

unsigned int
dbm_sequence_number_get (dbm_memory_t *dbmp, int pkg)
{
    return dbmp ? dbmp->dm_sequence[ pkg ] : ~0U;
}

void
dbm_sequence_number_set (dbm_memory_t *dbmp, int pkg, unsigned int seq)
{
    dbmp->dm_sequence[ pkg ] = seq;
}

size_t
dbm_size (dbm_memory_t *dbmp)
{
    return dbmp->dm_size;
}

void
dbm_lock (dbm_memory_t *dbmp)
{
    dbm_mmap_t *map;
    int rc;

    for (map = dbm_mmap_top - 1; map >= dbm_mmap_table; map--)
	if (map->dmm_dbmp == dbmp) {

	    if (!(map->dmm_flags & DMMF_READONLY)) {
		INSIST(map->dmm_lock_count >= 0);
                
		/*
		 *  Call flock only if we don't already have a lock.
		 */
		if (map->dmm_lock_count == 0) {
		    while ((rc = flock(map->dmm_fd, LOCK_EX)) < 0 &&
			   errno == EINTR)
			continue;
		    INSIST(rc == 0);
		}

		map->dmm_lock_count += 1;
	    }

	    /*
             * Check if the database changed size, if it did we update
             * our memory map.
             */
	    if (map->dmm_size != dbmp->dm_size) {
		caddr_t addr;

                addr = dbm_re_mmap(map, dbmp->dm_size);
		if (addr != (caddr_t) dbmp) {
		    if (malloc_error_func)
			(*malloc_error_func)("database could not be remapped");
		    INSIST(!"mmap: could not remap file");
		}

		rc = dbm_set_vmbase_map(addr, dbmp->dm_size, TRUE);
		if (rc) {
		    if (malloc_error_func)
			(*malloc_error_func)("database could not be remapped");
		    INSIST(!"mmap: could not remap file");
		}
	    }

	    return;
	}

    INSIST(!"dbm: lock: mmap: dbm not found");
}

void
dbm_unlock (dbm_memory_t *dbmp)
{
    dbm_mmap_t *map;

    for (map = dbm_mmap_top - 1; map >= dbm_mmap_table; map--)
	if (map->dmm_dbmp == dbmp) {
	    if (!(map->dmm_flags & DMMF_READONLY)) {
		if (map->dmm_lock_count > 1) {
		    map->dmm_lock_count -= 1;
		} else if (flock(map->dmm_fd, LOCK_UN) < 0) {
		    INSIST(!"dbm: lock: funlock failed");
		} else map->dmm_lock_count = 0;
	    }
	    return;
	}

    INSIST(!"dbm: lock: mmap: dbm not found");
}

int
dbm_lock_depth (dbm_memory_t *dbmp)
{
    dbm_mmap_t *map;

    for (map = dbm_mmap_top - 1; map >= dbm_mmap_table; map--)
	if (map->dmm_dbmp == dbmp)
	    return map->dmm_lock_count;
    return -1;				/* invalid dbmp */
}

int
dbm_get_creator (char *buf, size_t bufsiz, const char *path)
{
    int fd, rc = FALSE, len;
    dbm_memory_t dm;

    fd = open(path, O_RDONLY);
    if (fd < 0) return TRUE;

    len = read(fd, &dm, sizeof(dm));
    if (len <= 0) rc = TRUE;
    else {
	if (strncmp(dm.dm_version, MGD_COMPONENT_NAME, 3)) rc = TRUE;
	else strncpy(buf, dm.dm_version, bufsiz);
    }

    close(fd);
    return rc;
}

/*
 * dbm_open is the last function in this file so that we can redefine ERRMSG
 */
#undef ERRMSG
#define ERRMSG(_tag, _prio, _fmt...)			\
do {							\
    if (malloc_error_func) (*malloc_error_func)(_fmt);	\
    logging_event(_prio, #_tag, ERRMSG_TAG_ENTRY(_tag), #_tag ": " _fmt);\
} while (0)

/*
 * dbm_open(): opens (or creates) the in-core database configuration memory
 * image. This is a memory mapped file.
 */
dbm_memory_t *
dbm_open (const char *fname, caddr_t dbm_address, size_t header_size,
	  size_t init_size, unsigned *flagsp)
{
    int fd, error;
    size_t size;
    dbm_mmap_t *map;
    struct stat st;
    dbm_memory_t *dbmp;
    caddr_t addr;
    int oflags, protect, stolen = FALSE, dflags = 0;
    
    if (flagsp) *flagsp &= ~DBMF_OPENED;

    /*
     * Search for matching db address starting from the top of stack
     * backwards.  We need to save the size of the db, prior to switching
     * so that we mmap the correct value when we make reparations.
     */
    if (dbm_address) {
	if (dbm_address == dbm_schema_address())
	    dflags |= DMMF_FIXED;
	else if (dbm_address == dbm_compat_address()) {
	    if (ddl_api_compat.reserve_address_space)
		dflags |= DMMF_FIXED;
	} else if (*flagsp & DBMF_FIXED) {
	    dflags |= DMMF_FIXED;
	}
	
	for (map = dbm_mmap_top - 1; map >= dbm_mmap_table; map--) {
	    if (dbm_address == (caddr_t) map->dmm_dbmp) {
		stolen = TRUE;
		map->dmm_size = map->dmm_dbmp->dm_size;
		dflags |= DMMF_STOLEN;
		break;
	    }
	}

	/*
	 * Iterate through the table until we find an empty mmap
	 * entry.  Check for duplicate filenames along the way.
	 */
	if (!stolen)
	    map = dbm_mmap_table;
    } else
	map = dbm_mmap_table;

    for ( ; map < &dbm_mmap_table[ NUM_DBM_MAP ]; map++) {
	if (map->dmm_filename == 0) break;
	if (streq(map->dmm_filename, fname)) {
	    if (flagsp && !(*flagsp & DBMF_QUIET))
		ERRMSG(UI_DBASE_OPEN_ALREADY, LOG_ERR,
		       "Database open failed for file '%s': already open",
		       fname);
	    return NULL;
	}
    }

    if (map == &dbm_mmap_table[ NUM_DBM_MAP ]) {
	if (malloc_error_func)
	    (*malloc_error_func)("available database map entry not found");
	    if (flagsp && !(*flagsp & DBMF_QUIET))
		ERRMSG(UI_DBASE_OPEN_FULL, LOG_ERR,
	       "Database open failed for '%s': no empty table slot found",
	       fname);
	return NULL;
    }

    oflags = 0;
    protect = PROT_READ;
    
    if (flagsp && (*flagsp & DBMF_CREATE))
        oflags |= O_CREAT;

    if (flagsp && (*flagsp & DBMF_EXCL))
        oflags |= O_EXCL;
    
    if (flagsp && (*flagsp & DBMF_WRITE)) {
        oflags |= O_RDWR;
        protect |= PROT_WRITE;
    } else {
        oflags |= O_RDONLY;
	dflags |= DMMF_READONLY;
    }
    
    fd = open(fname, oflags, 0640);
    if (fd < 0) {
	if (malloc_error_func)
	    (*malloc_error_func)("could not open database: %s: %s",
				 fname, strerror(errno));
	    if (flagsp && !(*flagsp & DBMF_QUIET))
		ERRMSG(UI_DBASE_OPEN_FAILED, LOG_ERR,
		       "Database open failed for file '%s': %s",
		       fname, strerror(errno));
	return NULL;
    }

    /*
     * We need to lock the database so that no one can modify it
     * while we're checking the header fields. Otherwise, if one MGD is
     * extending the file (changing the size) while we are checking the size,
     * we'll assume some evil is afoot and offer to rebuild the database.
     */
    while (flagsp && (*flagsp & DBMF_LOCK) && flock(fd, LOCK_EX) < 0) {
	if (errno == EINTR)
	    continue;
	if (malloc_error_func)
	    (*malloc_error_func)("could not get initial lock on database: %s",
				 strerror(errno));
	if (flagsp && !(*flagsp & DBMF_QUIET))
	    ERRMSG(UI_DBASE_LOCK_FAILED, LOG_ERR,
	           "Database lock failed for file '%s': %s",
	           fname, strerror(errno));
	abort();
	close(fd);
	return NULL;
    }

    if (flagsp) *flagsp |= DBMF_OPENED;

    /*
     * Make sure that the initial size is at least enough
     * to contain the header.
     */
    if (init_size < header_size || init_size < sizeof(*dbmp))
	init_size += sizeof(*dbmp) + header_size;

    if (init_size & PAGE_MASK)
	init_size = (init_size + PAGE_SIZE - 1) & ~PAGE_MASK;

    size = fstat(fd, &st) ? 0 : st.st_size;

    if (size < init_size) {
	if (ftruncate(fd, init_size)) {
	    if (malloc_error_func)
		(*malloc_error_func)("database truncation failed");
	    if (flagsp && !(*flagsp & DBMF_QUIET))
		ERRMSG(UI_DBASE_EXTEND_FAILED, LOG_ERR,
		       "Unable to extend configuration database file "
		       "'%s' to size %#lx: %s",
		       fname, (u_long) init_size, strerror(errno));
	    goto fd_abort;
	}
    } else init_size = size;

    /* just in case dbm_reserve_address_space didn't happen yet,
     * attempt to reserve the largest possible address space
     */
    if (dbm_address)
	dbm_reclaim_reserved_address_space(dbm_address);

    addr = mmap(dbm_address, init_size, protect,
		MAP_SHARED | (dflags & DMMF_FIXED ? MAP_FIXED : 0) | MAP_CORE,
		fd, (off_t) 0);
    if (addr == (caddr_t) -1 || addr == NULL) {
	if (malloc_error_func)
	    (*malloc_error_func)("database memory map failed: %s",
				 strerror(errno));
	    if (flagsp && !(*flagsp & DBMF_QUIET))
		ERRMSG(UI_DBASE_ACCESS_FAILED, LOG_ERR,
		       "Unable to reaccess configuration database file '%s', "
		       "address %p, size %#lx: %s",
		       map->dmm_filename, dbm_address, (u_long) init_size,
		       (addr == (caddr_t) -1)
		       ? strerror(errno) : "invalid address");
	goto fd_abort;
    }

    if ((dflags & DMMF_FIXED) && addr != dbm_address) {
	if (malloc_error_func)
	    (*malloc_error_func)("database address mismatch");
	    if (flagsp && !(*flagsp & DBMF_QUIET))
		ERRMSG(UI_DBASE_ADDRESS_FAILED, LOG_ERR,
		       "Database address incorrect for file '%s': "
		       "expecting %p, got %p", fname, dbm_address, addr);
	goto mmap_abort;
    }

    /* Advise vm system of random access pattern */
    (void) madvise(addr, init_size, MADV_RANDOM);

    error = dbm_set_vmbase_map(addr, init_size, TRUE);
    if (error) {
	ERRMSG(UI_DBASE_ACCESS_FAILED, LOG_ERR,
	       "Unable to reaccess configuration database file '%s', "
	       "address %p, size %#lx: %s",
	       map->dmm_filename, dbm_address, (u_long) init_size,
	       strerror(error));
	goto mmap_abort;
    }

    dbmp = (dbm_memory_t *) addr;
    if (size == 0) {
	if (flagsp) *flagsp |= DBMF_CREATED;

	/* Initialize the non-zero fields in the dbm_memory_t header */
	dbmp->dm_endian = DBM_MY_ENDIAN;
	dbmp->dm_major = DBM_VERSION_MAJOR;
	dbmp->dm_minor = DBM_VERSION_MINOR;
	memcpy(dbmp->dm_sequence, sequence_number, sizeof(dbmp->dm_sequence));
	dbmp->dm_size = init_size;
	dbmp->dm_top = (sizeof(*dbmp) + header_size + PAGE_SIZE - 1) & ~PAGE_MASK;
	dbmp->dm_header = header_size;
	strncpy(dbmp->dm_version, swversion_what(), sizeof(dbmp->dm_version));

	dbm_malloc_init(dbmp);
  
	ddl_object_changed(dbmp, NULL);
    } else {
	if (flagsp) *flagsp &= ~DBMF_CREATED;

	/* Sanity check the dbm_memory_t header */
	if (dbmp->dm_endian != DBM_MY_ENDIAN) {
	    if (flagsp && !(*flagsp & DBMF_QUIET))
		ERRMSG(UI_DBASE_MISMATCH_ENDIAN, LOG_ERR,
		       "Database header endian-ness mismatch for file '%s': "
		       "expecting %#x, got %#x",
		       fname, DBM_MY_ENDIAN, dbmp->dm_endian);
	    goto mmap_abort;
	}

	if (dbmp->dm_major != DBM_VERSION_MAJOR) {
	    if (flagsp && !(*flagsp & DBMF_QUIET))
		ERRMSG(UI_DBASE_MISMATCH_MAJOR, LOG_ERR,
		       "Database header major version number mismatch for "
		       "file '%s': expecting %#x, got %#x",
		       fname, DBM_VERSION_MAJOR, dbmp->dm_major);
	    goto mmap_abort;
	}

	/*
	 * Skip database minor version number if we don't care.  Only daemons
	 * that are privy to internal data-structures should care -- mgd/ffp.
	 */
	if (flagsp) {
	    if (!(*flagsp & DBMF_NO_MINOR_CHECK) &&
		dbmp->dm_minor != DBM_VERSION_MINOR) {
		
		if (!(*flagsp & DBMF_QUIET))
		    ERRMSG(UI_DBASE_MISMATCH_MINOR, LOG_ERR,
			   "Database header minor version number mismatch for "
			   "file '%s': expecting %#x, got %#x",
			   fname, DBM_VERSION_MINOR, dbmp->dm_minor);
		goto mmap_abort;
	    }
	}

	if (flagsp && (*flagsp & DBMF_CHKSEQ)
		       && !ddl_sequence_number_match(dbmp->dm_sequence,
						     sequence_number)) {
 	    if (flagsp && !(*flagsp & DBMF_QUIET)) {
	        malloc_error_func_set(js_warning);
	        ERRMSG(UI_DBASE_MISMATCH_SEQUENCE, LOG_WARNING,
		       "Database header sequence numbers mismatch for "
		       "file\n         '%s'. If a package has just been\n "
		       "        added or deleted, please verify and commit "
		       "the configuration.", fname);
	        malloc_error_func_set(js_error);
	    }
	    goto mmap_abort;
	}

	if ((caddr_t)dbmp != addr) {
	    if (flagsp && !(*flagsp & DBMF_QUIET))
		ERRMSG(UI_DBASE_MISMATCH_ADDRESS, LOG_ERR,
		       "Database header address mismatch for file '%s': "
		       "expecting %p, got %p",
		       fname, addr, dbmp);
	    goto mmap_abort;
	}

	if (dbmp->dm_size < size) {
	    if (flagsp && !(*flagsp & DBMF_QUIET))
		ERRMSG(UI_DBASE_MISMATCH_SIZE, LOG_ERR,
		       "Database header size mismatch for file '%s': "
		       "expecting %#zx, got %#x",
		       fname, size, dbmp->dm_size);
	    goto mmap_abort;
	}

	if (dbmp->dm_size < dbmp->dm_top) {
	    if (flagsp && !(*flagsp & DBMF_QUIET))
		ERRMSG(UI_DBASE_MISMATCH_EXTENT, LOG_ERR,
		       "Database header extent mismatch for file '%s': "
		       "expecting %#x, got %#x",
		       fname, dbmp->dm_size, dbmp->dm_top);
	    goto mmap_abort;
	}
    }

    map->dmm_filename = strdup(fname);
    map->dmm_fd = fd;
    map->dmm_dbmp = dbmp;
    map->dmm_size = init_size;
    if (dbm_mmap_top <= map) dbm_mmap_top = map + 1;
    map->dmm_flags = dflags;

    if ((caddr_t) dbmp == dbm_schema_address())
	map->dmm_max_size = dbm_max_address() - dbm_schema_address();
    else if ((caddr_t) dbmp == dbm_compat_address())
	map->dmm_max_size = dbm_schema_address() - dbm_compat_address();
    else
	map->dmm_max_size = 0;

    map->dmm_lock_count = 0;
    if (flagsp && (*flagsp & DBMF_LOCK) && flock(fd, LOCK_UN) < 0) {
	    if (flagsp && !(*flagsp & DBMF_QUIET))
		ERRMSG(UI_DBASE_UNLOCK_FAILED, LOG_ERR,
		       "Database unlock failed for file '%s': %s",
		       fname, strerror(errno));
	abort();
	return NULL;
    }

    return dbmp;

mmap_abort:
    dbm_set_vmbase_map(addr, size, FALSE);
    munmap(addr, size);

    /* keep the largest possible address space reserved */
    dbm_reclaim_reserved_address_space(addr);

    if (stolen)
	dbm_make_reparations(NULL, dbm_address);

fd_abort:
    if (flagsp && (*flagsp & DBMF_LOCK))
	(void) flock(fd, LOCK_UN);
    close(fd);
    return NULL;
}

const char *
dbm_filename (dbm_memory_t *dbmp)
{
    dbm_mmap_t *map;

    if (dbmp) {
	/*
	 * Search backward, the one opened last is the one we are interested
	 * in.
	 */
	for (map = &dbm_mmap_table[ NUM_DBM_MAP - 1 ];
	     map >= &dbm_mmap_table[ 0 ]; map--) {
	    if (map->dmm_dbmp == dbmp)
		return map->dmm_filename;
	}
    }
    return NULL;
}

int
dbm_fd (dbm_memory_t *dbmp)
{
    dbm_mmap_t *map;

    if (dbmp) {
	/*
	 * Search backward, the one opened last is the one we are interested
	 * in.
	 */
	for (map = &dbm_mmap_table[ NUM_DBM_MAP - 1 ];
	     map >= &dbm_mmap_table[ 0 ]; map--) {
	    if (map->dmm_dbmp == dbmp)
		return map->dmm_fd;
	}
    }
    return -1;
}

ddl_boolean_t
ddl_is_dbm (dbm_memory_t *dbmp, ddl_node_t *dnp)
{
    return (dbmp && dnp && dnf_is_config(dnp));
}

void *
ddl_malloc (dbm_memory_t *dbmp, ddl_node_t *dnp, size_t size)
{
    if (ddl_is_dbm(dbmp, dnp))
	return dbm_malloc(dbmp, size);
    return DDL_PRIVATE_MALLOC(size);
}

void *
ddl_calloc (dbm_memory_t *dbmp, ddl_node_t *dnp, size_t count, size_t size)
{
    if (ddl_is_dbm(dbmp, dnp))
	return dbm_calloc(dbmp, count, size);
    return DDL_PRIVATE_CALLOC(count, size);
}

void
ddl_free (dbm_memory_t *dbmp, ddl_node_t *dnp, void *ptr)
{
    if (ddl_is_dbm(dbmp, dnp)) {
	db_free_oid(dbmp, ptr);
	dmf_clear_nomem(dbmp);
	return dbm_free(dbmp, ptr);
    }
    return DDL_PRIVATE_FREE(ptr);
}

void *
dbm_header (dbm_memory_t *dbmp)
{
    return (caddr_t) dbmp + sizeof(*dbmp);
}

size_t
dbm_max_size (dbm_memory_t *dbmp)
{
    dbm_mmap_t *map;

    map = dbm_find_mmap(dbmp);
    if (map)
	return map->dmm_max_size;

    return 0;
}

/*
 * Returns readonly status of a database
 *
 * @return 1 if readonly
 * @return 0 if writable
 */
int
dbm_is_readonly (dbm_memory_t *dbmp)
{
    dbm_mmap_t *map;

    map = dbm_find_mmap(dbmp);
    if (map && map->dmm_flags & DMMF_READONLY)
	return 1;

    return 0;
}

#ifdef __powerpc__
static int
dbm_product_is_mx80 (void)
{
    static int retval = -1;

    if (retval == -1) {
	char prod_name[64] = "\0";
	int rc;
	int len = sizeof(prod_name);
        rc = sysctlbyname("hw.product.model", &prod_name, &len, NULL, 0);
        if (rc == 0) {
          char *s;
          size_t l = strlen(prod_name);
          s = strstr("mx80 mx40-t mx10-t mx5-t mx80-t mx80-p mx80-48t ", prod_name);
          if ((s != NULL) && (*(s + l) == ' ')) {
             retval = TRUE;
          } else {
             retval = FALSE;
          }
        }
    }
    return retval;
}
#endif

/*
 * Returns DBM_MAX_ADDRESS which may need
 * adjustment if running under a 64-bit
 * kernel if we are a 32-bit process.
 */
caddr_t dbm_max_address(void)
{
#if DBM_MAX_ADDRESS_32
    int iscompat32;
    int rc;
    int len = sizeof(iscompat32);
    rc = sysctlbyname("kern.iscompat32", &iscompat32, &len, NULL, 0);
    if (rc == 0 && iscompat32)
        return (caddr_t) (DBM_MAX_ADDRESS_32);
#elif __powerpc__
    if (dbm_product_is_mx80() == TRUE) {
        return (caddr_t) (DBM_MAX_ADDRESS_MX80);
    }
#endif

    return (caddr_t) (DBM_MAX_ADDRESS);
}

/*
 * Returns DBM_SCHEMA_ADDRESS which may need
 * adjustment if running under a 64-bit
 * kernel if we are a 32-bit process.
 */
caddr_t dbm_schema_address(void)
{
#if DBM_SCHEMA_ADDR_32
    int iscompat32;
    int rc;
    int len = sizeof(iscompat32);
    rc = sysctlbyname("kern.iscompat32", &iscompat32, &len, NULL, 0);
    if (rc == 0 && iscompat32)
        return (caddr_t) (DBM_SCHEMA_ADDR_32);
#elif __powerpc__
    if (dbm_product_is_mx80() == TRUE) {
        return (caddr_t) (DBM_SCHEMA_ADDR_MX80);
    }
#endif

    return (caddr_t) (DBM_SCHEMA_ADDR);
}

/*
 * Returns DBM_COMPAT_ADDRESS which may need
 * adjustment if running under a 64-bit
 * kernel if we are a 32-bit process.
 */
caddr_t dbm_compat_address(void)
{
#if DBM_COMPAT_ADDR_32
    int iscompat32;
    int rc;
    int len = sizeof(iscompat32);
    rc = sysctlbyname("kern.iscompat32", &iscompat32, &len, NULL, 0);
    if (rc == 0 && iscompat32)
        return (caddr_t) (DBM_COMPAT_ADDR_32);
#elif __powerpc__
    if (dbm_product_is_mx80() == TRUE) {
        return (caddr_t) (DBM_COMPAT_ADDR_MX80);
    }
#endif

    return (caddr_t) (DBM_COMPAT_ADDR);
}

#ifdef __GNUC__
/*
 * Attempt to reserve the address space that potentially
 * gets occupied by the DDL database at the earliest possible moment
 * without having to mess with all the daemon code
 */
static void dbm_reserve_address_space (void) __attribute__((constructor));

static void
dbm_reserve_address_space (void)
{
    static char error_message[] = "fatal error - could not reserve address space in \"" __FILE__ "\"\n";
    void *addr;

    addr = mmap((void *)dbm_schema_address(), dbm_max_address() - dbm_schema_address(),
		PROT_NONE, MAP_ANON | MAP_NOCOUNT, -1, 0);

    if (addr != (void *)dbm_schema_address()) {
	write(2, error_message, strlen(error_message));
	abort();
    }

    if (ddl_api_compat.reserve_address_space) {
	addr = mmap((void *)dbm_compat_address(), dbm_schema_address() - dbm_compat_address(),
		    PROT_NONE, MAP_ANON | MAP_NOCOUNT, -1, 0);

	if (addr != (void *)dbm_compat_address()) {
	    write(2, error_message, strlen(error_message));
	    abort();
	}
    }
}
#endif
