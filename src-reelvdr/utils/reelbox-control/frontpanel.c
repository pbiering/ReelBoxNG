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

#include "frontpanel.h"
unsigned long long get_timestamp(void);

int is_avg3=0;

#define OVER_ALL_FPS(x) \
	for(int nn=0;nn<MAX_FRONTPANELS;nn++) { \
		if (frontpanels[nn]) { \
			frontpanels[nn]->x; \
		} \
	}

// Avoid ICE
#define OVER_ALL_FPS_X(x) \
	for(int nn=0;nn<MAX_FRONTPANELS;nn++) { \
		if (frontpanels[nn]) { \
			if (!is_avg3 || strcmp(frontpanels[nn]->fp_get_name(),"ICE")) \
				frontpanels[nn]->x;			\
		} \
	}


#ifndef RBMINI

// Use AVR-FP if available ("executive" FP)
#define OVER_MAIN_FP(x) \
	{ frontpanel* f=fp_get_frontpanel("AVR"); \
		if (f) \
			f->x; \
		else { \
			f=fp_get_frontpanel("ICE"); \
			if (f) \
				f->x; \
		} \
	}	
	
#else

#define OVER_MAIN_FP(x) \
	{ frontpanel* f=fp_get_frontpanel("NCL1"); \
		if (f) \
			f->x; \
	}

#endif

/*-------------------------------------------------------------------------*/
frontpanel_registry::frontpanel_registry(void)
{
	for(int n=0;n<MAX_FRONTPANELS;n++)
		frontpanels[n]=NULL;
	num_frontpanels=0;

	struct stat st;
	int r=stat("/dev/.have_frontpanel",&st);
	if (r==0)
		is_avg3=1;
}
/*-------------------------------------------------------------------------*/
frontpanel_registry::~frontpanel_registry(void)
{
}
/*-------------------------------------------------------------------------*/
void frontpanel_registry::register_unit(frontpanel* f)
{
	for(int n=0;n<MAX_FRONTPANELS;n++) {
		if (frontpanels[n]==NULL) {
			frontpanels[n]=f;
//			printf("Register %s\n",f->fp_get_name());
			num_frontpanels++;
			break;
		}
	}
}
/*-------------------------------------------------------------------------*/
void frontpanel_registry::fp_noop(void)
{
	OVER_ALL_FPS_X(fp_noop());
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_get_version(void)
{
	OVER_ALL_FPS_X(fp_get_version());
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_enable_messages(int n)
{
	OVER_ALL_FPS_X(fp_enable_messages(n));
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_display_brightness(int n)
{
	OVER_ALL_FPS_X(fp_display_brightness(n));
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_display_contrast(int n)
{
	OVER_ALL_FPS_X(fp_display_contrast(n));
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_clear_display(void)
{
	OVER_ALL_FPS_X(fp_clear_display());
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_set_leds(int blink, int state)
{
	OVER_ALL_FPS_X(fp_set_leds(blink,state));
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_set_clock(void)
{
	OVER_ALL_FPS_X(fp_set_clock());
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_set_wakeup(time_t t)
{
	OVER_ALL_FPS_X(fp_set_wakeup(t));
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_set_displaymode(int m)
{
	OVER_ALL_FPS_X(fp_set_displaymode(m));
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_set_switchcpu(int timeout, int mode)
{
#ifndef RBMINI
	// If ICE available, use it (AVR FP can't switch)
	frontpanel* f1=fp_get_frontpanel("AVR");
	frontpanel* f2=fp_get_frontpanel("ICE");
	if (f2)
		f2->fp_set_switchcpu(timeout, mode);
	else if (f1)
		f1->fp_set_switchcpu(timeout, mode);
#else
	frontpanel* f1=get_frontpanel("NCL1");
	if (f1)
		f1->fp_set_switchcpu(timeout, mode);
#endif
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_get_clock(void)
{
	OVER_MAIN_FP(fp_get_clock());
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_get_wakeup(void)
{
	OVER_MAIN_FP(fp_get_wakeup());
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_get_key(void)
{
	OVER_MAIN_FP(fp_get_key());
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_set_clock_adjust(int v1, int v2)
{
	OVER_ALL_FPS_X(fp_set_clock_adjust(v1,v2));
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_set_led_brightness(int v)
{
	OVER_ALL_FPS(fp_set_led_brightness(v));
}

/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_get_flags(void)
{
	OVER_ALL_FPS_X(fp_get_flags());
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_power_control(int v)
{
#ifndef RBMINI
	frontpanel* f1=fp_get_frontpanel("AVR");
	frontpanel* f2=fp_get_frontpanel("ICE");
	if (f2 && (v&0xff000000)==0x13000000) {
		if ((v&1)==0) // allow WOL
			f2->fp_power_control(0x005a1200);
		else {
			f2->fp_power_control(0x005a1300); // disable WOL power
			if (f1)
				f2->fp_power_control(0x005a1100); // Full power off (wakeup not by ICE)
		}			
	}
	else if (!f1 && f2) {
		f2->fp_power_control(v);
	}
	else if (f1)
		f1->fp_power_control(v);	
#else
	OVER_ALL_FPS(fp_power_control(v));
#endif
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_eeprom_flash(int a, int d)
{
	OVER_ALL_FPS_X(fp_eeprom_flash(a,d));
}
/*-------------------------------------------------------------------------*/
void  frontpanel_registry::fp_show_pic(unsigned char **LCD)
{
	OVER_ALL_FPS_X(fp_show_pic(LCD));
}
/*-------------------------------------------------------------------------*/
int frontpanel_registry::fp_get_fd(const char* name)
{
	int n;
	for(n=0;n<MAX_FRONTPANELS;n++) {
		if (frontpanels[n]!=NULL) {
			if (strcmp(frontpanels[n]->fp_get_name(),name)==0)
				return frontpanels[n]->fp_get_fd();
		}
	}
	return -1;
}
/*-------------------------------------------------------------------------*/
frontpanel* frontpanel_registry::fp_get_frontpanel(const char* name)
{
	int n;
	for(n=0;n<MAX_FRONTPANELS;n++) {
		if (frontpanels[n]!=NULL) {
			if (strcmp(frontpanels[n]->fp_get_name(),name)==0)
				return frontpanels[n];
		}
	}
	return NULL;
}
/*-------------------------------------------------------------------------*/
void frontpanel_registry::fp_start_handler(void)
{
	OVER_ALL_FPS_X(fp_start_handler());
}
/*-------------------------------------------------------------------------*/
void frontpanel_registry::force_ice(void)
{
	is_avg3=0;
}
/*-------------------------------------------------------------------------*/
int u_sleep(long long usec)
{
	struct timespec st_time;
	st_time.tv_sec = 0;
	st_time.tv_nsec = (usec * 1000);
	return nanosleep(&st_time,NULL);
}
