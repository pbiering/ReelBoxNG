/*****************************************************************************/
/*
 * Channel Interface for Geode&BSP15
 *
 * Copyright (C) 2006 Georg Acher, BayCom GmbH (acher at baycom dot de)
 *
 * #include <GPL-header>
 *
 * $Id: bspchannel.c,v 1.2 2006/03/06 23:12:33 acher Exp $
 */
/*****************************************************************************/

#ifdef __LINUX__
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#else
#include "etilib/etiTypes.h"
#include "etilib/etiLocal.h"
#include "etilib/etiList.h"
#include "etilib/etiOsal.h"
#endif

#include <bspchannel.h>
#include <bspshm_user_structs.h>

#ifndef __LINUX__
#define usleep(x) osalThreadSleep(x*1024)
#define printf __SYSLOG
#else

#endif

#define memcpy_fast(a,b,c) memcpy(a,b,c)

#ifdef __LINUX__
#define BUSY_WAIT 10
#else
#define BUSY_WAIT 0
#endif

#define dbg1(format, arg...) do {} while (0)

static bspshm_area_t *bsa;
static bspd_data_t *bsd;
//---------------------------------------------------------------------------
void dump_bcc(bsp_channel_control_t *bcc)
{
	printf("ID %i, size %x, rp %i wp %i\n",
	       bcc->channel,bcc->bufsize,bcc->read_entry,bcc->write_entry);
}
//---------------------------------------------------------------------------
// Always creates Buffer in local memory!
bsp_channel_t *bsp_channel_create(int channel, int bufsize, int dir)
{
	int id;
	bsp_channel_t bsc,*bscr;
	bsp_channel_control_t *bcc;
	int location;
	int size,n,flags;
	int retry_count=0;
	
	if (channel<0 || channel>MAX_CHANNELS)
		return 0;

	id=CHANNEL_TO_ID(channel);
	bsc.channel=channel;
	bsc.dir=dir;
	bsc.control_area=NULL;
	for(n=0;n<BSP_CHANNEL_MAX_AREAS;n++)
		bsc.data_area[n]=NULL;
	
	location=bspshm_local_memory();

	bsc.control_area=bsp_create_area(id+CH_CONTROL, NULL, CONTROL_SIZE, location);
	if (!bsc.control_area)
		return NULL;

	size=(bufsize+4095)&~4095;
	flags=location|BSPSHM_MEM_CACHEABLE;

	// FIXME multiple areas!
retry:	
	bsc.data_area[0]=bsp_create_area(id+1,NULL,size, flags);

	if (!bsc.data_area[0]) {
		bsp_free_area(bsc.control_area);
		return NULL;
	}
	
	if (bsc.data_area[0]->length!=size || bsc.data_area[0]->flags!=flags) {
		if(retry_count)
			return NULL;
		printf("Resize data area, old size %i, old flags %i\n",bsc.data_area[0]->length,bsc.data_area[0]->flags);
		bsp_destroy_area(bsc.data_area[0]);
		bsp_free_area(bsc.data_area[0]);
		retry_count++;
		goto retry;
	}
	bcc=(bsp_channel_control_t*)bsc.control_area->mapped;
	memset(bcc,0, CONTROL_SIZE);

	bcc->channel=channel;
	bcc->dir=BSP_CHANNEL_WRITE;

	// host centric view
	if ((location==BSPSHM_MEM_MAIN && dir==BSP_CHANNEL_READ) ||
		(location==BSPSHM_MEM_BSP&& dir==BSP_CHANNEL_WRITE)) {
		bcc->dir=BSP_CHANNEL_READ;
	}

	bcc->bufsize=size;
	bcc->used_areas=1;
	bcc->area_sizes[0]=size;
	bcc->usage=0;
	bcc->max_entries=CONTROL_ENTRIES;
	bcc->read_entry=0;
	bcc->write_entry=0;
	bcc->location=location;

	memset(bcc->entries,0,sizeof(bsp_channel_entry_t)*CONTROL_ENTRIES);
	bsc.generation=(bsd?bsd->generation:0);
	bsc.location=location;
	bsc.creator=1;
	bsc.flush_ctrl=0;
	
	bscr=(bsp_channel_t*)malloc(sizeof(bsp_channel_t));
	memcpy(bscr,&bsc,sizeof(bsp_channel_t));

	bcc->state=CH_STATE_RUNNING;
	printf("channel %i phys control %x areas %i\n",channel,(int)bsc.control_area->physical,bcc->used_areas);
	return bscr;
}
//---------------------------------------------------------------------------
// FIXME real lock against multiple opens

