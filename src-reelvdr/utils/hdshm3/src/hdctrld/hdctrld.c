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
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#ifdef CONFIG_MIPS
#include "decypher_tricks.h"
#endif
#include "hdshmlib.h"
#include "hdshm_user_structs.h"

volatile hd_data_t *hdd;

#define HDPLAYER "hdplayer"
#define HDPLAYER_CMD "/tmp/"HDPLAYER

#define POWER_OFF "/bin/snooze on"
#define POWER_ON "/bin/snooze off"

// killall doesn't work always
//#define KILL_HDPLAYER "kill `ps|grep "HDPLAYER"|grep -v grep |cut -b -6` >/dev/null 2>&1 "
#define KILL_HDPLAYER "kill `pidof "HDPLAYER"` >/dev/null 2>&1"
#define KILL9_HDPLAYER "kill -9 `pidof "HDPLAYER"` >/dev/null 2>&1"

extern void set_video_from_string(char *vm, char* dv, char* asp, char * analog_mode, char * port_mode, char* digital_audio_mode);
extern void set_audio_mix(int audiomix);
extern void set_audio_volume(int volume);
extern void set_audio_volume1(int volume);
extern void set_deinterlacer(int deint);
extern void set_bcg(int brightness, int contrast, int gamma);
extern void set_dont_touch_osd(int dont_touch_osd);

#ifdef CONFIG_MIPS
extern void video_check_settings(void);

/* --------------------------------------------------------------------- */
void hdp_kill(void)
{
	int to=20;
	printf("hdp_kill\n");
	hdd->hdp_terminate=1;
	spdif_mute();
	while (hdd->hdp_terminate && to) {
		usleep(10*1000);
		to--;
	}
	if (hdd->hdp_terminate) { // Kill hard
		hdd->hdp_terminate=0;
//		printf("%s\n",KILL_HDPLAYER);
		system(KILL_HDPLAYER);
		usleep(100*1000);
	}
	hdd->hdp_running=0;
}
/* --------------------------------------------------------------------- */
void* hd_thread(void* x)
{
	hdp_kill();

	hdd->hdp_running=1; // FIXME move to hdplayer?
	hdd->event++;
//	system(POWER_ON);
	if (hdd->hd_shutdown) {
		sleep(3);
		video_check_settings();
	}
	hdd->hd_shutdown=0;
	printf("%s\n",HDPLAYER_CMD);
	system(HDPLAYER_CMD);
	system(KILL9_HDPLAYER); // Remove Zombies
	hdd->hdp_running=0;
	hdd->event++;
	printf("###### Player dead\n");
	return NULL;
}
/* --------------------------------------------------------------------- */
void sighandle(int x)
{
}
/* --------------------------------------------------------------------- */
void hd_daemon(void)
{
	pthread_t pt1;

	init_decypher_tricks();
	video_check_settings();
	signal(SIGCHLD,sighandle);

	while(1) {
//		printf("enable: %i running: %i, term %i\n",hdd->hdp_enable, hdd->hdp_running,hdd->hdp_terminate );
		if (hdd->hdp_enable) {
			if (!hdd->hdp_running && !hdd->hdp_terminate) {
				if (hdd->hd_shutdown) {
					hdd->audio_control.changed++;
					hdd->hw_control.changed++;
					hdd->video_mode.changed++;
				}
				pthread_create(&pt1, NULL, hd_thread,NULL);
				pthread_detach(pt1);
				sleep(1);
			}
		}

		if (!hdd->hdp_enable && hdd->hdp_running) {
			hdp_kill();
		}

		video_check_settings();

		usleep(200*1000);
		if (!hdd->hdp_enable && !hdd->hdp_running && hdd->hd_shutdown==1) {
			printf("Power off decoder\n");
			system(POWER_OFF);
			hdd->hd_shutdown=2;
		}
	}
	system(KILL_HDPLAYER);
	hdd->hdp_running=0;
	hdd->hdc_running=0;
}
#endif // CONFIG_MIPS
/* --------------------------------------------------------------------- */
#define ALIGN_DOWN(v, a) ((v) & ~((a) - 1))
#define ALIGN_UP(v, a) ALIGN_DOWN((v) + (a) - 1, a)

