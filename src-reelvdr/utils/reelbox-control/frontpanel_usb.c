/*
 * Driver for USB OLED display
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
#include <usb.h>
#include <pthread.h>

#include "frontpanel.h"

// Some magic timings
#define IR_1MIN  80
#define IR_1MAX  350
#define IR_TMIN  915
#define IR_TMAX  1025
#define IR_STEP  137
#define IR_MAX   3300

#define VENDOR_ID         0x16d0
#define PRODUCT_ID        0x054b

// USB cmds
#define LED_RED 0xC3
#define LED_GREEN 0xC4
#define GET_IR0 0xC5
#define GET_IR1 0xC6

unsigned long long get_timestamp(void);

/*-------------------------------------------------------------------------*/
void frontpanel_usb::fp_display_brightness(int n)
{
	brightness=(n>>4)&15;
}
/*-------------------------------------------------------------------------*/
#define BRIGHTNESS_STATE "/tmp/brightness_usb.dat"

void frontpanel_usb::fp_set_led_brightness(int v)
{
	FILE *f;
	v=2*v;
	if (v<0) 
		v=0;
	if (v>50)
		v=50;

	brightness_usb=v;
	f=fopen(BRIGHTNESS_STATE,"w");
	if (f) {
		fprintf(f,"%i\n",v);
		fclose(f);
	}
}
/*-------------------------------------------------------------------------*/
void frontpanel_usb::fp_set_leds(int blink, int state)
{
	int n;
	char dummy[64];
	if (brightness_usb==-1) {
		FILE *f;
		f=fopen(BRIGHTNESS_STATE,"r");
		if (f) {
			fscanf(f,"%i",&brightness_usb);
			fclose(f);
		}
	}

	if (brightness_usb<0) 
		brightness_usb=0;
	if (brightness_usb>50)
		brightness_usb=50;

	for(n=0;n<MAX_USB_HANDLES;n++) {
		usb_dev_handle *h;
		pthread_mutex_lock(&displays[n].lock);
		if (!displays[n].state) {
			pthread_mutex_unlock(&displays[n].lock);
			continue;
		}
		h=displays[n].handle;
		if (blink==0x10) { // on
			if (state&4)
				usb_control_msg(h, 0xA0, LED_RED, brightness_usb, 1, dummy, 2, 100);
			if (state&1)
				usb_control_msg(h, 0xA0, LED_GREEN, brightness_usb, 1, dummy, 2, 100);
		}
		if (blink==0x30) { // blink
			if (state&4)
				usb_control_msg(h, 0xA0, LED_RED, brightness_usb, 1, dummy, 2, 100);
			if (state&1)
				usb_control_msg(h, 0xA0, LED_GREEN, brightness_usb , 1, dummy, 2, 100);
		}
		if (blink==0x20) { // off
			if (state&4)
				usb_control_msg(h, 0xA0, LED_RED, 0, 1, dummy, 2, 100);
			if (state&1)
				usb_control_msg(h, 0xA0, LED_GREEN, 0, 1, dummy, 2, 100);
		}
		pthread_mutex_unlock(&displays[n].lock);
	}
}

/*-------------------------------------------------------------------------*/
#define DISP_RESET_ON     0xb9
#define DISP_RESET_OFF    0xb8
#define DISP_NORMAL       0xc2

#define INIT_DATA_SIZE    90

