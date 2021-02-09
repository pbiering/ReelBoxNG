/*****************************************************************************
 *
 * HD Player management daemon for DeCypher
 *
 * #include <GPLV2-header>
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#ifdef CONFIG_MIPS
#include <linux/types.h>
#include <linux/go8xxx-video.h>
#include "decypher_tricks.h"
#endif
#include <math.h>


#include "hdshmlib.h"
#include "hdshm_user_structs.h"

#define DSPC_SHOW 0x8101
#define DSPC_HIDE 0x8100

#define GPIO_AUDIO_SEL  10
#define GPIO_SCART_RGB  12     // Mode      0: RGB, 1: FBAS

extern volatile hd_data_t *hdd;

#ifdef CONFIG_MIPS
extern void hdp_kill(void);
static hd_video_mode_t  last_video_mode={0};
static hd_hw_control_t   last_hw_control={0};
static hd_audio_control_t last_audio_control={0};
static hd_aspect_t last_aspect={0};
static hd_plane_config_t last_plane_config[4]={{0},{0},{0},{0}};
static hd_player_status_t last_player_status={0};

/*------------------------------------------------------------------------*/
// FIXME: Change to ioctl
void set_gpio(int n,int val)
{
        char path[256];
        int fdx;
        sprintf(path,"/sys/bus/platform/drivers/go7x8x-gpio/pin%i",n);
        fdx=open(path,O_RDWR);
        sprintf(path,"%i\n",val);
        write(fdx,path,sizeof(path));
        close(fdx);
}
/* --------------------------------------------------------------------- */
void set_videomode(char *vop_mode)
{
        dspc_vop_t vmode; 
        int fd;

        vmode.oformat=DSPC_OFORMAT_YUV444;
        vmode.oport= 0;
        vmode.tv_out = DSPC_TV_BOTH;

	if (!strcasecmp(vop_mode,"1080p")) { 
                vmode.vop_mode = VOP_MODE_1080p;
        } else if (!strcasecmp(vop_mode,"1080i")) {
                vmode.vop_mode = VOP_MODE_1080i;
        } else if (!strcasecmp(vop_mode, "1080i50")) {
                vmode.vop_mode = VOP_MODE_1080i50;              
        } else if (!strcasecmp(vop_mode, "720p")) {
                vmode.vop_mode = VOP_MODE_720p;
        } else if (!strcasecmp(vop_mode, "720p50")) {
                vmode.vop_mode = VOP_MODE_720p50;               
        } else if (!strcasecmp(vop_mode, "576p")) {
                vmode.vop_mode = VOP_MODE_576p;
        } else if (!strcasecmp(vop_mode, "576i")) {
                vmode.vop_mode = VOP_MODE_576i;
        } else if (!strcasecmp(vop_mode, "480p")) {
                vmode.vop_mode = VOP_MODE_480p;
        } else if (!strcasecmp(vop_mode, "480i")) {
                vmode.vop_mode = VOP_MODE_480i;
        } else if (!strcasecmp(vop_mode, "vga")) {
                vmode.vop_mode = VOP_MODE_VGA;
        } else if (!strcasecmp(vop_mode, "xga")) {
                vmode.vop_mode = VOP_MODE_XGA;
        } else if (!strcasecmp(vop_mode, "1080i48")) {
                vmode.vop_mode = VOP_MODE_1080i48;
        } else { 
                vmode.vop_mode = VOP_MODE_720p;
        }
	fd=open("/dev/fb0",O_RDONLY);
	if (fd<0)
		return;
	printf("VMODE %i\n",vmode.vop_mode);
        if (ioctl(fd, DSPC_SET_VOP, &vmode))
                printf("set_videomode: %s\n",strerror(errno));
	close(fd);
}
/* --------------------------------------------------------------------- */
void set_planeconfig(int plane, int xpos, int ypos, int w, int h, int ow, int oh, int bpp, int cmode)
{
	dspc_cfg_t cfg;
	int fd;
	cfg.id=plane;
	cfg.xpos=xpos;
	cfg.ypos=ypos;
	cfg.width=w;
	cfg.height=h;
	cfg.owidth=ow;
	cfg.oheight=oh;
	cfg.bpp=bpp;
	cfg.cmode=cmode;
	fd=open("/dev/fb0",O_RDONLY);
	if (fd<0)
		return;
	printf("PLANECONFIG id %i, %i.%i, %i*%i, %i*%i, bpp %i, cmode %i\n",plane,xpos,ypos,w,h,ow,oh,bpp,cmode);
        if (ioctl(fd, DSPC_P_CFG, &cfg))
                printf("set_planeconfig: %s\n",strerror(errno));
	close(fd);
	
}
/* --------------------------------------------------------------------- */
void set_deinterlacer_raw(int mode)
{
	int fd;
	fd=open("/dev/fb0",O_RDONLY);
	if (fd<0)
		return;
        if (ioctl(fd,DSPC_SET_SCALER ,&mode))
                printf("set_deinterlacer: %s\n",strerror(errno));
	close(fd);
}
/* --------------------------------------------------------------------- */
void set_aspect(int automatic, int aw, int ah, int overscan, int amode)
{
        int fd;
        dspc_aspect_ratio_t aspect_ratio;
        int aspect_mode;
#if 0       
       char cmdstr[256]="";
       char *scalemode[4]={"F2S","F2A","C2F","DBD"};
       sprintf(cmdstr,"aspect -a %i -w %i -h %i -o %i -m %s >/dev/null",
               automatic, aw, ah, 
               overscan, scalemode[amode]);
       printf("#### %s\n",cmdstr);
       system(cmdstr);
#else
  
	fd=open("/dev/fb0",O_RDONLY);
	if (fd<0)
		return;
        if (ioctl(fd, DSPC_GET_ASPECT_RATIO, &aspect_ratio) < 0)
                goto out;

        if (amode==0)
                aspect_mode=ASPECT_MODE_FILL2SCREEN;
        else if (amode==1)
                aspect_mode=ASPECT_MODE_FILL2ASPECT;
        else if (amode==2)
                aspect_mode=ASPECT_MODE_CROP2FILL;
        else 
                aspect_mode=ASPECT_MODE_DOTBYDOT;
        
        aspect_ratio.automatic_flag=automatic;     
        aspect_ratio.width=aw;
        aspect_ratio.height=ah;
        aspect_ratio.overscan=overscan;

//	printf("ASPECT %i %i %i %i, %i\n",automatic,aw,ah,overscan,aspect_mode);
        if (ioctl(fd, DSPC_SET_ASPECT_RATIO, &aspect_ratio) < 0)
                goto out;
        if (ioctl(fd, DSPC_SET_ASPECT_MODE, &aspect_mode) < 0)
                goto out;
        
        dspc_init_t idata;
                
        memset(&idata, 0, sizeof(dspc_init_t));

	// Force aspect update
        idata.cfg.id = DSPC_MP;
        if (ioctl(fd, DSPC_GET_INFO, &idata) < 0)
            	goto out;

        idata.cfg.flags |= DSPC_P_CFG_FORCE_UPD;
        ioctl(fd, DSPC_P_CFG, &idata.cfg);

out:
        close(fd);        
#endif
}
/* --------------------------------------------------------------------- */
void enable_fb_layer(int enable, int alpha) 
{
    int pid = 2; // FB
    dspc_alpha_parms_t t;

    int fd = open("/dev/fb0", O_RDWR|O_NDELAY);
    if (fd == -1) 
	return;
    ioctl(fd, enable ? DSPC_SHOW : DSPC_HIDE,&pid);

    t.global_alpha_en[0] = 1;
    t.global_alpha[0] = alpha;
    t.global_alpha_en[1] = 0;
    t.global_alpha[1] = 0;
    t.global_alpha_en[2] = 0;
    t.global_alpha[2] = 0;
    ioctl(fd, DSPC_ALPHA, &t);

// tiqq: save bandwidth?
//    pid = 0; // MP
//    ioctl(fd, enable ? (enable & HD_VM_SCALE_PIC_A ? DSPC_SHOW : DSPC_HIDE) : DSPC_SHOW,&pid);
    close(fd);
}
/* --------------------------------------------------------------------- */
void set_framebuffer(volatile hd_video_mode_t *vm, hd_plane_config_t *plane, hd_plane_config_t *old, int force )
{
	int fw,fh;
	int fvw,fvh;
	int fx,fy;
	int fb,fm;
	int vw,vh;
	int alpha=plane->alpha;

	vw=vm->current_w;
	vh=vm->current_h;

	if (force || old->w!=plane->w ||
	    old->h!=plane->h ||
	    old->ws!=plane->ws ||
	    old->hs!=plane->hs ||
	    old->x!=plane->x ||
	    old->y!=plane->y ||
	    old->mode!=plane->mode ||
	    old->osd_scale!=plane->osd_scale) {

		fw=plane->w; 
		fh=plane->h;
		if (fw<720)
			fw=720;
		if (fh<576)
			fh=576;
		
		if (fw==720 && fh==576 && plane->osd_scale==HD_VM_SCALE_FB_FIT) {
			fvw=0;
			fvh=0;
			fx=0;
			fy=0;
		}
		else if (plane->ws!=-1) {
			fvw=plane->ws;
			fvh=plane->hs;
			fx=plane->x;
			fy=plane->y;
		}
		else {
			fvw=fw;
			fvh=fh;
			
			fx=(vw-fw)/2;
			fy=(vh-fh)/2;
		}
		// DeCypher is a bit intolerant
		if (fx<0) fx=0;
		if (fy<0) fy=0;
		if (fx+fvw>vw) {
			fx=0; fvw=0;
		}
		if (fy+fvh>vh) {
			fy=0; fvh=0;
		}
			
		fb=(plane->mode&0xf0)>>4;
		fm=(plane->mode&0x0f);
		if (fb<2 || fb>4 || fm<0 || fm>6) {
			fb=4;
			fm=1;
		}
		printf("Framebuffer (%d %d %d %d %d %d %d %d)\n", fx,fy,fw,fh,fvw,fvh,fb,fm);
		set_planeconfig(2,fx,fy,fw,fh,fvw,fvh,fb,fm);
	}
	      
	printf ("FB enable: %i, alpha %i\n",plane->enable,alpha);
	enable_fb_layer(plane->enable,alpha);
}
/* --------------------------------------------------------------------- */
void set_vmode(volatile hd_video_mode_t *vm, volatile hd_aspect_t *va, volatile hd_player_status_t *st)
{
	char vmodestr[256]="";
	char hdmistr[256]="";
	char analogstr[256]="";
	char fmtstr[256]="";
	char cmdstr[256]="";
	int w=vm->width;
	int h=vm->height;
	int i=vm->interlace;
	int fps=vm->framerate;
	int xfd;
	int vs=va->scale & HD_VM_SCALE_VID;
	int dvihdmi=0;
	int filter_trick=0;

	xfd=open("/sys/class/wis/adec0/passthrough",O_WRONLY);
	
	if (xfd) {
		if (vm->outputda == HD_VM_OUTDA_AC3) {
			printf("Switching on AC3-loopthrough\n");
			write(xfd,"1\n",2);
		}
		else {
			write(xfd,"0\n",2);
			printf("Switching off AC3-loopthrough\n");
		}
		close(xfd);
	}


	if (vm->outputd == HD_VM_OUTD_HDMI) {		
		strcpy(hdmistr,"hdmi");
		dvihdmi=1;
	}
	else {
		strcpy(hdmistr,"dvi");
		dvihdmi=0;
	}
	if(hdd->video_mode.auto_format && st){//&&(h!=576)&&((vm->outputa&HD_VM_OUTA_OFF)==HD_VM_OUTA_OFF)) {
		if(!st->w || !st->h) {
			printf(">>>>>>>>>>>>>>>>>>>>>>>>>Unknown format %dx%d\n", st->w, st->h);
		} else if(st->w <= 720 && st->h <= 576) {
			if ((vm->outputa&HD_VM_OUTA_OFF)==HD_VM_OUTA_OFF) {
				strcpy(fmtstr,"576i"); 
			} else {
				strcpy(fmtstr,"576p"); 
				strcpy(analogstr,"576i"); 
			}
			w = 720; h = 576;
		} else if(st->w <= 1280 && st->h <= 720 && h >= 720) {
			strcpy(fmtstr,"720p"); w = 1280; h = 720;
			if(fps!=60)strcat(fmtstr, "50");
		} else if(st->w <= 1920 && st->h <= 1080 && h >= 1080) {
			strcpy(fmtstr,"1080i"); w = 1920; h = 1080;
			if(fps!=60)strcat(fmtstr, "50");
		} // if
	} // if
	
	if(fmtstr[0]) {
		printf(">>>>>>>>>>>>>>>>>>>>>>>>>Forcing %s output\n", fmtstr);
	} else {
		switch(h) {
		case 1080:
#if 0	
			if (i==0) { // 1080p
				strcpy(fmtstr,"1080p");
			}
			else 
#endif
			{
				if (fps==60) { // 1080i60
					strcpy(fmtstr,"1080i");
				}
				else if (fps==48){ // 1080i48
					strcpy(fmtstr,"1080i48");
				}
				else {
					strcpy(fmtstr,"1080i50");
				}
			}
			break;
	
		case 720:
			if (fps==60) { // 720p60
				strcpy(fmtstr,"720p");
	
			}
			else { // 720p50
				strcpy(fmtstr,"720p50");
			}
			break;
	
		case 480:
			if (i==0) { // 480p
				strcpy(fmtstr,"480p");
			}
			else {
				if ((vm->outputa&HD_VM_OUTA_OFF)==HD_VM_OUTA_OFF) // Real interlace on HDMI
						strcpy(fmtstr,"480i");
				else
					strcpy(fmtstr,"480p");
					
				strcpy(analogstr,"480i");	
			}
	
			break;
	
		case 576:
		default: 
			if (i==0) { // 576p
				strcpy(fmtstr,"576p");
			}
			else {
				if ((vm->outputa&HD_VM_OUTA_OFF)==HD_VM_OUTA_OFF)  // Real interlace on HDMI
					strcpy(fmtstr,"576i");
				else
					// HDMI progressive, Focus making interlace out of it
					strcpy(fmtstr,"576p");
	
				strcpy(analogstr,"576i"); 			
			}
			
			break;
		} // switch
	} // if
    
	vm->current_w=w;
	vm->current_h=h;

	strcpy(vmodestr,fmtstr);
	
	if (vm->outputd==HD_VM_OUTD_OFF) 
		strcpy(hdmistr,"off");
	else
		strcat(hdmistr,fmtstr);

	if (!analogstr[0])
		strcpy(analogstr,fmtstr);


	if ((vm->outputa&0xff)==HD_VM_OUTA_OFF) {  // powerdown
		strcpy(analogstr,"off");
	}
	else if (!strcmp(analogstr,"576i") || !strcmp(analogstr,"480i") ) {
		if ((vm->outputa&0xff00)==HD_VM_OUTA_PORT_SCART)
			strcat (analogstr, " out_scart");
		else if ((vm->outputa&0xff00)==HD_VM_OUTA_PORT_SCART_V0) // no switch voltage
			strcat (analogstr, " out_scart0");
		else if ((vm->outputa&0xff00)==HD_VM_OUTA_PORT_MDIN)
			strcat (analogstr, " out_mdin");

		if ((vm->outputa&0xff)==HD_VM_OUTA_YUV)
			strcat (analogstr, " yuv");
		else if ((vm->outputa&0xff)==HD_VM_OUTA_YC)
			strcat (analogstr, " yc");
		else if ((vm->outputa&0xff)==HD_VM_OUTA_RGB)
			strcat (analogstr, " rgb");
		filter_trick=1;
	}
	else {
		strcat(analogstr,"Y");
	}

	{
#define SI9030_DVIHDMI	_IOR ('d', 351, int)  // DVI=0, HDMI=1
		int fd;
		fd=open("/dev/si9030",O_RDWR);
		if (fd>=0) {
			ioctl(fd,SI9030_DVIHDMI,dvihdmi);
			close(fd);
		}	
	}

	set_videomode(vmodestr);
	printf(">>>>>>>>>>>>videoChange: %s\n",vmodestr);
#if 1
	if (!strcmp(hdmistr,"off")) {
		    sprintf(cmdstr,"setsi9030 %s >/dev/null",hdmistr);
		    printf("#### %s\n",cmdstr);
		    system(cmdstr);
	}
#endif


	// Deint off for 480/576i over HDMI
	if (!strncmp(fmtstr,"576i",4) || !strncmp(fmtstr,"480i",4)) {		
		set_deinterlacer_raw(1); // deint off
		filter_trick=1;  // Filter disable
	}
	else {
		va->overscan=0;
		set_deinterlacer_raw(0); // normal mode
	}
        
	if (!strncmp(fmtstr,"576p",4))
		filter_trick=1;  // Filter disable

	// Now aspect
	if (vs>=0 && vs<=HD_VM_SCALE_DBD) {
                set_aspect(va->automatic, va->w, va->h,
                                va->overscan, vs);
	}

	sprintf(cmdstr,"setfs453 %s >/dev/null",analogstr);
	printf("#### %s\n",cmdstr);
	system(cmdstr);	

	// Disable inter-field filtering
	if (filter_trick ) {
		decypher_set_vfilter(1,4);
	}
	if (hdd->video_mode.deinterlacer==2)
		decypher_set_vfilter(2,4);

	// Signal video on/off state on Scart pin 16 for HDMI/YUV
#if 1
	if (vm->outputd==HD_VM_OUTD_OFF && vm->outputa==HD_VM_OUTA_OFF)
		set_gpio(GPIO_SCART_RGB,1); // 0V
	else if (!strcmp(analogstr,"off") || !strcmp(analogstr,"yuv"))
		set_gpio(GPIO_SCART_RGB,0); // 5V
#endif	
}

