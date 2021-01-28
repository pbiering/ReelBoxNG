/*
  fan_control.c - Controling main fan for Reelbox Avantgarde
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fcntl.h>

#include "frontpanel.h"

#define MINIMUM_FILE0 "/etc/reel/fan_control0"
#define MINIMUM_FILE1 "/etc/reel/fan_control1"

#define SENSOR_MAINBOARD "/sys/devices/platform/w83627hf.2560/temp1_input"
#define SENSOR_CPU "/sys/devices/platform/w83627hf.2560/temp2_input"

//#define SENSOR_MAINBOARD_AMD "/sys/devices/platform/i2c-adapter:i2c-9191/9191-0a10/temp1_input" // old kernel 2.6.22?? 24??
#define SENSOR_MAINBOARD_AMD "/sys/devices/platform/w83627ehf.2576/temp1_input"   //2.6.24-28
#define SENSOR_CPU_AMD "/sys/devices/pci0000:00/0000:00:18.3/temp1_input" // Core 1

#define SENSOR_CPU_ICE "/sys/class/hwmon/hwmon0/device/temp1_input"

#define DEFAULT_TEMP 50
#define CHECK_PERIOD 20

#define min(a,b) ((a)>(b)?(b):(a))
#define max(a,b) ((a)<(b)?(b):(a))

#define MIN_CPU_FAN 300
#define MIN_PS_FAN  112	

extern int daemon_debug;

int last_fan;
int mb_type=0;

int fan_set[2];
int fan_min[2]={MIN_CPU_FAN,MIN_PS_FAN};

int get_file_min(const char *file)
{
	FILE *f;
	int val=-1;

	f=fopen(file,"r");
	if (!f)
		return val;
	fscanf(f,"%i",&val);
	fclose(f);
	if (val==-1)
		return val;

	if (val<20)
		val=20;
	if (val>0x7ff)
		val=0x7ff;
	return val;
}

double get_temp(int mode)
{
	int fd,t;
	char buf[256];

	mb_type=0;
	if (!mode)
		fd=open(SENSOR_MAINBOARD,O_RDONLY);
	else
		fd=open(SENSOR_CPU,O_RDONLY);

	if (fd<0) {
		if (!mode)
			fd=open(SENSOR_MAINBOARD_AMD, O_RDONLY);
		else
			fd=open(SENSOR_CPU_AMD, O_RDONLY);
		mb_type=1;
	}

	if (fd<0) {
		mb_type=2; // AVG3
		if (mode==1) 
			fd=open(SENSOR_CPU_ICE,O_RDONLY);
		// Netclient2?
		struct stat sb;
		if (!stat("/dev/.fpctl.noavr",&sb))
			mb_type=3; // NCL2!
	}

	if (fd<0)
		return DEFAULT_TEMP;

	if (read(fd,buf,256)) {
		t=atoi(buf);
		t=t/1000;
		close(fd);
		if (t>95)
			t=95;
		if (t<20)
			t=20;
#ifdef DEBUG
		if (mode)
			printf("Temp. CPU (%s): %d\n", mb_type>=2?"ICE":(mb_type==1?"AMD":"Intel"),t);
		else
			printf("Temp. MB:  %d\n", t);
#endif
		return t;
	}
	close(fd);
	return DEFAULT_TEMP;
}

/*
  Fan speed 320 (slow) - 48 (fast)
*/

double temp_avg[2];

