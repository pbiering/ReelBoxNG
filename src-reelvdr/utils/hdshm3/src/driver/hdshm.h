/*****************************************************************************/
/*
 * Shared memory driver for DeCypher
 *
 * Copyright (C) 2007-2008 Georg Acher, BayCom GmbH (acher at baycom dot de)
 *
 * #include <GPLV2-header>
 *
 */
/*****************************************************************************/

#ifndef _HDSHM_H
#define _HDSHM_H
#ifndef __x86_64
#define __x86_64
#endif

// debug
//extern uint32_t hd_debug_mask;
#define HD_DEBUG_BIT_MODULE_TODO	0x01000000
#define HD_DEBUG_BIT_MODULE_INIT	0x00010000
#define HD_DEBUG_BIT_MODULE_EXIT	0x00020000
#define HD_DEBUG_BIT_MODULE_IOCTL	0x00001000
#define HD_DEBUG_BIT_MODULE_MMAP	0x00002000
#define HD_DEBUG_BIT_MODULE_HDFB	0x00000100

typedef struct {
	int id;
	unsigned long physical;
	char *real_mapped;
	char *mapped;
	int length;
	int real_length;
	int flags;
	int usage;
	char *kernel_mem;
} hdshm_area_t;

typedef struct {
	int id;
	int remote_offset;
	int direction;		// 0: local (HDE) -> remote (host), 1: remote -> local
	void *local;
	int len;
} hdshm_dma_t;

#define IOCTL_HDSHM_GET_AREA     _IOWR('d', 0x1, hdshm_area_t* ) 
#define IOCTL_HDSHM_SET_ID       _IOWR('d', 0x2, int ) 
#define IOCTL_HDSHM_CREATE_AREA  _IOWR('d', 0x3, hdshm_area_t* ) 
#define IOCTL_HDSHM_DESTROY_AREA  _IOWR('d', 0x4, hdshm_area_t* ) 
#define IOCTL_HDSHM_GET_STATUS    _IOR('d', 0x5, int) 
#define IOCTL_HDSHM_RESET         _IOR('d', 0x6, int) 
#define IOCTL_HDSHM_UNMAP_AREA   _IOWR('d', 0x7, int ) 
#define IOCTL_HDSHM_GET_ROOT     _IOR('d', 0x8, int ) 
#define IOCTL_HDSHM_FLUSH     _IOR('d', 0x9, int ) 
#define IOCTL_HDSHM_PCIBAR1     _IOR('d', 0xa, int ) 
#define IOCTL_HDSHM_SHUTDOWN     _IOWR('d', 0xb, int) 
#define IOCTL_HDSHM_DMA          _IOWR('d', 0xc, hdshm_dma_t*)
#define IOCTL_HDSHM_CMDLINE      _IOR('d', 0xc, char[256])

#define HDSHM_MAGIC 0x47124321

// Flags for hdshm_area_t
#define HDSHM_MEM_MAIN 0
#define HDSHM_MEM_HD 1  // Forced!
#define HDSHM_MEM_CACHEABLE 2
#define HDSHM_MEM_DMA 4


// Flags for get_status
#define HDSHM_STATUS_LOCK_TIMEOUT 1
#define HDSHM_STATUS_NOT_BOOTED 2

// 1MB starting at 16MB
#define MAP_START 0x1000000
#define MAP_SIZE  0x0300000

// !!! Manual tuning: PAGES_FOR_ROOT*4096 < sizeof(hdshm_root_t)+sizeof(hdshm_entry_t)* ENTRIES_FOR_ROOT !!!

#define PAGES_FOR_ROOT (2)
#define ENTRIES_FOR_ROOT (2*112) // Almost 2 Pages
#define MAPPED_IDS_PER_FILE (128)
#define HD_ALLOC_PAGES (MAP_SIZE/4096)

#ifdef __KERNEL__

// Exchange structure for Host and DeCypher
typedef struct {
#ifndef __x86_64
	dma_addr_t phys; // 4 Byte	
#else
	uint32_t phys; // 4 Byte
#endif
#ifndef __x86_64
	unsigned int kernel; 
	unsigned int kernel1; 
	int length;
	int flags;
	int usage; // Counting only host usage!
	int dummmy[2];  // 32 Byte
#else
	uint32_t kernel; 
	uint32_t kernel1; 
	int32_t length;
	int32_t flags;
	int32_t usage; // Counting only host usage!
	int32_t dummmy[2];  // 32 Byte
#endif
} hdshm_entry_t;

typedef struct {
#ifndef __x86_64
	int magic;
	int running;
#else
	int32_t magic;
	int32_t running;
#endif
	atomic_t table_lock;
#ifndef __x86_64
	int host_usage; // mapped host mem
	int hd_usage;  // mapped hd mem
	int mapped_by_hd;
	int booted; // cleared at host startup and hdboot, set by DeCypher driver startup
	int dummy;	
	int dummy1[8];
	int alloc_bits[HD_ALLOC_PAGES/32];
	int ids[ENTRIES_FOR_ROOT];
#else
	int32_t host_usage; // mapped host mem
	int32_t hd_usage;  // mapped hd mem
	int32_t mapped_by_hd;
	int32_t booted; // cleared at host startup and hdboot, set by DeCypher driver startup
	int32_t dummy;	
	int32_t dummy1[8];
	int32_t alloc_bits[HD_ALLOC_PAGES/32];
	int32_t ids[ENTRIES_FOR_ROOT];
#endif
	hdshm_entry_t entries[0];
} hdshm_root_t;


typedef struct 
{
	struct pci_dev *hd_pci;
	void* bar1;
	void* start_phys;
	void* start;
	void* start_nc;

	hdshm_root_t *hd_root;
	int num_areas;
	atomic_t open_files;
	struct semaphore table_sem;
	int lock_tries;
	int lock_timeout;
	void* pci_start;
	void* pci_start_nc;
	int event_count;
} hdshm_data_t;

typedef struct {
	int id;
	
} hdshm_mapped_t;

struct hdshm_file 
{
	int id;
	hdshm_mapped_t bsm[MAPPED_IDS_PER_FILE];
};
#endif // __KERNEL__
#endif
