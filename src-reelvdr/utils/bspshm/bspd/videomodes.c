/*****************************************************************************/
/*
 * BSP15 management daemon for Reelbox -- Video Mode Management
 *
 * Copyright (C) 2006 Georg Acher, BayCom GmbH (acher at baycom dot de)
 *
 * #include <GPL-header>
 *
 * $Id: videomodes.c,v 1.1 2006/03/20 22:44:35 acher Exp $
 */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>

#include "bspchannel.h"

#define min(a,b) ((a)>(b)?(b):(a))
#define max(a,b) ((a)<(b)?(b):(a))

#define FS453_FOLDER "/etc/fs453"
#define VDR_SETUP "/etc/vdr/setup.conf"

extern volatile bspd_data_t *bspd;

static bsp_video_attributes_t last_va[2];

static bsp_video_mode_t last_vm={-1};
static bsp_picture_attributes_t last_picture;
int scart_state=0;
static int hdtv_state=0;

//                             ???  23.976 24  25  29.97 30  50  59.94 60
static unsigned int fps_tab[9]={50, 50,    50, 50, 30,   30, 50, 60,   60};

typedef struct {
	char name[32];
	char focus[32];
	int sil;
	int bcc;
	int w,h;
	int pclk;
	int fps;
	int ext_interlaced;
	int int_interlaced;
	int hsw, hfp, hbp;
	int vsh, vfp, vbp;
	int bsp_pclk;
	int hs_pol;
	int vs_pol;
	int asp_x;
	int asp_y;
} video_timing_t;

