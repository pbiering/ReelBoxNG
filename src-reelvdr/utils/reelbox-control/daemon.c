/* 
 *  daemon.c : Handle daemon initialization
 */


#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <stdarg.h>

#include "daemon.h"

// static const char *svnid = "$Id$"; // Wunused-variable

extern int daemon_debug;

static int
lock_fd(int fd)
{
    struct flock lock;

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    return fcntl(fd, F_SETLK, &lock);
}

void
perr(const char *fmt,...)
{
    char buf[1024];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (daemon_debug) {
	perror(buf);
    } else
	syslog(LOG_ERR, "%s: %m", buf);

    exit(EXIT_FAILURE);
}

void
pabort(char const *fmt,...)
{
    char buf[1024];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (daemon_debug) {
	fprintf(stderr, "%s\n", buf);
    } else
	syslog(LOG_ERR, "%s", buf);

    exit(EXIT_FAILURE);
}

void
daemon_setup()
{
    /* Set up standard daemon environment */
    pid_t pid;
    // mode_t old_umask; // Wunused-but-set-variable
    int fd;
    FILE *fp;

    if (!daemon_debug) {
	close(0);
	close(1);
	close(2);
	if ((open("/dev/null", O_RDWR) != 0) ||
	    (open("/dev/null", O_RDWR) != 1) ||
	    (open("/dev/null", O_RDWR) != 2)) {
	    perr("Error redirecting I/O");
	}
	pid = fork();
	if (pid == -1) {
	    perr("Cannot fork");
	} else if (pid != 0) {
	    exit(0);
	}
    }
    // old_umask = umask(S_IWGRP | S_IWOTH); // Wunused-but-set-variable
    (void) setsid();

    fd = open(PIDFILE, O_RDWR | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);

    if (fd == -1) {

	if (errno != EEXIST)
	    perr("Cannot open " PIDFILE);

	if ((fd = open(PIDFILE, O_RDWR)) < 0)
	    perr("Cannot open " PIDFILE);

	fp = fdopen(fd, "rw");
	if (fp == NULL) {
	    perr("Cannot open " PIDFILE " for reading");
	}
	pid = -1;
	if ((fscanf(fp, "%d", &pid) != 1) || (pid == getpid())
	    || (lock_fd(fileno(fp)) == 0)) {
	    int rc;

	    syslog(LOG_NOTICE, "Removing stale lockfile for pid %d", pid);

	    rc = unlink(PIDFILE);

	    if (rc == -1) {
		perr("Cannot unlink " PIDFILE);
	    }
	} else {
	    pabort("Another reelbox-ctrld already running with pid %d", pid);
	}
	fclose(fp);

	unlink(PIDFILE);
	fd = open(PIDFILE, O_RDWR | O_CREAT | O_EXCL,
		  S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);


	if (fd == -1)
	    perr("Cannot open " PIDFILE " the second time round");

    }

    if (lock_fd(fd) == -1)
	perr("Cannot lock " PIDFILE);

    fp = fdopen(fd, "w");
    if (fp == NULL)
	perr("Special weirdness: fdopen failed");

    fprintf(fp, "%d\n", getpid());

    /* We do NOT close fd, since we want to keep the lock. However, we don't
     * want to keep the file descriptor in case of an exec().
     */
    fflush(fp);
    fcntl(fd, F_SETFD, (long) 1);


    return;
}

void 
daemon_cleanup()
{
	unlink(PIDFILE);
}
