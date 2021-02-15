/*****************************************************************************/
/*
 * Channel Interface
 *
 * Copyright (C) 2006,2007 Georg Acher, BayCom GmbH (acher at baycom dot de)
 *
 * #include <GPLv2-header>
 *
 * $Id: hdchannel.c,v 1.2 2006/03/06 23:12:33 acher Exp $
 */
/*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef CONFIG_MIPS
int cacheflush(char *addr, int nbytes, int cache);
#include <asm/cachectl.h>
#endif

#include <hdchannel.h>
//#include <hdshm_user_structs.h>


#define memcpy_fast(a,b,c) memcpy(a,b,c)

#define BUSY_WAIT 1


#define dbg1(format, arg...) do {} while (0)

//static hdshm_area_t *bsa;
//static hdd_data_t *bsd;
/*------------------------------------------------------------------------*/
static void hexdump(volatile unsigned char *b, int l)
{
        int n;
        for(n=0;n<l;n++)
        {
                printf("%02x ",*(b+n));
                if ((n%32)==31)
                        puts("");
        }
        puts("");
}
//---------------------------------------------------------------------------
void dump_bcc(hd_channel_control_t *bcc)
{
	printf("ID %i, size %x, rp %i wp %i\n",
	       bcc->channel,bcc->bufsize,bcc->read_entry,bcc->write_entry);
	hexdump((volatile unsigned char *)bcc,sizeof(hd_channel_control_t));
}
//---------------------------------------------------------------------------
// Always creates Buffer in local memory!
hd_channel_t *hd_channel_create(int channel, int bufsize, int dir)
{
	int id;
	hd_channel_t bsc,*bscr = NULL;
	hd_channel_control_t *bcc = NULL;
	int location;
	int size,n,flags;
	int retry_count=0;
	
	if (channel<0 || channel>MAX_CHANNELS)
		return 0;

	id=HD_CHANNEL_TO_ID(channel);
	bsc.channel=channel;
	bsc.dir=dir;
	bsc.control_area=NULL;
	for(n=0;n<HD_CHANNEL_MAX_AREAS;n++)
		bsc.data_area[n]=NULL;
	
	// Force memory to be on Decypher
	location=HDSHM_MEM_HD;

	bsc.control_area=hd_create_area(id+CH_CONTROL, NULL, HD_CONTROL_SIZE, location);
//	printf("bsc.control %p\n",	bsc.control_area);
	if (!bsc.control_area)
		return NULL;

	size=(bufsize+4095)&~4095;
	flags=location|HDSHM_MEM_CACHEABLE;

	// FIXME multiple areas!
retry:	
	bsc.data_area[0]=hd_create_area(id+1,NULL,size, flags);

	if (!bsc.data_area[0]) {
		hd_free_area(bsc.control_area);
		return NULL;
	}
	
	if (bsc.data_area[0]->length!=size || bsc.data_area[0]->flags!=flags) {
		if(retry_count)
			return NULL;
		printf("Resize data area, old size %i, old flags %i\n",bsc.data_area[0]->length,bsc.data_area[0]->flags);
		hd_destroy_area(bsc.data_area[0]);
		hd_free_area(bsc.data_area[0]);
		retry_count++;
		goto retry;
	}
	bcc=(hd_channel_control_t*)bsc.control_area->mapped;
	memset(bcc,0, HD_CONTROL_SIZE);

	bcc->channel=channel;
	bcc->dir=HD_CHANNEL_WRITE;

	// transform host centric view to local view
#ifdef CONFIG_MIPS
	if ( dir==HD_CHANNEL_WRITE) {
		bcc->dir=HD_CHANNEL_READ;
	}
#else
	if ( dir==HD_CHANNEL_READ) {
		bcc->dir=HD_CHANNEL_READ;
	}
#endif
	bcc->bufsize=size;
	bcc->used_areas=1;
	bcc->area_sizes[0]=size;
	bcc->usage=0;

		
	bcc->max_entries=CONTROL_ENTRIES;

	bcc->read_entry=0;
	bcc->write_entry=0;
	bcc->location=location;

	memset(bcc->entries,0,sizeof(hd_channel_entry_t)*CONTROL_ENTRIES);
//	bsc.generation=(bsd?bsd->generation:0);
	bsc.location=location;
	bsc.creator=1;
	bsc.flush_ctrl=0;
	
	bscr=(hd_channel_t*)malloc(sizeof(hd_channel_t));
	memcpy(bscr,&bsc,sizeof(hd_channel_t));

	bcc->state=CH_STATE_RUNNING;
	printf("channel %i phys control %x areas %i\n",channel,(int)bsc.control_area->physical,bcc->used_areas);
	return bscr;
}
//---------------------------------------------------------------------------
// FIXME real lock against multiple opens

