/*
  external_ctrl.c
  General RS232/UDP command channel

  (c) Georg Acher, BayCom GmbH, http://www.baycom.de

  #include <gplV2-header.h>

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define STD_FLG (CS8|CREAD|HUPCL)

#include "frontpanel.h"
#include "rb_control.h"

// 0 when vdr not running
extern int power_state;
extern int running_vdr(void);
extern int cdeject(void);
extern kbdx_t * find_named_key(char *name, int *shifted);

//extern volatile int vdr_is_up;
/*-------------------------------------------------------------------------*/
void poweroff(void)
{
	system("touch /tmp/vdr.deepstandby");
	unlink(STANDBYFILE);
}
/*-------------------------------------------------------------------------*/
void standby(int off)
{
	power_state = running_vdr();
	///power_state: 1=running,  0=standby
	DDD("current power_state: %d", power_state);
	if (off && power_state) {
		//unlink(STANDBYFILE);
		power_state=0;
		DDD("turning off ReelVDR");
	}

	if (!off) {
		unlink(STANDBYFILE);
		power_state=1;
		DDD("Waking up ReelVDR");
	}
}
/*-------------------------------------------------------------------------*/
int external_send_raw(char* buf)
{
	ir_cmd_t cmd;
	int ret=1; // default: cmd failed

	DDD("SEND CMD '%s'",buf);

	if (!strcasecmp(buf,"Startup")) {
		// start vdr
		standby(0);
		ret=0;
	}
	else {
		if (!strcasecmp(buf,"Standby")) {
		    if (power_state) {
			standby(1);
			strcpy(cmd.name,"Power");
		    }
		    else {
		        DDD("already in standby. nothing to do.");
		        ret=0;
		    }
		}
		else if (!strcasecmp(buf,"Poweroff")) {
			poweroff();
			strcpy(cmd.name,"Power");
		}
		else if (!strcasecmp(buf,"Eject") && !power_state) {
			cdeject();
			strcpy(cmd.name,"Eject");
			ret=0; // works also in standby
		}
		else {
			strcpy(cmd.name,buf);
		}
		int shifted=0;
		kbdx_t * kbdx=find_named_key(cmd.name, &shifted);
		if (!kbdx)
			return 0;
		cmd.kbdx=kbdx;
		cmd.source=0;
		cmd.cmd=0;        // dummy
		cmd.device=0;
		cmd.shift=shifted;
		cmd.flags=0;
		cmd.repeat=0;
		cmd.timestamp=get_timestamp();

		cmd.flags=0;
		cmd.state=REEL_IR_STATE_REEL;
		put_code(cmd);
		if (power_state)
			ret=0;
	}
	return ret;
}
/*-------------------------------------------------------------------------*/
// Split line into space separated commands
int external_send(char* buf)
{
	char *p,*q=buf;
	int ret=0;
	while(1) {
		p=strchr(q,' ');
		if (!p) {
			ret+=external_send_raw(q);
			return ret;
		}
		else {
			*p=0;
			if (*q!=' ')
	 			ret+=external_send_raw(q);
 			q=p+1;
		}
	}	
}
/*-------------------------------------------------------------------------*/
int init_rs232(char *interface)
{
	int bdflag=B115200;
	struct termios tty;
	int fd;
	char *p;

	p=strchr(interface+6,':');
	if (p) {
		*p=0;
		if (!strcmp(p+1,"300"))
			bdflag=B300;
		else if (!strcmp(p+1,"1200"))
			bdflag=B1200;
		else if (!strcmp(p+1,"2400"))
			bdflag=B2400;
		else if (!strcmp(p+1,"4800"))
			bdflag=B4800;
		else if (!strcmp(p+1,"9600"))
			bdflag=B9600;
		else if (!strcmp(p+1,"19200"))
			bdflag=B19200;
		else if (!strcmp(p+1,"38400"))
			bdflag=B38400;
		else if (!strcmp(p+1,"57600"))
			bdflag=B57600;
		else if (!strcmp(p+1,"115200"))
			bdflag=B115200;
	}

	fd=open(interface+6,O_RDWR|O_NONBLOCK|O_NOCTTY);
	if (fd<0)
		return -1;

	tcgetattr(fd, &tty);
	tty.c_iflag = IGNBRK | IGNPAR;
	tty.c_oflag = 0;
	tty.c_lflag = 0;
	tty.c_line = 0;
	tty.c_cc[VTIME] = 0;
	tty.c_cc[VMIN] = 1;
	tty.c_cflag =STD_FLG;
	cfsetispeed(&tty,bdflag);
	cfsetospeed(&tty,bdflag);
	tcsetattr(fd, TCSAFLUSH, &tty);
	fcntl(fd, F_SETFL, 0); // switch to blocking mode
	return fd;
}
/*-------------------------------------------------------------------------*/
int init_udp(char *interface)
{
	int port=0;
	int fd;
	struct sockaddr_in sa;
	int x;

	if (strlen(interface)<4)
		return -1;
	port=atoi(interface+4);
	if (port<1023) {
		fprintf(stderr,"UDP-Port %i must be greater than 1023\n", port);
		errno=-EINVAL;
		return -1;
	}
	fd=socket(PF_INET, SOCK_DGRAM, 0);
	if (fd<0) {
		fprintf(stderr,"UDP-Socket can't be created\n");
		return -1;
	}
	x=1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x));
	sa.sin_family=AF_INET;
	sa.sin_addr.s_addr=htonl(INADDR_ANY);
	sa.sin_port=htons(port);
	if (bind(fd, (struct sockaddr *) &sa, sizeof(sa))) {
		fprintf(stderr,"UDP bind failed\n");
		return -1;
	}
	return fd;
}
/*-------------------------------------------------------------------------*/
#define MAXLEN 512
void* external_command_thread(void* para)
{
	char *interface=(char*)para;
	int fd;
	char buf[MAXLEN];
	int l,n;
	int mode=0;

	printf("External IO Thread started, device %s\n",interface);

	if (!strncmp(interface, "rs232:",6)) {
		fd=init_rs232(interface);
	}
	else if (!strncmp(interface, "udp:",4)) {
		fd=init_udp(interface);
		mode=1;
	}
	else {
		// FIXME: udp&Co
		fprintf(stderr,"Unsupported interface type %s\n",interface);
		return NULL;
	}

	if (fd<0) {
		char err[256];
		strerror_r(errno,err,sizeof(err));
		fprintf(stderr,"Unable to open interface type %s (%s)\n",interface,err);
		return NULL;
	}

	l=0;

	while(1) {
		if (!mode) {
			// RS232

			n=read(fd,buf+l,1);

			if (n==1) {
				if (buf[l]=='\r' || buf[l]=='\n') {

					buf[l]=0;
					if (!external_send(buf))
						write(fd,"OK\n",3);
					else
						write(fd,"NAK\n",4);
					l=0;
				}
				else {
					if (l==MAXLEN-1)
						l=0;
					else
						l++;
				}
			}
			else
				usleep(40*1000);
		}
		else {
			char *p;
			n=read(fd,buf,MAXLEN-1);
			buf[MAXLEN-1]=0;
			// Only first line in packet
			p=strchr(buf,'\n');
			if (p)
				*p=0;
			external_send(buf);
		}
	}
	return NULL;
}
/*-------------------------------------------------------------------------*/
