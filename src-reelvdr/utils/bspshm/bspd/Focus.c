/***************************************************************************
 *   Copyright (C) 2005 by Reel Multimedia                                 *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/***************************************************************************/
//  Focus.c
/***************************************************************************/

#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "bspchannel.h"
#include <pthread.h>

extern volatile bspd_data_t *bspd;

typedef unsigned int __u32;
typedef unsigned short __u16;
typedef unsigned char __u8;
typedef unsigned char BYTE;
            
#define I2C_RDWR        0x0707  /* Combined R/W transfer (one stop only)*/
#define I2C_M_RD        0x01
            
struct i2c_msg {
	__u16 addr;     /* slave address                        */
	__u16 flags;            
	__u16 len;              /* msg length                           */
	__u8 *buf;              /* pointer to msg data                  */
};
            
struct i2c_rdwr_ioctl_data {
	struct i2c_msg *msgs;    /* pointers to i2c_msgs */
	__u32 nmsgs;                    /* number of i2c_msgs */
};
            
#define FS453_I2C_ADDR 0x6a

//---------------------------------------------------------------------------
unsigned long long GetTime()
{
	struct  timeval  ti;
	struct  timezone tzp;
	gettimeofday(&ti,&tzp);
	return ti.tv_usec + 1000000LL * ti.tv_sec;
}
//---------------------------------------------------------------------------
unsigned int ReadFs453(int fd, int offset, int length)
{
	struct i2c_rdwr_ioctl_data data;
	struct i2c_msg msg[2];
	unsigned char buf[20];
	unsigned char buf1[20];
	data.msgs=msg;
	data.nmsgs=2;
                
	msg[0].addr=FS453_I2C_ADDR;
	msg[0].flags=0;
	msg[0].len=1;
	msg[0].buf=buf;
	buf[0]=offset;
                
	msg[1].addr=0x6a;
	msg[1].flags=I2C_M_RD;
	msg[1].len=length;
	msg[1].buf=buf1;
                
	buf1[0]=buf1[1]=buf1[2]=buf1[3]=0;
	ioctl(fd, I2C_RDWR,&data);
	return buf1[0]+(buf1[1]<<8)+(buf1[2]<<16)+(buf1[3]<<24);
}
//---------------------------------------------------------------------------
void WriteFs453_sub(int fd, int offset, unsigned int value, int length)
{
	struct i2c_rdwr_ioctl_data data;
	struct i2c_msg msg[2];
	unsigned char buf[20];
            
	data.msgs=msg;
	data.nmsgs=1;
            
	msg[0].addr=FS453_I2C_ADDR;
	msg[0].flags=0;
	msg[0].len=1+length;
	msg[0].buf=buf;
	buf[0]=offset;
	buf[1]=value&255;
	buf[2]=(value>>8)&255;
	buf[3]=(value>>16)&255;
	buf[4]=(value>>24)&255;
            
	ioctl(fd, I2C_RDWR, &data);
}
//---------------------------------------------------------------------------
void WriteFs453(int fd, int offset, unsigned int value, int length)
{
	int n;
	unsigned long xvalue;
	for(n=0;n<5;n++) {
		WriteFs453_sub(fd, offset, value,  length);		
		xvalue=ReadFs453(fd,offset,length);
		if (value==xvalue || offset==0xae || offset==0xb0)
			break;
		printf("RETRY\n");
	}
}
//---------------------------------------------------------------------------
void WaitFocusFieldSync(int fd)
{
	int v, c = 0;
	unsigned long long t1,t2;
	t1 = GetTime();
	WriteFs453(fd, 0xae, 0xffff, 2);
	for (;;)
	{
		v = ReadFs453(fd, 0xae, 2);
		if (v == 0)
		{
                        return;
		}
		t2 = GetTime();
		if ( (t2-t1) > 60*1000LL) // Timeout after 60ms
		{
                        printf("bspd: WaitFocusFieldSync timeout!\n");
                        return;
		}
		++c;
	}
}

void RealtimePrioOff()
{
    pthread_t threadId = pthread_self();
    int p;
    struct sched_param param;
    pthread_getschedparam(threadId, &p, &param);
    param.sched_priority = 0;
    pthread_setschedparam(threadId, SCHED_OTHER, &param);
}

void RealtimePrioOn()
{
    pthread_t threadId = pthread_self();
    int p;
    struct sched_param param;
    pthread_getschedparam(threadId, &p, &param);
    param.sched_priority = 4;
    pthread_setschedparam(threadId, SCHED_RR, &param);
}

