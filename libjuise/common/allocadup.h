/*
 * $Id: allocadup.h 346460 2009-11-14 05:06:47Z ssiano $
 * 
 * Copyright (c) 1997-2001, 2007, Juniper Networks, Inc.
 * All rights reserved.
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