static unsigned char ReelUSBFP_InitData[] =
{ 
0x15, 0x00, /* Set Column Address */
0x00, 0x00, /* Start = 0 */
0x3F, 0x00, /* End = 127 */

0x75, 0x00, /* Set Row Address */
0x00, 0x00, /* Start = 0 */
0x3F, 0x00, /* End = 63 */

0x81, 0x00, /* Set Contrast Control (1) */
0x7F, 0x00, /* 0 ~ 127 */

0x86, 0x00, /* Set Current Range 84h:Quarter, 85h:Half, 86h:Full*/

0xA0, 0x00, /* Set Re-map */
0x41, 0x00, /* [0]:MX, [1]:Nibble, [2]:H/V address [4]:MY, [6]:Com Split Odd/Even "1000010"*/

0xA1, 0x00, /* Set Display Start Line */
0x00, 0x00, /* Top */

0xA2, 0x00, /* Set Display Offset */
0x44, 0x00, /*44 Offset 76 rows */

0xA4, 0x00, /* Set DisplaMode,A4:Normal, A5:All ON, A6: All OFF, A7:Inverse */

0xA8, 0x00, /* Set Multiplex Ratio */
0x3F, 0x00, /* 64 mux*/

0xB1, 0x00, /* Set Phase Length */
0x53, 0x00, /*53 [3:0]:Phase 1 period of 1~16 clocks */

/* [7:4]:Phase 2 period of 1~16 clocks POR = 0111 0100 */

0xB2, 0x00, /* Set Row Period */
0x46, 0x00, /* 46 [7:0]:18~255, K=P1+P2+GS15 (POR:4+7+29)*/

0xB3, 0x00, /* Set Clock Divide (2) */
0x91, 0x00, /* [3:0]:1~16, [7:4]:0~16, 70Hz */

0xBF, 0x00, /* Set VSL */
0x0D, 0x00, /* [3:0]:VSL */

0xBE, 0x00, /* Set VCOMH (3) */
0x02, 0x00, /* [7:0]:VCOMH, (0.51 X Vref = 0.51 X 12.5 V = 6.375V)*/

0xBC, 0x00, /* Set VP (4) */
0x0F, 0x00, /* [7:0]:VP, (0.67 X Vref = 0.67 X 12.5 V = 8.375V) */

0xB8, 0x00, /* Set Gamma with next 8 bytes */
0x01, 0x00, /* L1[2:1] */
0x11, 0x00, /* L3[6:4], L2[2:0] 0001 0001 */
0x22, 0x00, /* L5[6:4], L4[2:0] 0010 0010 */
0x32, 0x00, /* L7[6:4], L6[2:0] 0011 1011 */
0x43, 0x00, /* L9[6:4], L8[2:0] 0100 0100 */
0x54, 0x00, /* LB[6:4], LA[2:0] 0101 0101 */
0x65, 0x00, /* LD[6:4], LC[2:0] 0110 0110 */
0x76, 0x00, /* LF[6:4], LE[2:0] 1000 0111 */

0xAD, 0x00, /* Set DC-DC */
0x03, 0x00, /* 03=ON, 02=Off */

0xAF, 0x00, /* AF=ON, AE=Sleep Mode */
0xA4, 0x00, /* Set DisplaMode,A4:Normal, A5:All ON, A6: All OFF, A7:Inverse */
0xA1, 0x00, /* Set Display Start Line */
0x00, 0x00 /* Top */
};

void frontpanel_usb::fp_show_pic(unsigned char **LCD)
{
	char receive_data[64];
        int send_status;
	unsigned char lcd_data[8192];
	int i,j,k,n;

	// Convert image
	n=0;
	for(i=63;i>=0;i--) {
		for(j=15;j>=0;j--) {
			unsigned char v=LCD[j][i];
			for(k=0;k<8;k+=2) {
				int x=0;
				if (v&(1<<k))
					x|=(brightness<<4);
				if (v&(1<<(k+1)))
					x|=brightness;
				lcd_data[2*n]=x;
				lcd_data[2*n+1]=0xff;
				n++;
			}
		}
	}

	for(i=0;i<MAX_USB_HANDLES;i++) {
		usb_dev_handle *disp_handle;
		pthread_mutex_lock(&displays[i].lock);
		if (!displays[i].state) {
			pthread_mutex_unlock(&displays[i].lock);
			continue;
		}
		disp_handle=displays[i].handle;

		send_status = usb_control_msg(disp_handle, 0xA0, DISP_RESET_ON, 0, 1, receive_data, 4, 200);
		usleep(10*1000);
		send_status = usb_control_msg(disp_handle, 0xA0, DISP_RESET_OFF, 0, 1, receive_data, 4, 200);
		usleep(50*1000);
		send_status = usb_control_msg(disp_handle, 0xA0, DISP_NORMAL, 0, 1, receive_data, 64, 200);
		usleep(100*1000);

		usb_claim_interface(disp_handle, 0);
		// int usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int size, int timeout);
		send_status = usb_bulk_write(disp_handle, 2, (char*) (ReelUSBFP_InitData), INIT_DATA_SIZE, 200);
		
		if (send_status != INIT_DATA_SIZE)
			goto error_out;

		usb_bulk_write(disp_handle, 2, (char*) (lcd_data), 8192, 100);

	error_out:
		usb_release_interface(disp_handle, 0);	
		pthread_mutex_unlock(&displays[i].lock);
	}

}
/*-------------------------------------------------------------------------*/
void frontpanel_usb::fp_refresh_usb(void)
{
        struct usb_bus *bus;
        struct usb_device *dev;
        usb_dev_handle *disp_handle = 0;
        int ret1,ret2,i;

        ret1=usb_find_busses();
	if (ret1 == 0) { }; // dummy
        ret2=usb_find_devices();
	if (ret2 == 0) { }; // dummy
        for (bus = usb_busses; bus; bus = bus->next)
        {
		// printf("fp_refresh_usb: bus=%p\n", bus);
                for (dev = bus->devices; dev; dev = dev->next)
                {
			// printf("fp_refresh_usb: dev=%p\n", dev);
                        if ( ((dev->descriptor.idVendor == VENDOR_ID) && (dev->descriptor.idProduct == PRODUCT_ID)) 
			     // || ((dev->descriptor.idVendor == VENDOR_ID1) && (dev->descriptor.idProduct == PRODUCT_ID1))
				)
                        {
                                int found=0;
                                for(i=0;i<MAX_USB_HANDLES;i++) {
                                        if (displays[i].state && displays[i].device==dev) { // display already known
                                                displays[i].state=2;
                                                found=1;
                                                break;
                                        }
                                }
                                if (found)
                                        break;
                                // printf("New GrauTec display device found at bus %s, device %s \n", dev->bus->dirname, dev->filename);
                                for(i=0;i<MAX_USB_HANDLES;i++) {
                                        if (!displays[i].state) {
                                                disp_handle = usb_open(dev);
                                                if (disp_handle) {
//                                                        int ret=SetupDisplay(disp_handle);
                                                        displays[i].handle=disp_handle;
                                                        displays[i].device=dev;
                                                        displays[i].state=2;     // State must be last
                                                        // printf("Allocated display %i\n",i);
                                                }
                                                break;
                                        }
                                }
                        }
                }
        }

        // Cleanup dead displays

        for(i=0;i<MAX_USB_HANDLES;i++) {
                if (displays[i].state==1) {
			pthread_mutex_lock(&displays[i].lock);
                        // printf("Deallocated display %i\n",i);
                        if (displays[i].handle)
                                usb_close(displays[i].handle);
                        displays[i].state=0;
                        displays[i].handle=NULL;
                        displays[i].device=NULL;
			pthread_mutex_unlock(&displays[i].lock);
                }
		else if (displays[i].state==2) {
			displays[i].state=1; // no race here
                }
        }
//        last_refresh=time(0);
}
/*-------------------------------------------------------------------------*/
// put into thread, as the refresh seems to block sometimes for a quite long time in the kernel
void *fp_usb_refresh_thread(void* para)
{
	frontpanel_usb *fp=(frontpanel_usb*)para;
	sleep(5); // Avoid immediate refresh in case of reelfpctl
	while(1) {
		fp->fp_refresh_usb();
		sleep(10);
	}
	return NULL;
}
/*-------------------------------------------------------------------------*/
#define RUWI0A (30) // initial pulse sometimes very short
#define RUWI0B (480)