bsp_channel_t *bsp_channel_open_sub(bsp_channel_t *bsc,int channel)
{	int id;
	bsp_channel_control_t *bcc;
	int n;

	if (channel<0 || channel>MAX_CHANNELS)
		return 0;       

	id=CHANNEL_TO_ID(channel);
	bsc->channel=channel;

	bsc->control_area=NULL;
	for(n=0;n<BSP_CHANNEL_MAX_AREAS;n++)
		bsc->data_area[n]=NULL;

	bsc->control_area=bsp_get_area(id+CH_CONTROL);

	if (!bsc->control_area)
		return NULL;

	bcc=(bsp_channel_control_t*)bsc->control_area->mapped;

	if (bcc->state!=CH_STATE_RUNNING) {
		bsp_free_area(bsc->control_area);
		return NULL;	
	}

	// FIXME areas
	bsc->data_area[0]=bsp_get_area(id+1);

	if (!bsc->data_area[0]) {
		bsp_free_area(bsc->control_area);
		return NULL;
	}

	// user centric view
//	printf("CH_OPEN: %i\n",channel);
	if (bspshm_local_memory()!=BSPSHM_MEM_BSP)
		bsc->dir=bcc->dir;
	else
		bsc->dir=1-bcc->dir;

	bcc->usage++;
	bsc->generation=(bsd?bsd->generation:0);
	bsc->location=bcc->location;
	bsc->creator=0;
	bsc->flush_ctrl=0;
	printf("CH_OPEN: channel %i phys control %x used areas %i\n",channel,(int)bsc->control_area->physical,bcc->used_areas);
	return bsc;
}
//---------------------------------------------------------------------------
bsp_channel_t *bsp_channel_open(int channel)
{
	bsp_channel_t bsc,*bscr;

	bscr=bsp_channel_open_sub(&bsc,channel);
	if (bscr) {
		bscr=(bsp_channel_t*)malloc(sizeof(bsp_channel_t));
		memcpy(bscr,&bsc,sizeof(bsp_channel_t));
		return bscr;
	}
	return NULL;
}
//---------------------------------------------------------------------------
void bsp_channel_free_areas(bsp_channel_t *handle)
{
	int n,areas;
	bsp_channel_control_t *bcc;
	bcc=(bsp_channel_control_t*)handle->control_area;
	areas=bcc->used_areas;
	if (handle->creator)
		bcc->state=0;
	printf("bsp_channel_free_areas: used areas %i\n",bcc->used_areas); // FIXME BUG!!!
	bsp_free_area(handle->control_area);
	for(n=0;n<1;n++) 
		bsp_free_area(handle->data_area[n]);
}
//---------------------------------------------------------------------------
int bsp_channel_close(bsp_channel_t *handle)
{
	int channel;
	if (!handle)
		return 0;
	channel=handle->channel;
	dbg1("CH_CLOSE %i\n",channel);

	bsp_channel_free_areas(handle);

	free(handle);
	printf("CH_CLOSE %i done\n",channel);
	return 0;
}
//---------------------------------------------------------------------------
// Checks for changed BSP15-created channels and re-opens them