// Focus.c
void focus_set_WSS(int norm, int value);
int focus_set_bccf_sdtv(int brightness, int contrast, int colour, int flicker);
int focus_set_pll(int khz);
void focus_set_tff(int tff);
/* --------------------------------------------------------------------- */
// Low-Level BSP and picture control
/* --------------------------------------------------------------------- */
void video_set_bsp15(int mode, int display_type,video_timing_t *vt) 
{
	int n;

	if (mode==BSP_HW_VM_CUSTOM) {
		bspd->hw_video_mode.settings[0] = vt->w;
		bspd->hw_video_mode.settings[1] = vt->h;
		bspd->hw_video_mode.settings[2] = vt->fps;
		bspd->hw_video_mode.settings[3] = vt->int_interlaced;
		if (vt->int_interlaced)
			bspd->hw_video_mode.settings[1] = vt->h/2;		
		bspd->hw_video_mode.settings[4] = vt->hsw;
		bspd->hw_video_mode.settings[5] = vt->hfp;
		bspd->hw_video_mode.settings[6] = vt->hbp;
		if (vt->int_interlaced==2) {
			bspd->hw_video_mode.settings[0] *=2;
			bspd->hw_video_mode.settings[4] *=2;
			bspd->hw_video_mode.settings[5] *=2;
			bspd->hw_video_mode.settings[6] *=2;			
		}
		bspd->hw_video_mode.settings[7] = vt->vsh;
		bspd->hw_video_mode.settings[8] = vt->vfp;
		bspd->hw_video_mode.settings[9] = vt->vbp;
		bspd->hw_video_mode.settings[10] = 0;
		bspd->hw_video_mode.settings[11] = 0;
		bspd->hw_video_mode.settings[12] = 0;
		bspd->hw_video_mode.settings[13] = 0;
		bspd->hw_video_mode.settings[14] = vt->bsp_pclk;
		bspd->hw_video_mode.settings[15] = vt->hs_pol;
		bspd->hw_video_mode.settings[16] = vt->vs_pol;
		if (display_type==BSP_VM_DISPLAY_43) {
			bspd->hw_video_mode.settings[17] = 4;
			bspd->hw_video_mode.settings[18] = 3;
		}
		else {
			bspd->hw_video_mode.settings[17] = 16;
			bspd->hw_video_mode.settings[18] = 9;
		}
		bspd->hw_video_mode.settings[19] = 1;
		bspd->hw_video_mode.settings[20] = 1;
		bspd->hw_video_mode.settings[21] = 24;
		printf("bspd: Download custom video mode %s %i %i, fps %i\n",vt->name,vt->w,vt->h,vt->fps);
	}
	else 
		printf("bspd: Set BSP video mode %i\n",mode);

	bspd->hw_video_mode.mode=mode;
	bspd->hw_video_mode.set=1;

	for(n=0;n<50;n++) {
		if (!bspd->hw_video_mode.set || !bspd->status) 
			break;
		usleep(100*1000);
	}
}
/* --------------------------------------------------------------------- */
void video_set_gamma(float gamma, float bottom, float top)
{
	int val,n;
	float span=top-bottom;
	
	if (gamma<0.01)
		return;
	val=0;
	for(n=0;n<256;n++) {
		float gv;
		if (n<=bottom)
			gv=bottom;
		else
			gv=bottom+span*pow((n-bottom)/256,1.0/gamma);

		gv=max(bottom,min(gv,top));
		gv=max(0,min(gv,255));

		val=(val<<8)|(((int)gv)&255);

		if ((n&3)==3) {
			bspd->drc.gamma_table[0][n>>2]=val; // R
			bspd->drc.gamma_table[1][n>>2]=val; // G
			bspd->drc.gamma_table[2][n>>2]=val; // B
			val=0;
		}
	}
	
	bspd->drc.gamma_changed++;
}
/* --------------------------------------------------------------------- */   
#if 0
void video_calc_coefs(double s, int taps, int phases, double *coefs)
{
	int n,m,maxv;
	double max1,max2,x;
	double t1=taps-1;
	double p1=phases;
	double sh,k;

	for(n=0;n<phases;n++) {	
		
		
		coefs[n*taps+2]=128;
		coefs[n*taps+3]=32*(1-s);
		coefs[n*taps+1]=32*(1-s);
		coefs[n*taps+4]=16*(1-s);
		coefs[n*taps+0]=16*(1-s);

		for(m=0;m<taps-1;m++)
			coefs[n*taps+m]=coefs[n*taps+m]+k*n*(coefs[n*taps+m+1]-coefs[n*taps+m])/p1;
	}
//	return;
	// Normalize to equal gain and adjust rounding error
	for(n=0;n<phases;n++) {
		max1=0;		
		for(m=0;m<taps;m++) {
			max1+=coefs[n*taps+m];
		}
		for(m=0;m<taps;m++) {
			coefs[n*taps+m]=round(coefs[n*taps+m]*128.0/max1);
		}
		max1=0;
		max2=0;
		maxv=0;
		for(m=0;m<taps;m++) {
			max1+=coefs[n*taps+m];
			if (max2<coefs[n*taps+m]) {
				maxv=m;
				max2=coefs[n*taps+m];
			}
		}
		coefs[n*taps+maxv]+=128-max1;
	}		

}
#endif
/* --------------------------------------------------------------------- */   
void video_set_sharpness(int value)
{
	bspd->drc.hor_filter_luma[0][0]=value;
	bspd->drc.filter_changed++;

}
/* --------------------------------------------------------------------- */   
void video_set_picture( bsp_picture_attributes_t *picture, int force)
{
	float y_min=0;
	float y_max=255;
	switch(bspd->video_mode.main_mode) {
	case BSP_VMM_SD:
		if (force || last_picture.values[0]!=picture->values[0] ||
		    last_picture.values[1]!=picture->values[1] ||
		    last_picture.values[2]!=picture->values[2] ||
		    last_picture.values[5]!=picture->values[5]) {

			focus_set_bccf_sdtv(picture->values[0], picture->values[1],
					    picture->values[2], picture->values[5]);
		}
		break;
	case BSP_VMM_SDHD:
		if (hdtv_state==0)
			focus_set_bccf_sdtv(picture->values[0], picture->values[1],
					    picture->values[2], picture->values[5]);
		else {
			y_min=0+(picture->values[0]-500)/4;
			y_max=(int)((255+(picture->values[0]-500)/4.0)*( picture->values[1]/500.0));
		}
		break;
	case BSP_VMM_PC:
		y_min=0+(picture->values[0]-500)/4;
		y_max=(int)((255.0+(picture->values[0]-500)/4.0)*( picture->values[1]/500.0));
		break;		
	}
	y_min=min(255,max(0,y_min));
//	y_max=min(255,max(0,y_max));

	video_set_gamma(picture->values[4]/128.0,y_min,y_max);
#if 1
	if (force || last_picture.values[3]!=picture->values[3])
		video_set_sharpness(picture->values[3]);
#endif

}
/* --------------------------------------------------------------------- */
/* Scart voltage setting
   scart_mode=0: always, scart_mode=1: an startup, start_mode=2: never
*/
int set_scart_voltage(int scart_mode, int aspectx, int colormode)
{
//	printf("SCART %i %i\n",scart_mode,scart_state);
	if (scart_mode==BSP_VM_FLAGS_SCART_ON || 
	    (scart_mode==BSP_VM_FLAGS_SCART_STARTUP && scart_state==0)) {
		if (aspectx==4) {
			if (colormode)
				system("reelhwctl -scart-tv or -scart-vcr or");
			else
				system("reelhwctl -scart-tv oc -scart-vcr oc");
		}
		else {
			if (colormode)
				system("reelhwctl -scart-tv wr -scart-vcr wr");
			else
				system("reelhwctl -scart-tv wc -scart-vcr wc");
		}
		scart_state=1;
	}
	else {
		if (colormode)
			system("reelhwctl -scart-tv ir -scart-vcr ir");
		else
			system("reelhwctl -scart-tv ic -scart-vcr ic");
	}
	return 0;
}
/* --------------------------------------------------------------------- */
int video_set_wss(int norm, int scart_mode, int aspectx, int aspecty, int colormode)
{
	set_scart_voltage(scart_mode, aspectx, colormode);
	
	if (aspectx==4) {  // looks like 4:3

		if (norm==0) 
			focus_set_WSS(0,0x80000); // PAL
		else
			focus_set_WSS(1,0x180000);
	}
	else {  // 16:9
		// Fixme 14:
		if (norm==0) 
			focus_set_WSS(0,0x70000);
		else
			focus_set_WSS(1,0x600001);
	}
	return 0;
}
/* --------------------------------------------------------------------- */
void vdr_read_setup(void)
{
	FILE *f;
	char buf[256]= {0};
	char cmd[256];
	int val;
	f=fopen(VDR_SETUP,"r");
	if (!f)
		return;
	
	while(!feof(f)) {
		fgets(buf,255,f);
		if (buf[0]=='#')
			continue;
		cmd[0]=0;
		sscanf(buf,"%s = %i",cmd,&val);

		if (!strcasecmp(cmd,"reelbox.Aspect"))
			bspd->video_mode.aspect=val;
		else if (!strcasecmp(cmd,"reelbox.Brightness"))
			bspd->picture.values[0]=val;
		else if (!strcasecmp(cmd,"reelbox.Colour"))
			bspd->picture.values[2]=val;
		else if (!strcasecmp(cmd,"reelbox.Contrast"))
			bspd->picture.values[1]=val;
		else if (!strcasecmp(cmd,"reelbox.DisplayType"))
			bspd->video_mode.display_type=val;
		else if (!strcasecmp(cmd,"reelbox.Flicker"))
			bspd->picture.values[5]=val;
		else if (!strcasecmp(cmd,"reelbox.Framerate"))
			bspd->video_mode.framerate=val;
		else if (!strcasecmp(cmd,"reelbox.Gamma"))
			bspd->picture.values[4]=val;
		else if (!strcasecmp(cmd,"reelbox.Norm"))
			bspd->video_mode.norm=val;
		else if (!strcasecmp(cmd,"reelbox.Resolution"))
			bspd->video_mode.resolution=val;
		else if (!strcasecmp(cmd,"reelbox.Sharpness"))
			bspd->picture.values[3]=val;
		else if (!strcasecmp(cmd,"reelbox.VMM"))
			bspd->video_mode.main_mode=val;
		else if (!strcasecmp(cmd,"reelbox.VSM"))
			bspd->video_mode.sub_mode=val;
		else if (!strcasecmp(cmd,"reelbox.Scartmode"))
			bspd->video_mode.flags=(bspd->video_mode.flags&~BSP_VM_FLAGS_SCART_MASK)|(val&BSP_VM_FLAGS_SCART_MASK);
		else if (!strcasecmp(cmd,"reelbox.Deint"))
			bspd->video_mode.flags=(bspd->video_mode.flags&~BSP_VM_FLAGS_DEINT_MASK)|(val&BSP_VM_FLAGS_DEINT_MASK);

	}
	fclose(f);
}
/* --------------------------------------------------------------------- */
// Video timing
/* --------------------------------------------------------------------- */
int video_parse_modeline(char* line, video_timing_t *vt) 
{
	char hpol[32]="";
	char vpol[32]="";
	char iint[32]="";
	int dummy;

	vt->name[0]=0;
	vt->focus[0]=0;
	vt->asp_y=-1;

	//            n    f    s  b  p  f  i  h  h  h  v  v  v  c  h   v     x  y
	sscanf(line,"%31s %31s %i %i %i %i %i %i %i %i %i %i %i %i %31s %31s %i %i",
	       vt->name, vt->focus, &vt->sil, &vt->bcc,
	       &vt->pclk, &vt->fps, &vt->int_interlaced, 
	       &vt->hsw, &vt->hfp, &vt->hbp,
	       &vt->vsh, &vt->vfp, &vt->vbp,
	       &vt->bsp_pclk, hpol, vpol,
	       &vt->asp_x, &vt->asp_y);

	if (vt->name[0] == '#' ||
	    vt->asp_y   == -1)
		return 1;

	vt->hs_pol= (hpol[0]=='+'?1:0);
	vt->vs_pol= (vpol[0]=='+'?1:0);

	sscanf(vt->name,"%ix%i_%i%c",
	       &vt->w,&vt->h, &dummy, iint);

	if (iint[0]==0)
		return 1;

	vt->ext_interlaced=(iint[0]=='i'?1:0);

	return 0;
}
/* --------------------------------------------------------------------- */
int video_search_modeline(int w, int h, int fps, int interlaced, video_timing_t *vt) 
{
	FILE *file;
	char buffer[256];
	char search[64];

	file=fopen(FS453_FOLDER "/modelines","r");
	if (!file)
		return 1;

	sprintf(search,"%ix%i_%i%c",w,h,fps,(interlaced?'i':'p'));
	printf("bspd: Search Modeline %s\n",search);

	while(!feof(file)) {
		fgets(buffer,255,file);
		if (!video_parse_modeline(buffer,vt)) {
			if (!strcmp(search,vt->name)) {
//				puts(buffer);
				fclose(file);
				return 0;
			}
		}
	}
	fclose(file);
	printf("bspd: Video mode %s not found\n",search);
	return 1;
}
/* --------------------------------------------------------------------- */
/*
  Focus command file assembly:
  SDTV: start+(pal50/pal60/ntsc)+(yuv/rgb/cvbs/yc)+end
  SDHD: start+(res0/res1/res2)+(yuv/rgb)+end     , PLL set by bspd
  PC:   start+(res0/res1/res2/res3)+end          , PLL set by bspd

*/
int video_setup_focus(int vmm, int vsm, int norm, int res, video_timing_t *vt) 
{
	char cmd[512];
	char *folders[3]={ FS453_FOLDER "/sdtv",  FS453_FOLDER "/sdhd", FS453_FOLDER "/pc"} ;
	char *folder;
	char *setup1_sd[3]={"pal50","pal60","ntsc"};
	char *setup2[4]={"yuv","rgb","cvbs","yc"};
	int ft;
	
	hdtv_state=0;

	switch(vmm) {
	case BSP_VMM_SD:
		folder=folders[0];
		if (vt->fps==50) 
			ft=0; // PAL50
		else if (norm==BSP_VM_NORM_NTSC)
			ft=2; // NTSC60
		else
			ft=1; // PAL60

		sprintf(cmd,"cat %s/start %s/%s %s/%s %s/end | fs453 >/dev/null 2>&1",
			folder, folder, setup1_sd[ft], folder,setup2[vsm], folder);	
		break;

	case BSP_VMM_SDHD:

		if (vt->ext_interlaced && vt->h<=576) {			
			// Reuse SDTV values
			folder=folders[0];
			if (vt->fps==60)
				ft=1;
			else
				ft=0;
			sprintf(cmd,"cat %s/start %s/%s %s/%s %s/end | fs453 >/dev/null 2>&1",
				folder, folder, setup1_sd[ft], folder,setup2[vsm], folder);	
		}
		else {
			hdtv_state=1;
			folder=folders[1];
			sprintf(cmd,"cat %s/start %s/res%ix%i_%i %s/%s %s/end | fs453 >/dev/null 2>&1",
				folder, folder, vt->w,vt->h,vt->fps, folder,setup2[vsm], folder);
		}
		break;

	case BSP_VMM_PC:
		folder=folders[2];
		sprintf(cmd,"cat %s/start %s/res%ix%i %s/%s %s/end | fs453 >/dev/null 2>&1",
			folder, folder, vt->w,vt->h, folder,setup2[vsm],folder);
		break;
	default:
		return 1;
	}
	puts(cmd);
	system(cmd);

	if (vt->pclk)
		focus_set_pll(vt->pclk);
	
	video_set_picture((bsp_picture_attributes_t*)&bspd->picture, 1);

	return 0;
}
/* --------------------------------------------------------------------- */
void set_video_mode(int flags)
{
	video_timing_t vt;
	int pc_res[4][2]={{640,480},{800,600},{1024,768},{1280,720}};
	int fpsc;
	int w=720,h=576,fps=50,i=1; // Defaults
	int interlace;
	int vw,vh;
	int main_mode,sub_mode,res,norm;
	
	fpsc=bspd->video_attributes[0].fpsc;
	if (fpsc<9)
		fps=fps_tab[fpsc];
	
	vw=bspd->video_attributes[0].frameWidth;
	vh=bspd->video_attributes[0].frameHeight;
	interlace=bspd->video_attributes[0].interlace;

	res=bspd->video_mode.resolution;
	main_mode=bspd->video_mode.main_mode;
	sub_mode=bspd->video_mode.sub_mode;
	norm=bspd->video_mode.norm;

	printf("bspd: video mode %i, sub %i, res %i, needed fps %i\n", main_mode, sub_mode, res, fps);
	// Find new video timing

	switch(main_mode)
	{
	case BSP_VMM_SD:
		if ((vh==480 || fps==60 || fps==30 ||
		     norm==BSP_VM_NORM_NTSC)&&norm!=BSP_VM_NORM_PAL50) {
			fps=60;
			h=480;
		}
		break;

	case BSP_VMM_SDHD:

		if (vh<=480) {
			w=720;
			h=480;
			if (interlace)
				i=1;
			else
				i=0;
		}
		else if (vh<=576) {
			w=720;
			h=576;
			if (interlace)
				i=1;
			else
				i=0;
		}
		else if (vh<=720) {
			w=1280;
			h=720;
			i=0;
		}
		else if (vh<=1088) {
			w=1920;
			h=1080;
			i=1;
		}
		else {
			w=720;
			h=576;
			i=1;
		}
		// Force 720p/1080i
		if (res>0 && interlace && (vh==576 || vh==480)) {
			if (res==2) {
				w=1280;
				h=720;
				i=0;
			}
			else {
				w=1920;
				h=1080;
				i=1;
			}
		}
		break;

	case BSP_VMM_PC:
		i=0;
		w=pc_res[res][0];
		h=pc_res[res][1];
		switch(bspd->video_mode.framerate) {
		case BSP_VM_FRATE_AUTO1:
			if (fps==50) fps=75;
			break;
		case BSP_VM_FRATE_AUTO3:
			if (fps==50) fps=67;
			break;
		case BSP_VM_FRATE_50:
			fps=50;
			break;
		case BSP_VM_FRATE_60:
			fps=60;
			break;
		case BSP_VM_FRATE_67:
			fps=67;
			break;
		case BSP_VM_FRATE_75:
			fps=75;
			break;
		default:
			break;
		}
	default:
		break;
	}

	// Check for real fps changes
	if (flags==2 && fps==bspd->hw_video_mode.settings[2]) {
		printf("bspd: No effective fps change\n");
		return;
	}

	// FIXME: Better fallback if mode not found
	if (video_search_modeline(w,h, fps, i, &vt)) {
		fps=60;
		if (video_search_modeline(w,h, fps, i, &vt))
			return;
	}

	// BSP video off
	video_set_bsp15(BSP_HW_VM_OFF, 0, NULL);

	// Focus set mode
	video_setup_focus( main_mode, sub_mode , norm, res, &vt);
	printf("bspd: Set Focus done\n");
	
	// BSP download new mode
	video_set_bsp15(BSP_HW_VM_CUSTOM, bspd->video_mode.display_type, &vt);

	printf("bspd: Set to main mode %i, submode %i\n",bspd->video_mode.main_mode, bspd->video_mode.sub_mode);
	if (bspd->video_mode.main_mode==BSP_VMM_SD || bspd->video_mode.main_mode==BSP_VMM_SDHD ) {
		// FIXME: Wait until drc and player are really running again
		sleep(2);
		bspd->need_focus_sync=1;
	}
}
/* --------------------------------------------------------------------- */
// Check if a HW video switch is needed due to a changed resolution
int video_check_res_change(int player, bsp_video_attributes_t *va)
{
	int main_mode=bspd->video_mode.main_mode;
#if 1
	switch(main_mode) {
	case BSP_VMM_SD:
		// SDTV: No known reason until now
		return 0;
	case BSP_VMM_SDHD:
		if (last_va[0].frameHeight!=0 && 
		    (va->frameHeight != last_va[0].frameHeight ||
		     va->interlace != last_va[0].interlace))
			return 1;
		return 0;
	case BSP_VMM_PC:
		// Nope.
		return 0;
	}
#endif
	return 0;
}
/* --------------------------------------------------------------------- */
void video_check_attributes(int force)
{

	int va_changed=bspd-> video_attributes[0].changed;
	int norm=0; // FIXME
	unsigned int fpsc,last_fpsc;
	int need_videoswitch=0;
	bsp_video_attributes_t va[2];

	if (va_changed!=last_va[0].changed) {
		va[0]=(const bsp_video_attributes_t&)bspd->video_attributes[0];

		printf("bspd: Attributes %i:%i, fpscode %i, interlace %i, tff %i\n", 
		       va[0].imgAspectRatioX,
		       va[0].imgAspectRatioY,
		       va[0].fpsc,
		       va[0].interlace,
		       va[0].tff
			);

		// Detect FPS change

		fpsc=va[0].fpsc;
		last_fpsc=last_va[0].fpsc;
		if (fpsc<9 && last_fpsc<9) {
			if ( fps_tab[fpsc]!=fps_tab[last_fpsc] &&
			     bspd->hw_video_mode.settings[2]!=fps_tab[fpsc]) {
				need_videoswitch=2;
			}
		}

		// Detect stream resolution change
		if (force || video_check_res_change(0,&va[0]))
			need_videoswitch=1;

		if (need_videoswitch) {
			set_video_mode(need_videoswitch);
		}

		// Detect aspect ration
		switch(bspd->video_mode.main_mode==BSP_VMM_SD && bspd->video_mode.aspect) {
		case BSP_VM_ASPECT_WSS:
			video_set_wss(norm, bspd->video_mode.flags&BSP_VM_FLAGS_SCART_MASK, 
				va[0].imgAspectRatioX, va[0].imgAspectRatioY, bspd->video_mode.sub_mode==BSP_VSM_RGB?1:0);
			break;
		default:
			set_scart_voltage(bspd->video_mode.flags&BSP_VM_FLAGS_SCART_MASK, 
					  4, bspd->video_mode.sub_mode==BSP_VSM_RGB?1:0);
			break;
		}
#if 0
		if (va[0].tff!=last_va[0].tff)
			focus_set_tff(va[0].tff);
#endif
		last_va[0]=va[0];
//		last_va[0]=bspd->video_attributes[0];
		last_va[0].changed=va_changed;
	}
}

