
#ifndef _HDSHM_GEN_H
#define _HDSHM_GEN_H
#ifndef __x86_64
#define __x86_64
#endif

#include "hdshm.h"

/* --------------------------------------------------------------------- */
// Needs lock
hdshm_entry_t *hdshm_find_area(int id, int * entry_num)
{
	int n;
	if (!hdd.hd_root || id==0)
		return NULL;
	for(n=0;n<ENTRIES_FOR_ROOT;n++) {
		if (hdd.hd_root->ids[n]==id) {
			if (entry_num)
				*entry_num=n;
			return &(hdd.hd_root->entries[n]);
		}
	}
	return NULL;
}
/* --------------------------------------------------------------------- */
// Needs table_lock
int hdshm_unmap_area_sub(int id)
{
	hdshm_entry_t *bse;
//	dbg("bspshm_unmap_area_sub %x",id);
	bse=hdshm_find_area(id,NULL);
	if (bse) {
		bse->usage--;
		if (bse->flags&HDSHM_MEM_HD)
			hdd.hd_root->hd_usage--;
		else
			hdd.hd_root->host_usage--;
	}
	
	return 0;
}
/* --------------------------------------------------------------------- */
int hdshm_find_free_entry(void)
{
	int n;
	for(n=0;n<ENTRIES_FOR_ROOT;n++) {
		if (hdd.hd_root->ids[n]==0) {
			return n;
		}
	}
	return -1;
}
/* --------------------------------------------------------------------- */

// HACK: Could be more efficient...

#define HD_GET_PAGE_BIT(array, pnum) (array[pnum/32]&(1<<(pnum&31)))
#define HD_SET_PAGE_BIT(array, pnum,val) array[pnum/32]=((array[pnum/32])&(~(1<<(pnum&31))))|(val*(1<<(pnum&31)))

int hdshm_allocate_sub(hdshm_root_t* hdr, int startpage, int pages, int *allocated_page)
{
	int n;
	int end;
	end=startpage+pages;
	*allocated_page=-1;

	if (end>HD_ALLOC_PAGES)
		return -1;

	for(n=startpage;n<startpage+pages;n++) {
		if (HD_GET_PAGE_BIT(hdr->alloc_bits,n)) {
			*allocated_page=n;
			return -1;
		}
	}
	return startpage;
}
/* --------------------------------------------------------------------- */
void hdshm_allocate_mark(hdshm_root_t* hdr, int startpage, int pages, int val)
{
	int n;
	if (startpage+pages>HD_ALLOC_PAGES)
		return;

	for(n=startpage;n<startpage+pages;n++)
		HD_SET_PAGE_BIT(hdr->alloc_bits,n,val);
}
/* --------------------------------------------------------------------- */
void hdshm_free_memory(hdshm_data_t* hdd, int phys, int length)
{
	int startpage=(phys-MAP_START)/4096;
	int pages=((length+4095)/4096);
	hdshm_allocate_mark(hdd->hd_root, startpage, pages, 0);
}
/* --------------------------------------------------------------------- */
void* hdshm_allocate_memory(hdshm_data_t* hdd, int length, int *phys)
{
	int pages;
	int n;
	int acp,ret;
	dbg("allocate mem: %ibytes\n",length);
	pages=((length+4095)/4096);
	for(n=0;n<HD_ALLOC_PAGES;n++) {
		ret=hdshm_allocate_sub(hdd->hd_root, n, pages, &acp);
		if (ret==-1 && acp==-1)
			return NULL; // OUT OF MEMORY
		if (ret==-1) {
			n=acp;
			continue;
		}
		if (ret>=0) {
			hdshm_allocate_mark(hdd->hd_root,ret,pages,1);
			*phys=MAP_START+4096*ret;
			dbg("allocated starts at page %i, addr %x\n",ret,*phys);

			return hdd->start+4096*ret;
		}
	}
	return NULL;
}
/* --------------------------------------------------------------------- */

#endif