/* --------------------------------------------------------------------- */
void set_hw_control(hd_hw_control_t *hwc)
{
	if (hwc->audiomix)
		set_gpio(GPIO_AUDIO_SEL,0);
	else
		set_gpio(GPIO_AUDIO_SEL,1);
}
/* --------------------------------------------------------------------- */
void set_audio_control(hd_audio_control_t *hac)
{
	int fd;
	char volstr[32];
	fd=open("/sys/class/wis/adec0/volume",O_RDWR);
	if (fd>=0) {
//DL		float nv=pow((float)(hac->volume),1.5)/15.96;
		float nv=(pow(26.5,(float)(hac->volume)/255)-1)*10;
		sprintf(volstr,"0x%x\n", (int)nv);
		write(fd,volstr,strlen(volstr));
		close(fd);
	}
	fd=open("/sys/class/wis/adec1/volume",O_RDWR);
	if (fd>=0) {
//DL		float nv=pow((float)(hac->volume1),1.5)/15.96;
		float nv=(pow(26.5,(float)(hac->volume)/255)-1)*10;
		sprintf(volstr,"0x%x\n", (int)nv);
		write(fd,volstr,strlen(volstr));
		close(fd);
	}
}
/* --------------------------------------------------------------------- */
// misnomer: checks all settings for changes
void video_check_settings(void)
{
	int n;
	int force_fb_update=0;

	if (hdd->video_mode.changed!=last_video_mode.changed) {
//		printf("############# %i %i\n",hdd->video_mode.changed, last_video_mode.changed);
		int n;
			
		last_video_mode=hdd->video_mode;

		hdp_kill();
		for(n=0;n<10;n++) {
			if (!hdd->hdp_running)
				break;
			usleep(100*1000);
		}
		usleep(500*1000);

		last_player_status = hdd->player_status[0];
		set_vmode(&hdd->video_mode, &hdd->aspect, &last_player_status);
		force_fb_update=1;
	}

	if(hdd->video_mode.auto_format && hdd->player_status[0].h && (hdd->player_status[0].h != last_player_status.h)) {
//		printf("\n\n\n>>>>>>>>>>>>>>>>>>>>>>>>>Changing vmode\n\n\n\n\n");
		last_player_status = hdd->player_status[0];
		set_vmode(&hdd->video_mode, &hdd->aspect, &last_player_status);
		force_fb_update=1;
	} // if
	
	if (hdd->aspect.changed != last_aspect.changed) {
		last_aspect=hdd->aspect;
		
		set_aspect(hdd->aspect.automatic, hdd->aspect.w, hdd->aspect.h,
				   hdd->aspect.overscan, hdd->aspect.scale & HD_VM_SCALE_VID);	
	}

	if (hdd->hw_control.changed!=last_hw_control.changed) {
		last_hw_control=hdd->hw_control;
		set_hw_control(&last_hw_control);
	}

	if (hdd->audio_control.changed!=last_audio_control.changed) {
		last_audio_control=hdd->audio_control;
		set_audio_control(&last_audio_control);
	}
	
	for(n=0;n<4;n++) {
		if (hdd->plane[n].changed!=last_plane_config[n].changed || (n==2 && force_fb_update)) {
			hd_plane_config_t old_plane_vals;
			memcpy(&old_plane_vals, &last_plane_config[n], sizeof(hd_plane_config_t));
			memcpy(&last_plane_config[n], &hdd->plane[n], sizeof(hd_plane_config_t));

			if (n==0)
				decypher_set_bcg(hdd->plane[n].gamma/100.0,hdd->plane[n].brightness*8-1024,hdd->plane[n].contrast/128.0);
			if (n==2)
				set_framebuffer(&hdd->video_mode, &hdd->plane[n], &old_plane_vals, force_fb_update);
		}
	}
}
#endif // CONFIG_MIPS
/* --------------------------------------------------------------------- */
void set_dont_touch_osd(int dont_touch_osd)
{
        hdd->osd_dont_touch = dont_touch_osd;
#ifdef CONFIG_MIPS
        enable_fb_layer(dont_touch_osd, 255);
#endif
}
/* --------------------------------------------------------------------- */
// asp: WF WS NF NS Wide/Normal Fill/Scale

