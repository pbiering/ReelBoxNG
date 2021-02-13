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
 * $Id: bspshm.c,v 1.2 2006/03/06 23:12:33 acher Exp $
 */
/*****************************************************************************/
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/poll.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <asm/atomic.h>

#define _BSPSHM_IOCTLS
#include "bspshm.h"

static bspshm_data_t bsd;

#include "bspshm_gen.h"

#define dbg1(format, arg...) do {} while (0)
#define dbg(format, arg...) do {} while (0) //printk(format"\n", ## arg)
#define err(format, arg...) printk(format"\n", ## arg)

#define BSPSHM_MAJOR 245

/* --------------------------------------------------------------------- */
int bspshm_init_struct(void)
{
	int n;
	bspshm_root_t *bsr;

	bsd.bsp_dev=pci_find_device_reverse(0x12d5,0x1000,NULL);
	if (!bsd.bsp_dev)
		return -1;
	
	bsd.bsp_pci_start=pci_resource_start(bsd.bsp_dev,0);
	dbg1("bspshm: Found BSP at %x",bsd.bsp_pci_start);

	bsd.num_areas=0;
	bsd.bsp_root=NULL;
	bsr=pci_alloc_consistent (bsd.bsp_dev, 4096*PAGES_FOR_ROOT, &bsd.bsp_root_d);
	if (!bsr) {
		err("Failed to get DMA for BSP Root");
		return -1;
	}
	bsr->magic=BSPSHM_MAGIC;
	bsr->running=0;

	bsr->host_usage=0;
	bsr->bsp_usage=0;
	bsd.event_count=0;

	for(n=0;n<ENTRIES_FOR_ROOT;n++) {
		memset(&bsr->entries[n],0,sizeof(bspshm_entry_t));
	}

	bsd.lock_tries=600;
	bsd.lock_timeout=0;
	atomic_set(&bsd.open_files,0);
	atomic_set(&bsr->table_lock,1);
	
	sema_init(&bsd.table_sem,1);
	bsd.bsp_root=bsr;
	bsd.pci_start=ioremap(bsd.bsp_pci_start,64*1024*1024);
	bsd.pci_start_nc=ioremap(bsd.bsp_pci_start,64*1024*1024);
	err("bspshm: BSP-phys %x, PCI %p, PCI-NC %p, BSP-Root virt %p",bsd.bsp_pci_start, bsd.pci_start,bsd.pci_start_nc,bsr);
	return 0;
}
/* --------------------------------------------------------------------- */
// The lock is shared between Geode and BSP
int bspshm_lock_table(void)
{
	int tries=0;

	if (bsd.lock_timeout)
		return 1;

	down(&bsd.table_sem);
	while(1) {
		if (atomic_dec_and_test(&bsd.bsp_root->table_lock)) {
			return 0;
		}

		tries++;
		if (tries>bsd.lock_tries) {
			bsd.lock_timeout=1;
			up(&bsd.table_sem);
			return 1;
		}
		if (tries<500) {
			udelay(1);
		}
		else
			msleep_interruptible(1);		
	}
}
/* --------------------------------------------------------------------- */
void bspshm_unlock_table(void)
{
	atomic_inc(&bsd.bsp_root->table_lock);
	up(&bsd.table_sem);
}
/* --------------------------------------------------------------------- */
int bspshm_get_area(struct bspshm_file *bsf, unsigned long arg)
{
	bspshm_area_t bsa;
	bspshm_entry_t *bse;
	int ret=0;

	if (copy_from_user(&bsa,(void*)arg,sizeof(bspshm_area_t)))
		return -EFAULT;

	dbg1("get_area %x",bsa.id);

	if (bspshm_lock_table())
		return -ETIMEDOUT;

	bse=bspshm_find_area(bsa.id, NULL);
	if (!bse) {
		bspshm_unlock_table();
		return -ENOENT;
	}
	bsa.physical=bse->phys;
	bsa.length=bse->length;
	bsa.flags=bse->flags;
	bsa.usage=bse->usage;
	
	if (copy_to_user((void*)arg,&bsa,sizeof(bspshm_area_t)))
		ret=-EFAULT;
	bspshm_unlock_table();
	return ret;
}