void calc_speed(double temp[2], int mbtrip, int cputrip, int fanret[2])
{
	int n;
	int fan[2]={0x7ff,0x7ff};
	int fanx;
	int hyst=0;

	if (last_fan!=0x7ff)
		hyst=5;
		
	for(n=0;n<2;n++) {
		temp_avg[n]=(temp_avg[n]*2+temp[n])/3;
		if (temp_avg[n]>95)
			temp_avg[n]=95;
		if (temp_avg[n]<20)
			temp_avg[n]=20;
	}


	if (mb_type==1)		// AMD
		fan[0]=0x100;

	if (mb_type==2)		// ICE AVG3
		fan[0]=0xf8;

	if (mb_type==3) {      // special calc mode for NCL3, cputrip=start
		fan[1]=0x00; // off
		fanret[0]=0x7ff;
		if (last_fan!=0x0 && last_fan!=0x7ff)
			hyst=5;
		else
			hyst=0;
		
		if (temp_avg[1]>(cputrip-hyst)) {
			// Base speed 0xe0, high speed 0x10, emergency 0x01, off 0x00
			if (temp_avg[1]>90)
				fanret[1]=1;
			else {
				fanret[1]=(0xe0-(temp_avg[1]-(cputrip-hyst))/(90.0-(cputrip-hyst))*0xd0);
				if (fanret[1]<0x10)
					fanret[1]=0x10;
				if (fanret[1]>0xe0) // sanity, shouldn't happen
					fanret=0;
			}
		}		
		return;
	}
	
	// AVG1/2/3
//	printf("T1 %f %f, T2 %f %f\n",temp_avg[0], (float)mbtrip-hyst, temp_avg[1],(float)cputrip-hyst);
	if (temp_avg[0]>mbtrip-hyst)
		fan[0]=450-(temp_avg[0]-mbtrip)*20;

	if (temp_avg[1]>cputrip-hyst) {
		if (temp_avg[0]>mbtrip)
			fan[1]=300-((temp_avg[1]-cputrip )*17 + (temp_avg[0]-mbtrip)*20);
		else
			fan[1]=300-(temp_avg[1]-cputrip)*17;
	}
	fanx=fan[0];
	if (fan[1]<fan[0])
		fanx=fan[1];

	if (fanx<48)
		fanx=48;

	fanret[1]=fanx;	// CPU fan
	fanret[0]=max(0x40,0x80-(max(0,(temp_avg[1]-cputrip )*2) + max(0,(temp_avg[0]-25))*1.5)); // PS fan
	// GA-test? fanret[0]=max(0x40,0x74-(max(0,(temp_avg[1]-cputrip )*5) + max(0,(temp_avg[0]-25))*2)); // PS fan
}


void* fan_control_thread(void* para)
{
	frontpanel_rs232 *fp=(frontpanel_rs232*)para;
	double temp[2]={0};
	int n,fan[2];

	temp_avg[0]=30;
	temp_avg[1]=30;
	last_fan=0x7ff;
	
	while(1) {
		fan_min[0]=get_file_min(MINIMUM_FILE0);
		fan_min[1]=get_file_min(MINIMUM_FILE1);
//		printf("MIN %i %i\n",fan_min[0],fan_min[1]);
		for(n=0;n<2;n++)
			temp[n]=get_temp(n);

		if (mb_type==3)      
			calc_speed(temp, 0,  68,fan);  // ICE NCL3 (sensor is about 10deg less than thermal)
		else if (mb_type==2)      
			calc_speed(temp, 70, 65,fan);  // ICE (sensor is about 10deg less than thermal)
		else if (mb_type==1)
			calc_speed(temp, 45, 65,fan);  // AMD (core temps are higher)
		else
			calc_speed(temp, 45, 55,fan);  // Intel

		if (fan[0]!=0x7ff) {
			int min_val;
			if (fan_min[0]==-1)
				min_val=MIN_PS_FAN;
			else
				min_val=fan_min[0];
			fan[0]=min(fan[0],min_val);
		}

		if (fan_min[1]!=-1)
			fan[1]=min(fan[1],fan_min[1]);
		
		fan_set[0]=fan[0];
		fan_set[1]=fan[1];
		

		if (mb_type==3) {
			int ctrl=0x5a5000|((fan[1]&0xff)<<4);
			if (daemon_debug == 1)
			printf("FAN %x (%x)\n",fan[1],ctrl);
			fp->fp_power_control(ctrl);
			last_fan=fan[1];
		}
		else {
			if (daemon_debug == 1)
			printf("FAN %x %x (%x|%x)\n",fan[0],fan[1],(0x11<<24)|fan[0],(0x12<<24)|fan[1]);
			fp->fp_power_control((0x11<<24)|fan[0]); // Fan 1 / PS
			sleep(CHECK_PERIOD/2);
			fp->fp_power_control((0x12<<24)|fan[1]); // Fan 2/ CPU
			last_fan=fan[1];
		}
		sleep(CHECK_PERIOD/2);
	}
	return NULL;
}
