/*****************************************************************************/
/*
 * bspchannel.h 
 * Header file for SHM Channels (aka FIFOs)
 *
 * Copyright (C) 2006 Georg Acher, BayCom GmbH (acher at baycom dot de)
 *
 * #include <GPL-header>
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
 * d) Do not access one endpoint from both sides or simultanesously from one side 
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

#ifndef _BSPCHANNEL_H
#define _BSPCHANNEL_H

#include <bspshmlib.h>
#include <bspshm_user_structs.h>

#define BSP_CHANNEL_MAX_AREAS 31

#define BSP_CHANNEL_WRITE 0
#define BSP_CHANNEL_READ  1

#define BSP_CHANNEL_NOFLUSH 1

// User handle

typedef struct {
	int channel;		// Channel Id
	int dir;		// as seen from the host
	int creator;            // 1: handle via bsp_channel_create()
	int generation;		// mapping data from BSP run #
	int location;		// 
	int flush_ctrl;
	bspshm_area_t *control_area;	// Pointer to control area (bsp_channel_control_t)
	bspshm_area_t *data_area[BSP_CHANNEL_MAX_AREAS];	// Data areas

	void * read_data;	//
	int  read_size;     	//

	void * write_entry_set;	
} bsp_channel_t;

/*------------------------------------------------------------------------*/
// User calls
/*------------------------------------------------------------------------*/

// Initialize channels and return bspd-management structure
bspd_data_t* bsp_channel_init(void);

// Create Channel (aktive side)
bsp_channel_t *bsp_channel_create(int channel, int bufsize, int dir);

// Get Channel (passive side)
bsp_channel_t *bsp_channel_open(int channel);

// close mapping
int bsp_channel_close(bsp_channel_t *bsc);

// zero-copy read
int bsp_channel_read_start(bsp_channel_t *bsc, void **buf, int *size, int timeout);
int bsp_channel_read_finish(bsp_channel_t *bsc);

// zero-copy write
int bsp_channel_write_start(bsp_channel_t *bsc, void **buf, int size, int timeout);
int bsp_channel_write_finish(bsp_channel_t *bsc, int size);

// regular (copy) read/write
int bsp_channel_write(bsp_channel_t *bsc, void *buf, int size, int timeout); // tm in ms
int bsp_channel_read(bsp_channel_t *bsc, void *buf, int size, int timeout);

// Empty channel buffer (executed on next read)
int bsp_channel_invalidate(bsp_channel_t *bsc, int timeout);

// Check for data
int bsp_channel_empty(bsp_channel_t *bsc);

// Other stuff
void bsp_channel_flush_ctrl(bsp_channel_t *bsc, int val);

/*------------------------------------------------------------------------*/
// Internal structures
/*------------------------------------------------------------------------*/

typedef struct {
	int area_num;		// Area number
	int size;		// user data size
	int area_offset;	// data start relative to area start
	int rsize; 		// real/rounded size (for alignment/reservation)
} bsp_channel_entry_t;

typedef struct {
	int channel;		// Channel ID
	int state;		// reserved
	int dir; 		// As seen from Host
	int location; 		// BSPSHM_MEM_MAIN or BSPSHM_MEM_BSP 
	int bufsize;		// total buffer size
	int dummy1;
	int used_areas;		// allocated areas
	int dummy2;
	int usage;		// reserved (currently hard to maintain)
	int invalidate;   	// 1: invalidate at next read
	int max_entries;	// number of bsp_channel_entry_t in entries
	int read_entry;		// next read entry
	int write_entry;	// next write entry
	int area_sizes[BSP_CHANNEL_MAX_AREAS];
	bsp_channel_entry_t entries[];	// Pointer to packet descriptors
} bsp_channel_control_t;

#define CONTROL_ENTRIES (2*1024-256)
#define CONTROL_SIZE (CONTROL_ENTRIES*sizeof(bsp_channel_entry_t)+sizeof(bsp_channel_control_t))

// Conversion from Channel ID to Shared Memory ID
#define CHANNEL_TO_ID(x) (0x40000000+(BSP_CHANNEL_MAX_AREAS+1)*x)
#define CH_CONTROL 0

#define CH_STATE_RUNNING 1
#define MAX_CHANNELS 1024
#endif
