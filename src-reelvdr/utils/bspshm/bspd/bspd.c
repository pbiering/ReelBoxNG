/*****************************************************************************/
/*
 * BSP15 management daemon for Reelbox
 *
 * Copyright (C) 2006 Georg Acher, BayCom GmbH (acher at baycom dot de)
 *
 * #include <GPL-header>
 *
 * $Id: bspd.c,v 1.3 2006/03/20 22:44:35 acher Exp $
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
#include <signal.h>

#include <sys/io.h>

#include "bspchannel.h"
#include "config.h"

// Geode Watchdog I/O

#define WDTO 0x9000
#define WDCNFG 0x9002
#define WDSTS 0x9004
 
volatile bspd_data_t *bspd;
bsp_channel_t *bsc_osd,*bsc_cmd, *bsc_pes, *bsc_pes_pip;

extern void SyncWithFocus(volatile bspd_data_t *bspd);
extern void video_check_settings(int force);
void  video_default(void);

volatile int last_watchdog=0;
volatile int crash_restart=0;
int last_killed=0;

extern int scart_state;

// Taken from reelbox-plugin/VideoPlayer.h
// FIXME

typedef struct 
{
	unsigned int flags_;
	unsigned int generation_;
	unsigned int streamPos_;
	unsigned int timestamp_;
} VideoPacketHeader;

/* --------------------------------------------------------------------- */
void* bsp_thread(void* x)
{
	char *cmd=(char*)x;
	printf("##################################################################################\n");
	printf("bspd: Starting <%s>\n",cmd);
	bspd->bsp_watchdog=0;
	last_watchdog=0;
	bspd->event++;
	bspd-> video_attributes[0].changed=0;
	system(BSP_KILL);
	system(cmd);	
	bspd->status=0;
	bspd->event++;

	if (bspd->bsp_auto_restart) {
		bspd->bsp_enable=1;
		crash_restart=1;
		printf("bspd: BSP dead, restarting\n");
	}
	else
		printf("bspd: BSP dead\n");

	bspd->bsp_watchdog=0;
	last_watchdog=0;

	return NULL;
}
/* --------------------------------------------------------------------- */
void* demo_thread(void* x)
{
	FILE *file;
	int ret,size;
	char *buffer;
	int loop = 12;
	int n=0;
	int streampos=0;

	printf("bspd: DEMO thread started\n");
	while(!bspd->status)
		usleep(10*1000);

	// bsp_channel_invalidate(bsc_cmd,2000);

	file=fopen(STARTUP_FILE1,"r");
	if (!file) {
        printf("bspd: DEMO not found!\n");
		bspd->demo_running=0;
		return NULL;
	}

    bspd->video_player[0].stream_generation = 1;

	while(!bspd->demo_stop && bspd->status)
    {
		if (file) {
			int packetLength=2048;
			ret=bsp_channel_write_start(bsc_pes,(void**)&buffer,packetLength+sizeof(VideoPacketHeader),2000);
			if (ret) {
				VideoPacketHeader headerVals = {0, 1, streampos, 0};
				VideoPacketHeader *header = (VideoPacketHeader *)buffer;
				*header = headerVals;
				buffer+=sizeof(VideoPacketHeader);
				size=fread(buffer,1,2048 - sizeof(VideoPacketHeader),file);
				if (size <= 0) {
					if (!loop) {
                        break;
					}
					else
						fseek(file, 0, SEEK_SET);
					loop -= 1;
				}
                else
                {
                    bsp_channel_write_finish(bsc_pes,sizeof(VideoPacketHeader)+size);
                }
				streampos+=size;
			}       
		}	
		else
		{
			usleep(1000);
		}
	}
	bspd->demo_stop=0;
	fclose(file);	

	bspd->video_player[0].changed++;

	usleep(200*1000);
	bspd->demo_running=0;
	// printf("bspd: DEMO thread stopped\n");
	return NULL;
}
/* --------------------------------------------------------------------- */
void set_geode_watchdog(int n)
{
	if (!n) {
		outw(0x0d, WDCNFG);
		printf("bsdp: Disabled Geode Watchdog\n");
	}
	else {
		outb(0x1,WDSTS);   // clear overflow
		outw(n*4,WDTO);
		outw(0x03d,WDCNFG); // Reset on timeout, 0.25s tics
	}
}
/* --------------------------------------------------------------------- */
void ctrlc_handler(int)
{      
	set_geode_watchdog(0);
	exit(0);
}
/* --------------------------------------------------------------------- */
void* focus_sync_thread(void *parm)
{
	int v=*(int*)parm;
	if (v==3)
		sleep(3);
	printf("bspd: Start Focus Sync\n");
	SyncWithFocus(bspd);
	printf("bspd: Focus Sync done\n");
	return NULL;
}
/* --------------------------------------------------------------------- */
void bsp_daemon(char *bsp_binary, int demo_mode, int geode_watchdog)
{
	int start,n;
	char cmd[128];
	bspshm_area_t * bsa;
	pthread_t pt1,pt2,pt3;
	int last_watchdog_time=0;
	int watchdog_enable=0;
	int force=0;
	int sync_var;
	int bsp_shutdown=0;
	
	if (geode_watchdog) {
		signal(SIGINT,ctrlc_handler); // For debugging
		set_geode_watchdog(geode_watchdog);
		printf("bsdp: Enabled Geode Watchdog, timeout %i s\n", geode_watchdog);
	}

	bsp_init(0);

	if (sizeof(bspd_data_t)>2*4096) {
		fprintf(stderr,"bspd: Adjust BSPID_BSPD area size!\n");
		exit(-1);
	}

	bsa=bsp_create_area(BSPID_BSPD,0,2*4096,0);
	if (!bsa) {
		fprintf(stderr,"bspd: bspshm driver not loaded!\n");
		exit(0);
	}
	start=bspshm_get_root();
	bspshm_reset();
	bsp_channel_init();

	bspd=(bspd_data_t*)bsa->mapped;
	memset((void*)bspd,0,sizeof(bspd_data_t));
	video_default();

	bsc_osd=bsp_channel_create(BSPCH_OSD, 0x20000, BSP_CHANNEL_WRITE);
    bsc_pes=bsp_channel_create(BSPCH_PES1, 0x40000, BSP_CHANNEL_WRITE);
    bsc_pes_pip=bsp_channel_create(BSPCH_PES2, 0x20000, BSP_CHANNEL_WRITE);

	// Default Videomode already set by bspd main

    bspd->video_player[0].channel=BSPCH_PES1;
    bspd->video_player[0].use_xywh=0;
    bspd->video_player[0].x=bspd->drc_status.width/2-720/4;
    bspd->video_player[0].y=bspd->drc_status.height/2-576/4;
    bspd->video_player[0].w=720/2;
    bspd->video_player[0].h=576/2;
    bspd->video_player[0].aspect[0]=4; // Hack
    bspd->video_player[0].aspect[1]=3;
    bspd->video_player[0].stream_generation=1;
    bspd->video_player[0].trickspeed=0;
    bspd->video_player[0].changed++;
    bspd->video_player[1].pip = 0;

    bspd->video_player[1].channel=BSPCH_PES2;
    bspd->video_player[1].use_xywh=0;
    bspd->video_player[1].x=bspd->drc_status.width/2-720/4;
    bspd->video_player[1].y=bspd->drc_status.height/2-576/4;
    bspd->video_player[1].w=720/2;
    bspd->video_player[1].h=576/2;
    bspd->video_player[1].aspect[0]=4; // Hack
    bspd->video_player[1].aspect[1]=3;
    bspd->video_player[1].stream_generation=1;
    bspd->video_player[1].trickspeed=0;
    bspd->video_player[1].changed++;
    bspd->video_player[1].pip = 1;

	printf("########### Try %i\n",bspd->hw_video_mode.mode);
	sprintf(cmd,"%s %s %x",BSP_APP_START_COMMAND, bsp_binary, start);

	bspd->demo_start=demo_mode;

	system(BSP_KILL); // Kill running BSP

	bspd->demo_running=0; // GT

	n=0;
	while(!bspd->bspd_shutdown) {
		
		//------------ Run command
		if (bspd->bsp_enable) {
			bspd->bsp_enable=0;
			if (!bspd->status) {
				if (last_killed) {
					force=1; // Force focus settings
					system("reelhwctl -spdif-in bsp -video-mute 15 -audio-mute 0 -scart-enable 1");
				}
				pthread_create(&pt1, NULL, bsp_thread,cmd);
				pthread_detach(pt1);
				usleep(1000*1000);
				watchdog_enable=1;
				last_watchdog_time=0;
				bspd->need_focus_sync=1;
				last_killed=0;
				if (bsp_shutdown)
					force=1;
				bsp_shutdown=0;
			}			
		}

		//------------ Kill command

		if (bspd->bsp_kill) {
			if (!bspd->bsp_auto_restart) {
				bsp_shutdown=1;
				scart_state=0;
				system("reelhwctl -scart-enable 0 -scart-tv ic -scart-vcr ic");
			}
			printf("bsdp: Got Kill\n");
			bspd->bsp_kill=0;
			bspd->demo_stop=1;
			if (bspd->status) {
				usleep(1000*1000); // Let the crash log come out
				watchdog_enable=0;
				last_watchdog_time=0;
				system(BSP_KILL);
				bspd->event++;
				last_killed=1;
			}
		}

		//------------ Watchdog

		if (last_watchdog!=bspd->bsp_watchdog) {
			last_watchdog_time=time(NULL);
			last_watchdog=bspd->bsp_watchdog;
//			printf("bspd: WD %i\n",last_watchdog);
		}

		if (bspd->status && watchdog_enable && last_watchdog_time && 
		    (time(NULL)-last_watchdog_time)>WD_TIMEOUT) {
			printf("bspd: WATCHDOG Timeout, Kill BSP\n");
			bspd->bsp_kill=1;
			last_watchdog_time=0;
		}
	
		//------------ Demo mode

		if (bspd->demo_start) {
			printf("GOT DEMO START\n");
			bspd->demo_start=0;
			if (!bspd->status) {
				printf("GOT DEMO START1\n");
				bspd->bsp_enable=1;
				bspd->demo_running=1;
				bspd->demo_stop=0;
				pthread_create(&pt2, NULL, demo_thread,NULL);
				pthread_detach(pt2);
			}
		}
		
		//------------ Video mode management

		video_check_settings(force);

		//------------ Focus sync

		if (bspd->status && bspd->need_focus_sync && (bspd->video_mode.main_mode==BSP_VMM_SD || bspd->video_mode.main_mode==BSP_VMM_SDHD)) {
			sync_var=bspd->need_focus_sync;
			bspd->need_focus_sync=0;
			pthread_create(&pt3, NULL, focus_sync_thread,&sync_var);
			pthread_detach(pt3);
		}

		usleep(50*1000);
		force=0;
		n++;
		if ((n&31)==0 && geode_watchdog) {
//			printf("Retrigger\n");
			set_geode_watchdog(geode_watchdog);
		}

	}
	bspd->demo_stop=1;

	for(n=0;(n<30) && bspd->demo_running;n++)
		usleep(100*1000);

	usleep(500*1000);
	system(BSP_KILL);
	bspd->status=0;
	bspd->bspd_shutdown=0;
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
}
/* --------------------------------------------------------------------- */
void issue_focus_sync(void)
{
	slave_bspd_init();
	bspd->need_focus_sync=1;
}
/* --------------------------------------------------------------------- */
void issue_start_demo(void)
{
	slave_bspd_init();
	bspd->demo_start=1;
}
/* --------------------------------------------------------------------- */
void issue_kill_bsp(void)
{
	slave_bspd_init();
	bspd->bsp_auto_restart=0;
	bspd->bsp_kill=1;
	system("reelhwctl -scart-enable 0 -scart-tv ic -scart-vcr ic");
}

