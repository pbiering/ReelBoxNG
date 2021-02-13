/*****************************************************************************/
/* bspecho.c -- Textoutput to BSP15 OSD framebuffer
 *
 * Copyright (C) 2006 Georg Acher, BayCom GmbH (acher at baycom dot de)
 *               for Reel Multimedia AG
 *
 * #include <GPL-header>
 *
 * $Id: bspd.c,v 1.2 2006/03/06 23:12:01 acher Exp $
 */
/*****************************************************************************/

// FIXME: Use RGBA4444 if needed.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glcdgraphics/font.h>

#include "bspchannel.h"

volatile bspd_data_t *bspd;
bsp_channel_t *bsc_osd;
bspshm_area_t *bsa_fb;
int *fbmem;
GLCD::cFont font;
#define FONTPATH "/usr/share/graphlcd/fonts/"

/* --------------------------------------------------------------------- */
void bsp_clear_display(unsigned int bgcolor)
{
	int ow=bspd->osd.width;
	int oh=bspd->osd.height;
	int n;
	int *dst;

	dst=fbmem;
	for(n=0;n<ow*oh;n+=4) {
		*dst++=bgcolor;
		*dst++=bgcolor;
		*dst++=bgcolor;
		*dst++=bgcolor;
	}	
}
/* --------------------------------------------------------------------- */
void bsp_draw_bitmap(const unsigned char* bm, int w, int h, int x, int y, unsigned int fgcolor, unsigned  int bgcolor)
{
	int ow=bspd->osd.width;
	int oh=bspd->osd.height;
	int *dst;
	int n,m;
	unsigned char val=0;

//	printf("%i %i %i %i\n",x,y,w,h);
	if (w+x>ow)
		return;

	if (h+y>oh)
		return;
				
	for(n=0;n<h;n++) {
		dst=fbmem+x+(y+n)*ow;
		for(m=0;m<w;m++) {
			if ((m&7)==0)
				val=*bm++;
			if (val&128)
				*dst++=fgcolor;
			else
				*dst++=bgcolor;
			val<<=1;
		}
	}
}
/* --------------------------------------------------------------------- */
// Slow but it works at least :-)
void bsp_print_text(char *txt, int x, int y, unsigned int fgcolor, unsigned int bgcolor, char *fontname)
{
	unsigned int n;
	int sx=x;
	int hh=20,ww=20;
	char fontfile[256];
	strcpy(fontfile,FONTPATH);
	strcat(fontfile,fontname);
	strcat(fontfile,".fnt");
	puts(fontfile);
	if (!font.LoadFNT(fontfile))
		return;
	
	for(n=0;n<strlen(txt);n++) {
		const GLCD::cBitmap *bm;
		if (txt[n]!='\n') {
			bm=font.GetCharacter(txt[n]);
			if (bm && bm->Data()) {
				bsp_draw_bitmap(bm->Data(),bm->Width(),bm->Height(),x,y,fgcolor,bgcolor);
				ww=bm->Width();
				hh=bm->Height();
			}
			x+=ww;

		}
		else {
			y+=hh-2;
			x=sx;
		}
	}	
}
/* --------------------------------------------------------------------- */
// shift>0: scroll up
void bsp_scroll(int shift, unsigned int bgcolor)
{
	int ow=bspd->osd.width;
	int oh=bspd->osd.height;
	int n;
	int *src,*dst;
	int ww;
	int a,b,c,d;
	
	if (shift<0)
		return;
	dst=fbmem;
	src=fbmem+shift*ow;
	ww=(oh-shift)*ow;

	for(n=0;n<ww;n+=8) {
		a=*src++;
		b=*src++;
		c=*src++;
		d=*src++;
		*dst++=a;
		*dst++=b;
		*dst++=c;
		*dst++=d;
		a=*src++;
		b=*src++;
		c=*src++;
		d=*src++;
		*dst++=a;
		*dst++=b;
		*dst++=c;
		*dst++=d;
	}

	ww=ow*shift;
	for(n=0;n<ww;n+=8) {
		*dst++=bgcolor;
		*dst++=bgcolor;
		*dst++=bgcolor;
		*dst++=bgcolor;		
		*dst++=bgcolor;
		*dst++=bgcolor;
		*dst++=bgcolor;
		*dst++=bgcolor;		
	}
}
/* --------------------------------------------------------------------- */
void slave_bspd_init(void)
{
	bspshm_area_t * bsa;
	bsp_init(0);
	bsa=bsp_get_area(BSPID_BSPD);
	if (!bsa) {
		fprintf(stderr,"bspd not running in daemon mode\n");
		exit(0);
	}
	bspd=(bspd_data_t*)bsa->mapped;	
	if (!bspd->status) {
		fprintf(stderr,"BSP not running\n");
		exit(0);
	}
	bsa_fb=bsp_get_area(BSPID_FRAMEBUFFER_RGBA_0);
	if (!bsa_fb) {
		fprintf(stderr,"No BSP framebuffer area found\n");
		exit(0);
	}
	fbmem=(int*)bsa_fb->mapped;
}
/* --------------------------------------------------------------------- */
void usage(void)
{
	fprintf(stderr,
		"bspecho: Print text on BSP OSD\n"
		"Options:  -b <hex>     Background color (default ff000000)\n"
		"          -f <hex>     Foreground color (default 00ffffff)\n"
		"          -c           Clear display to background\n"
		"          -F <name>    Set font name\n"
		"          -x <pos>     Set X-position\n"
		"          -y <pos>     Set Y-position\n"
		"          -s <lines>   Scroll up\n"
		"          -t <text>    Print text\n"
 
		);
	exit(-1);
}
/* --------------------------------------------------------------------- */
int main(int argc, char** argv)
{
	char c;
	char *fontname="verdana-020";
	int x=0,y=0;
	unsigned int fgcolor=0x00ffffff;
	unsigned int bgcolor=0xff000000;
	
	slave_bspd_init();
	bspd->osd.width=720; // FIXME
	bspd->osd.height=576; // FIXME

	while (1)
        {
		c = getopt(argc, argv, "b:f:cx:y:F:t:s:?");

		if (c == -1)
                        break;
		
		switch (c) 
		{
		case 'b':
			bgcolor=(unsigned int)strtoll(optarg,NULL,16);
			break;
		case 'f':
			fgcolor=strtol(optarg,NULL,16);
			break;
		case 'c':
			bsp_clear_display(bgcolor);
			break;
		case 'F':
			fontname=optarg;
			break;			
		case 'x':
			x=atoi(optarg);
			break;
		case 'y':
			y=atoi(optarg);
			break;
		case 's':
			bsp_scroll(atoi(optarg),bgcolor);
			break;
		case 't':
			bsp_print_text(optarg,x,y,fgcolor,bgcolor,fontname);
			break;
		default:
                        usage();
                        return 1;
                }
	}
	
	return 0;
}
