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

#include "juiseconfig.h"
#include <libjuise/common/aux_types.h>
#include <libjuise/string/strextra.h>

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

    return rc;
}