//---------------------------------------------------------------------------
void SyncWithFocus(volatile bspd_data_t *bspd)
{
       bspd->BspPortFocusSync=0x11111111;
       while (bspd->BspPortFocusSync == 0)
       {
               usleep(5000); // sleep 5 ms
       }
       usleep(200*1000); 

       // Synchronize field with Focus.
       int fd = open("/dev/i2c-0",O_RDWR);
       if (fd < 0) 
       {
               printf("bspd: Can't open /dev/i2c-0!\n");
               bspd->BspPortFocusSync=0x11111111;
               return;
       }

       RealtimePrioOn();

       // nice(-20); // highest Prio (not realtime...) FIXME real realtime priority on
       WaitFocusFieldSync(fd);
       usleep(8000); // sleep 8 ms
       bspd->BspPortFocusSync=0x11111111;

       // FIXME real realtime priority off
       // nice(20);
       RealtimePrioOff();

       while (bspd->BspPortFocusSync == 0x11111111)
               usleep(1000);
       close(fd);
}
//---------------------------------------------------------------------------
// FIXME: own implementation
int focus_set_bccf_sdtv(int brightness, int contrast, int colour, int flicker)
{
	FILE *f=popen("fs453 -n >/dev/null", "w");
	if (f) {
		
		fprintf(f,"b %i\n", bspd->picture.values[0]);
		fprintf(f,"c %i\n", bspd->picture.values[1]);
		fprintf(f,"o %i\n", bspd->picture.values[2]);
		fprintf(f,"f %i\n", bspd->picture.values[5]);
		fprintf(f,"q\n");
		pclose(f);
	}
       return 0;
}
//---------------------------------------------------------------------------
int focus_set_pll( int khz)
{
	int xtal;
	int n,m,p,fr,gain;
	int r,found=0;

	xtal=27000;
	n=216;
	fr=xtal/n;

	m=0;
	printf("bspd: SET PLL %i\n",khz);
	for(p=1;p<10;p++) {
		if (p*khz>=100000 && p*khz<300000) {
			m=(p*khz)/fr;
			if (m>3000)
				continue;
			else {
				found=1;
				break;
			}
		}
	}

	if (!found) {
		printf("bspd: PLL set failed\n");
		return 1;
	}

	if ((khz*p)<125000)
		gain=1;
	else if ((khz*p)<175000)
		gain=2;
	else if ((khz*p)<225000)
		gain=3;
	else if ((khz*p)<275000)
		gain=4;
	else
		gain=5;
	
//	printf("n %i, m %i, p %i, f %i, fc %i\n",n,m,p,khz,xtal*m/(n*p));
	int fd = open("/dev/i2c-0",O_RDWR);
	WriteFs453(fd, 0x10, 0, 4);
	WriteFs453(fd, 0x14, 0, 4);
	WriteFs453(fd, 0x18, (gain<<12)|(m-17), 4);
	WriteFs453(fd, 0x1a, n-1, 2);
	WriteFs453(fd, 0x1c, ((p-1)<<8)|(p-1), 2);
	r=ReadFs453(fd, 0xc,2);
	WriteFs453(fd, 0x0c, r&~2, 2);
	WriteFs453(fd, 0x0c, r|2, 2);
	close(fd);
	return 0;
}
//---------------------------------------------------------------------------
void focus_set_WSS(int norm, int value)
{
	int fd = open("/dev/i2c-0",O_RDWR);
	if (fd<0)
		return;

	printf("bspd: WSS Norm %i, val %x\n",norm,value);
	if (norm==0) { // PAL
		// FIXME PAL-60
		WriteFs453(fd, 0x89, 0x16,1);  // x10 for PAL-60
		WriteFs453(fd, 0x8a, 0x16,1);
		WriteFs453(fd, 0x81, 0x62f,2);
		WriteFs453(fd, 0x8b, 0xc0,2);
		WriteFs453(fd, 0x83, value,3);
		WriteFs453(fd, 0x86, value,3);
		WriteFs453(fd, 0x80, 0x76,1); 
	}
	else {
		// FIXME Check
		WriteFs453(fd, 0x89, 0x13, 1);
		WriteFs453(fd, 0x8a, 0x13, 1);
		WriteFs453(fd, 0x81, 0xc21, 2);
		WriteFs453(fd, 0x83, value, 3);
		WriteFs453(fd, 0x86, value, 3);
		WriteFs453(fd, 0x80,0x66,1); 
	}
	close(fd);
}
//---------------------------------------------------------------------------
void focus_set_tff(int tff)
{
	int val;
	int fd = open("/dev/i2c-0",O_RDWR);
	if (fd<0)
		return;
	val=ReadFs453(fd, 0x2,2);
	val=val&~1;
	val+=(tff?0:1);
	WriteFs453(fd, 0x2, val, 2);
	
}
//---------------------------------------------------------------------------
