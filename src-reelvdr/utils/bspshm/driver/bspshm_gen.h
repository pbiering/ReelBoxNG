
#ifndef _BSPSHM_GEN_H
#define _BSPSHM_GEN_H

/* --------------------------------------------------------------------- */
// Needs lock
bspshm_entry_t *bspshm_find_area(int id, int * entry_num)
{
	int n;
	if (!bsd.bsp_root || id==0)
		return NULL;
	for(n=0;n<ENTRIES_FOR_ROOT;n++) {
		if (bsd.bsp_root->ids[n]==id) {
			if (entry_num)
				*entry_num=n;
			return &(bsd.bsp_root->entries[n]);
		}
	}
	return NULL;
}
/* --------------------------------------------------------------------- */
// Needs table_lock
int bspshm_unmap_area_sub(int id)
{
	bspshm_entry_t *bse;
//	dbg("bspshm_unmap_area_sub %x",id);
	bse=bspshm_find_area(id,NULL);
	if (bse) {
		bse->usage--;
		if (bse->flags&BSPSHM_MEM_BSP)
			bsd.bsp_root->bsp_usage--;
		else
			bsd.bsp_root->host_usage--;
	}
	
	return 0;
}
/* --------------------------------------------------------------------- */
int bspshm_find_free_entry(void)
{
	int n;
	for(n=0;n<ENTRIES_FOR_ROOT;n++) {
		if (bsd.bsp_root->ids[n]==0) {
			return n;
		}
	}
	return -1;
}
/* --------------------------------------------------------------------- */

#endif
