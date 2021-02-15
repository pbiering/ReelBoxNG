/*****************************************************************************
 *
 * Get commandline from host
 *
 * #include <GPL-header>
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "hdshmlib.h"

/* --------------------------------------------------------------------- */
int main(int argc, char** argv)
{
	int fd;
	char cmdline[256]="";
	fd=open("/dev/hdshm",O_RDONLY);
	if (fd>=0) {
		ioctl(fd, IOCTL_HDSHM_CMDLINE, cmdline);
		puts(cmdline);
	}
	exit(0);
}
