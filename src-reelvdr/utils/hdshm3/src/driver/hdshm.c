/*****************************************************************************/
/*
  Linux kernel driver for shared memory communication 
  between host CPU and DeCypher (MIPS)

  (c) 2007 Georg Acher (acher at baycom dot de)
  
  #include <GPLv2.h>  
  
  Bugs: 
    Not tested on x86_64
    Undefined effects on MIPS reboots    
*/
/*****************************************************************************/

#include <linux/version.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33))
#include <linux/autoconf.h>
#else
#include <generated/autoconf.h>
#endif
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

#include <linux/poll.h>
#include <linux/pci.h>
#include <linux/device.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17))
#include <linux/devfs_fs_kernel.h>
#else
#include <linux/fs.h>
#include <linux/io.h>
#endif

#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <asm/atomic.h>

#define _HDSHM_IOCTLS
#include "hdshm.h"

uint32_t hd_dbg_mask = 0xffffffff;
static hdshm_data_t hdd; // FIXME allow multiple devices

#ifdef CONFIG_MIPS
#warning "Compiling for MIPS"
#include <asm/mach-go700x/go700x_pci.h>
#define IS_HD
#else
// #warning "Compiling for x86-host"
#define IS_HOST
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
/*only for avg*/
#define HAS_HD_FB
#endif
#endif


//#define dbg1(format, arg...) do {} while (0)
#define dbg(format, arg...)  printk(format"\n", ## arg)
#define err(format, arg...) printk(format"\n", ## arg)

#define STRIPPATH(s)\
    (sizeof(s) > 2 && (s)[sizeof(s)-2] == '/' ? (s) + sizeof(s) - 1 : \
    sizeof(s) > 3 && (s)[sizeof(s)-3] == '/' ? (s) + sizeof(s) - 2 : \
    sizeof(s) > 4 && (s)[sizeof(s)-4] == '/' ? (s) + sizeof(s) - 3 : \
    sizeof(s) > 5 && (s)[sizeof(s)-5] == '/' ? (s) + sizeof(s) - 4 : \
    sizeof(s) > 6 && (s)[sizeof(s)-6] == '/' ? (s) + sizeof(s) - 5 : \
    sizeof(s) > 7 && (s)[sizeof(s)-7] == '/' ? (s) + sizeof(s) - 6 : \
    sizeof(s) > 8 && (s)[sizeof(s)-8] == '/' ? (s) + sizeof(s) - 7 : \
    sizeof(s) > 9 && (s)[sizeof(s)-9] == '/' ? (s) + sizeof(s) - 8 : \
    sizeof(s) > 10 && (s)[sizeof(s)-10] == '/' ? (s) + sizeof(s) - 9 : \
    sizeof(s) > 11 && (s)[sizeof(s)-11] == '/' ? (s) + sizeof(s) - 10 : (s))

#define hd_dbg(hd_dbg_bit, format, arg...) if (hd_dbg_bit & hd_dbg_mask) { printk("hdshm: DEBUG(%08x) %s:%d:%s: " format, hd_dbg_bit, STRIPPATH(__FILE__), __LINE__, __FUNCTION__, ##arg); };
#define hd_inf(format, arg...) printk("hdshm: INFO            %s: " format, __FUNCTION__, ##arg);
#define hd_err(format, arg...) printk("hdshm: ERROR           %s: " format, __FUNCTION__, ##arg);

#define dbg1(format, arg...) hd_dbg(HD_DEBUG_BIT_MODULE_TODO, format, ## arg)

#include "hdshm_gen.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0))
#define HDSHM_MAJOR 246
#else
// conflicts with: 246 watchdog -> use 146 by "best guess"
#define HDSHM_MAJOR 199
#endif

/* --------------------------------------------------------------------- */
// To be executed on Decypher
int hdshm_init_struct_hd(void)
{
	int n;
	hdshm_root_t *hdr;

	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "called\n")
	hdd.num_areas=0;
	
	hdd.start_phys=(void*)MAP_START;
	hdd.start=ioremap(MAP_START, MAP_SIZE);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0))
	hdd.start_nc=ioremap_nocache(MAP_START, MAP_SIZE);
