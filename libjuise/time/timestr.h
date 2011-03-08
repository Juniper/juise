/*
 * $Id: timestr.h 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Additional time to string routines
 *
 * Copyright (c) 1997, 2000-2007, Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef __JNX_TIMESTR_H__
#define __JNX_TIMESTR_H__

/*
 * Use C linkage when using a C++ compiler
 */
#ifdef __cplusplus
extern "C" { 
    namespace junos {
#endif /* __cplusplus */

extern char *time_isostr __P((const time_t *));
extern char *time_isostr_utc __P((const time_t *));
extern char *time_diffstr __P((const time_t *));
extern char *time_valstr __P((const time_t));

#ifdef __cplusplus
    }
}
#endif /* __cplusplus */

#endif /* __JNX_TIMESTR_H__ */
