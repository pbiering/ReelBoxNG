#ifndef _HDSHMLIB_H
#define _HDSHMLIB_H

#include "../driver/hdshm.h"

int hd_init(int start);
int hd_deinit(int wait);
#if 0
int hdshm_lock_table(void);
void hdshm_unlock_table(void);
#endif

hdshm_area_t* hd_get_area(int id);
void hd_unmap_area(hdshm_area_t *handle);
void hd_free_area(hdshm_area_t *handle);
int hd_check_area(hdshm_area_t *bah);
hdshm_area_t* hd_create_area(int id, char *address, int length, int flags);
int hd_destroy_area(hdshm_area_t *handle);

int hdshm_get_root(void);
int hd_status(void);
int hdshm_reset(void);
int hdshm_local_memory(void);
int hd_flush(void);
#endif
