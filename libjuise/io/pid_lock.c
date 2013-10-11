/*
 * pid_lock.c - create a pid file and flock it
 * $Id: pid_lock.c 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Copyright (c) 1998-2006, Juniper Networks, Inc.
 * All rights reserved.
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>

#include <libjuise/common/aux_types.h>
#include <libjuise/io/logging.h>
#include <libjuise/io/pid_lock.h>
#if 0
#include <jnx/swversion.h>
#include <jnx/system_time.h>
#endif

/* 
 * See if the pid file is locked, and if it is, pull the PID
 * out of the file and check if the process is running.
 *
 * out_pid, if not null, will receive the pid for the process 
 *		IFF the process is running, or -1 if the process
 * 		is not running
 *
 * Returns < 0 if we encounter an error and sets errno, otherwise
 * returns the running process PID.  Errno will be ESRCH if the 
 * process is not running.
 */
int
pid_get_process (const char* pidfile_path)
{  
    char buf[BUFSIZ] = "";
    int fd;
    FILE* f;
    int pid = -1;
    struct stat sb;
    int error = 0;

    fd = open(pidfile_path, O_RDONLY);
    if (fd < 0) {
	return -1;
    }

    if (!flock(fd, LOCK_EX|LOCK_NB)) {
	/*
	 * Oops we got the lock. Undo the lock.
	 */
	flock(fd, LOCK_UN|LOCK_NB);
	close(fd);
	errno = ESRCH;
	return -1;
    } 

    /* Check to see that at least one process has a lock on the file */
    if (errno != EWOULDBLOCK) {
	return -1;
    }
    
    if (!(f = fdopen(fd, "r"))) {
	error = errno;
	close(fd);
	errno = error;
	return -1;
    }
    fgets(buf, sizeof buf, f);

    fclose(f);

    if (sscanf(buf, "%d", &pid) != 1) {
	return -1;
    }
    
    if (pid <= 0) {
	errno = ESRCH;
	return -1;
    }
    
    /* now make sure the process shows up in the procfs */
    snprintf(buf, sizeof(buf), "/proc/%d", pid);
    if (stat(buf, &sb)) {
	if (errno == ENOENT) errno = ESRCH;
	return -1;
    } 
    
    return pid;
}


int
pid_update (int fd)
{
    char pid_buf[MAX_PID_SIZE];

    memset(pid_buf, '\0', sizeof(pid_buf));
    snprintf(pid_buf, sizeof pid_buf, "%d\n", getpid());

    if (lseek(fd, 0, SEEK_SET) < 0) {
	logging(LOG_ERR, "%s: lseek: %m", __FUNCTION__);
	return -1;
    }

    if (write(fd, pid_buf, strlen(pid_buf)) < 0) {
	logging(LOG_ERR, "%s: write: %m", __FUNCTION__);
	return -1;		/* failure */
    }

    return 0;			/* success */
}

int
pid_lock (const char *filename)
{
    int fd;

    fd = open(filename, O_CREAT|O_RDWR, 0644);
    if (fd < 0) {
        logging(LOG_ERR, "error opening %s for writing: %m", filename);
        return -1;
    }

    if (flock(fd, LOCK_EX|LOCK_NB) < 0) {
        logging(LOG_ERR, "unable to lock %s: %m", filename);
        logging(LOG_ERR, "is another copy of this program running?");

        close(fd);
        return -1;
    }

    if (pid_update(fd) < 0) {
	close(fd);
	return -1;
    }
    return fd;
}

/*
 * pid_is_locked
 *
 * Test whether a file exists, and is locked.  Unlike pid_lock, this
 * function doesn't create file if it doesn't exist.
 *
 * Returns TRUE  if filename is locked.
 *         FALSE if filename doesn't exist, or isn't locked.
 */
int
pid_is_locked (const char *filename)
{
    int fd;
    int ret;

    fd = open(filename, O_RDWR);
    if (fd < 0) {
	return FALSE;
    }

    /*
     * Success if we fail to lock the file, hopefully because some
     * other process has it locked.
     */
    if (flock(fd, LOCK_EX|LOCK_NB) == 0) {
	/*
	 * Oops we got the lock. Undo the lock.
	 */
	ret = FALSE;
	flock(fd, LOCK_UN|LOCK_NB);

    } else {
	/*
	 * Another process has a lock only if errno is EWOULDBLOCK
	 */
	if (errno == EWOULDBLOCK) {
	    ret = TRUE;
	} else {
	    ret = FALSE;
	}
    }

    close(fd);
    return ret;
}

