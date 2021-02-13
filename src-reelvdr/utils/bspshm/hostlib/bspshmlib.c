/*****************************************************************************/
/*
 * Shared memory lib for host (Geode) system (User Space)
 *
 * Copyright (C) 2006 Georg Acher, BayCom GmbH (acher at baycom dot de)
 *
 * #include <GPL-header>
 *
 * $Id: bspshmlib.c,v 1.2 2006/03/06 23:12:21 acher Exp $
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

#define _BSPSHM_IOCTLS
#include "../driver/bspshm.h"

#define BSPD_ID 0xFF000001
#define SHMBSPDEV "/dev/bspshm"

static int bspfd=-1;
#define dbg1(format, arg...) do {} while (0)
#define dbg(format, arg...) do {} while (0) //printk(format, ## arg)

//extern void bsp_channel_cleanup(void);

//---------------------------------------------------------------------------
bspshm_area_t* bsp_get_area(int id)
{
	int ret;
	char *p;
	char *mapped;
//	int x;
	int real_length;
//	int fd;

	bspshm_area_t bah,*br;

	dbg1("HOST: get_area ID %x\n",id);
	bah.id=id;
	ret=ioctl(bspfd,IOCTL_BSPSHM_GET_AREA,&bah);

	if (ret!=0)
		return NULL;

	ioctl(bspfd,IOCTL_BSPSHM_SET_ID, id);

	real_length=((bah.physical&4095)+bah.length+4095)&~4095;
#if 0
	fd=open("/dev/mem",O_RDWR);
	x=0xf0000000+(bah.physical&~4095);
	printf("XX %x\n",x);
	p=mmap(NULL, real_length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, x);
	close(fd);
#else

	p=(char*)mmap(NULL, real_length, PROT_READ|PROT_WRITE, MAP_SHARED, bspfd, 0 );
	mapped=p+(bah.physical&4095);
#endif

	if (p==(void*)-1)
		return NULL;

	dbg("HOST: Mapped ID %x, phys %x to %x, rphys %x to virt %x, length %x\n",
	       id,
	       (int)bah.physical,(int)p,
	       (int)bah.physical&~4095,(int)mapped,real_length);
	br=(bspshm_area_t*)calloc(1,sizeof(bspshm_area_t));
	br->real_mapped=p;
	br->mapped=mapped;
	br->length=bah.length;
	br->real_length=real_length;
	br->flags=bah.flags;
	br->physical=bah.physical;
	br->id=id;
	dbg("handle %x\n",br);
	return br;
}
//---------------------------------------------------------------------------
void bsp_unmap_area(bspshm_area_t *handle)
{
	// int ret; // FIXED: warning: variable 'ret' set but not used
	if (!handle || !handle->mapped)
		return;
	dbg("unmap  %p, length %x\n",handle->real_mapped,handle->real_length);
	munmap(handle->real_mapped,handle->real_length);
	dbg1("ioctl unmap\n");
	/*ret=*/ioctl(bspfd,IOCTL_BSPSHM_UNMAP_AREA,handle->id); // FIXED: warning: variable 'ret' set but not used
	handle->mapped=NULL;
}
//---------------------------------------------------------------------------
void bsp_free_area(bspshm_area_t *handle)
{
	if (!handle)
		return;
	bsp_unmap_area(handle);
	free(handle);
}
//---------------------------------------------------------------------------
/*
  return 0: Area OK
  1: Area changed
*/
int bsp_check_area(bspshm_area_t *bah)
{
	// int ret; // FIXED: warning: variable 'ret' set but not used
	bspshm_area_t bah1;

	/*ret=*/ioctl(bspfd,IOCTL_BSPSHM_GET_AREA,&bah1); // FIXED: warning: variable 'ret' set but not used
	
	if (bah->physical!=bah1.physical || bah->length!=bah1.length) {
//		FIXME
		return 1;	
	}

	return 0;
}
//---------------------------------------------------------------------------

bspshm_area_t* bsp_create_area(int id, char *address, int size, int flags)
{
	bspshm_area_t bsa;
	int ret;

	bsa.id=id;
	bsa.physical=0;
	bsa.length=size;
	bsa.flags=flags;
	dbg("HOST: create_area id %x\n",id);
	ret=ioctl(bspfd,IOCTL_BSPSHM_CREATE_AREA,&bsa);
	if (ret && errno!=EEXIST && errno!=0) {
		return NULL;
	}
	return bsp_get_area(id);
}
//---------------------------------------------------------------------------
int bsp_destroy_area(bspshm_area_t *handle)
{
	int ret;
	ret=ioctl(bspfd,IOCTL_BSPSHM_DESTROY_AREA,handle->id);
	return ret;
}
//---------------------------------------------------------------------------
int bspshm_get_root(void)
{
	int ret;
	ret=ioctl(bspfd,IOCTL_BSPSHM_GET_ROOT);
	return ret;
}
//---------------------------------------------------------------------------
int bspshm_reset(void)
{
	int ret;
//	bsp_channel_cleanup();
	ret=ioctl(bspfd,IOCTL_BSPSHM_RESET);
	return ret;
}
//---------------------------------------------------------------------------
int bsp_status(void)
{
	return ioctl(bspfd,IOCTL_BSPSHM_GET_STATUS);
}
//---------------------------------------------------------------------------
// Returns local memory location
int bspshm_local_memory(void)
{
	return BSPSHM_MEM_MAIN;
}
//---------------------------------------------------------------------------

int bsp_init(int start)
{
	if (bspfd==-1) {
		bspfd=open(SHMBSPDEV,O_RDWR);
		if (bspfd==-1)
			return -1;
	}
	return 0;
}
//---------------------------------------------------------------------------
int bsp_deinit(int wait)
{
	close(bspfd);
	return 0;
}

//---------------------------------------------------------------------------