/* --------------------------------------------------------------------- */
void issue_kill_daemon(void)
{
	slave_bspd_init();
	bspd->bspd_shutdown=1;
}
/* --------------------------------------------------------------------- */
void usage(void)
{
	fprintf(stderr,
		"bspd: BSP15 Management Daemon\n"
		"Options:   -d            Start Daemon\n"
		"           -b <file>     BSP-binary\n"
		"           -k            Kill BSP, disable restart\n"
		"           -n            No demo/no autostart \n"
		"           -F            Issue Focus resync\n"
		"           -D            Issue demo mode\n"
		"           -K            Kill Daemon\n"
		"           -W <timeout in s>  Enable Geode Watchdog (disable: n=0)\n"
		);
	exit(-1);
}
/* --------------------------------------------------------------------- */
int main(int argc, char** argv)
{
	char c;
	int daemon_mode=0;
	int demo_mode=1;
	char *bsp_binary=BSP_APP_NAME;
	int geode_timeout=0;

	iopl(3);
	while (1)
        {
		c = getopt(argc, argv, "b:dkDFKnW:?");

		if (c == -1)
                        break;
		
		switch (c) 
		{
		case 'b':
			bsp_binary=optarg;
			break;
		case 'd':
			daemon_mode=1;
			break;
		case 'F':
			issue_focus_sync();
			break;
		case 'k':
			issue_kill_bsp();
			break;
		case 'D':
			issue_start_demo();
			break;
		case 'K':
			issue_kill_daemon();
			break;
		case 'n':
			demo_mode=0;
			break;
		case 'W':
			geode_timeout=atoi(optarg);
			set_geode_watchdog(geode_timeout);
			break;
		default:
                        usage();
                        return 1;
                }
	}

	if (daemon_mode)
		bsp_daemon(bsp_binary, demo_mode, geode_timeout);
	return 0;
}