/* --------------------------------------------------------------------- */
// Housekeeping for file-based mappings
int bspshm_unmap_area(struct bspshm_file *bsf, unsigned long arg)
{
	int id=(int)arg;
	int n;

	dbg("explicit unmap for %x",id);
	if (bspshm_lock_table())
		return -ETIMEDOUT;

	for(n=0;n<MAPPED_IDS_PER_FILE;n++) {
		int id=bsf->bsm[n].id;
		if (id!=0 && bsf->id==id) {
			bspshm_unmap_area_sub(id);			
			bsf->bsm[n].id=0;
			break;
		}
	}
	bspshm_unlock_table();
	return 0;
}
/* --------------------------------------------------------------------- */
int bspshm_set_id(struct bspshm_file *bsf, unsigned long arg)
{	
	bspshm_entry_t *bse;
	int id=(int)arg;
	dbg1("set_area %x",id);
	if (!id) {
		bsf->id=id;
		return 0;
	}

	if (bspshm_lock_table())
		return -ETIMEDOUT;

	bse=bspshm_find_area(id,NULL);
	bspshm_unlock_table();

	if (!bse) 
		return -ENOENT;
	
	bsf->id=id;
	return 0;
}
/* --------------------------------------------------------------------- */
int bspshm_create_area(struct bspshm_file *bsf, unsigned long arg)
{	
	bspshm_area_t bsa;
	bspshm_entry_t *bse;
	int found;

	if (copy_from_user(&bsa,(void*)arg,sizeof(bsa)))
		return -EFAULT;

	dbg1("create_area id=%x, flags %x",bsa.id,bsa.flags);
	if (bsa.flags&BSPSHM_MEM_BSP || !bsa.id)
		return -EINVAL;

	if (bspshm_lock_table())
		return -ETIMEDOUT;

	bse=bspshm_find_area(bsa.id,NULL);
	if (bse) {
		bspshm_unlock_table();
		return -EEXIST;
	}
	
	found=bspshm_find_free_entry();

	if (found==-1) {
		bspshm_unlock_table();
		return -ENFILE;
	}
	
	bse=&(bsd.bsp_root->entries[found]);

	bse->kernel=pci_alloc_consistent (bsd.bsp_dev, bsa.length, &bse->phys);

	dbg("create_area id %x bse %p  physical %x, kernel %p length %x",bsa.id, bse,(int)bse->phys, bse->kernel, bsa.length);
	if (!bse->kernel) {
		bspshm_unlock_table();
		return -ENOMEM;
	}
	bse->usage=0;
	bse->length= bsa.length;
	bse->flags=bsa.flags;
	bsd.bsp_root->ids[found]=bsa.id;
	bsd.num_areas++;
	bspshm_unlock_table();
	return 0;
}
/* --------------------------------------------------------------------- */
// Needs table_lock
int bspshm_destroy_area_sub(int id)
{
	int num_entry;
	bspshm_entry_t *bse;

	bse=bspshm_find_area(id,&num_entry);
	if (!bse) 
		return -ENOENT;

	if (bse->usage || bse->flags&BSPSHM_MEM_BSP) 
		return -EBUSY;
	
	dbg1("destroy_area bse=%p",bse);
	dbg("free %x %x %p %x", (int)bsd.bsp_dev, bse->length, bse->kernel, bse->phys);
	pci_free_consistent(bsd.bsp_dev, bse->length, bse->kernel, bse->phys);
	bsd.bsp_root->ids[num_entry]=0;
	bsd.num_areas--;

	return 0;		
}
/* --------------------------------------------------------------------- */
int bspshm_destroy_area(struct bspshm_file *bsf, unsigned long arg)
{
	int id=(int)arg;
	int retval;
	dbg1("destroy_area id=%x",id);
	if (id==0)
		return -ENOENT;

	if (bspshm_lock_table())
		return -ETIMEDOUT;

	retval=bspshm_destroy_area_sub(id);

	bspshm_unlock_table();

	return retval;	
}
/* --------------------------------------------------------------------- */
int bspshm_get_status(struct bspshm_file *bsf, unsigned long arg)
{
	int flags=0;
	if (bsd.lock_timeout)
		flags|= BSPSHM_STATUS_LOCK_TIMEOUT;
	return flags;
}
/* --------------------------------------------------------------------- */
int bspshm_reset(struct bspshm_file *bsf, unsigned long arg)
{
	int n;
	down(&bsd.table_sem);
	// Clear all BSP mappings
	for(n=0;n<ENTRIES_FOR_ROOT;n++) {
		if (bsd.bsp_root->ids[n]) {
			if (bsd.bsp_root->entries[n].flags&BSPSHM_MEM_BSP)
				bsd.bsp_root->ids[n]=0;	
		}
	}
	bsd.event_count++;
	up(&bsd.table_sem);
	
	bsd.lock_timeout=0;
	return 0;
}
/* --------------------------------------------------------------------- */
int bspshm_open (struct inode *inode, struct file *file)
{
	struct bspshm_file *bsf;
	int n;

	if (bsd.lock_timeout)
		return -ETIMEDOUT;

	bsf=(struct bspshm_file *)kmalloc(sizeof(struct bspshm_file),GFP_KERNEL);
	if (!bsf)
		return -ENOMEM;
	
	bsf->id=0;
	for(n=0;n<MAPPED_IDS_PER_FILE;n++) {
		bsf->bsm[n].id=0;
	}

	file->private_data=bsf;
	atomic_inc(&bsd.open_files);
	return 0;
}
/* --------------------------------------------------------------------- */
int bspshm_release (struct inode *inode, struct file *file)
{
	struct bspshm_file *bsf=(struct bspshm_file*)file->private_data;
	int n,id;
	dbg1("close");
	if (bspshm_lock_table()) {
		goto release_error;
	}
	
	for(n=0;n<MAPPED_IDS_PER_FILE;n++) {
		id=bsf->bsm[n].id;
		if (id!=0) {
			dbg("unmap_area_sub %x",id);
			bspshm_unmap_area_sub(id);
		}
	}
	bspshm_unlock_table();

release_error:
	kfree(bsf);
	atomic_dec(&bsd.open_files);
	dbg("usage h %i b %i",bsd.bsp_root->host_usage,bsd.bsp_root->bsp_usage);

	return 0;
}

