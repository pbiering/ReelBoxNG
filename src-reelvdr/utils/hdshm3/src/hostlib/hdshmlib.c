/*****************************************************************************/
/*
 *
 * Copyright (C) 2006 Georg Acher, BayCom GmbH (acher at baycom dot de)
 *
 * #include <GPL-header>
 *
 * $Id: hdshmlib.c,v 1.2 2006/03/06 23:12:21 acher Exp $
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

// #ifdef CONFIG_MIPS
// #warning "Compiling for MIPS"
// #else
// #warning "Compiling for x86"
// #endif

#define _HDSHM_IOCTLS
#include "../driver/hdshm.h"

#define HDD_ID 0xFF000001
#define SHMHDDEV "/dev/hdshm"

static int hdfd =- 1;
#define dbg1(format, arg...) do {} while (0)
#define dbg(format, arg...) do {} while (0)     //printf(format, ## arg) //do {} while (0) //printk(format, ## arg)

//extern void hd_channel_cleanup(void);

//---------------------------------------------------------------------------
hdshm_area_t *hd_get_area(int id)
{
    int ret;
    char *p = NULL;
    char *mapped = NULL;
    int real_length;

    hdshm_area_t bah, *br = NULL;

    dbg1("HOST: get_area ID %x\n", id);
    memset(&bah, 0, sizeof(bah));
    bah.id = id;
    ret = ioctl(hdfd, IOCTL_HDSHM_GET_AREA, &bah);

    if (ret != 0)
        return NULL;

    ioctl(hdfd, IOCTL_HDSHM_SET_ID, id);

    real_length = ((bah.physical & 4095) + bah.length + 4095) & ~4095;
#if 0                           //ndef CONFIG_MIPS
    {
        // mmap via /dev/mem
        unsigned long x;
        int fd;
        unsigned long bar1;

        fd = open("/dev/mem", O_RDWR);
        x = 0xe2000000 + (bah.physical & ~4095);
        bar1 = (unsigned int)ioctl(hdfd, IOCTL_HDSHM_PCIBAR1);  // not 64bit clean
        x = bar1 + 0x2000000 + (unsigned int)(bah.physical & ~4095);

        p = mmap(NULL, real_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, x);
        printf("bar1 %p, %i,  %p -> %p\n", (void *)bar1, bah.length, (void *)x, p);
        close(fd);
        mapped = p;
    }
#else
    p = (char *)mmap(NULL, real_length, PROT_READ | PROT_WRITE, MAP_SHARED, hdfd, 0);
    mapped = p + (bah.physical & 4095);
#endif

    if (p == (void *)-1)
        return NULL;

    printf
        ("HOST: Mapped ID %x, phys %p to %p, rphys %p to virt %p, length %x, kernel_mem %p\n",
         id, (void *)bah.physical, p, (void *)(bah.physical & ~4095), mapped, real_length,
         bah.kernel_mem);
    br = (hdshm_area_t *) calloc(1, sizeof(hdshm_area_t));
    br->real_mapped = p;
    br->mapped = mapped;
    br->length = bah.length;
    br->real_length = real_length;
    br->flags = bah.flags;
    br->physical = bah.physical;
    br->kernel_mem = bah.kernel_mem;
    br->id = id;
    dbg1("handle %p\n", br);
    return br;
}

//---------------------------------------------------------------------------
void hd_unmap_area(hdshm_area_t * handle)
{
    int ret;
    if (!handle || !handle->mapped)
        return;
    dbg1("%s: unmap  %p, length %x\n", __FUNCTION__, handle->real_mapped, handle->real_length);
    munmap(handle->real_mapped, handle->real_length);
    dbg1("%s: ioctl unmap\n", __FUNCTION__);
    ret = ioctl(hdfd, IOCTL_HDSHM_UNMAP_AREA, handle->id);
    if (ret == 0) { }; // TODO catch -Wunused-but-set-variable
    dbg1("%s: ioctl result: %d\n", __FUNCTION__, ret);
    handle->mapped = NULL;
}

//---------------------------------------------------------------------------
void hd_free_area(hdshm_area_t * handle)
{
    if (!handle)
        return;
    hd_unmap_area(handle);
    free(handle);
}

//---------------------------------------------------------------------------
/*
  return 0: Area OK
  1: Area changed
*/
int hd_check_area(hdshm_area_t * bah)
{
    int ret;
    hdshm_area_t bah1;

    ret = ioctl(hdfd, IOCTL_HDSHM_GET_AREA, &bah1);
    if (ret == 0) { }; // TODO catch -Wunused-but-set-variable

    if (bah->physical != bah1.physical || bah->length != bah1.length)
    {
//              FIXME
        return 1;
    }

    return 0;
}

//---------------------------------------------------------------------------

hdshm_area_t *hd_create_area(int id, char *address, int size, int flags)
{
    hdshm_area_t bsa;
    int ret;

    bsa.id = id;
    bsa.physical = 0;
    bsa.length = size;
    bsa.flags = flags;

    ret = ioctl(hdfd, IOCTL_HDSHM_CREATE_AREA, &bsa);
    dbg("HOST: create_area id %x: %i\n", id, ret);
    if (ret && errno != EEXIST && errno != 0)
    {
        printf("hd_create_area: %s\n", strerror(errno));
        return NULL;
    }
    return hd_get_area(id);
}

//---------------------------------------------------------------------------
int hd_destroy_area(hdshm_area_t * handle)
{
    int ret;
    ret = ioctl(hdfd, IOCTL_HDSHM_DESTROY_AREA, handle->id);
    return ret;
}

//---------------------------------------------------------------------------
int hdshm_get_root(void)
{
    int ret;
    ret = ioctl(hdfd, IOCTL_HDSHM_GET_ROOT);
    return ret;
}

//---------------------------------------------------------------------------
int hdshm_reset(void)
{
    int ret;
//      hd_channel_cleanup();
    ret = ioctl(hdfd, IOCTL_HDSHM_RESET);
    return ret;
}

//---------------------------------------------------------------------------
int hd_status(void)
{
    return ioctl(hdfd, IOCTL_HDSHM_GET_STATUS);
}

//---------------------------------------------------------------------------
// Returns local memory location
int hdshm_local_memory(void)
{
    return HDSHM_MEM_MAIN;
}

//---------------------------------------------------------------------------

int hd_init(int start)
{
    if (hdfd == -1)
    {
        hdfd = open(SHMHDDEV, O_RDWR);
        if (hdfd == -1)
            return -1;
    }
    return 0;
}

//---------------------------------------------------------------------------
int hd_deinit(int wait)
{
    close(hdfd);
    hdfd = -1;
    return 0;
}

//---------------------------------------------------------------------------