#else
	hdd.start_nc=ioremap(MAP_START, MAP_SIZE);
#endif

	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "hdd.start(p)=%p hdd.start_nc(p)=%p\n", hdd.start, hdd.start_nc);

	hdr=hdd.start;
	memset(hdr, 0, sizeof(hdshm_root_t));

	hdr->magic=HDSHM_MAGIC;
	hdr->running=0;

	hdr->host_usage=0;
	hdr->hd_usage=0;
	hdd.event_count=0;

	for(n=0;n<ENTRIES_FOR_ROOT;n++) {
		memset(&hdr->entries[n],0,sizeof(hdshm_entry_t));
	}

	hdd.lock_tries=600;
	hdd.lock_timeout=0;
	atomic_set(&hdd.open_files,0);
	atomic_set(&hdr->table_lock,1);

	hdshm_allocate_mark(hdr, 0, 2, 1); // For root

	sema_init(&hdd.table_sem,1);
	hdd.hd_root=hdr;
	hdr->booted=1;
	
	return 0;
}
/* --------------------------------------------------------------------- */
// To be executed on Host CPU
int hdshm_init_struct_host(void)
{
	struct pci_dev *hd_pci;
	u64 addr64;
	int retval;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
	hd_pci=pci_get_device(0x1905,0x8100,NULL);
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
        hd_pci=pci_get_device_reverse(0x1905,0x8100,NULL);
#else
        hd_pci=pci_find_device_reverse(0x1905,0x8100,NULL);
#endif
#endif
        if (!hd_pci)
                return -1;

	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "found PCI device 1905:8100\n")

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0))
	// parts taken from https://stackoverflow.com/questions/35682482/how-to-access-pci-memory-from-linux-kernel-space-by-memory-mapping-kernel-3-14
	// enabling the device
	retval = pci_enable_device(hd_pci);
	if(retval) {
		hd_err("failed to enable PCI device 1905:8100\n");
		return -1;
	};
	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "enabled PCI device successfully 1905:8100\n")

	// take ownership of pci related regions
	pci_request_regions(hd_pci, "bar1");

	// checking if PCI-device reachable by checking that BAR1 is defined and memory mapped
	if( !(pci_resource_flags(hd_pci,1) & IORESOURCE_MEM) ) {
		hd_err("incorrect BAR1 configuration of PCI device 1905:8100\n");
		return(-ENODEV);
	};
	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "found BAR1 configuration for PCI device 1905:8100")
#endif

        hdd.hd_pci=hd_pci;
	hdd.bar1=(void*)pci_resource_start(hd_pci,1);
	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "found BAR1 address for PCI device 1905:8100 hdd.bar1=0x%lx\n", hdd.bar1)
        hdd.start_phys= hdd.bar1+0x02000000+MAP_START;
/*      hdd.start=ioremap((long)hdd.start_phys,MAP_SIZE);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0))
        hdd.start_nc=ioremap_nocache((long)hdd.start_phys,MAP_SIZE);
#else
        hdd.start_nc=ioremap((long)hdd.start_phys,MAP_SIZE);
#endif
	hdd.event_count=0; */
        addr64=(u64) hdd.start_phys & 0xFFFFFFFF;
        hdd.start=ioremap(addr64,MAP_SIZE);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0))
        hdd.start_nc=ioremap_nocache(addr64,MAP_SIZE);
#else
        hdd.start_nc=ioremap(addr64,MAP_SIZE);
