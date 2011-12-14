/*
 * $Id$
 * 
 * Copyright (c) 1997-2001, 2007, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef _ALLOCDUP_H
#define _ALLOCDUP_H

/*
 * Use C linkage when using a C++ compiler
 */
#ifdef __cplusplus
extern "C" { 
    namespace junos {
#endif /* __cplusplus */

/*
 * ALLOCADUP(): think of is as strdup + alloca
 */
static inline char *
allocadupx (char *to, const char *from)
{
    if (to) strcpy(to, from);
    return to;
}
#define ALLOCADUP(s) allocadupx((char *) alloca(strlen(s) + 1), s)
#define ALLOCADUPX(s) ((s) ? allocadupx((char *) alloca(strlen(s) + 1), s) : NULL)

#ifdef __cplusplus
    }                                                                     
}
#endif /* __cplusplus */

#endif /* _ALLOCDUP_H */