/* --------------------------------------------------------------------- */
void video_check_mode(int force)
{
	int vm_changed=bspd->video_mode.changed;

	if (vm_changed!=last_vm.changed || force) {

		// FIXME: Don't fully reload on minor changes
		if (force || bspd->video_mode.main_mode!= last_vm.main_mode ||
		    bspd->video_mode.sub_mode != last_vm.sub_mode ||
		    bspd->video_mode.display_type!=last_vm.display_type ||
		     bspd->video_mode.aspect != last_vm.aspect ||
		    bspd->video_mode.framerate != last_vm.framerate ||
		    bspd->video_mode.norm != last_vm.norm ||
		    bspd->video_mode.resolution != last_vm.resolution) {
			printf("VCM: %i\n",force);
			set_video_mode(1);
		}	

		if (bspd->video_mode.main_mode==BSP_VMM_SD &&
		    bspd->video_mode.sub_mode != last_vm.sub_mode ||
		    bspd->video_mode.flags != last_vm.flags) {	
			if (bspd->video_mode.sub_mode==BSP_VSM_RGB)
				set_scart_voltage(bspd->video_mode.flags&BSP_VM_FLAGS_SCART_MASK, 4, 1);
			else
				set_scart_voltage(bspd->video_mode.flags&BSP_VM_FLAGS_SCART_MASK, 4, 0);
		}

		last_vm=(const bsp_video_mode_t&)bspd->video_mode;
//		last_vm=bspd->video_mode;
		last_vm.changed=vm_changed;
		bspd->video_mode.last_changed=vm_changed;
	}
}
/* --------------------------------------------------------------------- */
void video_check_picture(int force)
{
	int picture_changed=bspd->picture.changed;
	bsp_picture_attributes_t picture;
	if (picture_changed!=last_picture.changed) {
		last_picture.changed=picture_changed;
		picture=(const bsp_picture_attributes_t&)bspd->picture;

		video_set_picture(&picture, force);
		last_picture=picture;
	}
}
/* --------------------------------------------------------------------- */
void video_check_settings(int force)
{
	video_check_attributes(force);
	video_check_mode(force);
	video_check_picture(force);
}
/* --------------------------------------------------------------------- */
void video_default(void)
{
	int alpha[4]={256, 0, 359, -16};
	int beta[4] ={256,-88,-183,-128};
	int gamma[4]={256,454,0,-128};
	int n;

//	bspd->hw_video_mode.mode=BSP_HW_VM_PAL;
//	bspd->hw_video_mode.set=1;
		
	bspd->picture.values[0]=450;
	bspd->picture.values[1]=500;
	bspd->picture.values[2]=600;
	bspd->picture.values[3]=128;
	bspd->picture.values[4]=128;
	bspd->picture.values[5]=0;

	bspd->video_mode.main_mode=BSP_VMM_SD;
	bspd->video_mode.sub_mode=BSP_VSM_YUV;
	bspd->video_mode.display_type=BSP_VM_DISPLAY_43;
	bspd->video_mode.aspect=BSP_VM_ASPECT_WSS;
	bspd->video_mode.framerate=BSP_VM_FRATE_AUTO1;
	bspd->video_mode.norm=BSP_VM_NORM_PAL50_60;
	bspd->video_mode.resolution=BSP_VM_RESOLUTION_800x600;
	bspd->video_mode.flags=0;

	// Override with vdr settings
	vdr_read_setup();
	
	// Default BSP mode
	bspd->video_attributes[0].frameWidth=720;
	bspd->video_attributes[0].frameHeight=576;
	bspd->video_attributes[0].interlace=1;
	bspd->video_attributes[0].fpsc=6; // 50Hz

#if 0
	printf("bspd: PICTURE SETTINGS %i %i %i %i %i %i\n",
	       bspd->picture.values[0],bspd->picture.values[1],
	       bspd->picture.values[2],bspd->picture.values[3],
	       bspd->picture.values[4],bspd->picture.values[5]);
#endif

	bspd->picture.changed++;
	bspd->video_mode.changed++;

	for(n=0;n<4;n++) {
		bspd->drc.matrix[0][n]=alpha[n];
		bspd->drc.matrix[1][n]=beta[n];
		bspd->drc.matrix[2][n]=gamma[n];
	}
	bspd->drc.matrix_changed++;	
	video_check_settings(1); 
	system("reelhwctl -spdif-in bsp -video-mute 15 -audio-mute 0 -scart-enable 1");
}