#endif
        hdd.event_count=0;

        if (!hdd.start)
        {
#ifndef __x86_64
                printk("hdshm_init_struct: error, hdd.start is NULL while hdd.start_phys=%lx (%llx)\n", hdd.start_phys, addr64);
#else
                printk("hdshm_init_struct: error, hdd.start is NULL while hdd.start_phys=%p (%llx)\n", hdd.start_phys, addr64);
#endif
                return -1;
        }
         
	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "hdd.start_phys=0x%lx hdd.start=%p (0x%lx) hd.start_nc=%p (0x%lx) addr64=%llx\n", hdd.start_phys, hdd.start, hdd.start, hdd.start_nc, hdd.start_nc, addr64);
	sema_init(&hdd.table_sem,1);
        hdd.hd_root=hdd.start_nc;
        memset(hdd.start_nc, 0, MAP_SIZE);
        hdd.hd_root->booted=0;
	if (hdd.hd_root->booted != 0) {
		hd_err("hdd.hd_root->booted=%x (read from shared memory returned unexpected value != 0)\n", hdd.hd_root->booted);
		return -1;
	};
	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "hdd.hd_root->booted=%x (successful read from shared memory)\n", hdd.hd_root->booted);
        return 0;  
}
/* --------------------------------------------------------------------- */
/* The lock is shared between Host and Decypher 

   Locking is broken, as there's no easy way for a atomic operation
   over PCI. So we are just trying to reduce the possibility of races
   which are not very likely to appear anyway in a typical HDE setup
*/
int hdshm_lock_table(void)
{
	int tries=0;

//	if (hdd.lock_timeout)
//		return 1;
	down(&hdd.table_sem);
//	printk("hdshm: LOCK TABLE START\n");
	while(1) {
		if (atomic_dec_and_test(&hdd.hd_root->table_lock)) {
//			printk("hdshm: LOCK TABLE\n");
			up(&hdd.table_sem);
			return 0;
		}

		tries++;
		if (tries>hdd.lock_tries) {
//			hdd.lock_timeout=1;			
			up(&hdd.table_sem);
//			printk("hdshm: LOCK TABLE TIMEOUT\n");
			return 0; // Ignore Timeout
		}
/*		if (tries<200) {
			udelay(1);
		}
		else
*/
			msleep_interruptible(1);		
	}
}
/* --------------------------------------------------------------------- */
void hdshm_unlock_table(void)
{
	atomic_inc(&hdd.hd_root->table_lock);
//	up(&hdd.table_sem);
//	printk("UNLOCK TABLE %i\n",hdd.hd_root->table_lock);
}
/* --------------------------------------------------------------------- */
int hdshm_get_area(struct hdshm_file *bsf, unsigned long arg)
{
	hdshm_area_t bsa;
	hdshm_entry_t *bse;
	int ret=0;

	if (!hdd.hd_root->booted)
	        return -ENODEV;
	        
	if (copy_from_user(&bsa,(void*)arg,sizeof(hdshm_area_t)))
		return -EFAULT;

	dbg1("get_area %x",bsa.id);

	if (hdshm_lock_table())
		return -ETIMEDOUT;

	bse=hdshm_find_area(bsa.id, NULL);
	if (!bse) {
		hdshm_unlock_table();
		return -ENOENT;
	}
	{
//		int* z=(int*)bse;
//		printk("XX %08x %08x %08x %08x %08x\n",*z,*(z+1),*(z+2),*(z+3),*(z+4));
	}
	bsa.physical=bse->phys;
	bsa.length=bse->length;
	bsa.flags=bse->flags;
	bsa.usage=bse->usage;
	bsa.kernel_mem=(char*)hdd.start +((long)bse->phys-(long)hdd.start_phys);
	
	if (copy_to_user((void*)arg,&bsa,sizeof(hdshm_area_t)))
		ret=-EFAULT;
	hdshm_unlock_table();
	return ret;
}

