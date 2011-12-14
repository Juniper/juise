/*
 * $Id$
 *
 * Copyright (c) 2002-2006, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * file_copy and related functionality
 */

#ifndef __JNX_FILECOPY_H__
#define __JNX_FILECOPY_H__

#include <libjuise/common/aux_types.h>

boolean file_copy (const char *dst_path, const char *src_path, int mode);

#endif /* __JNX_FILECOPY_H__ */

