/*****************************************************************************/
/*
 * Shared memory driver for BSP15
 * Combined with the right libs on host and BSP15, it allows
 * a flexible NUMA I/O.
 *
 * Copyright (C) 2006 Georg Acher, BayCom GmbH (acher at baycom dot de)
 *
 * #include <GPL-header>
 *
 * $Id: bspshm.h,v 1.1 2006/03/01 01:57:55 acher Exp $
 */
/*****************************************************************************/

#ifndef _BSPSHM_H
#define _BSPSHM_H

typedef struct {
	int id;
	unsigned long physical;
	char *real_mapped;
	char *mapped;
	int length;
	int real_length;
	int flags;
	int usage;
} bspshm_area_t;

#ifdef _BSPSHM_IOCTLS

#define IOCTL_BSPSHM_GET_AREA     _IOWR('d', 0x1, bspshm_area_t* ) 
#define IOCTL_BSPSHM_SET_ID       _IOWR('d', 0x2, int ) 
#define IOCTL_BSPSHM_CREATE_AREA  _IOWR('d', 0x3, bspshm_area_t* ) 
#define IOCTL_BSPSHM_DESTROY_AREA  _IOWR('d', 0x4, bspshm_area_t* ) 
#define IOCTL_BSPSHM_GET_STATUS    _IOR('d', 0x5, int) 
#define IOCTL_BSPSHM_RESET         _IOR('d', 0x6, int) 
#define IOCTL_BSPSHM_UNMAP_AREA   _IOWR('d', 0x7, int ) 
#define IOCTL_BSPSHM_GET_ROOT     _IOR('d', 0x8, int ) 
#define IOCTL_BSPSHM_TEST     _IOR('d', 0x9, int ) 

#endif

#define BSPSHM_MAGIC 0x47114321

// Flags for bspshm_area_t
#define BSPSHM_MEM_MAIN 0
#define BSPSHM_MEM_BSP 1
#define BSPSHM_MEM_CACHEABLE 2


// Flags for get_status
#define BSPSHM_STATUS_LOCK_TIMEOUT 1


#define PAGES_FOR_ROOT (2)
#define ENTRIES_FOR_ROOT (2*112)
#define MAPPED_IDS_PER_FILE (128)

#ifdef __KERNEL__

typedef struct {
	dma_addr_t phys; // 4 Byte
	void* kernel;
	int length;
	int flags;
	int usage; // Counting only host usage!
	int dummmy[3];
} bspshm_entry_t;

typedef struct {
	int magic;
	int running;
	atomic_t table_lock;
	int host_usage; // mapped host mem
	int bsp_usage;  // mapped bsp mem
	int mapped_by_bsp;
	int dummy[2];	
	int dummy1[8];
	int ids[ENTRIES_FOR_ROOT];
	bspshm_entry_t entries[0];
} bspshm_root_t;


typedef struct 
{
	struct pci_dev *bsp_dev;
	int bsp_pci_start;
	dma_addr_t bsp_root_d;
	bspshm_root_t *bsp_root;
	int num_areas;
	atomic_t open_files;
	struct semaphore table_sem;
	int lock_tries;
	int lock_timeout;
	void* pci_start;
	void* pci_start_nc;
	int event_count;
} bspshm_data_t;

typedef struct {
	int id;
	
} bspshm_mapped_t;

struct bspshm_file 
{
	int id;
	bspshm_mapped_t bsm[MAPPED_IDS_PER_FILE];
};
#endif // __KERNEL__
#endif
