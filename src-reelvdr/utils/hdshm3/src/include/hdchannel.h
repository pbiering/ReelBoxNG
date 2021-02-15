/*****************************************************************************/
/*
 * bspchannel.h 
 * Header file for SHM Channels (aka FIFOs)
 *
 * Copyright (C) 2006-2007 Georg Acher, BayCom GmbH (acher at baycom dot de)
 *
 * #include <GPLv2-header>
 *
 * $Id: bspchannel.h,v 1.2 2006/03/06 23:12:33 acher Exp $
 */
/*****************************************************************************/

/*
 * Usage rules:
 *
 * a) Channels are unidirectional.
 * b) Channel endpoints can be on the same system side.
 * c) The storage area is always local to bsp_channel_create() which creates the 
 *    first endpoint.
 * d) bsp_channel_open() creates the second endpoint. The direction
 *    is automatically determined.
 * d) Do not access one endpoint from both sides or simultaneously from one side 
 *    without (external) locking!
 * e) Do not write more memory as reserved in bsp_channel_write_start()!
 * f) bsp_channel_read() is packet oriented. Reading less than written flushes 
 *    the remaining data of the packet.
 * g) Call bsp_channel_init() before using any channels. 
 * h) bsp_channel_invalidate() frees the buffer only when a read is attempted.
 * 
 * Prototype restrictions:
 *
 * a) Resizing of a previous existing channel may fail without bspshm-driver reloading
 * b) Buffer size is limited to 128K when created on Linux side. Bigger buffers may 
 *    not always work.
 * c) Timeouts in ms-units are only rough guidelines. 
 *
 */

#ifndef _HDCHANNEL_H
#define _HDCHANNEL_H

#include <hdshmlib.h>
#include <hdshm_user_structs.h>

#define HD_CHANNEL_MAX_AREAS 31

#define HD_CHANNEL_WRITE 0
#define HD_CHANNEL_READ  1

#define HD_CHANNEL_NOFLUSH 1

// User handle

typedef struct {
	int channel;		// Channel Id
	int dir;		// as seen from the host
	int creator;            // 1: handle via hd_channel_create()
	int generation;		// mapping data from HD run #
	int location;		// 
	int flush_ctrl;
	hdshm_area_t *control_area;	// Pointer to control area (hd_channel_control_t)
	hdshm_area_t *data_area[HD_CHANNEL_MAX_AREAS];	// Data areas

	void * read_data;	//
	int  read_size;     	//

	void * write_entry_set;	
} hd_channel_t;

/*------------------------------------------------------------------------*/
// User calls
/*------------------------------------------------------------------------*/

// Initialize channels and return hdd-management structure
//hdd_data_t* hd_channel_init(void);

// Create Channel (aktive side)
hd_channel_t *hd_channel_create(int channel, int bufsize, int dir);

// Get Channel (passive side)
hd_channel_t *hd_channel_open(int channel);

// close mapping
int hd_channel_close(hd_channel_t *bsc);

// zero-copy read
int hd_channel_read_start(hd_channel_t *bsc, void **buf, int *size, int timeout);
int hd_channel_read_start_kmem(hd_channel_t *bsc, void **buf, void **kmem, int *size, int timeout);
int hd_channel_read_finish(hd_channel_t *bsc);

// zero-copy write
int hd_channel_write_start(hd_channel_t *bsc, void **buf, int size, int timeout);
int hd_channel_write_finish(hd_channel_t *bsc, int size);

// regular (copy) read/write
int hd_channel_write(hd_channel_t *bsc, void *buf, int size, int timeout); // tm in ms
int hd_channel_read(hd_channel_t *bsc, void *buf, int size, int timeout);

// Empty channel buffer (executed on next read)
int hd_channel_invalidate(hd_channel_t *bsc, int timeout);

// Check for data
int hd_channel_empty(hd_channel_t *bsc);

// Other stuff
void hd_channel_flush_ctrl(hd_channel_t *bsc, int val);


/*------------------------------------------------------------------------*/
// Internal structures
/*------------------------------------------------------------------------*/

typedef struct {
	int area_num;		// Area number
	int size;		// user data size
	int area_offset;	// data start relative to area start
	int rsize; 		// real/rounded size (for alignment/reservation)
} hd_channel_entry_t;

typedef struct {
	int channel;		// Channel ID
	int state;		// reserved
	int dir; 		// As seen from Host
	int location; 		// HDSHM_MEM_MAIN or HDSHM_MEM_HD 
	int bufsize;		// total buffer size
	int read_priority;      // Index of read_entry
	int used_areas;		// allocated areas
	int usage;		// reserved (currently hard to maintain)
	int invalidate;   	// 1: invalidate at next read
	int max_entries;	// number of hd_channel_entry_t in entries
	int read_entry;		// next read entry
	int write_entry;	// next write entry
	int area_sizes[HD_CHANNEL_MAX_AREAS];
	hd_channel_entry_t entries[];	// Pointer to packet descriptors
} hd_channel_control_t;

#define CONTROL_ENTRIES (2*1024-256)  // Max number of packets
#define HD_CONTROL_SIZE (CONTROL_ENTRIES*sizeof(hd_channel_entry_t)+sizeof(hd_channel_control_t))

// Conversion from Channel ID to Shared Memory ID
#define HD_CHANNEL_TO_ID(x) (0x40000000+(HD_CHANNEL_MAX_AREAS+1)*x)
#define CH_CONTROL 0

#define CH_STATE_RUNNING 1
#define MAX_CHANNELS 1024
#endif
