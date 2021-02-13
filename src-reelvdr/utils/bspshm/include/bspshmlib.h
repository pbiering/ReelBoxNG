#ifndef _BSPSHMLIB_H
#define _BSPSHMLIB_H

#include "../driver/bspshm.h"

int bsp_init(int start);
#if 0
int bspshm_lock_table(void);
void bspshm_unlock_table(void);
#endif

bspshm_area_t* bsp_get_area(int id);
void bsp_unmap_area(bspshm_area_t *handle);
void bsp_free_area(bspshm_area_t *handle);
int bsp_check_area(bspshm_area_t *bah);
bspshm_area_t* bsp_create_area(int id, char *address, int length, int flags);
int bsp_destroy_area(bspshm_area_t *handle);

int bspshm_get_root(void);
int bsp_status(void);
int bspshm_reset(void);
int bspshm_local_memory(void);

#endif