/* --------------------------------------------------------------------- */
// Housekeeping for file-based mappings
int hdshm_unmap_area(struct hdshm_file *bsf, unsigned long arg)
{
	int id=(int)arg;
	int n;

	dbg("explicit unmap for %x",id);
	if (hdshm_lock_table())
		return -ETIMEDOUT;

	for(n=0;n<MAPPED_IDS_PER_FILE;n++) {
		int id=bsf->bsm[n].id;
		if (id!=0 && bsf->id==id) {
			hdshm_unmap_area_sub(id);			
			bsf->bsm[n].id=0;
			break;
		}
	}
	hdshm_unlock_table();
	return 0;
}
/* --------------------------------------------------------------------- */
int hdshm_set_id(struct hdshm_file *bsf, unsigned long arg)
{	
	hdshm_entry_t *bse;
	int id=(int)arg;
	dbg1("IOCTL_HDSHM_SET_ID %x",id);
	if (!id) {
		bsf->id=id;
		return 0;
	}

	if (hdshm_lock_table())
		return -ETIMEDOUT;

	bse=hdshm_find_area(id,NULL);
	hdshm_unlock_table();

	if (!bse) 
		return -ENOENT;
	
	bsf->id=id;
	return 0;
}
/* --------------------------------------------------------------------- */
int hdshm_create_area(struct hdshm_file *bsf, unsigned long arg)
{	
	hdshm_area_t bsa;
	hdshm_entry_t *bse;
	int found;

	if (!hdd.hd_root->booted)
	        return -ENODEV;

	if (copy_from_user(&bsa,(void*)arg,sizeof(bsa)))
		return -EFAULT;

	dbg1("create_area id=%x, flags %x",bsa.id,bsa.flags);

#ifdef CONFIG_MIPS
	// Only HD based maps allowed
	if (!(bsa.flags&HDSHM_MEM_HD) || !bsa.id)
		return -EINVAL;
#else
        /* Host can allocate host and HDE memory
           Host memory mus be explicitely flagged with HDSHM_MEM_DMA!
           DMA-flagged areas cannot be mmaped but must be accessed
           with IOCTL_HDSHM_DMA on HDE
        */
        if (!bsa.id || (!(bsa.flags&HDSHM_MEM_HD) && !(bsa.flags&HDSHM_MEM_DMA)) ||
                        ((bsa.flags&HDSHM_MEM_HD) && (bsa.flags&HDSHM_MEM_DMA)) )
                return EINVAL;
#endif

	if (hdshm_lock_table())
		return -ETIMEDOUT;

	bse=hdshm_find_area(bsa.id,NULL);
	if (bse) {
		hdshm_unlock_table();
		return -EEXIST;
	}
	
	found=hdshm_find_free_entry();

	if (found==-1) {
		hdshm_unlock_table();
		return -ENFILE;
	}
	
	bse=&(hdd.hd_root->entries[found]);

#ifdef CONFIG_MIPS
	bse->kernel=(int)hdshm_allocate_memory(&hdd, bsa.length, &bse->phys);
#else
	if (bsa.flags&HDSHM_MEM_HD)
        	bse->kernel=(int)hdshm_allocate_memory(&hdd, bsa.length, &bse->phys);
        else
#ifndef __x86_64
                bse->kernel=pci_alloc_consistent (hdd.hd_pci, bsa.length, &bse->phys);
#else
                bse->kernel=pci_alloc_consistent (hdd.hd_pci, (size_t) bsa.length, (dma_addr_t) &bse->phys);
#endif // __x86_64
#endif // CONFIG_MIPS

//	dbg("create_area id %x bse %p,  physical %p, kernel %p length %x", 
//	    bsa.id, bse, (void*)bse->phys, (void*)bse->kernel, bsa.length); // TODO -Wint-to-pointer-cast
	if (!bse->kernel) {
		hdshm_unlock_table();
		return -ENOMEM;
	}
	bse->usage=0;
	bse->length= bsa.length;
	bse->flags=bsa.flags;
	hdd.hd_root->ids[found]=bsa.id;
	hdd.num_areas++;
	hdshm_unlock_table();
	return 0;
}
/* --------------------------------------------------------------------- */
// Needs table_lock
int hdshm_destroy_area_sub(int id)
{
	int num_entry;
	hdshm_entry_t *bse;

	bse=hdshm_find_area(id,&num_entry);
	if (!bse) 
		return -ENOENT;

	if (bse->usage || bse->flags&HDSHM_MEM_HD) 
		return -EBUSY;
	
	dbg1("destroy_area bse=%p",bse);
	// dbg("free %x %p %p", bse->length, (void*)bse->kernel, (void*)bse->phys); // TODO -Wint-to-pointer-cast
	hdshm_free_memory(&hdd, bse->phys, bse->length);

	hdd.hd_root->ids[num_entry]=0;
	hdd.num_areas--;

	return 0;		
}
/* --------------------------------------------------------------------- */
int hdshm_destroy_area(struct hdshm_file *bsf, unsigned long arg)
{
	int id=(int)arg;
	int retval;
	dbg1("destroy_area id=%x",id);
	if (id==0)
		return -ENOENT;

	if (hdshm_lock_table())
		return -ETIMEDOUT;

	retval=hdshm_destroy_area_sub(id);

	hdshm_unlock_table();

	return retval;	
}
/* --------------------------------------------------------------------- */
int hdshm_get_status(struct hdshm_file *bsf, unsigned long arg)
{
	int flags=0;
	if (hdd.lock_timeout)
		flags|= HDSHM_STATUS_LOCK_TIMEOUT;
        if (!hdd.hd_root->booted)
                flags|= HDSHM_STATUS_NOT_BOOTED;
	return flags;
}
/* --------------------------------------------------------------------- */
int hdshm_reset(struct hdshm_file *bsf, unsigned long arg)
{
	int n;
	down(&hdd.table_sem);
	// Clear all HD mappings
	for(n=0;n<ENTRIES_FOR_ROOT;n++) {
		if (hdd.hd_root->ids[n]) {
			if (hdd.hd_root->entries[n].flags&HDSHM_MEM_HD)
				hdd.hd_root->ids[n]=0;	
		}
	}
	hdd.event_count++;
	up(&hdd.table_sem);
	
	hdd.lock_timeout=0;
	return 0;
}
/* --------------------------------------------------------------------- */
int hdshm_open (struct inode *inode, struct file *file)
{
	struct hdshm_file *bsf;
	int n;

	bsf=(struct hdshm_file *)kmalloc(sizeof(struct hdshm_file),GFP_KERNEL);
	if (!bsf)
		return -ENOMEM;
	
	bsf->id=0;
	for(n=0;n<MAPPED_IDS_PER_FILE;n++) {
		bsf->bsm[n].id=0;
	}

	file->private_data=bsf;
	atomic_inc(&hdd.open_files);
	return 0;
}
/* --------------------------------------------------------------------- */
int hdshm_release (struct inode *inode, struct file *file)
{
	struct hdshm_file *bsf=(struct hdshm_file*)file->private_data;
	int n,id;
	dbg1("called");
	if (hdshm_lock_table()) {
		goto release_error;
	}
	
	for(n=0;n<MAPPED_IDS_PER_FILE;n++) {
		id=bsf->bsm[n].id;
		if (id!=0) {
			dbg1("unmap_area_sub %x",id);
			hdshm_unmap_area_sub(id);
		}
	}
	hdshm_unlock_table();

release_error:
	kfree(bsf);
	atomic_dec(&hdd.open_files);
//	dbg("usage h %i b %i",hdd.hd_root->host_usage,hdd.hd_root->hd_usage);

	return 0;
}

