/*
 * $Id: filecopy.h 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Copyright (c) 2002-2006, Juniper Networks, Inc.
 * All rights reserved.
 *
 * file_copy and related functionality
 */

#ifndef __JNX_FILECOPY_H__
#define __JNX_FILECOPY_H__

#include <libjuise/common/aux_types.h>

boolean file_copy (const char *dst_path, const char *src_path, int mode);

#endif /* __JNX_FILECOPY_H__ */