/* --------------------------------------------------------------------- */
int bsptest(unsigned long arg)
{
	int n;
	int* z;
	int a=0;

	if (arg)
		z=(int*)(bsd.pci_start+16*1024*1024);
	else
		z=(int*)(bsd.pci_start_nc+16*1024*1024);

	for(n=0;n<4*1024;n++) {
	
	}
	return a;
}
/* --------------------------------------------------------------------- */
static int bspshm_ioctl (struct inode *inode, struct file *file, 
	unsigned int cmd, unsigned long arg)
{
	int ret=0;
	struct bspshm_file *bsf=(struct bspshm_file*)file->private_data;

	switch(cmd) {
	case IOCTL_BSPSHM_GET_AREA:
		ret=bspshm_get_area(bsf,arg);
		break;
	case IOCTL_BSPSHM_SET_ID:
		ret=bspshm_set_id(bsf,arg);
		break;
	case IOCTL_BSPSHM_CREATE_AREA:
		ret=bspshm_create_area(bsf,arg);
		break;
	case IOCTL_BSPSHM_DESTROY_AREA:
		ret=bspshm_destroy_area(bsf,arg);
		break;
	case IOCTL_BSPSHM_GET_STATUS:
		ret=bspshm_get_status(bsf,arg);
		break;
	case IOCTL_BSPSHM_RESET:
		ret=bspshm_reset(bsf,arg);
		break;
	case IOCTL_BSPSHM_UNMAP_AREA:
		ret=bspshm_unmap_area(bsf,arg);
		break;
	case IOCTL_BSPSHM_GET_ROOT:
		ret=bsd.bsp_root_d;
		break;
	case IOCTL_BSPSHM_TEST:
		bsptest(arg);
		break;
	default:
		break;
	}
	dbg("usage h %i b %i",bsd.bsp_root->host_usage,bsd.bsp_root->bsp_usage);
	return ret;
}
/* --------------------------------------------------------------------- */
void show_page(unsigned long virtaddr)
{
	struct page * page;
	page=virt_to_page(__va(virtaddr));

	dbg("Virt %x Page %p Reserved %x, count %x, mcount %x mapping %p",
	    (int)virtaddr,
	    page,(int)PageReserved(page),
	    (page->_count),
	    (page->_mapcount),
	    page->mapping );
}
/* --------------------------------------------------------------------- */
// address: virtual address in user space
struct page *bspshm_nopage(struct vm_area_struct *vma, unsigned long address, int *type)
{
	bspshm_entry_t *bse=(bspshm_entry_t *)vma->vm_private_data;
	unsigned long virtaddr;
	unsigned long offset;
	struct page * page;
	
	offset=address - vma->vm_start + (vma->vm_pgoff << PAGE_SHIFT);

	if (bse->flags&BSPSHM_MEM_BSP) {
		virtaddr=bsd.bsp_pci_start+(bse->phys&~4095);
	}
	else {
		virtaddr = (unsigned long)bse->kernel;
	}

	virtaddr+=offset;

//	dbg("nopage offset %x, va %x, user virt start %x, user %x prot %x", (int)offset,(int)virtaddr,
//	    (int)vma->vm_start,(int)address,(int)vma->vm_page_prot.pgprot);

