/*****************************************************************************/
/* osddump.c -- Screendump of BSP15 OSD into pnm-Format
 *
 * Copyright (C) 2006 Georg Acher, BayCom GmbH (acher at baycom dot de)
 *               for Reel Multimedia AG
 *
 * #include <GPL-header>
 *
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

#include "bspchannel.h"

volatile bspd_data_t *bspd;
bsp_channel_t *bsc_osd;
bspshm_area_t *bsa_fb;
int *fbmem;

/* --------------------------------------------------------------------- */
void osddump(void)
{
	char buffer[8192];
	int ow=bspd->osd.width;
	int oh=bspd->osd.height;
	int n,m,val;

	sprintf(buffer,"P6\n%i %i\n255\n",ow,oh); // PPM header
	write(1,buffer,strlen(buffer));
	for(n=0;n<=oh;n++) {
		for(m=0;m<ow;m++) {
			val=*fbmem++;
			buffer[3*m+2]=val&255;
			buffer[3*m+1]=(val>>8)&255;
			buffer[3*m]=(val>>16)&255;
		}
		write(1,buffer,3*ow);
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
		"osddump: Dump BSP15 OSD into PPM-format\n"
		"Options:  <none>\n"
		);
	exit(-1);
}
/* --------------------------------------------------------------------- */
int main(int argc, char** argv)
{
	char c;

	slave_bspd_init();
	bspd->osd.width=720; // FIXME
	bspd->osd.height=576; // FIXME

	while (1)
        {
		c = getopt(argc, argv, "h?");

		if (c == -1)
                        break;
		
		switch (c) 
		{
		default:
                        usage();
                        return 1;
                }
	}

	osddump();
	return 0;
}