void set_video_from_string(char *vm, char* dv, char* asp, char * analog_mode, char * port_mode, char* digital_audio_mode)
{
	int w,h,i,fps;
	int dvimode=HD_VM_OUTD_HDMI;
	hd_aspect_t hda={0};
	int default_asp=1; // 16:9
	int vchange=0;
	w=h=i=fps=0;

	if(!strcasecmp(digital_audio_mode, "AC3")) {
		hdd->video_mode.outputda=HD_VM_OUTDA_AC3;
		vchange=1;
	}
	else if (!strcasecmp(digital_audio_mode, "PCM")) {
		hdd->video_mode.outputda=HD_VM_OUTDA_PCM;
		vchange=1;
	}

	if (!strcasecmp(vm,"1080p")) {
		w=1920;
		h=1080;
		i=0;
		fps=60;
	}
	else if (!strcasecmp(vm,"1080i")) {
		w=1920;
		h=1080;
		i=1;
		fps=60;
	}
	else if (!strcasecmp(vm,"1080i50")) {
		w=1920;
		h=1080;
		i=1;
		fps=50;
	}
	else if (!strcasecmp(vm,"720p")) {
		w=1280;
		h=720;
		i=0;
		fps=60;
	}
	else if (!strcasecmp(vm,"720p50")) {
		w=1280;
		h=720;
		i=0;
		fps=50;
	}
	else if (!strcasecmp(vm,"480p")) {
		w=720;
		h=480;
		i=0;
		fps=60;
		default_asp=0;
	}
	else if (!strcasecmp(vm,"480i")) {
		w=720;
		h=480;
		i=1;
		fps=60;
		default_asp=0;
	}
	else if (!strcasecmp(vm,"576p")) {
		w=720;
		h=576;
		i=0;
		fps=50;
		default_asp=0;
	}
	else if (!strcasecmp(vm,"576i")) {
		w=720;
		h=576;
		i=1;
		fps=50;
		default_asp=0;
	}

	if (!strcasecmp(dv,"DVI")) {
		dvimode=HD_VM_OUTD_DVI;
		vchange=1;
	}

        if (!strcasecmp(dv,"HDMI")) {
                dvimode=HD_VM_OUTD_HDMI;
                vchange=1;
        }

	if (!strcasecmp(asp,"WF") ) {
		hda.w=16;
		hda.h=9;
		hda.scale=HD_VM_SCALE_F2S;
		hda.overscan=0;
		hda.automatic=0;
	}
	if (!strcasecmp(asp,"WS") || (strlen(asp)==0 && default_asp)) {
		hda.w=16;
		hda.h=9;
		hda.scale=HD_VM_SCALE_F2A;
		hda.overscan=0;
		hda.automatic=0;
	}
	if (!strcasecmp(asp,"WC")) {
		hda.w=16;
		hda.h=9;
		hda.scale=HD_VM_SCALE_C2F;
		hda.overscan=0;
		hda.automatic=0;
	}
	if (!strcasecmp(asp,"NF")) {
		hda.w=4;
		hda.h=3;
		hda.scale=HD_VM_SCALE_F2S;
		hda.overscan=0;
		hda.automatic=0;
	}
	if (!strcasecmp(asp,"NS")) {
		hda.w=4;
		hda.h=3;
		hda.scale=HD_VM_SCALE_F2A;
		hda.overscan=0;
		hda.automatic=0;
	}
	if (!strcasecmp(asp,"NC")) {
		hda.w=4;
		hda.h=3;
		hda.scale=HD_VM_SCALE_C2F;
		hda.overscan=0;
		hda.automatic=0;
	}

	if (w) {
		hdd->video_mode.width=w;
		hdd->video_mode.height=h;
		hdd->video_mode.interlace=i;
		hdd->video_mode.framerate=fps;
		hdd->video_mode.outputd=HD_VM_OUTD_HDMI;
		hdd->video_mode.outputa=HD_VM_OUTA_AUTO;
		vchange=1;
	}
	
	if (hda.w) {
		hda.changed=hdd->aspect.changed;
		hdd->aspect=hda;
		hdd->aspect.changed=hda.changed+1;
	}
	
	hdd->video_mode.outputd=dvimode;
	
	if (!strcasecmp(vm,"off")) {
		hdd->video_mode.outputd=HD_VM_OUTD_OFF;
		hdd->video_mode.outputa=HD_VM_OUTA_OFF;
		vchange=1;
	}
	else {
		int vchange1;
		vchange1=1;
		if (!strcasecmp(analog_mode,"yuv"))
			hdd->video_mode.outputa=HD_VM_OUTA_YUV;
		else if (!strcasecmp(analog_mode,"rgb"))
			hdd->video_mode.outputa=HD_VM_OUTA_RGB;
		else if (!strcasecmp(analog_mode,"yc"))
			hdd->video_mode.outputa=HD_VM_OUTA_YC;
		else
			vchange1=0;

		if (!strcasecmp(port_mode,"scart")) {
			hdd->video_mode.outputa|=HD_VM_OUTA_PORT_SCART;
			vchange=1;
		}
		else if (!strcasecmp(port_mode,"MDIN")){
			hdd->video_mode.outputa|=HD_VM_OUTA_PORT_MDIN;
			vchange=1;
		}
		vchange=vchange | vchange1;
	}

	if (vchange)
		hdd->video_mode.changed++;
}
/* --------------------------------------------------------------------- */
void set_audio_mix(int audiomix)
{
	hdd->hw_control.audiomix=audiomix;
	hdd->hw_control.changed++;
}
/* --------------------------------------------------------------------- */
void set_audio_volume(int volume)
{
	hdd->audio_control.volume=volume;
	hdd->audio_control.changed++;
}
/* --------------------------------------------------------------------- */
void set_audio_volume1(int volume)
{
	hdd->audio_control.volume1=volume;
	hdd->audio_control.changed++;
}
/* --------------------------------------------------------------------- */
void set_bcg(int brightness, int contrast, int gamma)
{
	if (brightness!=-1)
		hdd->plane[0].brightness=brightness;
	if (contrast!=-1)
		hdd->plane[0].contrast=contrast;
	if (gamma!=-1)
		hdd->plane[0].gamma=gamma;
	hdd->plane[0].changed++;

}
/* --------------------------------------------------------------------- */
//#ifndef CONFIG_MIPS
void set_deinterlacer(int deint)
{
	if (hdd->video_mode.deinterlacer!=deint) {
		hdd->video_mode.deinterlacer=deint;
		hdd->video_mode.changed++;
	}
}
//#endif
