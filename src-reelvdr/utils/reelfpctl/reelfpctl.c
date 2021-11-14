/*
 * Frontpanel control for Reelbox
 *
 * (c) Georg Acher, acher (at) baycom dot de
 *     BayCom GmbH, http://www.baycom.de
 *
 * #include <gpl-header.h>
 *
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "frontpanel.h"

#ifndef NOGLCD
#include <glcdgraphics/bitmap.h>
#include <glcdgraphics/font.h>
#include <glcddrivers/config.h>
#include <glcddrivers/driver.h>
#include <glcddrivers/drivers.h>

//#define FONTPATH "/usr/share/graphlcd/fonts/verdanab-009.fnt"  //RC: done in Make.config now
#endif



// Set by Makefile
const char *fp_device=FP_DEVICE;
const char *fp_ice_device="/dev/ttyS2";
frontpanel_registry fpr;


/*-------------------------------------------------------------------------*/
void usage(void)
{
	fprintf(stderr,"reelfpctl -- Frontpanel control V1.21 for Reelbox\n"
		"Usage: reelfpctl <Restriction> <Options>\n"
		"Restrictions:    <> -only-avr -only-usb -only-ice -only-ncl1\n"
		"Options:         -nowait\n"
		"                 -capability\n"
		"                 -wakeup <unix-time>\n"
		"                 -shutdown <seconds>\n"
		"                 -brightness <val>\n"
		"                 -contrast <val>\n"
		"                 -getversion\n"
		"                 -getclock\n"
		"                 -setclock\n"
		"                 -adjustclock <val>\n"
		"                 -displaymode <mode>\n"
#ifndef NOGLCD
		"                 -toptext <text>\n"
		"                 -bottomtext <text>\n"
#endif
		"                 -showpnm <pnmfile>\n"
		"                 -clearlcd\n"
		"                 -setled <val>\n"
		"                 -blinkled <val>\n"
		"                 -clearled <val>\n"
		"                 -setledbrightness <val>\n"
		"                 -getkeystate\n"
		"                 -getwakeup\n"
		"                 -getflags\n"
		"                 -enablemsg <hexval>\n"
		"                 -pwrctl <hexval>\n"
		"                 -dwnldir <file>\n"
		"                 -watchdog <seconds>\n"
		);
	exit(0);
}
/*-------------------------------------------------------------------------*/
// taken from graphlcd-demo
#ifndef NOGLCD

GLCD::cFont font;
GLCD::cDriver * lcd=NULL;
GLCD::cBitmap * bitmap;

void glcd_init(void)
{
	const char *fontFileName=FONTPATH;
	unsigned int displayNumber;

	if (GLCD::Config.Load("/etc/graphlcd.conf")==false) {
		fprintf(stderr,"Error loading /etc/graphlcd.conf\n");
		exit(2);
	}
	if (GLCD::Config.driverConfigs.size() > 0)
        {
		for (displayNumber = 0; displayNumber < GLCD::Config.driverConfigs.size(); displayNumber++)
		{
			if (GLCD::Config.driverConfigs[displayNumber].name == "reel_usbfp")
				goto foundit;
		}

		for (displayNumber = 0; displayNumber < GLCD::Config.driverConfigs.size(); displayNumber++)
		{
			if (GLCD::Config.driverConfigs[displayNumber].name == "st7565r")
				break;
			if (GLCD::Config.driverConfigs[displayNumber].name == "st7565r-reel")
				break;
		}

		if (displayNumber == GLCD::Config.driverConfigs.size())
		{
			fprintf(stderr, "ERROR: st7565r/st7565r-reel or reel_usbfp not found in config file!\n");
			exit(3);
		}
	foundit:
		;
        }
        else
        {
                fprintf(stderr, "ERROR: No displays specified in config file!\n");
                exit(4);
        }

	//if (font.Load(fontFileName) == false)
	if (font.LoadFNT(fontFileName) == false)
	    {
                fprintf(stderr, "ERROR: Failed loading font file %s\n", fontFileName);
                exit(7);
        }
	lcd = GLCD::CreateDriver(GLCD::Config.driverConfigs[displayNumber].id, 
						 &GLCD::Config.driverConfigs[displayNumber]);
	if (!lcd)
        {
                fprintf(stderr, "ERROR: Failed creating display object (displayNumber=%u name=%s)\n", displayNumber, GLCD::Config.driverConfigs[displayNumber].name.c_str());
                exit(8);
        }
        if (lcd->Init() != 0)
        {
                fprintf(stderr, "ERROR: Failed initializing display\n");
                delete lcd;
                exit(9);
        }

        bitmap = new GLCD::cBitmap(lcd->Width(), lcd->Height());
	bitmap->Clear();
}