hd_channel_t *hd_channel_open_sub(hd_channel_t *bsc,int channel)
{	int id;
	hd_channel_control_t *bcc = NULL;
	int n;

	if (channel<0 || channel>MAX_CHANNELS)
		return 0;       

	id=HD_CHANNEL_TO_ID(channel);
	bsc->channel=channel;

	bsc->control_area=NULL;
	for(n=0;n<HD_CHANNEL_MAX_AREAS;n++)
		bsc->data_area[n]=NULL;

	bsc->control_area=hd_get_area(id+CH_CONTROL);

	if (!bsc->control_area)
		return NULL;

	bcc=(hd_channel_control_t*)bsc->control_area->mapped;

	if (bcc->state!=CH_STATE_RUNNING) {
		hd_free_area(bsc->control_area);
		return NULL;	
	}

	// FIXME areas
	bsc->data_area[0]=hd_get_area(id+1);

	if (!bsc->data_area[0]) {
		hd_free_area(bsc->control_area);
		return NULL;
	}

	// user centric view
	printf("CH_OPEN: %i\n",channel);

#ifdef CONFIG_MIPS
		bsc->dir=1-bcc->dir;
#else
		bsc->dir=bcc->dir;
#endif

	bcc->usage++;
//	bsc->generation=(bsd?bsd->generation:0);
	bsc->location=bcc->location;
	bsc->creator=0;
	bsc->flush_ctrl=0;
	printf("CH_OPEN: channel %i phys control %x used areas %i\n",channel,(int)bsc->control_area->physical,bcc->used_areas);
	return bsc;
}
//---------------------------------------------------------------------------
hd_channel_t *hd_channel_open(int channel)
{
	hd_channel_t bsc,*bscr = NULL;

	bscr=hd_channel_open_sub(&bsc,channel);
	if (bscr) {
		bscr=(hd_channel_t*)malloc(sizeof(hd_channel_t));
		memcpy(bscr,&bsc,sizeof(hd_channel_t));
		return bscr;
	}
	return NULL;
}
//---------------------------------------------------------------------------
void hd_channel_free_areas(hd_channel_t *handle)
{
	int n,areas;
	hd_channel_control_t *bcc = NULL;
	bcc=(hd_channel_control_t*)handle->control_area;
	areas=bcc->used_areas;
	if (handle->creator)
		bcc->state=0;
	printf("hd_channel_free_areas: used areas %i\n",areas);
	hd_free_area(handle->control_area);
	for(n=0;n<1;n++) 
		hd_free_area(handle->data_area[n]);
}
//---------------------------------------------------------------------------
int hd_channel_close(hd_channel_t *handle)
{
	int channel;
	if (!handle)
		return 0;
	channel=handle->channel;
	dbg1("CH_CLOSE %i\n",channel);

	hd_channel_free_areas(handle);

	free(handle);
	printf("CH_CLOSE %i done\n",channel);
	return 0;
}
//---------------------------------------------------------------------------
// Checks for changed HD-created channels and re-opens them

