/*
 * $Id: dmalloc.h 561086 2013-01-17 19:11:21Z ib-builder $
 * 
 * Copyright 1998-2000, Juniper Network, Inc
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
 *
 * This package uses a mmap()'d file to provide shared memory allocation
 * functions. The core structure is located at the head of the memory
 * segment and must contain _all_ information used by these routines.
 * It is of type "dbm_memory_t", and must be passes to all routines.
 */

#ifndef __DBM_MALLOC_H__
#define __DBM_MALLOC_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/*
 * Function prototype for malloc error routine
 */
typedef void (*malloc_error_func_t)(const char *fmt, ... );

void *dbm_malloc(dbm_memory_t *, size_t);
void *dbm_calloc(dbm_memory_t *, size_t, size_t);
void *dbm_realloc(dbm_memory_t *, void *, size_t);
void *dbm_valloc(dbm_memory_t *, size_t);
void *dbm_memalign(dbm_memory_t *, size_t, size_t);
void *dbm_emalloc(dbm_memory_t *, size_t);
void *dbm_ecalloc(dbm_memory_t *, size_t, size_t);
void *dbm_erealloc(dbm_memory_t *, void *, size_t);
char *dbm_strdup(dbm_memory_t *, const char *);
char *dbm_strsave(dbm_memory_t *, const char *);
void dbm_free(dbm_memory_t *, void *);
void dbm_cfree(dbm_memory_t *, void *);
void dbm_malloc_init (dbm_memory_t *dbmp);

dbm_memory_t *dbm_init(char *);
malloc_error_func_t malloc_error_func_set(malloc_error_func_t func);
void dbm_mal_statsdump(dbm_memory_t *dbmp, FILE *fp);

void dbm_mal_debug(dbm_memory_t *, int);
void dbm_mal_dumpleaktrace(dbm_memory_t *, FILE *);
void dbm_mal_heapdump(dbm_memory_t *, FILE *);
void dbm_mal_leaktrace(dbm_memory_t *, int);
void dbm_mal_sbrkset(dbm_memory_t *, size_t);
void dbm_mal_slopset(dbm_memory_t *, int);
void dbm_mal_statsdump(dbm_memory_t *, FILE *);
void dbm_mal_setstatsfile(FILE *);
void dbm_mal_trace(int);
int dbm_mal_verify(dbm_memory_t *, int);
void dbm_mal_mmap(dbm_memory_t *, char *);

dbm_memory_t *dbm_open(const char *, caddr_t, size_t, size_t, unsigned *);

/* Flags for dbm_open: */
#define DBMF_CREATED	(1<<0)	/* OUT: Created the database */
#define DBMF_OPENED	(1<<1)	/* OUT: Opened the database */
#define DBMF_CHKSEQ	(1<<2)	/* IN: Check sequence number */
#define DBMF_LOCK	(1<<3)	/* IN: Lock the file */
#define DBMF_CREATE	(1<<4)	/* IN: Create the database if it doesn't exist */
#define DBMF_WRITE	(1<<5)	/* IN: Open the database for writing */
#define DBMF_QUIET	(1<<6)	/* IN: Do not show error messages */

#define DBMF_NO_MINOR_CHECK (1<<7) /* IN: Don't check minor version number */
#define DBMF_EXCL	(1<<8)	/* IN: open file with O_EXCL (for DBMF_CREATE */
#define DBMF_FIXED	(1<<9)	/* IN: Use MAP_FIXED on mmap() */

void dbm_sync(dbm_memory_t *);
void dbm_close(dbm_memory_t *);
unsigned int dbm_sequence_number_get(dbm_memory_t *, int pkg);
void dbm_sequence_number_set (dbm_memory_t *dbmp, int pkg, unsigned int seq);
size_t dbm_size(dbm_memory_t *);
int dbm_get_creator (char *buf, size_t bufsiz, const char *path);

void dbm_lock (dbm_memory_t *dbmp);
void dbm_unlock (dbm_memory_t *dbmp);
int  dbm_lock_depth (dbm_memory_t *dbmp);

#endif /* __DBM_MALLOC_H__ */ /* Do not add anything after this line */