	if (bse->flags&BSPSHM_MEM_BSP) {		
		page=virt_to_page(__va(virtaddr));
		page->mapping=NULL;
		SetPageReserved(page);	
		atomic_set(&page->_count,1);
		atomic_set(&page->_mapcount,1);
	}
	else {
		page=virt_to_page(virtaddr);		
		SetPageReserved(page);
	}

#if 0
	if (!PageReserved(page))
		get_page(page);
#endif
//	show_page(virtaddr);

	if (type)
                *type = VM_FAULT_MINOR;
	return page;
}

/* --------------------------------------------------------------------- */
struct vm_operations_struct bspshm_vm_ops=
{
	.nopage = bspshm_nopage,
};

/* --------------------------------------------------------------------- */
int bspshm_mmap(struct file * file, struct vm_area_struct * vma)
{
	struct bspshm_file *bsf=(struct bspshm_file*)file->private_data;
	bspshm_entry_t *bse;
	int n,uncached=1;
	dma_addr_t phys_start;
	int length;

	dbg("mmap id %x flags %x",bsf->id,(int)vma->vm_flags);

	if (!bsf->id) 
		return -EINVAL;		

	if (bspshm_lock_table())
		return -ETIMEDOUT;

	// Only one mapping per id per process
	for(n=0;n<MAPPED_IDS_PER_FILE;n++) {
		int id=bsf->bsm[n].id;
		if (id!=0 && bsf->id==id) {
			bspshm_unlock_table();
			return -EEXIST;
		}
	}
	dbg1("mmap find");
	bse=bspshm_find_area(bsf->id,NULL);
	
	if (!bse) {
		dbg("mmap not found");
		bspshm_unlock_table();
		return -ENOENT;		
	}
	dbg("mmap bse=%p user %x phys=%x size %x offset %x",
	    bse,
	    (int)vma->vm_start,
	    (int)bse->phys,
	    (int)(vma->vm_end - vma->vm_start),
	    (int)vma->vm_pgoff);

	if (bse->flags&BSPSHM_MEM_CACHEABLE)
		uncached=0;

	if (uncached)
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	dbg1("uncached %x pgprot %x",uncached,(int)pgprot_val(vma->vm_page_prot));

	// BSP Memory may be not be page aligned
	phys_start=bse->phys&~4095;
	length=vma->vm_end-vma->vm_start; //((bse->phys+bse->length)-phys_start+4095)&~4095;

	vma->vm_private_data=bse;
	vma->vm_flags |= VM_RESERVED|VM_SHARED;

	if (bse->flags&BSPSHM_MEM_BSP) {
		
		bsd.bsp_root->bsp_usage++;		
	}
	else {	
		bsd.bsp_root->host_usage++;       
	}

	vma->vm_ops=&bspshm_vm_ops;	// Map later with nopage
	bse->usage++;	
	
	for(n=0;n<MAPPED_IDS_PER_FILE;n++) {
		int id=bsf->bsm[n].id;
		if (!id) {
			bsf->bsm[n].id=bsf->id;
			break;
		}
	}
	
	bspshm_unlock_table();

	return 0;
}
/* --------------------------------------------------------------------- */
static struct file_operations bspshm_fops =
{
        owner:          THIS_MODULE,
        ioctl:          bspshm_ioctl,
        open:           bspshm_open,
//      read:           bspshm_read,
	mmap:           bspshm_mmap,
        release:        bspshm_release,
};
                                
/* --------------------------------------------------------------------- */

static int __init bspshm_init(void) 
{
        int retval;
	retval=bspshm_init_struct();
	if (retval)
		return retval;

        retval= register_chrdev(BSPSHM_MAJOR, "bspshm", &bspshm_fops);
	
        devfs_mk_cdev(MKDEV(BSPSHM_MAJOR, 0),
                        S_IFCHR | S_IRUSR | S_IWUSR,
                        "bspshm", 0);
	
	return retval;
}

/* --------------------------------------------------------------------- */
                
static void __exit bspshm_exit(void)
{
	int n;
	for(n=0;n<ENTRIES_FOR_ROOT;n++)
		if (bsd.bsp_root->ids[n])
			bspshm_destroy_area_sub(bsd.bsp_root->ids[n]);

	bsd.bsp_root->running=0;
	bsd.bsp_root->magic=0;
	iounmap(bsd.pci_start);
	iounmap(bsd.pci_start_nc);
	pci_free_consistent(bsd.bsp_dev, 4096*PAGES_FOR_ROOT,bsd.bsp_root, bsd.bsp_root_d);
        unregister_chrdev(BSPSHM_MAJOR,"bspshm");
}
                                
module_init(bspshm_init);
module_exit(bspshm_exit);

