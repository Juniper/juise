/*
 * Copyright (c) 2008, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include <stdlib.h>

#include "juiseconfig.h"
#include <libjuise/common/aux_types.h>
#include <libjuise/memory/memory.h>
#include <libjuise/memory/dmalloc.h>

malloc_error_func_t malloc_error_func;

malloc_error_func_t
malloc_error_func_set (malloc_error_func_t func)
{
    malloc_error_func_t old_func;

    old_func = malloc_error_func;
    malloc_error_func = func;

    return old_func;
}

int
dbm_mal_verify (dbm_memory_t *dbmp __unused, int fullcheck __unused)
{
    return 0;
}

void
dbm_mal_debug (dbm_memory_t *dbmp __unused, int level __unused)
{
}

void
dbm_mal_trace (int value __unused)
{
}

void
dbm_mal_setstatsfile (FILE *fp __unused)
{
}