int bsp_channel_check(bsp_channel_t *bsc, int timeout)
{
	bsp_channel_t *bscr;
	if (!bsd || bsc->location==BSPSHM_MEM_MAIN || bsc->generation==bsd->generation)
		return 0;

	bsp_channel_free_areas(bsc);

	while(1) {
		bscr=bsp_channel_open_sub(bsc,bsc->channel);
		if (bscr)
			return 0;
		timeout--;
		if (timeout<=0) {
			printf("Re-open of channel %i failed\n",bsc->channel);
			return 1;
		}
		usleep(1000);
	}
	return 0;
}
//---------------------------------------------------------------------------
int bsp_channel_read_start(bsp_channel_t *bsc, void **buf, int *size, int timeout)
{
	int rp,wp;
	bsp_channel_control_t *bcc;
	bsp_channel_entry_t *bce;
	bspshm_area_t *bsa;
	int first_run=0;
	
	if (!bsc || bsc->dir!=BSP_CHANNEL_READ) 
		return 0;

#ifdef __LINUX__
	if (bsp_channel_check(bsc,timeout))
		return 0;
#else
	timeout=timeout*10;
#endif
	// FIXME Direction check, otherwise race...
	
	bcc=(bsp_channel_control_t*)bsc->control_area->mapped;

	if (bcc->invalidate) {
		printf("READ: INVALIDATE CHANNEL %i\n",bsc->channel);
		bcc->invalidate=0;
		bcc->read_entry=bcc->write_entry;
		return 0;
	}
	if (bcc->used_areas!=1) {
		printf("uuur %i used areas %i\n",bcc->channel,bcc->used_areas);
		exit(0);
	}

#if 0
	while(!bcc->usage) {
		if (timeout<0)
			return 0;
		timeout--;
		usleep(1000);
	}
#endif
	while(1) {		
		rp=bcc->read_entry%bcc->max_entries;
		wp=bcc->write_entry%bcc->max_entries;
//		printf("rp %i wp %i \n",rp,wp);
		if (rp==wp) {
			if (timeout<=0) {
				bsc->read_size=0; // protect against finish()
				return 0;
			}
			timeout--;
			first_run++;
			if (first_run>BUSY_WAIT) {
#ifdef __LINUX__
				usleep(1000);
#else
				usleep(100);
#endif
			}
		}
		else 
			break;
	}

	bce=bcc->entries+rp;

	// Something is really screwed up...
	if (bce->area_num > bcc->used_areas) {
		// Fixme: Real repair action?
		printf("FIX FIFO GARBAGE INVALIDATE\n");
		bcc->invalidate=1;
		return 0;
	}

//	printf("read bce %x size %i offset %i, rp %i\n",bce,bce->size,bce->area_offset,rp);
	bsa=bsc->data_area[bce->area_num];

	if (buf) 
		*buf=bsa->mapped+bce->area_offset;
	if (size)
		*size=bce->size;

#ifndef __LINUX__
	if (bsa->flags&BSPSHM_MEM_CACHEABLE) {
		if (bsc->flush_ctrl==0)
			dcacheFlushRange(bsa->mapped+bce->area_offset,bce->size+64);
		else if (bsc->flush_ctrl>0)
			dcacheFlushRange(bsa->mapped+bce->area_offset,etiMIN(bsc->flush_ctrl,bce->size+64));
	}
#endif
	
	// Remember for potential internal usage...
	bsc->read_data=bsa->mapped+bce->area_offset;
	bsc->read_size=bce->size;

	return bsc->read_size;	
}
//---------------------------------------------------------------------------
// free current entry
int bsp_channel_read_finish(bsp_channel_t *bsc)
{
	bsp_channel_control_t *bcc;

	if (!bsc || bsc->dir!=BSP_CHANNEL_READ)
		return 0;

	bcc=(bsp_channel_control_t*)bsc->control_area->mapped;

	if (bsc->read_size)
		bcc->read_entry=(bcc->read_entry+1)%bcc->max_entries;
	
	bsc->read_size=0;
	return 0;
}
//---------------------------------------------------------------------------
// FIXME Extend to more areas