/* --------------------------------------------------------------------- */
#if 0 //def CONFIG_MIPS
static int pci_inited=0;
extern void wis_pci_init(void);
extern int pci_dma_block(u32 pci_addr, u32 mem_addr, int size, int dir);
static int hdshm_dma(struct hdshm_file *bsf, unsigned long arg)
{
        hdshm_dma_t hdma;
        hdshm_entry_t *bse;
	int ret;
	
	if (!pci_inited) {
		printk("PCI DMA INIT\n");
		wis_pci_init();
		printk("PCI DMA INIT DONE\n");
		pci_inited=1;
	}
	if (copy_from_user(&hdma,(void*)arg,sizeof(hdshm_dma_t)))
		return -EFAULT;

	ret=pci_dma_block(hdma.remote_offset, (u32)hdma.local, hdma.len, hdma.direction);
	return ret;
#if 0		
	if (hdshm_lock_table())
		return -ETIMEDOUT;

	bse=hdshm_find_area(hdma.id, NULL);
	if (!bse) {
		hdshm_unlock_table();
		return -ENOENT;
	}
*/
//	printk("DMA INIT %i\n",wis_pci_init_remote());
//	ret=pci_dma_block(hdma.remote_offset, (u32)hdma.local, hdma.len, 2);
	return ret;
#endif
}
#endif
/* --------------------------------------------------------------------- */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static int hdshm_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
#else
static long hdshm_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
	int ret=0;