void struct_init(int clear)
{
#ifdef CONFIG_MIPS
        int area_size = ALIGN_UP(sizeof(hd_data_t), 2*4096);
#endif

	hdshm_area_t *hsa;
	if (hd_init(0)) {
		fprintf(stderr,"hdctrld: hdshm driver not loaded!\n");
		exit(1);
	}

#ifdef CONFIG_MIPS
	hsa=hd_get_area(HDID_HDA);
	if (!hsa) {

		hsa=hd_create_area(HDID_HDA,0,area_size,HDSHM_MEM_HD);

		if (!hsa) {
			fprintf(stderr,"hdctrld: Can't create shm area!\n");
			exit(1);
		}

		hdd=(hd_data_t*)hsa->mapped;
                memset((void *)hdd, 0, area_size);
		hdd->hdc_running=1;
		hdd->hdp_running=0;
		hdd->event=0;
		hdd->hd_shutdown=0;
		hdd->hdp_terminate=0;
		hdd->video_mode.changed=0;
		hdd->video_mode.deinterlacer=HD_VM_DEINT_AUTO; // currently not changed via cmdline
		set_video_from_string("576p","dvi","NS", "AUTO", "AUTO", "PCM");
		set_audio_mix(0);
		hdd->audio_control.changed=0;
		set_audio_volume(0x80);
		set_audio_volume1(0x80);
		hdd->plane[0].brightness=128;
		hdd->plane[0].contrast=120;
		hdd->plane[0].gamma=115;
		hdd->plane[0].w=0;
		hdd->plane[0].h=0;

		hdd->plane[2].w=720;
		hdd->plane[2].h=576;
		hdd->plane[2].ws=0;
		hdd->plane[2].mode=0x41;
		hdd->plane[2].enable=1;
		hdd->plane[2].alpha=255;
		hdd->plane[2].vw=0;
		hdd->plane[2].vh=0;
		hdd->plane[2].vx=0;
		hdd->plane[2].vy=0;
	}
	else {
		hdd=(hd_data_t*)hsa->mapped;
		if (clear)
			memset((void *)hdd, 0, area_size);
		// Some more sane states
		hdd->plane[0].brightness=128;
		hdd->plane[0].contrast=120;
		hdd->plane[0].gamma=115;
		
		hdd->plane[2].alpha=255;
		hdd->plane[2].mode=0x41;
		hdd->plane[2].enable=1;
		hdd->video_mode.deinterlacer=1;
	}
#else
	hsa=hd_get_area(HDID_HDA);

	if (!hsa) {
		fprintf(stderr,"hdctrld: DeCypher HDSHM not running!\n");
		exit(1);
	}
	hdd=(hd_data_t*)hsa->mapped;
#endif

}
/* --------------------------------------------------------------------- */
void usage(void)
{
	fprintf(stderr,
		"hdctrld  --  Controlling tool for DeCypher HD\n"
		"Options:\n"
#ifdef CONFIG_MIPS
		"         -d         Start HD player daemon\n"
#endif
		"         -s         Start  HD player\n"
		"         -k         Stop   HD player\n"
		"         -S         Status HD player\n"
		"         -P         Power down decoder\n"
#ifndef RBLITE
		"         -M <0|1>   Mix analog audio with MB sound\n"
#endif
		"         -v <vmode>       Set video mode (see below)\n"
		"         -o <dvimode>     Set DVI mode (HDMI, DVI)\n"
		"         -A <audiomode>   Set digital audio mode (AC3, PCM)\n"
		"         -O <analogmode>  Set analog mode\n"
		"         -p <output port> Set analog port\n"
		"         -a <aspect> Set aspect mode (WF/WS/WC/NF/NS/NC)\n"
		"         -V <volume>      Set audio volume (0-255)\n"
		"         -X <0|1>  Forbid VDR to touch the OSD\n"
		"         -B <brightness>  0-255\n"
		"         -C <contrast>    0-255\n"
		"         -G <gamma>       0-255 (0-2.55)\n"
		"         -D <deinterlacer> 0,1\n"
		"\nAllowed video modes:\n"
		"    1080p, 1080i, 1080p50, 1080i50, 720p, 720p50\n"
		"    576p, 576i, 480p, 480i, off\n"
		"Aspect modes: Wide/Normal+Fill/Scale/Crop\n"
		"Analog modes: AUTO, YUV, RGB, YC\n"
		"Analog ports: AUTO, SCART, MDIN\n"
		);
	exit(0);
}
/* --------------------------------------------------------------------- */
int main(int argc, char** argv)
{
	int start_daemon=0;
	char vm[256]="",dm[256]="",am[256]="";
	char anm[256]="",pm[256]="",dam[256]="";
	int enable=-1;
	int status=0;
	int audiomix=-1;
	int volume=-1;
        int dont_touch_osd=-1;
        int deint=-1;
	int powerdown=0;
	int brightness=-1,contrast=-1,gamma=-1;

	while (1)
        {
#ifdef CONFIG_MIPS
		char c = getopt(argc, argv, "dskv:o:a:O:p:M:A:V:X:PB:C:G:D:S?h");
#else
		char c = getopt(argc, argv, "skv:o:a:O:p:M:A:V:X:PB:C:G:D:S?h");
#endif
		if (c == -1)
                        break;

		switch(c)
		{
		case 'd':
			start_daemon=1;
			break;
		case 's':
			enable=1;
			break;
		case 'k':
			enable=0;
			break;
		case 'S':
			status=1;
			break;
		case 'v':
			strncpy(vm,optarg,256);
			vm[255]=0;
			break;
		case 'o':
			strncpy(dm,optarg,256);
			dm[255]=0;
			break;
		case 'a':
			strncpy(am,optarg,256);
			am[255]=0;
			break;
		case 'A':
			strncpy(dam,optarg,256);
			dam[255]=0;
			break;
		case 'M':
			audiomix=atoi(optarg);
			break;
		case 'O':
			strncpy(anm,optarg,256);
			anm[255]=0;
			break;
		case 'p':
			strncpy(pm,optarg,256);
			pm[255]=0;
			break;
		case 'V':
			volume=atoi(optarg);
			break;
		case 'P':
			powerdown=1;
			enable=0;
			break;
		case 'X':
			dont_touch_osd=atoi(optarg);
			break;
		case 'B':
			brightness=atoi(optarg)&255;
			break;
		case 'C':
			contrast=atoi(optarg)&255;
			break;
		case 'G':
			gamma=atoi(optarg)&255;
			break;
		case 'D':
			deint=atoi(optarg)&255;
			break;
		case '?':
		case 'h':
		default:
			usage();
		}
	}
	if (start_daemon)
		struct_init(1);
	else
		struct_init(0);

	if (enable>=0)
		hdd->hdp_enable=enable;

	if (status>0) {
		printf("HD player running: %s\n", hdd->hdp_running ? "yes" : "no");
		return((hdd->hdp_running > 0) ? 0 : 1);
	};

	if (powerdown)
		hdd->hd_shutdown=powerdown;

	if (strlen(vm) || strlen(dm) || strlen(am)|| strlen(anm)|| strlen(pm) || strlen(dam)) {
#ifndef CONFIG_MIPS
		printf("SET vm='%s' dm='%s' am='%s'\n",vm,dm,am);
#endif
		set_video_from_string(vm,dm,am,anm,pm,dam);
	}

	if (deint!=-1)
		set_deinterlacer(deint);
		
	if (audiomix!=-1)
		set_audio_mix(audiomix);

	if (volume!=-1)
		set_audio_volume(volume);

	if(dont_touch_osd!=-1)
		set_dont_touch_osd(dont_touch_osd);

	if (brightness!=-1 || contrast!=-1 || gamma!=-1) {
		set_bcg(brightness,contrast,gamma);
	}

#ifdef CONFIG_MIPS
	if (start_daemon)
		hd_daemon();
#endif
	return 0;
}
