/* Simple TS player for DeCypher */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <hdchannel.h>
#include <hdshm_user_structs.h>

void stream(int file, int vpid, int apid, int tmode)
{
	hd_channel_t *tsch;
	hd_packet_ts_data_t header;
	int len,len1,sum;
	void* buffer;
	int n=0;
	int buffer_size;
	hdshm_area_t *area;
	hd_data_t volatile *hda;
	char *xbuf;
	hd_packet_trickmode_t trick;

	hd_init(0);
	
	area=hd_get_area(HDID_HDA);
	if (!area || !area->mapped) {
		printf("Player area not found\n");
		exit(-1);
	}

	tsch=hd_channel_open(HDCH_STREAM1);

	if (!tsch) {
		printf("TS-channel not found\n");
		exit(-1);
	}
	
	hda=(hd_data_t*)area->mapped;
	if (!tmode)
		len=100*188;
	else
		len=100*192;

	hd_channel_invalidate(tsch,100);

	hda->player[0].data_generation++;
	sum=0;
	xbuf=malloc(len);

	trick.header.magic = HD_PACKET_MAGIC;
	trick.header.type=HD_PACKET_TRICKMODE;
	trick.trickspeed=0;
	trick.i_frames_only=0;

	buffer_size = hd_channel_write_start(tsch, &buffer,
							    sizeof(trick), 0);
	memcpy(buffer,&trick,sizeof(trick));
	hd_channel_write_finish(tsch, sizeof(trick));

	while(1) {
		header.header.magic = HD_PACKET_MAGIC;
		header.header.seq_nr = n++;
		header.header.type = HD_PACKET_TS_DATA;
//		header.header.packet_size = sizeof(header) + len;
		header.generation=hda->player[0].data_generation;
		header.vpid=vpid;
		header.apid=apid;

		buffer_size=0;
		
		do {
			buffer_size = hd_channel_write_start(tsch, &buffer,
							    sizeof(header)+len, 0);
			if (buffer_size) 
				break;
			usleep(10000);
		} while(1);


		if (!tmode) {
			len1=read(file,buffer+sizeof(header),len);
			sum+=len1;
		}
		else {
			// Repack 192byte TS
			char xbuf[100*192];
			void* b=buffer+sizeof(header);
			int tb=4;
			int nl=0;

			len1=read(file,xbuf,len);

			while(len1>0) {
				memmove(b,xbuf+tb, 188);
				b+=188;
				tb+=192;
				len1-=192;
				nl+=188;
			}
			len1=nl;
		}

		printf("Write %i %i\n",len1,sum);
		header.header.packet_size = sizeof(header) + len1;
		
		memcpy(buffer, &header,sizeof(header));

		hd_channel_write_finish(tsch, sizeof(header)+len1);
		if (len1==0) {
			// loop if not pipe
			if (lseek(file,0,SEEK_SET)!=0)
				break;
		}
	}
}


int main(int argc, char **argv)
{
	int file;
	int apid=0,vpid=0;
	int tmode=0;

	if (argc!=4 && argc!=5) {
		fprintf(stderr,"Usage: hdtsplay <file> VPID(hex) APID(hex) <Mode>\n");
		exit(-1);
	}

	vpid=strtol(argv[2],NULL,16);
	apid=strtol(argv[3],NULL,16);
	if (argc==5)
		tmode=atoi(argv[4]);
	
	printf("VPID: 0x%x, APID: 0x%x, tmode %i\n",vpid,apid, tmode);
	file=open(argv[1],O_RDONLY);
	if (file<0) {
		perror(argv[1]);
		exit(-1);
	}
	stream(file,vpid,apid,tmode);
	exit(0);
}