#else
	long ret=0;
#endif
	struct hdshm_file *bsf=(struct hdshm_file*)file->private_data;

	switch(cmd) {
	case IOCTL_HDSHM_GET_AREA:
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "call IOCTL_HDSHM_GET_AREA with arg=%lx\n", arg)
		ret=hdshm_get_area(bsf,arg);
		break;
	case IOCTL_HDSHM_SET_ID:
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "call IOCTL_HDSHM_SET_ID with arg=%lx\n", arg)
		ret=hdshm_set_id(bsf,arg);
		break;
	case IOCTL_HDSHM_CREATE_AREA:
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "call IOCTL_HDSHM_CREATE_AREA with arg=%lx\n", arg)
		ret=hdshm_create_area(bsf,arg);
		break;
	case IOCTL_HDSHM_DESTROY_AREA:
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "call IOCTL_HDSHM_DESTROY_AREA with arg=%lx\n", arg)
		ret=hdshm_destroy_area(bsf,arg);
		break;
	case IOCTL_HDSHM_GET_STATUS:
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "call IOCTL_HDSHM_GET_STATUS with arg=%lx\n", arg)
		ret=hdshm_get_status(bsf,arg);
		break;
	case IOCTL_HDSHM_RESET:
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "call IOCTL_HDSHM_RESET with arg=%lx\n", arg)
		ret=hdshm_reset(bsf,arg);
		break;
	case IOCTL_HDSHM_UNMAP_AREA:
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "call IOCTL_HDSHM_UNMAP_AREA with arg=%lx\n", arg)
		ret=hdshm_unmap_area(bsf,arg);
		break;
	case IOCTL_HDSHM_GET_ROOT:
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "call IOCTL_HDSHM_GET_ROOT\n")
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
		ret=(int)hdd.hd_root; // HACK FIXME
#else
		ret=(long)hdd.hd_root; // HACK FIXME
#endif
		break;
	case IOCTL_HDSHM_FLUSH:
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "call IOCTL_HDSHM_FLUSH\n")
		break;
	case IOCTL_HDSHM_PCIBAR1:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "call IOCTL_HDSHM_PCIBAR1 result hdd.bar1=%lx\n", hdd.bar1)
		return (int)hdd.bar1;  // HACK FIXME
#else
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "call IOCTL_HDSHM_PCIBAR1 result hdd.bar1=%lx\n", hdd.bar1)
		return (long)hdd.bar1;  // HACK FIXME
#endif
		break;
	case IOCTL_HDSHM_SHUTDOWN:
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "call IOCTL_HDSHM_SHUTDOWN with arg=%d\n", arg)
	        hdd.hd_root->booted=arg;
	        break;
#ifdef CONFIG_MIPS
        case IOCTL_HDSHM_DMA:
//                ret=hdshm_dma(bsf,arg);
                break;
        case IOCTL_HDSHM_CMDLINE:
                copy_to_user(arg,KSEG1+0x0001008,256);
                break;
                