#define RUWI1A (546)
#define RUWI1B (806+80)

int frontpanel_usb::decode_rstep(unsigned int *ir_data, int len, unsigned int *c1)
{
	int n;
	int xir_state=0,rbit_sr=0,lb=0;
	int xir_data=0;

	for(n=0;n<len;n++) {
		int rbit,bit,diff;
		int old_xir_state;

		bit=!(n&1);
		diff=ir_data[n];

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

		//printf("%i %i, rb %i %i \n",diff,bit,rbit,xir_state);
        
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
					xir_state++;   
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
//			printf("Ruwido %x\n",xir_data);
			*c1=xir_data;
			return 1;
		}
	skip_ruwido:
		if (!xir_state) {
			rbit_sr=0;
		}

	}
	return 0;
}
/*-------------------------------------------------------------------------*/
void frontpanel_usb::decode_xmp(unsigned int *ir_data, unsigned int *c1, unsigned int *c2)
{
	unsigned int csum,c,i;

	// correct AGC distortion
	ir_data[4]=(ir_data[4]+2*ir_data[6])/3;
	ir_data[2]=(ir_data[2]+2*ir_data[4])/3;		       
			
	csum=0;
	c=0;
	for(i=0;i<8;i++) {
		int l=ir_data[2+2*i]/2+
			ir_data[3+2*i]+
			ir_data[4+2*i]/2;

		int v=(l-915)/137;

		if (v<0 || v>15) {
			c=-1;
			break;
		}
		c=(c<<4)|v;
		csum+=v;
	}
	csum&=15;
	//printf("C1: %08x  %i\n",c,csum);
	if (csum==0 && c!=0xffffffff)
		*c1=c;
			
	csum=0;
	c=0;
	for(i=9;i<17;i++) {
		int l=ir_data[2+2*i]/2+
			ir_data[3+2*i]+
			ir_data[4+2*i]/2;
		int v=(l-915)/137;
		if (v<0 || v>15) {
			c=-1;
			break;
		}
		c=(c<<4)|v;
		csum+=v;
	}

	csum&=15;
	//printf("C2: %08x  %i\n",c,csum);
	if (csum==0 && c!=0xffffffff)
		*c2=c;

}
/*-------------------------------------------------------------------------*/
#define MAX_CMDS 16