int hd_channel_check(hd_channel_t *bsc, int timeout)
{
	hd_channel_t *bscr = NULL;

//	if (!bsd || bsc->location==HDSHM_MEM_MAIN || bsc->generation==bsd->generation)
	if ( bsc->location==HDSHM_MEM_MAIN )
		return 0;

	hd_channel_free_areas(bsc);

	while(1) {
		bscr=hd_channel_open_sub(bsc,bsc->channel);
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
// Also returns kernel address of data in kmen

int hd_channel_read_start_kmem(hd_channel_t *bsc, void **buf, void** kmem, int *size, int timeout)
{
	int rp,wp;
	hd_channel_control_t *bcc = NULL;
	hd_channel_entry_t *bce = NULL;
	hdshm_area_t *bsa = NULL;
	int first_run=0;
	
	if (!bsc || bsc->dir!=HD_CHANNEL_READ) 
		return 0;
#if 0
#ifdef __LINUX__
	if (hd_channel_check(bsc,timeout))
		return 0;
#else
	timeout=timeout*10;
#endif
#endif
	// FIXME Direction check, otherwise race...
	
	bcc=(hd_channel_control_t*)bsc->control_area->mapped;

	if (bcc->invalidate) {
		int bwe=bcc->write_entry;
		printf("READ: INVALIDATE CHANNEL %i\n",bsc->channel);
		bcc->invalidate=0;
		bcc->read_entry=bwe;
		return 0;
	}
	if (bcc->used_areas!=1) {
		printf("Read sanity CH%i: Used areas %i.\n",bsc->channel,bcc->used_areas);
		dump_bcc(bcc);
		return 0;
//		exit(0);
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

		if (rp==wp) {
			if (timeout<=0) {
				bsc->read_size=0; // protect against finish()
				return 0;
			}
			timeout--;
			first_run++;
			usleep(1000);
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
	if (kmem)
		*kmem=bsa->kernel_mem+bce->area_offset;
	if (size)
		*size=bce->size;

#ifdef CONFIG_MIPS
	if (bsc->data_area[bce->area_num]->flags&HDSHM_MEM_CACHEABLE) 
		cacheflush(bsc->data_area[bce->area_num]->mapped+bce->area_offset, bce->size,DCACHE);
#endif


	// Remember for potential internal usage...
	bsc->read_data=bsa->mapped+bce->area_offset;
	bsc->read_size=bce->size;

	return bsc->read_size;	
}
//---------------------------------------------------------------------------
int hd_channel_read_start(hd_channel_t *bsc, void **buf, int *size, int timeout)
{
	return hd_channel_read_start_kmem(bsc, buf, NULL, size, timeout);
}
//---------------------------------------------------------------------------
// free current entry
int hd_channel_read_finish(hd_channel_t *bsc)
{
	hd_channel_control_t *bcc = NULL;
	int max_entries;

	if (!bsc || bsc->dir!=HD_CHANNEL_READ)
		return 0;

	bcc=(hd_channel_control_t*)bsc->control_area->mapped;

	if (bsc->read_size) {
		bsc->read_size=0;
		max_entries=bcc->max_entries;
		if (max_entries)
			bcc->read_entry=(bcc->read_entry+1)%max_entries;
		else {
			printf("hd_channel_read_finish sanity Check: max_entries==0\n");
		}
	}

	return 0;
}
//---------------------------------------------------------------------------
// FIXME Extend to more areas

hd_channel_entry_t *hd_channel_find_space(hd_channel_control_t *bcc, int size)
{
	int rp,wp,wpm;
	hd_channel_entry_t *bcer = NULL, *bcew = NULL, *bcex = NULL;
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
int hd_channel_write_start(hd_channel_t *bsc, void **buf, int size, int timeout)
{
	hd_channel_control_t *bcc = NULL;
	hd_channel_entry_t *bce = NULL;
	int first_run=0;
	int rsize;

	if (!bsc || bsc->dir!=HD_CHANNEL_WRITE || size==0)
		return 0;

#if 0
#ifdef __LINUX__
	if (hd_channel_check(bsc,timeout))
		return 0;
#endif
#endif

	bcc=(hd_channel_control_t*)bsc->control_area->mapped;
	if (bcc->used_areas!=1) {
		printf("Write sanity CH%i: Used areas %i.\n", bsc->channel, bcc->used_areas);
		dump_bcc(bcc);
		return 0;
//		exit(0);
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
		bce=hd_channel_find_space(bcc,rsize);
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
	
//	printf("Offset %x\n",bce->area_offset);
	if (buf)
		*buf=bsc->data_area[bce->area_num]->mapped+bce->area_offset;

	return size;
}
//---------------------------------------------------------------------------
int hd_channel_write_finish(hd_channel_t *bsc, int size)
{
	hd_channel_control_t *bcc = NULL;
	hd_channel_entry_t *bce = NULL;
	int max_entries;

	if (!bsc || bsc->dir!=HD_CHANNEL_WRITE || !bsc->write_entry_set)
		return 0;

	bcc=(hd_channel_control_t*)bsc->control_area->mapped;
	bce=(hd_channel_entry_t *)bsc->write_entry_set;

	if (size>bce->rsize) // Idiot, at least data gets corrupted with next write...
		bce->size=bce->rsize;
	else
		bce->size=size;
#ifdef CONFIG_MIPS
	if (bsc->data_area[bce->area_num]->flags&HDSHM_MEM_CACHEABLE)
		cacheflush(bsc->data_area[bce->area_num]->mapped+bce->area_offset, bce->size,DCACHE);
#endif

//	printf("hd_channel_write_finish done %x %i %i %i\n",bce,bce->area_offset,bcc->read_entry,bcc->write_entry);
	max_entries=bcc->max_entries;
	if (max_entries)
		bcc->write_entry=(bcc->write_entry+1)%max_entries;
	else {
		printf("hd_channel_write_finish sanity Check: max_entries==0\n");
	}
	return bce->size;
}
//---------------------------------------------------------------------------
// 0: flush succeeded
int hd_channel_invalidate(hd_channel_t *bsc, int timeout)
{
	hd_channel_control_t *bcc = NULL;

	if (!bsc)
		return 0;

	bcc=(hd_channel_control_t*)bsc->control_area->mapped;

	
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
int hd_channel_write(hd_channel_t *bsc, void *buf, int size, int timeout)
{
	void* ret_buf = NULL;
	int ret_size;

	ret_size=hd_channel_write_start(bsc,&ret_buf,size, timeout);
	if (!ret_size)
		return 0;

	memcpy_fast(ret_buf,buf,(size+3)&~3);
//	memcpy_fast(ret_buf,buf,size);

	return hd_channel_write_finish(bsc,size);
}
//---------------------------------------------------------------------------
int hd_channel_read(hd_channel_t *bsc, void *buf, int size, int timeout)
{
	int ret;
	int ret_size;
	void* ret_buf = NULL;

	ret=hd_channel_read_start(bsc, &ret_buf,&ret_size,timeout);
	if (ret) {
		memcpy_fast(buf,ret_buf,(ret_size>size?size:ret_size));
		hd_channel_read_finish(bsc);
		return (ret_size>size?size:ret_size);
	}
	return 0;
}
//---------------------------------------------------------------------------
// Checks if data is available (0) or not (1)
int hd_channel_empty(hd_channel_t *bsc)
{
	hd_channel_control_t *bcc = NULL;
	int rp,wp;
	
	if (!bsc)
		return 0;

	bcc=(hd_channel_control_t*)bsc->control_area->mapped;

	rp=bcc->read_entry%bcc->max_entries;
	wp=bcc->write_entry%bcc->max_entries;

	if (rp!=wp)
		return 0;
	else
		return 1;
}
//---------------------------------------------------------------------------
// Disables (partially) D-cache flush on HD15
// val: 0: always flush, -1: never flush, >0 flush n Bytes at beginning of packet

void hd_channel_flush_ctrl(hd_channel_t *bsc, int val)
{
	if (!bsc)
		return;
	bsc->flush_ctrl=val;
}
//---------------------------------------------------------------------------
// Should be called only be hdshm_reset()
void hd_channel_cleanup(void)
{
	// Reserved for future catastrophes
}

//---------------------------------------------------------------------------
#if 0
hdd_data_t* hd_channel_init(void)
{

	if (!bsa) {
		printf("hd_channel_init\n");
		bsa=hd_get_area(HDID_HDD);
		if (!bsa) {
			printf("hdd not running\n");
//		exit(0);
			return NULL;
		}
		
		bsd=(hdd_data_t*)bsa->mapped;
		{
			char bx[4096];
			memcpy(&bx,bsd,4096);
			memcpy(bsd,&bx,4096);
		}
	}		

	return bsd;
}
#endif
//---------------------------------------------------------------------------
#if 0
int main(int argc, char ** argv)
{
	hd_channel_t *bsc;
	char buf[131072];
	int ret,n=0;
	int x=0,lx=0;

	hd_init(0);
	hd_channel_init();
	if (argc==1) {
//		bsc=hd_channel_create(10, 131072, HD_CHANNEL_WRITE);
		bsc=hd_channel_open(10);
		while(1) {
			buf[0]=n++;
			hd_channel_write(bsc, buf, 8*1024, 1000*100);
//			sleep(1);
		}
	}
	else {
//		bsc=hd_channel_open(10);
		bsc=hd_channel_create(10, 131072, HD_CHANNEL_READ);

	        for(n=0;n<1024*16;){
			ret=hd_channel_get(bsc, 1000*10);	
//			ret=hd_channel_read(bsc,buf,16374,1000*10);
			if (ret) {
				x=bsc->read_data[0]&255;
//				printf("xxxx %x %i  %i %i\n",bsc->read_data,bsc->read_size,x,lx);
				if (((lx+1)&255)!=x && lx!=0) {
					printf("ERROR\n");
					exit(0);
				}
				lx=x;
				hd_channel_free(bsc);
				n++;
				if ((n&1023)==0)
					printf("%i\n",n);				
			}
			
		}
	}
		

}
#endif