bsp_channel_entry_t *bsp_channel_find_space(bsp_channel_control_t *bcc, int size)
{
	int rp,wp,wpm;
	bsp_channel_entry_t *bcer,*bcew,*bcex;
	int area_size0;

	rp=bcc->read_entry%bcc->max_entries;
	wp=bcc->write_entry%bcc->max_entries;
	wpm=(wp-1)%bcc->max_entries;
	if (wpm<0)
		wpm+=bcc->max_entries;
	area_size0=bcc->area_sizes[0];

	bcer=bcc->entries+rp;
	bcew=bcc->entries+wpm;
	bcex=bcc->entries+wp;
//	printf("f %i %i %i\n",rp,wp,wpm);
	if (rp==wp) {
		bcex->area_offset=0;
		bcex->size=0;
		
		if (size > area_size0)
			return NULL;
		return bcex;
	}

	if (bcer->area_offset <= bcew->area_offset) {

		if (bcew->area_offset+bcew->rsize+size < area_size0) {
			bcex->area_offset=bcew->area_offset+bcew->rsize;
			return bcex;
		}
		if (size < bcer->area_offset) {
			bcex->area_offset=0;
			return bcex;
		}

		return NULL;
	}
	else {
		if (bcew->area_offset+bcew->rsize+size < bcer->area_offset) {
			bcex->area_offset=bcew->area_offset+bcew->rsize;
			return bcex;
		}
		return NULL;
	}

	return NULL;	
}
//---------------------------------------------------------------------------
int bsp_channel_write_start(bsp_channel_t *bsc, void **buf, int size, int timeout)
{
	bsp_channel_control_t *bcc;
	bsp_channel_entry_t *bce;
	int first_run=0;
	int rsize;
	
	if (!bsc || bsc->dir!=BSP_CHANNEL_WRITE || size==0)
		return 0;
#if 0
#ifdef __LINUX__
	if (bsp_channel_check(bsc,timeout))
		return 0;
#endif
#endif

	bcc=(bsp_channel_control_t*)bsc->control_area->mapped;
	if (bcc->used_areas!=1) {
		printf("uuur used areas %i\n",bcc->used_areas);
		exit(0);
	}
#if 0
	while(!bcc->usage) {
		if (timeout<0)
			return 0;
		timeout--;
		usleep(1000);
	}
#endif
	rsize=(size+63)&~63;
	while(1) {
		bce=bsp_channel_find_space(bcc,rsize);
		if (!bce) {
			if (timeout<=0) {
				//dump_bcc(bcc);
				//printf("Channel %i: Write Timeout\n",bsc->channel);
				bsc->write_entry_set=NULL;
				return 0;
			}
			timeout--;
			first_run++;
			if (first_run>BUSY_WAIT)
				usleep(1000);
		}
		else
			break;
	}
	bce->size=0;
	bce->area_num=0;
	bce->rsize=rsize;
	bsc->write_entry_set=bce;
	
	if (buf)
		*buf=bsc->data_area[bce->area_num]->mapped+bce->area_offset;

	return size;
}
//---------------------------------------------------------------------------
int bsp_channel_write_finish(bsp_channel_t *bsc, int size)
{
	bsp_channel_control_t *bcc;
	bsp_channel_entry_t *bce;

	if (!bsc || bsc->dir!=BSP_CHANNEL_WRITE || !bsc->write_entry_set)
		return 0;

	bcc=(bsp_channel_control_t*)bsc->control_area->mapped;
	bce=(bsp_channel_entry_t *)bsc->write_entry_set;

	if (size>bce->rsize) // Idiot, at least data gets corrupted with next write...
		bce->size=bce->rsize;
	else
		bce->size=size;

#ifndef __LINUX__
	if (bsc->data_area[bce->area_num]->flags&BSPSHM_MEM_CACHEABLE) {
		// FIXME flush ctrl
		dcacheFlushRange(bsc->data_area[bce->area_num]->mapped+bce->area_offset,bce->size);
	}
#endif

//	printf("bsp_channel_write_finish done %x %i %i %i\n",bce,bce->area_offset,bcc->read_entry,bcc->write_entry);
	bcc->write_entry=(bcc->write_entry+1)%bcc->max_entries;

	return bce->size;
}
//---------------------------------------------------------------------------
// 0: flush succeeded
int bsp_channel_invalidate(bsp_channel_t *bsc, int timeout)
{
	bsp_channel_control_t *bcc;

	if (!bsc)
		return 0;

	bcc=(bsp_channel_control_t*)bsc->control_area->mapped;

	
	bcc->invalidate=1;

	while(bcc->invalidate) {
		if (timeout<0)
			return 1;
		timeout--;
		usleep(1000);
	}

	return bcc->invalidate;
}
//---------------------------------------------------------------------------
// Regular read/write
//---------------------------------------------------------------------------
int bsp_channel_write(bsp_channel_t *bsc, void *buf, int size, int timeout)
{
	void* ret_buf;
	int ret_size;

	ret_size=bsp_channel_write_start(bsc,&ret_buf,size, timeout);
	if (!ret_size)
		return 0;

	memcpy_fast(ret_buf,buf,size);

	return bsp_channel_write_finish(bsc,size);
}
//---------------------------------------------------------------------------
int bsp_channel_read(bsp_channel_t *bsc, void *buf, int size, int timeout)
{
	int ret;
	int ret_size;
	void* ret_buf;

	ret=bsp_channel_read_start(bsc, &ret_buf,&ret_size,timeout);
	if (ret) {
		memcpy_fast(buf,ret_buf,(ret_size>size?size:ret_size));
		bsp_channel_read_finish(bsc);
	}
	return (ret_size>size?size:ret_size);
}
//---------------------------------------------------------------------------
// Checks if data is available (0) or not (1)
int bsp_channel_empty(bsp_channel_t *bsc)
{
	bsp_channel_control_t *bcc;
	int rp,wp;

	if (!bsc)
		return 0;

	bcc=(bsp_channel_control_t*)bsc->control_area->mapped;

	rp=bcc->read_entry%bcc->max_entries;
	wp=bcc->write_entry%bcc->max_entries;

	if (rp!=wp)
		return 0;
	else
		return 1;
}
//---------------------------------------------------------------------------
// Disables (partially) D-cache flush on BSP15
// val: 0: always flush, -1: never flush, >0 flush n Bytes at beginning of packet