#endif        
	default:
		hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "called with unsupported cmd=%d arg=%d\n", cmd, arg)
		break;
	}
	// hd_dbg(HD_DEBUG_BIT_MODULE_IOCTL, "usage h %i b %i", hdd.hd_root->host_usage,hdd.hd_root->hd_usage)
	return ret;
}
/* --------------------------------------------------------------------- */
int hdshm_mmap(struct file * file, struct vm_area_struct * vma)
{
	struct hdshm_file *bsf=(struct hdshm_file*)file->private_data;
	hdshm_entry_t *bse;
	int n,uncached=1;
#ifndef __x86_64
	dma_addr_t phys_start;
#else
	uint32_t phys_start;
#endif
	int length;

	hd_dbg(HD_DEBUG_BIT_MODULE_MMAP, "mmap id %x flags %x",bsf->id,(int)vma->vm_flags);

	if (!bsf->id) 
		return -EINVAL;		

	if (hdshm_lock_table())
		return -ETIMEDOUT;

	// Only one mapping per id per process
	for(n=0;n<MAPPED_IDS_PER_FILE;n++) {
		int id=bsf->bsm[n].id;
		if (id!=0 && bsf->id==id) {
			hdshm_unlock_table();
			return -EEXIST;
		}
	}
	hd_dbg(HD_DEBUG_BIT_MODULE_MMAP, "mmap find");
	bse=hdshm_find_area(bsf->id,NULL);
	
	if (!bse) {
		dbg("mmap not found");
		hdshm_unlock_table();
		return -ENOENT;		
	}
	hd_dbg(HD_DEBUG_BIT_MODULE_MMAP, "mmap bse=%p user %x phys=%x size %x offset %x",
	    bse,
	    (int)vma->vm_start,
	    (int)bse->phys,
	    (int)(vma->vm_end - vma->vm_start),
	    (int)vma->vm_pgoff);
#ifndef IS_HD
	if (bse->flags&HDSHM_MEM_CACHEABLE)
		uncached=0;
#endif

	if (uncached)
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	hd_dbg(HD_DEBUG_BIT_MODULE_MMAP, "uncached %x pgprot %x",uncached,(int)pgprot_val(vma->vm_page_prot));	

	vma->vm_private_data=bse;
#if   LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
       vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP | VM_SHARED;
#else
       vma->vm_flags |= VM_RESERVED|VM_SHARED;
#endif

	if (bse->flags&HDSHM_MEM_HD) {		
		hdd.hd_root->hd_usage++;		
	}
	else {	
		hdd.hd_root->host_usage++;       
	}

#ifdef IS_HD
	if (bse->flags&HDSHM_MEM_HD) {
#ifndef __x86_64
		phys_start = (dma_addr_t)(bse->phys&~4095);
#else
                phys_start = (uint32_t)(bse->phys&~4095);
#endif
	}
	else {
#ifndef __x86_64
		phys_start = (dma_addr_t)bse->kernel;
#else
                phys_start = (uint32_t)bse->kernel;
#endif
	}
#else
	if (bse->flags&HDSHM_MEM_HD) {
#ifndef __x86_64
		phys_start = (dma_addr_t)(hdd.start_phys-MAP_START)+(bse->phys&~4095); // FIXME -MAP!
#else
                phys_start = (uint32_t) ((hdd.start_phys-MAP_START)+(bse->phys&~4095)) & 0xFFFFFFFF; // FIXME -MAP!
#endif
	}
	else {
#ifndef __x86_64
		phys_start = (dma_addr_t)bse->kernel; // FIXME CHECK
#else
                phys_start = (uint32_t)bse->kernel; // FIXME CHECK
#endif
	}
#endif

	length=vma->vm_end-vma->vm_start; 

        hd_dbg(HD_DEBUG_BIT_MODULE_MMAP, "MAP PHYSICAL: %p, length %x\n",(void*)phys_start, length);
        remap_pfn_range(vma, vma->vm_start, phys_start>>PAGE_SHIFT, length, vma->vm_page_prot);
        
	bse->usage++;	
	
	for(n=0;n<MAPPED_IDS_PER_FILE;n++) {
		int id=bsf->bsm[n].id;
		if (!id) {
			bsf->bsm[n].id=bsf->id;
			break;
		}
	}
	
	hdshm_unlock_table();
	return 0;
}
/* --------------------------------------------------------------------- */
static struct file_operations hdshm_fops =
{
        owner:          THIS_MODULE,
	#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
	ioctl:          hdshm_ioctl,
	#else
	unlocked_ioctl: hdshm_ioctl,
	#endif
        open:           hdshm_open,
//      read:           hdshm_read,
	mmap:           hdshm_mmap,
        release:        hdshm_release,
};

/* --------------------------------------------------------------------- */
#ifdef HAS_HD_FB
#include "hd_fb.h"
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17))
static struct class *hdshm_class;
#endif

