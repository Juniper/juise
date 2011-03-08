/*
 * $Id: xmllib_pub.h 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Copyright (c) 2000-2007, Juniper Networks, Inc.
 * All rights reserved.
 *
 * Miscellaneous xml functions
 */

#ifndef JUNOSCRIPT_XMLLIB_PUB_H
#define JUNOSCRIPT_XMLLIB_PUB_H

#include <jnx/swversion.h>

/*
 * Use C linkage when using a C++ compiler
 */
#ifdef __cplusplus
extern "C" { 
    namespace junos {
#endif /* __cplusplus */

#ifndef HOSTPROG
typedef enum show_version_list_subcode_args {
    VERSION_BRIEF,
    VERSION_DETAIL,
    VERSION_EXTENSIVE
} ddl_show_version_subcode_t;

int
xml_show_version_list(void *msp, const sw_version_t versions[], 
                      u_int32_t sw_version_count,
                      ddl_show_version_subcode_t detailed_extensive);
#endif

int
xml_show_version(void *msp, const sw_version_t *version, int show_extensive);

#ifdef __cplusplus
    }
}
#endif /* __cplusplus */
    
#endif /* JUNOSCRIPT_XMLLIB_PUB_H */

