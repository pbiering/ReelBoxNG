/*
 * Frontpanel communication ICE/internal keyboard
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
#include <sys/io.h>
#include "frontpanel.h"

// 82=GPIO = ANALOG_AUDIO_MUTE
// 81=GPIO3 = GREEN
// 80=GPIO2 = BLUE
// 79=GPIO1 = RED
// 78=GPIO0 = ENABLE_TUNER
#define STD_FLG (CS8|CREAD|HUPCL)

/*-------------------------------------------------------------------------*/
void frontpanel_ice::gpio_set(int num, int val)
{
	if (!init_done)
		gpio_init();

	if (num<0 || num>4)
		return;

	int v=inb(0x1088);
	v=(v&~(1<<num));
	v=(v|((val&1)<<num));
	outb(v,0x1088);
}
/*-------------------------------------------------------------------------*/
int frontpanel_ice::gpio_get(int num)
{
	if (!init_done)
		gpio_init();
	if (num<0 || num>4)
		return 0;

	int v=inb(0x1088);
	return (v>>num)&1;
}
/*-------------------------------------------------------------------------*/
void frontpanel_ice::gpio_init(void)
{
	iopl(3);
	outb(0x1f,0x1080);
	outb(inb(0x1084)&~0x1f,0x1084);
	init_done=1;
}
/*-------------------------------------------------------------------------*/
// red is latched during rising edge of blue
// storage
void frontpanel_ice::fp_set_leds(int blink, int state)
{
	// int cmd; // Wunused-variable
	int ledstate=(gpio_get(1)<<2)|(gpio_get(2)<<1)|(gpio_get(3));

	if (blink==0x10 || blink==0x30) { // on/blink
		ledstate|=state;

		if (ledstate&1)
			gpio_set(3,1);

		if (ledstate&4)
			gpio_set(1,1);

		gpio_set(2,0);
		gpio_set(2,1); // rising edge
		if (!(ledstate&2))
			gpio_set(2,0);
		
	}	
	if (blink==0x20) { // off
		ledstate&=~state;
		if (!(ledstate&1))
			gpio_set(3,0);	
		if (!(ledstate&4))
			gpio_set(1,0);

		gpio_set(2,0);
		gpio_set(2,1);
		if (!(ledstate&2))
			gpio_set(2,0);
	}

	if (blink==0x10 && state&0x10)
		gpio_set(0,1);
	if (blink==0x20 && state&0x10)
		gpio_set(0,0);
	if (blink==0x10 && state&0x20)
		gpio_set(4,1);
	if (blink==0x20 && state&0x20)
		gpio_set(4,0);

}
/*-------------------------------------------------------------------------*/
void frontpanel_ice::fp_send_cec(unsigned char* buf)
{

}