void bsp_channel_flush_ctrl(bsp_channel_t *bsc, int val)
{
	if (!bsc)
		return;
	bsc->flush_ctrl=val;
}
//---------------------------------------------------------------------------
// Should be called only be bspshm_reset()
void bsp_channel_cleanup(void)
{
	// Reserved for future catastrophes
}
//---------------------------------------------------------------------------
bspd_data_t* bsp_channel_init(void)
{
	if (!bsa) {
		printf("bsp_channel_init\n");
		bsa=bsp_get_area(BSPID_BSPD);
		if (!bsa) {
			printf("bspd not running\n");
//		exit(0);
			return NULL;
		}
		
		bsd=(bspd_data_t*)bsa->mapped;
		{
			char bx[4096];
			memcpy(&bx,bsd,4096);
			memcpy(bsd,&bx,4096);
		}
	}		

	return bsd;
}
//---------------------------------------------------------------------------
#if 0
int main(int argc, char ** argv)
{
	bsp_channel_t *bsc;
	char buf[131072];
	int ret,n=0;
	int x=0,lx=0;

	bsp_init(0);
	bsp_channel_init();
	if (argc==1) {
//		bsc=bsp_channel_create(10, 131072, BSP_CHANNEL_WRITE);
		bsc=bsp_channel_open(10);
		while(1) {
			buf[0]=n++;
			bsp_channel_write(bsc, buf, 8*1024, 1000*100);
//			sleep(1);
		}
	}
	else {
//		bsc=bsp_channel_open(10);
		bsc=bsp_channel_create(10, 131072, BSP_CHANNEL_READ);

	        for(n=0;n<1024*16;){
			ret=bsp_channel_get(bsc, 1000*10);	
//			ret=bsp_channel_read(bsc,buf,16374,1000*10);
			if (ret) {
				x=bsc->read_data[0]&255;
//				printf("xxxx %x %i  %i %i\n",bsc->read_data,bsc->read_size,x,lx);
				if (((lx+1)&255)!=x && lx!=0) {
					printf("ERROR\n");
					exit(0);
				}
				lx=x;
				bsp_channel_free(bsc);
				n++;
				if ((n&1023)==0)
					printf("%i\n",n);				
			}
			
		}
	}
		

}
#endif
