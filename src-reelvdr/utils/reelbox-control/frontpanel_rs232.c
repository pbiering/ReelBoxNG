/*
 * Frontpanel communication for Reelbox PVR1100/Lite/Avantgarde
 *
  * (c) Georg Acher, acher (at) baycom dot de
 *     BayCom GmbH, http://www.baycom.de
 *
 * #include <gpl-header.h>
 *
*/

#include <stdlib.h>
#include <linux/cdrom.h>
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
#include <poll.h>
#include <pthread.h>

#include "frontpanel.h"

unsigned long long get_timestamp(void);
#define STD_FLG (CS8|CREAD|HUPCL)

// Hack
extern int is_avg3;

#define CHECK_FD()  \
	if (fd==-1) \
		fp_open_serial_hw(device, baudrate);
/*-------------------------------------------------------------------------*/
frontpanel_rs232::frontpanel_rs232(const char* device)
{
	fd=-1;
	cmd_length=state=0;
}
/*-------------------------------------------------------------------------*/
frontpanel_rs232::~frontpanel_rs232(void)
{
	fp_close_serial();
}
/*-------------------------------------------------------------------------*/
const char* frontpanel_rs232::fp_get_name(void)
{
	return "RS232";
}
/*-------------------------------------------------------------------------*/
int frontpanel_rs232::fp_write(char *buf, int len)
{
	if (fd==-1) 
		fp_open_serial_hw(device, baudrate);
	return write(fd,buf,len);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_noop(void)
{
	char buf[]={(char) 0xa6};
	
	fp_write(buf,1);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_get_version(void)
{
	char buf[]={(char) 0xa5, (char) 0x00, (char) 0x00};
	fp_write(buf,3);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_enable_messages(int n)
{
	char buf[]={(char) 0xa5, (char) 0x01, (char) n};
	fp_write(buf,3);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_display_brightness(int n)
{
	char buf[]={(char) 0xa5, (char) 0x02, (char) n, (char) 0};
	fp_write(buf,4);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_display_contrast(int n)
{
	char buf[]={(char) 0xa5, (char) 0x03, (char) n, (char) 0};
	fp_write(buf,4);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_clear_display(void)
{
	char buf[]={(char) 0xa5, (char) 0x04};
	fp_write(buf,2);
}
/*-------------------------------------------------------------------------*/
// Ignore, use display_cmd or display_data instead
void frontpanel_rs232::fp_write_display(unsigned char *data, int datalen)
{
	char buf[]={(char) 0xa5, (char) 0x05};
	fp_write(buf,2);
	fp_write((char*)data,datalen);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_display_cmd(char cmd)
{
	char xbuf[]={(char) 0xa5, (char) 0x05, (char) 3, (char) 0, cmd};
	fp_write(xbuf,5);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_display_data(char *data, int l)
{
	char buf[64]={(char) 0xa5, (char) 0x05, (char) (l+2), (char) +1};
	memcpy(buf+4,data,l);
	fp_write(buf,l+4);
	u_sleep(3000);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_set_leds(int blink, int state)
{
	char buf[]={(char) 0xa5, (char) 0x06, (char) blink, (char) state};
	fp_write(buf,4);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_set_clock(void)
{
	time_t t;
	struct tm tm;
	t=time(0);
	localtime_r(&t,&tm);
	{
		char buf[]={(char) 0xa5, (char) 0x7, (char) tm.tm_hour, (char) tm.tm_min, (char) tm.tm_sec,
			    (char) (t>>24 & 255), (char) (t>>16 & 255), (char) (t>>8 & 255), (char) (t & 255)};
		fp_write(buf,9);
	}
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_set_wakeup(time_t t)
{
	char buf[]={(char) 0xa5, (char) 0x08, (char) ((t>>24) & 255), (char) ((t>>16) & 255), (char) ((t>>8) & 255), (char) t};
	fp_write(buf,6);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_set_displaymode(int m)
{
	char buf[]={(char) 0xa5, (char) 0x09, (char) (m & 255)};
	fp_write(buf,3);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_set_switchcpu(int timeout, int mode)
{
	if (!mode) {
		char buf[]={(char) 0xa5, (char) 0x0a, (char) 0x42, (char) 0x71, (char) (timeout & 255)};
		fp_write(buf,5);
	}
	else {
		char buf[]={(char) 0xa5, (char) 0x0a, (char) 0x42, (char) 0x7a, (char) (timeout & 255)};
		fp_write(buf,5);
	}
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_get_clock(void)
{
	char buf[]={(char) 0xa5, (char) 0x0b};
	fp_write(buf,2);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_get_wakeup(void)
{
	char buf[]={(char) 0xa5, (char) 0x0e};
	fp_write(buf,2);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_get_key(void)
{
	char buf[]={(char) 0xa5, (char) 0x0f};
	fp_write(buf,2);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_set_clock_adjust(int v1, int v2)
{
	char buf[]={(char) 0xa5, (char) 0x0a, (char) v1, (char) v2};
	fp_write(buf,4);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_set_led_brightness(int v)
{
	char buf[]={(char) 0xa5, (char) 0x10, (char) 0, (char) 0, (char) 0, (char) 0};
	buf[2]=v&255;
	buf[3]=(v>>8)&255;
	buf[4]=(v>>16)&255;
	buf[5]=(v>>24)&255;
	fp_write(buf,6);
}

/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_get_flags(void)
{
	char buf[]={(char) 0xa5, (char) 0x11};
	fp_write(buf,2);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_power_control(int v)
{
        char buf[]={(char) 0xa5, (char) 0x12, (char) 0, (char) 0, (char) 0, (char) 0};
        buf[2]=v&255;
        buf[3]=(v>>8)&255;
        buf[4]=(v>>16)&255;
        buf[5]=(v>>24)&255;
        fp_write(buf,6);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_eeprom_flash(int a, int d)
{
        char buf[]={(char) 0xa5, (char) 0x14, (char) 0, (char) 0};
        buf[2]=a;
        buf[3]=d;
        fp_write(buf,4);
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_show_pic(unsigned char **LCD)
{
        int x,y,xx,yy;
        int i;
        unsigned char c;
        int rx;
        int invert;
        int sizeX=128,sizeY=64;
        int xc=4;
	
        invert=0;

        for (y = 0; y < (sizeY); y+=8)
        {
                fp_display_cmd( 0xb0+((y/8)&15));
                                        
                for (x = 0; x < sizeX / 8; x+=4)
                {
                        unsigned char d[32]={0};
                        
                        for(yy=0;yy<8;yy++)
                        {
                                for (xx=0;xx<4;xx++)
                                {
                                        c = (LCD[x+xx][y+yy])^invert;
                                        
                                        for (i = 0; i < 8; i++)
                                        {
                                                d[i+xx*8]>>=1;
                                                if (c & 0x80)
                                                        d[i+xx*8]|=0x80;
                                                c<<=1;
                                        }
                                }
                        }
                        rx=x*8; 
//                              printf("X %i, y%i\n",rx,y);
                        
                        fp_display_cmd( 0x10+(((rx+xc)>>4)&15));
                        fp_display_cmd( 0x00+((rx+xc)&15));
                        fp_display_data( (char*)d,32);
                }
        }
}
/*-------------------------------------------------------------------------*/
int frontpanel_rs232::fp_open_serial(const char* fp_device, int bdflag)
{
	strcpy(device,fp_device);
	baudrate=bdflag;
	return 0;
}
/*-------------------------------------------------------------------------*/
int frontpanel_rs232::fp_open_serial_hw(const char* fp_device, int bdflag)
{
	struct termios tty;	

	fd=open(fp_device,O_RDWR|O_NONBLOCK|O_NOCTTY);
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

	return 0;
}
/*-------------------------------------------------------------------------*/
int frontpanel_rs232::fp_get_fd(void)
{
	return fd;
}
/*-------------------------------------------------------------------------*/
void frontpanel_rs232::fp_close_serial(void)
{	
	if (fd>0) {
		close(fd);
	}
}
/*-------------------------------------------------------------------------*/
size_t frontpanel_rs232::fp_read(unsigned char *b, size_t len)
{
	CHECK_FD();
	return read(fd,b,len);
}
/*-------------------------------------------------------------------------*/
int frontpanel_rs232::get_answer_length(int cmd)
{
	if (cmd==0xf2 || cmd==0xf1 || cmd==0x12)
		return 4;
	else if (cmd==0x00 || cmd==0xf3 || cmd==0x0f || cmd==0x11)
		return 2;
	else if (cmd==0xb || cmd==0xe)
		return 7;
	else
		return 0;
}
/*-------------------------------------------------------------------------*/
int frontpanel_rs232::fp_read_msg(unsigned char *msg, unsigned long long *ts)
{
	int l;

	CHECK_FD();

	struct pollfd pfd[]=  {{fd, POLLIN|POLLERR|POLLHUP,0}
	};

	while(1) {
		poll(pfd,1,20);
//		u_sleep(10*1000);

		l=fp_read(buf+state,1);
		if (l!=1)		
			return 0;
			
//                printf("%02x ",*(buf+state)); fflush(stdout);
		if (state==0) {
			if (buf[0]==0x5a || buf[0]==0x55) { // 0x55 RAW IR
				state=1;
#if !FPCTL
				timestamp=get_timestamp();
#endif
			}
			if (buf[0]==0xff) { // flash-mode active?
				return 1;
			}
		}
		else if (state==1) {
			if (buf[0]==0x55)
				cmd_length=2;
			else
				cmd_length=get_answer_length(buf[1]);
			if (cmd_length==0) {
				state=0;
				memcpy(msg,buf,2);
				buf[0]=0;
				if (ts)
					*ts=timestamp;
				return 2;
			}
			state++;
		}
		else {
			if (state==1+cmd_length) {
				state=0;
				memcpy(msg,buf,2+cmd_length);
				buf[0]=0;
				if (ts)
					*ts=timestamp;			
				return 2+cmd_length;
			}
			state++;
		}
	}

	return 0;
}
/*-------------------------------------------------------------------------*/
int frontpanel_avr::fp_available(const char *device, int force)
{
#ifdef RBMINI   
	return 0;
#else
	if (!force) {
		struct stat sbuf;
		if (!stat(CAP_AVR,&sbuf))
			return 1;
		if (!stat(CAP_NOAVR,&sbuf))
			return 0;
	}

	frontpanel_avr fp(device);
	fp.fp_noop();
	fp.fp_noop();
	fp.fp_noop();
	fp.fp_noop();
	fp.fp_get_version();
	unsigned char buf[20];
	int l;
	u_sleep(20*1000);
	l=fp.fp_read_msg(buf,NULL);
	if (l>0)
		return 1;
	else
		return 0;
#endif
}
/*-------------------------------------------------------------------------*/
int frontpanel_ice::fp_available(const char *device, int force)
{
#ifdef RBMINI   
	return 0;
#else
	struct stat sbuf;
	if (!force) {
		if (!stat(CAP_ICE,&sbuf))
			return 1;
		if (!stat(CAP_NOICE,&sbuf))
			return 0;
	}
	frontpanel_ice fp(device);
	fp.fp_noop();
	fp.fp_noop();
	fp.fp_noop();
	fp.fp_noop();
	fp.fp_get_version();
	unsigned char buf[20];
	int l;
	u_sleep(20*1000);
	l=fp.fp_read_msg(buf,NULL);
	if (l>0)
		return 1;
	else
		return 0;
#endif
}
/*-------------------------------------------------------------------------*/
// in fan_control.c
extern void* fan_control_thread(void* para);

void frontpanel_rs232::fp_start_handler(void)
{
#ifndef FPCTL
	pthread_t thread;

#if ! defined RBLITE && ! defined RBMINI
	// start fan_control thread (AVG1/2/3 + NCL2)
//	if (!strcmp(fp_get_name(),"AVR")) 
	{
		if (pthread_create(&thread, NULL, fan_control_thread, this))
			pthread_detach(thread);
	}
#endif
	
	// start ir handler
	if (pthread_create(&thread, NULL, fp_serial_thread, this)) 
                pthread_detach(thread); 
#endif
}
#ifndef FPCTL
/*-------------------------------------------------------------------------*/

// Scans the commands from frontpanel (RS232 on AVG) 
void * frontpanel_rs232::fp_serial_thread (void* p) 
{ 
	frontpanel *fp=(frontpanel*)p;
        unsigned char msg[16]; 
        unsigned long long ts; 
        int l; 
        ir_cmd_t cmd={0}; 
        ir_state_t state={0}; 
 
        while(1) { 
                handle_key_timing(&state, &cmd); // shift status etc. 
 
                state.available=0; 
                msg[0]=0; 
                l=fp->fp_read_msg(msg, &ts); 
                if (!l) 
                        continue; // message not yet complete 
 
                if (msg[0]==0x55) { 
                        handle_raw_ir(msg); 
                } 
                else if (msg[1]==0xf1 || msg[1]==0xf2) { 
                        handle_ir_key(&state, &cmd, msg, ts); 
                } 
                else if (msg[1]==0x12) { 
                        handle_fan_status(msg); 
                } 
        } 
        return NULL; 
} 
#endif
