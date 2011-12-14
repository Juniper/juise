/*
 * $Id$
 *
 * Copyright (c) 1998-2007, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * rotate_log.c - rotate log files
 * (Originally libjuniper/rotate_log.c)
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/signal.h>

#include <libjuise/env/env.h>
#include <libjuise/env/env_paths.h>
#include <libjuise/io/logging.h>
#include <libjuise/io/trace.h>
#include <libjuise/io/rotate_log.h>
#include <libjuise/juiseconfig.h>

void
rotate_log (const char *log_file, unsigned max_files, unsigned flags)
{
    char file1[MAXPATHLEN + 5];	/* Slop for lots of files. */
    char file2[MAXPATHLEN + 5];
    const char *filename_ptr;
    int file_number;
    struct stat stbuf;

    if (!(flags & RLF_EMPTY)) {
	/*
	 * Often we don't want to rotate if log_file is missing or
	 * is empty.
	 */
	if (stat(log_file, &stbuf) == 0) {
	    if (stbuf.st_size == 0)
		return;
	} else if (errno == ENOENT)
	    return;
    }
    
    /* Delete oldest log_file file. */
    
    file_number = max_files - 2;

    if (file_number >= 0) {
	snprintf(file2, sizeof (file2) - sizeof (COMPRESS_POSTFIX),
		 "%s.%d", log_file, file_number);
        unlink(file2);
        strcat(file2, COMPRESS_POSTFIX);
        unlink(file2);
    }
    
    /* Rename all the existing log_file files. */

    while (file_number >= 0) {
	snprintf(file2, sizeof (file2) - sizeof (COMPRESS_POSTFIX),
		 "%s.%d", log_file, file_number);
        
	if (file_number == 0) {
	    filename_ptr = log_file;
	} else {
	    snprintf(file1, sizeof (file1) - sizeof (COMPRESS_POSTFIX),
		    "%s.%d", log_file, file_number - 1);
	    filename_ptr = file1;
	}

        if (stat(filename_ptr, &stbuf)) {
            strcat(file2, COMPRESS_POSTFIX);
            strcat(file1, COMPRESS_POSTFIX);

            if (stat(filename_ptr, &stbuf)) {
                file_number--;
                continue;
            }
        }
        
	rename(filename_ptr, file2);

	file_number--;
    }
    
    if (flags & RLF_COMPRESS) {
	snprintf(file1, sizeof (file1), "%s.0", log_file);
	if (stat(file1, &stbuf) == 0) {
	    pid_t pid;

	    /*
	     * This looks a bid odd.  What we are doing is forking twice
	     * so that the first child can exit, orphaning the grandchild.
	     * This allows the main process to continue without delay,
	     * leaving init(8) to clean up the grandchild whom we don't
	     * care about.  By doing it this way we don't need to know/care
	     * what arrangments the main process has for dealing with
	     * SIGCHLD.
	     */
	    if ((pid = vfork()) == 0) {
		signal(SIGCHLD, SIG_IGN);
		if (vfork() == 0) {
#ifdef UNIT_TEST
		    sleep(15);
		    fprintf(stderr, "rotate_log[%d]: %s %s -f %s\n",
			    getpid(), PATH_GZIP, PROG_GZIP, file1);
#endif
		    execl(PATH_GZIP, PROG_GZIP, "-f", file1, (void *) NULL);
		    trace(NULL, TRACE_ALL,
			   "Unable to compress file '%s' using %s: %m",
			   file1, PATH_GZIP);
		}
		_exit(0);		/* init(8) now picks up grandchild */
	    } else if (pid > 0) {
		struct sigaction sa;
		int p;

		/*
		 * sigaction(2) is missleading when it speaks of
		 * wait(2) (or equivalent) blocking for all children
		 * when SA_NOCLDWAIT is set, that implies that
		 * waitpid(pid, ...) should not block - but it does.
		 * So we need to check.
		 */
		sa.sa_flags = 0;
		sigaction(SIGCHLD, NULL, &sa);
		if (!(sa.sa_handler == SIG_IGN &&
		      (sa.sa_flags & SA_NOCLDWAIT))) {
		    do {
			p = waitpid(pid, NULL, 0);
#ifdef UNIT_TEST
			fprintf(stderr, "waitpid(%d,...) -> %d (%d)\n",
				pid, p, (p < 0) ?  errno : 0);
#endif
		    } while (p < 0 && errno == EINTR);
		}
	    }
	}
    }
}

#ifdef UNIT_TEST
int
main (int argc, char *argv[])
{
    int max_files = 0;
    const char *log_file = argv[1];
    unsigned flags = 0;

    if (argc > 2)
	max_files = atoi(argv[2]);
    if (max_files < 1)
	max_files = 10;
    if (argc > 3)
	flags = atoi(argv[3]);
    if (argc > 4) {
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sa, NULL);

	if (fork() == 0) {
	    sleep(30);
	    fprintf(stderr, "child(%d) exiting...\n", getpid());
	    exit(0);
	}
    }
    rotate_log(log_file, max_files, flags);
    if (flags & RLF_COMPRESS)
	system("ps axl");
    exit(0);
}
#endif

