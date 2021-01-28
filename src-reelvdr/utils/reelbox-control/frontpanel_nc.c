/*
 * Frontpanel communication emulation for Reelbox NetClient
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

#define REELDRV_DEVICE "/dev/reeldrv"

unsigned long long get_timestamp(void);

// Some magic timings
#define IR_1MIN  80
#define IR_1MAX  350
#define IR_TMIN  915
#define IR_TMAX  1025
#define IR_STEP  137
#define IR_MAX   3300


// FIXME: get from reeldrv.h

#define IOCTL_REEL_LED_ON _IOWR('d', 0x1, int)
#define IOCTL_REEL_LED_OFF _IOWR('d', 0x2, int)
#define IOCTL_REEL_GET_KEY _IOWR('d', 0x3, int)
#define IOCTL_REEL_SPI_READ    _IOWR('d', 0x5, int)
#define IOCTL_REEL_SPI_WRITE   _IOWR('d', 0x6, int)
#define IOCTL_REEL_LED_BLINK   _IOWR('d', 0x8, int)

/*-------------------------------------------------------------------------*/
frontpanel_nc::frontpanel_nc(const char* device)
{
	xir_state=0;
	lb=0;
	ir_filter=0;
	irstate=0;
	last_keyval=0;
	rpt=0;
	fp_open_serial();
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::send_cmd_sub4(char *din, char *dout)
{
        int n,v,m;      
        for(n=0;n<4;n++) 
                ioctl(fd,IOCTL_REEL_SPI_WRITE,din[n]&255);

        for(n=0;n<4;n++) {
                for(m=0;m<100;m++) {
                        v=ioctl(fd,IOCTL_REEL_SPI_READ);
                        if (v!=-1) {
                                dout[n]=v;
                                break;
                        }                      
                        if (m>80)
                                usleep(1000);
                }
        }
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::send_cmd(char *d, char *r)
{
        int n,v,m;      
	send_cmd_sub4(d,r);
	send_cmd_sub4(d+4,r+4);
}

/*-------------------------------------------------------------------------*/
const char* frontpanel_nc::fp_get_name(void)
{
        return "NCL1";
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_get_version(void)
{
	char d[8]={0xaa,0x9,0xff, 0x00,  0x00,0x00,0x00,0x00};
	char r[8];
	int n;
	send_cmd(d,r);
	for(n=0;n<8;n++)
		printf("%02x ",r[n]&255);
	puts("");
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_enable_messages(int n)
{
	char d[8]={0xaa,0x19,0xff, 0x00,  0x00,0x00,0x00,0x00};
	char r[8];
	d[3]=(n>>6)&3;	
	send_cmd(d,r);
	ir_filter=(n>>6)&3;	
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_set_leds(int blink, int state)
{
	int cmd;
	if (blink==0x10) { // on
		ioctl(fd, IOCTL_REEL_LED_BLINK, state<<16);
		ioctl(fd, IOCTL_REEL_LED_ON, state);
	}
	if (blink==0x30) { // blink
		ioctl(fd, IOCTL_REEL_LED_ON, state);
		ioctl(fd, IOCTL_REEL_LED_BLINK, state);
	}
	if (blink==0x20) { // off
		ioctl(fd, IOCTL_REEL_LED_BLINK, state<<16);
		ioctl(fd, IOCTL_REEL_LED_OFF, state);
	}
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_set_clock(void)
{
	char d[8]={0xaa,0x11,0xff, 0x00,  0x00,0x00,0x00,0x00};
	char r[8];
	time_t t=time(0);

	d[3]=t>>24;
	d[4]=t>>16;
	d[5]=t>>8;
	d[6]=t>>0;

	send_cmd(d,r);
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_set_wakeup(time_t t)
{
	char d[8]={0xaa,0x13,0xff, 0x00,  0x00,0x00,0x00,0x00};
	char r[8];

	d[3]=t>>24;
	d[4]=t>>16;
	d[5]=t>>8;
	d[6]=t>>0;

	send_cmd(d,r);
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_set_switchcpu(int timeout, int mode)
{
	char d[8]={0xaa,0x28,0xff, timeout,  0x42,0x00,0x00,0x00};
	char r[8];
	int n;
	send_cmd(d,r);
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_get_clock(void)
{	
	char d[8]={0xaa,0x1,0xff, 0x00,  0x00,0x00,0x00,0x00};
	char r[8];
	int n;
	send_cmd(d,r);
	printf("00 00 00 "); // only unix time available
	for(n=4;n<8;n++)
		printf("%02x ",r[n]&255);
	puts("");
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_get_clock_adjust(void)
{
	char d[8]={0xaa,0x4,0xff, 0x00,  0x00,0x00,0x00,0x00};
	char r[8];
	int n;
	send_cmd(d,r);
	for(n=0;n<8;n++)
		printf("%02x ",r[n]&255);
	puts("");
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_set_clock_adjust(int v1, int v2)
{
	char d[8]={0xaa,0x4,0xff, 0x00,  0x00,0x00,0x00,0x00}; 	
	// FIXME
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_get_wakeup(void)
{
	char d0[8]={0xaa,0x2,0xff, 0x00,  0x00,0x00,0x00,0x00};  // reason
	char d1[8]={0xaa,0x3,0xff, 0x00,  0x00,0x00,0x00,0x00};  // time
	char r[8];
	int n;
	send_cmd(d0,r);
	printf("00 00 %02x ",r[4]&255);
	send_cmd(d1,r);
	for(n=4;n<8;n++)
		printf("%02x ",r[n]&255);
	puts("");
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_get_key(void)
{
	int keyval;
	keyval=ioctl(fd, IOCTL_REEL_GET_KEY, 0);
	printf("%02x %02x\n",(keyval>>8)&255,keyval&255);
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_eeprom_flash(int a, int di)
{
	char d[8]={0xaa,0x35,0xff, a, di, 0x42, 0x00, 0x00 };
	char r[8];
	send_cmd(d,r);
}
/*-------------------------------------------------------------------------*/
int frontpanel_nc::fp_open_serial(void)
{
	fd=open(REELDRV_DEVICE, O_RDWR|O_NONBLOCK);
	if (fd<0)
		return -1;

	return 0;
}
/*-------------------------------------------------------------------------*/
int frontpanel_nc::fp_get_fd(void)
{
	return fd;
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_close_serial(void)
{
	close(fd);
}
/*-------------------------------------------------------------------------*/
size_t frontpanel_nc::fp_read(unsigned char *b, size_t len)
{
	return 0;
}
/*-------------------------------------------------------------------------*/
// Ruwido Decoder rstep-1

#define RUWI0A (213)
#define RUWI0B (461)

#define RUWI1A (546)
#define RUWI1B (806)

// bit value reversed
int frontpanel_nc::decode_ruwido(unsigned char *msg,  unsigned long long *ts, int diff, int bit)
{	
        int rbit;
        int old_xir_state;

	bit=!bit;

        if (!bit && (diff >(10L*IR_TMAX))) {
                xir_state=0; // Reset after gap
        }

        if (diff>=RUWI0A && diff<=RUWI0B) {
                rbit=0;
                rbit_sr=(rbit_sr<<1);
        }
        else if (diff>=RUWI1A && diff<=RUWI1B) {
                rbit=1;
                rbit_sr=(rbit_sr<<1)|1;
        }
        else {
                xir_state=0;
                goto skip_ruwido;
        }

//        printf("%i %i, rb %i %i \n",diff,bit,rbit,xir_state);
        
        if ((rbit_sr&7)==5) { // plausi check
                xir_state=0;
                goto skip_ruwido;
        }

	old_xir_state=xir_state;

        if (bit==1) { // falling
                if ((xir_state==0 && !rbit) ||  // startbit
                    xir_state ) { 
                        if ((rbit!=lb) || !xir_state ) {    // -_-_   1 1    _--_ 0 1
                                xir_state++;   
                                lb=1;
                        }
                }
        }
       else { // rising
                if (xir_state) {
                        if ((rbit==lb)) { // _-_- 0 0     -__-  1 0
                                xir_state++;    //   /            /
                                lb=0; 
                        }
                }
        }

        if (xir_state!=old_xir_state) {
                xir_data=(xir_data<<1)|lb;

	}
        
        if (xir_state==23) {
		xir_data&=0x7fffff;
                xir_state=0;
		//printf("Ruwido %x\n",xir_data);
		msg[0]=0x00;
		msg[1]=0xf2;
		msg[2]=0;
		msg[3]=(xir_data>>16)&0xff;
		msg[4]=(xir_data>>8)&0xff;
		msg[5]=xir_data&0xff;
#if !FPCTL
       		if (ts)
	       		*ts=get_timestamp();
#endif
		return 1;
        }
skip_ruwido:
        if (!xir_state) {
                rbit_sr=0;
        }

	return 0;
}
/*-------------------------------------------------------------------------*/

int frontpanel_nc::fp_read_msg(unsigned char *msg, unsigned long long *ts)
{
	int buf;
	int len,bit,m;
	struct pollfd pfd[]=  {{fd, POLLIN|POLLERR|POLLHUP,0}};
	int keyval;
	keyval=ioctl(fd, IOCTL_REEL_GET_KEY, 0);

        if (!(keyval&(REEL_KBD_BT0|REEL_KBD_BT1)))
                rpt=0;

        if (rpt>=6 && ((rpt&2)==0))
                keyval&=~(REEL_KBD_BT0|REEL_KBD_BT1);
        rpt++;
        if (keyval!=last_keyval) {
                msg[0]=0x00;
                msg[1]=0xf1;
                msg[2]=0;
                msg[3]=keyval;
                msg[4]=0;
                msg[5]=0;
                last_keyval=keyval; 
#if !FPCTL
		if (ts)
			*ts=get_timestamp();
#endif
                return 6;
        }

	m=poll(pfd, 1, 10);

	while(read(fd, &buf, 4)==4) {
		len=buf&0xffffff;
		bit=buf>>24;

		if (decode_ruwido(msg, ts, len, bit) && (ir_filter!=2))
			return 6;

		if (ir_filter==1)
			return 0;

//		printf("%i %i\n",irstate, len);
		if (len>IR_MAX) {                       
			cmd=0;
			irstate=0;
		}
		else {
			if (!irstate && bit)
				irstate++;
			else if (irstate)
				irstate++;
			
			if (irstate>=2 && !(irstate&1)) {
				int avg;
				int nibble;
				avg=hist[1]/2+len/2+hist[0];
				if (avg<IR_TMIN || avg>IR_MAX || len>IR_1MAX || len<IR_1MIN) {
					irstate=0;
				}
				else {
					nibble=((avg-IR_TMIN)/IR_STEP)&0xf;
					cmd=(cmd<<4)|nibble;
					if (irstate==16) {
						int csum,n;
						csum=0;
						for(n=0;n<8;n++)
							csum+=(cmd>>(4*n))&0xf;
						csum&=0xf;
						irstate=0;

						if (csum==0) {
							msg[0]=0x00;
							msg[1]=0xf2;
							msg[2]=(cmd>>24)&0xff;
							msg[3]=(cmd>>16)&0xff;
							msg[4]=(cmd>>8)&0xff;
							msg[5]=cmd&0xff;
//							printf("---------------%x \n",cmd);
#if !FPCTL
							if (ts)
								*ts=get_timestamp();
#endif
							return 6;
						}
					}
				}
			}
		}
		hist[2]=hist[1];
		hist[1]=hist[0];
		hist[0]=len;		
	}

	return 0;
}
/*-------------------------------------------------------------------------*/
void frontpanel_nc::fp_start_handler(void)
{
#ifndef FPCTL
	pthread_t thread;
	// start ir handler
	if (pthread_create(&thread, NULL, frontpanel_rs232::fp_serial_thread, this)) 
                pthread_detach(thread); 
#endif
}