void frontpanel_usb::fp_read_ir(void)
{
	unsigned int n,i;
	unsigned int c1=0,c2=0;
	int busy=0;

	for(n=0;n<MAX_USB_HANDLES;n++) {
		unsigned char buffer0[64]={0};
		unsigned char buffer1[64]={0};

		int v1=0,v2=0;

		usb_dev_handle *h;
		if (!displays[n].state)
			continue;
		busy=1;
		h=displays[n].handle;

		v1 =usb_control_msg(h, 0xA0, GET_IR0, 0, 1, (char *) buffer0, 50, 50);
		if (v1>0 && v1<64)
			v2 =usb_control_msg(h, 0xA0, GET_IR1, 0, 1, (char *) buffer1, 50, 50);
		
		if(v1>0 && v2>0 && v1<64 && v2<64)  {
			if (v1!=50) { // old firmware
				buffer0[48]=36;
				buffer0[49]=0;
				buffer1[48]=36;
				buffer1[49]=0;
			}
			unsigned int ir_data[80]={0};
			
			unsigned int len=buffer0[48];
//			printf("---- %i \n",len);
			for (i = 0; i < len/2+1; i++) {
				ir_data[2*i]   = (buffer1[i*2] + buffer1[i*2+1]*256)*.25;
				ir_data[2*i+1] = (buffer0[i*2] + buffer0[i*2+1]*256)*.25;
//				printf("%i %i %i\n",i,ir_data[2*i],ir_data[2*i+1]);
			}

			if (!decode_rstep(ir_data,len+1,&c1)) {
				if (len==36)
					decode_xmp(ir_data,&c1,&c2); // ! side effect: changes ir_data!
			}		       
		}	
	}	
	
	if (c1) {
		cmd[cmd_wp++]=c1;
		cmd_wp=cmd_wp%MAX_CMDS;
	}

	if (c2) {
		cmd[cmd_wp++]=c2;
		cmd_wp=cmd_wp%MAX_CMDS;
	}
/*	if (c1 || c2)
		printf("XXXXXX %08x %08x\n",c1,c2);
*/	
	if (!busy)
		usleep(100*1000); // no display attached
}
/*-------------------------------------------------------------------------*/

int frontpanel_usb::fp_read_msg(unsigned char *msg, unsigned long long *ts)
{
	fp_read_ir();

	if (cmd_rp!=cmd_wp) {
		msg[0]=0x00;
		msg[1]=0xf2;
		msg[2]=(cmd[cmd_rp]>>24)&0xff;
		msg[3]=(cmd[cmd_rp]>>16)&0xff;
		msg[4]=(cmd[cmd_rp]>>8)&0xff;
		msg[5]=cmd[cmd_rp]&0xff;
		cmd_rp++;
		cmd_rp=cmd_rp%MAX_CMDS;
#if ! FPCTL
		*ts=get_timestamp();
#endif
		return 6;
	}
	return 0;
}
#ifndef FPCTL
/*-------------------------------------------------------------------------*/
void * fp_usb_thread(void* p)
{
	frontpanel_usb *fp=(frontpanel_usb*)p;
	unsigned char msg[16];
	unsigned long long ts;
	int l;
	ir_cmd_t cmd={0};
	ir_state_t state={0};

	while(1) {
		cmd.source=1; // USB
		handle_key_timing(&state, &cmd); // shift status etc.

		state.available=0;
		l=fp->fp_read_msg(msg, &ts);
		if (!l)
			continue; // message not yet complete

		if (msg[1]==0xf1 || msg[1]==0xf2) {
			handle_ir_key(&state, &cmd, msg, ts);
		}
	}
	return NULL;
}
#endif
/*-------------------------------------------------------------------------*/
void frontpanel_usb::fp_start_handler(void)
{
#ifndef FPCTL
	pthread_t thread;
	if (pthread_create(&thread, NULL, fp_usb_refresh_thread, this))
		pthread_detach(thread);

        // Start msg handler
	if (pthread_create(&thread, NULL, fp_usb_thread, this))
		pthread_detach(thread);        
#endif
}

/*-------------------------------------------------------------------------*/
// Constructor
frontpanel_usb::frontpanel_usb(const char *device)
{
	int n;
	// struct usb_bus *bus; // Wunused-variable
        // struct usb_device *dev; // Wunused-variable

	num_handles=0;
	brightness=8;
	brightness_usb=-1;
	cmd_wp=0;
	cmd_rp=0;

	usb_init();
	for(n=0;n<MAX_USB_HANDLES;n++) {
		// printf("frontpanel_usb: %d\n", n);
		pthread_mutex_init(&displays[n].lock,NULL);
		displays[n].state = 0; // init state
	}

	fp_refresh_usb();

}	

/*-------------------------------------------------------------------------*/
int frontpanel_usb::fp_available(const char *device, int force)
{
        return 1; // Always available
}

