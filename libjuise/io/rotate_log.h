/*
 * $Id: rotate_log.h 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Copyright (c) 2000-2006, Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef __JNX_ROTATE_LOG_H__
#define __JNX_ROTATE_LOG_H__

void rotate_log (const char *log_file, unsigned max_files, unsigned flags);

#define RLF_EMPTY	(1<<0)		/* Rotate log even if empty */
#define RLF_COMPRESS	(1<<1)		/* compress rotated logs */

#define COMPRESS_POSTFIX	".gz"
#define PROG_GZIP		"gzip"

#endif /* __JNX_ROTATE_LOG_H__ */