static int __init hdshm_init(void) 
{
        int retval;
	hd_inf("init start with hd_dbg_mask=%08x\n", hd_dbg_mask)
#ifdef IS_HD        
	retval=hdshm_init_struct_hd();
#else
        retval=hdshm_init_struct_host();
#endif	
	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "hdshm_init_struct_host retval=%d\n", retval)
	if (retval)
		return retval;

	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "register_chrdev call with major=%d\n", HDSHM_MAJOR)
        retval= register_chrdev(HDSHM_MAJOR, "hdshm", &hdshm_fops);
	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "register_chrdev retval=%d\n", retval)
	if (retval == -EBUSY) {
		hd_err("register_chrdev was not successful because major=%d already in use (check /proc/devices and HDSHM_MAJOR in source code)\n", HDSHM_MAJOR)
	};

	if (retval)
		return retval;

	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "register_chrdev successful\n")

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17))	
        devfs_mk_cdev(MKDEV(HDSHM_MAJOR, 0),
                        S_IFCHR | S_IRUSR | S_IWUSR,
                        "hdshm", 0);
#else
	hdshm_class = class_create(THIS_MODULE, "hdshm");
	// FIXME error checking
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
	class_device_create(hdshm_class, NULL, MKDEV(HDSHM_MAJOR, 0),
			    NULL, "hdshm");
#else
	device_create(hdshm_class, NULL, MKDEV(HDSHM_MAJOR, 0),
                           NULL, "hdshm");
#endif
#endif
#ifdef HAS_HD_FB
	hd_dbg(HD_DEBUG_BIT_MODULE_INIT, "call hdfb_init\n")
	retval = hdfb_init();
#endif
	if (retval == 0) {
		hd_inf("init finished successful\n");
	} else {
		hd_err("init finished with problem: retval=%d\n", retval)
	};
	return retval;
}

/* --------------------------------------------------------------------- */
                
static void __exit hdshm_exit(void)
{
	int n;
	hd_dbg(HD_DEBUG_BIT_MODULE_EXIT, "exit start\n")
#ifdef HAS_HD_FB
	hd_dbg(HD_DEBUG_BIT_MODULE_EXIT, "call hdfb_exit\n")
	hdfb_exit();
#endif
#if 1
	hd_dbg(HD_DEBUG_BIT_MODULE_EXIT, "call hdshm_destroy_area_sub\n")
	for(n=0;n<ENTRIES_FOR_ROOT;n++)
		if (hdd.hd_root->ids[n])
			hdshm_destroy_area_sub(hdd.hd_root->ids[n]);
#endif
	hdd.hd_root->running=0;
	hdd.hd_root->magic=0;

	hd_dbg(HD_DEBUG_BIT_MODULE_EXIT, "call iounmap for hdd.start\n")
	iounmap(hdd.start);
	hd_dbg(HD_DEBUG_BIT_MODULE_EXIT, "call iounmap for hdd.start_nc\n")
	iounmap(hdd.start_nc);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
	hd_dbg(HD_DEBUG_BIT_MODULE_EXIT, "call class_device_destroy\n")
	class_device_destroy(hdshm_class, MKDEV(HDSHM_MAJOR, 0));
	hd_dbg(HD_DEBUG_BIT_MODULE_EXIT, "call class_destroy\n")
	class_destroy(hdshm_class);
#else
	hd_dbg(HD_DEBUG_BIT_MODULE_EXIT, "call device_destroy\n")
	device_destroy(hdshm_class, MKDEV(HDSHM_MAJOR, 0));
	hd_dbg(HD_DEBUG_BIT_MODULE_EXIT, "call class_destroy\n")
	class_destroy(hdshm_class);
#endif
#endif
	
	hd_dbg(HD_DEBUG_BIT_MODULE_EXIT, "call unregister_chrdev for major=%d\n", HDSHM_MAJOR)
        unregister_chrdev(HDSHM_MAJOR,"hdshm");
	hd_dbg(HD_DEBUG_BIT_MODULE_EXIT, "exit finished\n")
}
                                
module_init(hdshm_init);
module_exit(hdshm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Georg Acher <acher at baycom dot de>");
MODULE_DESCRIPTION("Shared Memory Communication with HD Decoder");