/*-------------------------------------------------------------------------*/
int toptext(char *txt)
{
	//char buf[1024];
	char *c,*p;
	int y=0;

	if (!lcd)
		glcd_init();
	
	c=p=txt;

	while(c&&(*p)) {
		c=strchr(p,'\n');
		if (c)
			*c=0;
		//bitmap->DrawText(0, y, bitmap->Width() - 1, p, font);
		bitmap->DrawText(0, y, bitmap->Width() - 1, std::string(p), &font);
		p=c+1;
		y+=8;
	}
#ifdef GRAPHLCD_NEW
	lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
#else
	lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height(), bitmap->LineSize());
#endif
        lcd->Refresh(true);
	return 0;
}
/*-------------------------------------------------------------------------*/
int bottomtext(char *txt)
{
	if (!lcd)
		glcd_init();

	//bitmap->DrawText(0, 50, bitmap->Width() - 1, txt, font);
	bitmap->DrawText(0, 50, bitmap->Width() - 1, std::string(txt), &font);
#ifdef GRAPHLCD_NEW
	lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
#else
	lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height(), bitmap->LineSize());
#endif
        lcd->Refresh(true);
	return 0;
}
#else
int toptext(char *txt)
{
	fprintf(stderr,"toptext not supported\n");
	return 0;
}
int bottomtext(char *txt)
{
	fprintf(stderr,"bottomtext not supported\n");
	return 0;
}
#endif
/*-------------------------------------------------------------------------*/
unsigned char** alloc_lcd(void)
{
	unsigned char **LCD;
	int n,m;

	LCD =(unsigned char**) malloc(128/8*sizeof(unsigned char*));
        for(n=0;n<128/8;n++) 
                LCD[n]=(unsigned char*) malloc(64*sizeof(unsigned char));

	for(n=0;n<64;n++)
		for(m=0;m<16;m++)
			LCD[m][n]=0;
	return LCD;
}
/*-------------------------------------------------------------------------*/
void free_lcd(unsigned char** LCD)
{
	int n;
	for(n=0;n<128/8;n++)
		free(LCD[n]);
	free(LCD);	
}
/*-------------------------------------------------------------------------*/
int fp_clear_lcd(void)
{
	unsigned char **LCD;
	//int n,m;

	LCD=alloc_lcd();
	fpr.fp_show_pic(LCD);
	free_lcd(LCD);
	return 0;
}
/*-------------------------------------------------------------------------*/
int showpnm(char *fname)
{
        unsigned char **LCD;
	FILE *file;
        char buf[1024];
        int sizex,sizey;
        int n,m,o;

	LCD=alloc_lcd();

	file=fopen(fname,"rb");
	if (!file) {
		perror("Can't open pnmfile");
		return 1;
	}

        fscanf(file,"%s\n",buf);
        if (!strncmp(buf,"P5",2)) {

                sizex=sizey=0;
                fscanf(file,"%i %i\n",&sizex,&sizey);
                if ((sizex!=128) || (sizey!=64)) {
			fprintf(stderr,"PNM must be 128x64\n");
                        goto end;
		}
                
                fgets(buf,1024,file);
                
                for(n=0;n<64;n++) {
                        fread(buf,128,1,file);
                        for(m=0;m<16;m++) {
                                LCD[m][n]=0;
                                for(o=0;o<8;o++)
                                        if (!buf[m*8+o])
                                                LCD[m][n]|=(1<<(7-o));
                        }
                }
        }
        else if (!strncmp(buf,"P4",2)) {
                sizex=sizey=0;
                int cx,cy;
                fscanf(file,"%i %i\n",&sizex,&sizey);

                cx=(sizex-128)/16;
                cy=(sizey-64)/2;
		if (cx<0) cx=0;
		if (cy<0) cy=0;
                for(n=0;n<64;n++)
                        for(m=0;m<16;m++)
                                LCD[m][n]=0;
//                printf("sizex %i sizey %i cy %i\n",sizex,sizey,cy);
                for(n=0;n<cy;n++)
                        fread(buf,(sizex+7)/8,1,file);
                        
                for(n=0;n<64;n++) {
                        int l;
                        memset(buf,0,128);
                        l=fread(buf,1,(sizex+7)/8,file);
                        if (l>0)
                                for(m=0;m<l;m++)
					LCD[m][n]=buf[m+cx];
                }
        }
	else
		fprintf(stderr,"Unsupported PNM format, must be B/W (P4) or grayscale (P5) \n");
end:
        fclose(file);

	fpr.fp_show_pic(LCD);
	free_lcd(LCD);
	return 0;
}
/*-------------------------------------------------------------------------*/
int dwnldir(char *fname)
{
	FILE *file;
	int n,a,b;
	file=fopen(fname,"r");
	if (!file) {
		perror(fname);
		return 0;
	}
	n=0;
	while(fscanf(file,"%x %x\n",&a,&b)==2) {
		fpr.fp_eeprom_flash(n++,a);
		u_sleep(10*1000);
		fpr.fp_eeprom_flash(n++,b);
		u_sleep(10*1000);
	}
	fclose(file);
	return 1;
}
/*-------------------------------------------------------------------------*/
void capability(void)
{
	FILE *fd;
	const char *fn;
#ifndef RBMINI
        unlink(CAP_AVR);
        unlink(CAP_NOAVR);
        
	if (frontpanel_avr::fp_available(fp_device,1)) {
		fn=CAP_AVR;
		printf("AVR ");
	}
	else
		fn=CAP_NOAVR;

	fd=fopen(fn,"w");
	if (fd)
		fclose(fd);

	// avoid unnecessary probing
	struct stat st;
	int r;
	if (stat(CAP_ICE,&st) && stat(CAP_NOICE,&st)) {
		
		unlink(CAP_ICE);
		unlink(CAP_NOICE);
		if (frontpanel_ice::fp_available(fp_ice_device,1)) {
			fn=CAP_ICE;
			printf("ICE ");
		}
		else
			fn=CAP_NOICE;
		
		fd=fopen(fn,"w");
		if (fd)
			fclose(fd);
	} else {
		if (!stat(CAP_ICE,&st))
			printf("ICE ");
	}

#else
	printf("NCL1 ");
#endif
	if (frontpanel_usb::fp_available(""))
		printf("USB ");
	puts("");
}
/*-------------------------------------------------------------------------*/
int main(int argc, char** argv)
{
	int idx=1;
        char *opt;
	unsigned char buf[32];
	int l,wait,nowait=0;
	int use_avr=1;
	int use_ice=1;
	int use_usb=1;	
#ifdef RBMINI
	int use_ncl1=1;	
#endif

	// special function
	if (argc==2 && !strcmp(argv[1],"-capability")) {
		fpr.force_ice();
		capability();
		exit(0);
	}

	if (argc>=2 &&  !strcmp(argv[1],"-only-avr")) {
		use_ice=0;
		use_usb=0;
#ifdef RBMINI
		use_ncl1=0;
#endif
		idx++;
	}
	if (argc>=2 &&  !strcmp(argv[1],"-only-usb")) {
		idx++;
		use_ice=0;
		use_avr=0;
#ifdef RBMINI
		use_ncl1=0;
#endif
	}
	if (argc>=2 &&  !strcmp(argv[1],"-only-ice")) {
		idx++;
		use_avr=0;
		use_usb=0;
#ifdef RBMINI
		use_ncl1=0;
#endif
		fpr.force_ice();
	}
	if (argc>=2 &&  !strcmp(argv[1],"-only-ncl1")) {
		idx++;
		use_avr=0;
		use_usb=0;
		use_ice=0;
	}


#ifdef RBMINI
	if (use_ncl1) {
		frontpanel *f_main=new frontpanel_nc("");
		fpr.register_unit(f_main);
	}
#else
	int have_avr=0,have_ice=0;

	// Detection 
	if (use_avr && frontpanel_avr::fp_available(fp_device)) 
		have_avr=1;

	if (use_ice && frontpanel_ice::fp_available(fp_ice_device)) 
		have_ice=1;

	if (have_avr){
		frontpanel *f_main=new frontpanel_avr(fp_device);
		fpr.register_unit(f_main);
	}

	if (have_ice){
		frontpanel *f_ice=new frontpanel_ice(fp_ice_device);
		fpr.register_unit(f_ice);	
	}
	
#endif   
	if (use_usb) {
		frontpanel *f_usb=new frontpanel_usb("");
		fpr.register_unit(f_usb);
	}

        while(idx<argc) 
        {
                opt=argv[idx];
		wait=1;
		l=0;
                if ((idx+1<argc)) {
			l=1;
                        if (!strcmp(opt,"-wakeup")) 
                                fpr.fp_set_wakeup(atoi(argv[++idx]));
                        else if (!strcmp(opt,"-shutdown")) 
                                fpr.fp_set_switchcpu(atoi(argv[++idx]),0);
                        else if (!strcmp(opt,"-watchdog")) 
                                fpr.fp_set_switchcpu(atoi(argv[++idx]),1);
                        else if (!strcmp(opt,"-brightness")) {
				int val=atoi(argv[++idx]);
                                fpr.fp_display_brightness(val);
			}
                        else if (!strcmp(opt,"-contrast")) 
                                fpr.fp_display_contrast(atoi(argv[++idx]));
                        else if (!strcmp(opt,"-displaymode"))
				fpr.fp_set_displaymode(atoi(argv[++idx]));
                        else if (!strcmp(opt,"-adjustclock")) {
				int v=atoi(argv[++idx]);
				fpr.fp_set_clock_adjust((v>>16)&255,v&255);
			}
                        else if (!strcmp(opt,"-showpnm")) 
                                wait=showpnm(argv[++idx]);
                        else if (!strcmp(opt,"-toptext")) 
                                wait=toptext(argv[++idx]);
                        else if (!strcmp(opt,"-bottomtext"))
                                wait=bottomtext(argv[++idx]);
                        else if (!strcmp(opt,"-setled")) {
				int val=atoi(argv[++idx]);
				fpr.fp_set_leds(0x10,val);
			}
                        else if (!strcmp(opt,"-blinkled")) {
				int val=atoi(argv[++idx]);
				fpr.fp_set_leds(0x30,val);
			}
                        else if (!strcmp(opt,"-clearled")) {
				int val=atoi(argv[++idx]);
				fpr.fp_set_leds(0x20,val);
			}
                        else if (!strcmp(opt,"-setledbrightness")) {
				int v=atoi(argv[++idx]);
				fpr.fp_set_led_brightness(v);
			}
			else if (!strcmp(opt,"-enablemsg"))  {
                                int v=strtoul(argv[++idx],NULL,16);
                                fpr.fp_enable_messages(v);
                        }
			else if (!strcmp(opt,"-pwrctl"))  {
                                int v=strtoul(argv[++idx],NULL,16);
                                fpr.fp_power_control(v);
                        }
			else if (!strcmp(opt,"-dwnldir")) 
                                dwnldir(argv[++idx]);
                     
			else
				l=0;
                }
		if (l==0 ) {
                        
			if (!strcmp(opt,"-nowait"))
				nowait=1;
			else if (!strcmp(opt,"-getversion"))
				fpr.fp_get_version();
			else if (!strcmp(opt,"-getclock")) 
				fpr.fp_get_clock();
			else if (!strcmp(opt,"-getwakeup")) 
				fpr.fp_get_wakeup();
			else if (!strcmp(opt,"-setclock")) 
				fpr.fp_set_clock();
			else if (!strcmp(opt,"-getkeystate")) 
				fpr.fp_get_key();
			else if (!strcmp(opt,"-clearlcd")) 
				wait=fp_clear_lcd();
			else if (!strcmp(opt,"-getflags")) 
				fpr.fp_get_flags();
			else 
				usage();
		}

		l=0;
		if (wait && !nowait) {
			frontpanel *fp=fpr.fp_get_frontpanel("AVR");
			if (!fp)
				fp=fpr.fp_get_frontpanel("ICE");
			if (!fp)
				fp=fpr.fp_get_frontpanel("NCL1");
			if (fp) {
				l=fp->fp_read_msg(buf,NULL);
			}
		}

		if (l>2) {
			int n;
			for(n=2;n<l;n++)
				printf("%02x ",buf[n]);
			puts("");
		}

                idx++;
        }
	
}
