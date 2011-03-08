/*
 * pid_lock.h - create a pid file and flock it
 * $Id: pid_lock.h 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Copyright (c) 1997-2007, Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef __JNX_PID_LOCK_H__
#define __JNX_PID_LOCK_H__

/**
 * @file pid_lock.h
 * @brief PID file management APIs
 *
 * pid_lock()
 * Open lock, and write a PID file all in one fell swoop.
 *
 * It's useful to first open and lock the PID file.  If it fails, you can exit
 * cleanly with an error message on the console.  If it succeeds, you might
 * daemonize() and then have a new PID that you want to write into the file.
 *
 * There is no pid_close(), you never close your PID file, it gets closed
 * on exit, and the lock gets broken then.
 *
 * All routines return < 0 on failure.
 */

#include <stdio.h>
#include <libjuise/env/jnx_paths.h>

__BEGIN_DECLS

/**
 * @brief
 * Maximum number of digits in a PID string, including the NULL-terminator
 * character.
 */
#define MAX_PID_SIZE 10

/**
 * @brief
 * Silences pid lock failure messages
 */
extern int pid_lkquiet;

/**
 * @brief
 * Checks to see if the PID file is locked, and if it is, pull the PID
 * out of the file and check if the process is running.
 *
 * @param[in] filename
 *            The name of the PID file
 *
 * @return
 *     < 0 if we encounter an error and sets @c errno;
 *     Otherwise returns the running process PID.  
 *     @c errno will 
 *     be @c ESRCH if the process is not running.
 */
int
pid_lock (const char *filename);

/**
 * @brief
 * Update the PID file with our current process ID.
 * 
 * @param[in] fd
 *     File descriptor for the PID file
 * 
 * @return
 *     0 on success; otherwise < 0 if an error is encountered.
 */
int
pid_update (int fd);

/**
 * @brief
 * Generates a PID file given a daemon name
 *
 * @param[in]  dname
 *     Name of daemon used to generate the PID file
 * @param[in] is_internal
 *     Non-zero for Juniper-developed application, 0 otherwise
 * @param[out] buf
 *     Buffer to place the generated string
 * @param[in]  sz
 *     Size of output buffer
 * 
 * @return 
 *     A pointer to the buffer.
 */
static inline char *
pid_gen_filename (const char *dname, const char *prov_prefix, char *buf,
		  size_t sz)
{
    if (prov_prefix) {
	snprintf(buf, sz, PATH_PIDFILE_EXT_DIR "%s/%s.pid", prov_prefix, dname);
    } else {
	snprintf(buf, sz, PATH_PIDFILE_DIR "%s.pid", dname);
    }
    return buf;
}

/**
 * @brief
 * Tests whether a PID file exists, and is locked.  
 *
 * Unlike pid_lock(), this function doesn't create the file if it doesn't exist.
 *
 * @param[in] filename
 *     Name of the PID file
 *
 * @return 
 *     @c TRUE  if filename is locked;
 *     @c FALSE if filename doesn't exist, or isn't locked.
 */
int
pid_is_locked (const char *filename);

/**
 * @brief
 * See if the PID file is locked, and if it is, pull the PID
 * out of the file and check if the process is running.
 *
 * @param[in] pidfile_path
 *     Name of the pid file
 * 
 * @return
 *     < 0 if we encounter an error and sets @c errno; 
 *     Otherwise returns the running process PID.  
 *     @c errno will be set to @c ESRCH if the process is not running.
 */
int
pid_get_process (const char* pidfile_path);

__END_DECLS


#ifdef XXX_UNUSED
#ifndef __JUNOS_SDK__
#include <jnx/pid_lock_priv.h>
#endif /* __JUNOS_SDK__ */
#endif /* XXX_UNUSED */

#endif /* __JNX_PID_LOCK_H__ */

