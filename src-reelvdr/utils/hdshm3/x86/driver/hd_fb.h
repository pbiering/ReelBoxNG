/*****************************************************************************/
/*
	hdshm addon for framebuffer access to HDExtension (DeCypher) 
*/
#define FB_DEVICE_NAME "hde_fb"

//#define hd_fb_dbg(format, arg...) printk(FB_DEVICE_NAME": "format"\n", ## arg)
#define hd_fb_dbg(format, arg...)
#define hd_fb_err(format, arg...) printk(FB_DEVICE_NAME": "format"\n", ## arg)

#define MAX_PALETTE_NUM_ENTRIES         256
#define GO_BASE    0x2000000 
#define GO_FB_BASE 0x1600000
#define GO_FB_SIZE 0x300000
 
#include <linux/fb.h>
#include <linux/moduleparam.h>

#include "../../src/include/hdshm_user_structs.h"

static int has_fb = 0;
module_param(has_fb, int, 0);

struct hde_fb {
	struct fb_info fb_info; 
	int user_count;
	hd_data_t *hd_data;
	int oldw,oldh,oldm;
	int blank_w,blank_h,blank_m;
}; // hde_fb
static struct hde_fb hdefb;
static unsigned int pseudo_palette[MAX_PALETTE_NUM_ENTRIES];

/*****************************************************************************/
/*
	Tool functions
*/
int init_fb_info(void) {
	hd_fb_dbg("init_fb_info");
	memset(&hdefb, 0, sizeof(struct hde_fb));
	hdefb.oldw = 720;
	hdefb.oldh = 576;
	hdefb.oldm = 0x41;

	hdefb.fb_info.node  = -1;
	hdefb.fb_info.flags = FBINFO_FLAG_DEFAULT;

	hdefb.fb_info.var.xres = 720; /* visible resolution		*/
	hdefb.fb_info.var.yres = 576;
	hdefb.fb_info.var.xres_virtual = hdefb.fb_info.var.xres; /* virtual resolution		*/
	hdefb.fb_info.var.yres_virtual = hdefb.fb_info.var.yres;

	hdefb.fb_info.var.bits_per_pixel = 32;		/* guess what			*/
	hdefb.fb_info.var.red.length = 8;
	hdefb.fb_info.var.green.length = 8;
	hdefb.fb_info.var.blue.length = 8;
	hdefb.fb_info.var.transp.length = 8;	
	hdefb.fb_info.var.red.offset = 16;
	hdefb.fb_info.var.green.offset = 8;
	hdefb.fb_info.var.blue.offset = 0;
	hdefb.fb_info.var.transp.offset = 24;

	hdefb.fb_info.var.activate = FB_ACTIVATE_NOW;			/* see FB_ACTIVATE_*		*/
	hdefb.fb_info.var.height = -1;			/* height of picture in mm    */
	hdefb.fb_info.var.width = -1;			/* width of picture in mm     */

	hdefb.fb_info.var.vmode = FB_VMODE_NONINTERLACED;			/* see FB_VMODE_*		*/

//	struct fb_fix_screeninfo fix;	/* Current fix */
	strcpy(hdefb.fb_info.fix.id, FB_DEVICE_NAME); /* identification string */
	hdefb.fb_info.fix.smem_start = (unsigned long)hdd.bar1+GO_BASE+GO_FB_BASE;	/* Start of frame buffer mem (physical address) */
	hdefb.fb_info.fix.smem_len = GO_FB_SIZE;			/* Length of frame buffer mem */
	hdefb.fb_info.fix.type = FB_TYPE_PACKED_PIXELS;

	hdefb.fb_info.fix.visual = FB_VISUAL_TRUECOLOR;			/* see FB_VISUAL_*		*/ 

	hdefb.fb_info.fix.line_length = hdefb.fb_info.var.xres_virtual * hdefb.fb_info.var.bits_per_pixel / 8;		/* length of a line in bytes    */
	hdefb.fb_info.fix.accel = FB_ACCEL_NONE;	/* Indicate to driver which	specific chip/card we have	*/
//	hdefb.fb_info.monspecs = monspecs;	/* Current Monitor specs */
//	struct fb_chroma chroma;
//	struct fb_videomode *modedb;	/* mode database */
//	struct work_struct queue;	/* Framebuffer event queue */
//	struct fb_pixmap pixmap;	/* Image hardware mapper */
//	hdefb.fb_info.pixmap.scan_align = 4;	
//	struct fb_pixmap sprite;	/* Cursor hardware mapper */
//	struct fb_cmap cmap;		/* Current cmap */
//	struct list_head modelist;      /* mode list */
//	struct fb_videomode *mode;	/* current mode */
// 	struct device *device;		/* This is the parent */
//	struct device *dev;		/* This is this fb device */
//	int class_flag;                    /* private sysfs flags */
	hdefb.fb_info.screen_base = ioremap(hdefb.fb_info.fix.smem_start, hdefb.fb_info.fix.smem_len); /* Virtual address */
//	unsigned long screen_size;	/* Amount of ioremapped VRAM or 0 */ 
	hdefb.fb_info.pseudo_palette = pseudo_palette;		/* Fake palette of 16 colors */ 
//	u32 state;			/* Hardware state i.e suspend */
//	void *fbcon_par;                /* fbcon use-only private area */
	/* From here on everything is device dependent */
//	void *par;	 
	return 0;
} // init_fb_info

int get_hd_data (struct fb_info *info) {
	hdshm_entry_t *bse;
	struct hde_fb* fb = (struct hde_fb*)info;
	if(!info)
	    return -EINVAL;  
	hd_fb_dbg("get_hd_data %p", fb->hd_data);
	if (fb->hd_data) 
	    return 0;
	if (hdshm_lock_table()) {
	    hd_fb_err("dev_open: lock_tables timed out");
	    return -ETIMEDOUT;
	} // if
	bse=hdshm_find_area(HDID_HDA, NULL);
	if (!bse) {
	    hd_fb_err("dev_open: area not found");
	    hdshm_unlock_table();
	    return -ENOENT;
	} // if
	fb->hd_data = (hd_data_t *)ioremap((unsigned long)hdd.bar1+GO_BASE+bse->phys, bse->length); /* Virtual address */
	hd_fb_dbg("dev_open %dx%d 0x%x 0x%x", fb->hd_data->video_mode.width, fb->hd_data->video_mode.height, fb->hd_data->aspect.scale, bse->phys);
	hdshm_unlock_table();
	return 0;
} // get_hd_data

//---------------------------------------------------------------------------
int update_fbinfo(struct fb_var_screeninfo *var, int w, int h, int xm)
{		
	int bytesperpixel=(xm&0xf0)>>4;
	
	hd_fb_dbg("update_fbinfo %i %i %x\n", w,h,xm);
	
	var->xres = w;
	var->yres = h;
//		var->xres_virtual = var->xres;
//		var->yres_virtual = var->yres;
		
	if (bytesperpixel==4) {
		var->bits_per_pixel = 32;
		
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		
		var->transp.length = 8;	
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		
		var->transp.offset = 24;
	}
	else if (bytesperpixel==2) {
		var->bits_per_pixel = 16;
		
		var->red.length = 5;
		var->red.offset = 11;
		
		var->green.length = 6;
		var->green.offset = 5;
		
		var->blue.length = 5;
		var->blue.offset = 0;			
		
		var->transp.length = 0;			
		var->transp.offset = 0;
	}
	
	return 0;
}
//---------------------------------------------------------------------------
int set_pic_mode(struct fb_info *info, int w, int h, int xm) {
	struct hde_fb* fb = (struct hde_fb*)info;
 
	if(!info)
	    return -EINVAL;  

	hd_fb_dbg("set_pic_mode %ix%i, %x", w, h, xm);

	if (!fb->hd_data) 
	    return -EINVAL;

	fb->hd_data->plane[2].w=w;
	fb->hd_data->plane[2].h=h;
	fb->hd_data->plane[2].x=0;
	fb->hd_data->plane[2].y=0;

	fb->hd_data->plane[2].ws=-1; // Center
	fb->hd_data->plane[2].hs=0;
	fb->hd_data->plane[2].mode=xm;
	
	fb->hd_data->plane[2].vx=0;  // Virtual screens currently not used
	fb->hd_data->plane[2].vy=0;
	fb->hd_data->plane[2].vw=w;
	fb->hd_data->plane[2].vh=h;

	if (w==720 && h==576) { // Default OSD
		hd_fb_dbg("Default OSD");
		fb->hd_data->osd_dont_touch=0;
		fb->hd_data->plane[2].alpha=255;
		fb->hd_data->plane[2].mode=0x41;
	}
	else {
		fb->hd_data->osd_dont_touch=1;
		fb->hd_data->plane[2].alpha=160;
	}
	fb->hd_data->plane[2].enable=1;
	fb->hd_data->plane[2].changed++;

	// Avoid unnecessary switches
	// Clear only for default OSD
	if (fb->oldw!=w || fb->oldh!=h || fb->oldm!= xm) {
		if (fb->fb_info.screen_base && w==720 && h==576)
			memset(fb->fb_info.screen_base, 0, w*h*((xm>>4)&6));
	}
	fb->oldw=w;
	fb->oldh=h;
	fb->oldm=xm;
	return 0;
} // set_pic_mode

/*****************************************************************************/
/*
	Module functions
*/
int dev_open (struct fb_info *info, int user) {
	struct hde_fb* fb = (struct hde_fb*)info;  
	hd_fb_dbg("---------- dev_open %d", user);
	if (user) {
		int ret = get_hd_data(info);
		if(ret)
			return ret;
		fb->user_count++;
	} // if
	return 0;
} // dev_open

//---------------------------------------------------------------------------

int dev_release (struct fb_info *info, int user) {
	hd_fb_dbg("---------- dev_release %d", user);
	if (user) {
		struct hde_fb* fb = (struct hde_fb*)info;  
		if (fb->user_count > 0) {
			fb->user_count--;
			if (!fb->user_count) {
				set_pic_mode(info, 720, 576, 0x41);
			}
		} // if
	} // if
	return 0;
} // dev_release

//---------------------------------------------------------------------------

int dev_check_var(struct fb_var_screeninfo *var, struct fb_info *info) {
	int w=0,h=0,xm=0;

	hd_fb_dbg("dev_check_var");
	if(!var)
		return -EINVAL;

	if (var->bits_per_pixel==16)
		xm=0x25;
	else if (var->bits_per_pixel==24 || var->bits_per_pixel==32)
		xm=0x40;
	else 
		return -1;
	
	w=var->xres;
	h=var->yres;

	update_fbinfo(var, w,h,xm);

	hd_fb_dbg("check_var w %d h %d vw %d vh %d xo %d yo %d bps %d ww %d hh %d", 
	       var->xres, var->yres, var->xres_virtual, var->yres_virtual,
	       var->xoffset, var->yoffset, var->bits_per_pixel, var->height, var->width);
	return 0;
} // dev_check_var

//---------------------------------------------------------------------------

int dev_set_par (struct fb_info *info) {
	struct fb_var_screeninfo *var = &info->var;
	int w,h,xm;

	w=0;h=0;xm=0;

	hd_fb_dbg("dev_set_par");
	if(!var)
		return EINVAL;

	hd_fb_dbg("set_var w %d h %d vw %d vh %d xo %d yo %d bps %d ww %d hh %d", 
	       var->xres, var->yres, var->xres_virtual, var->yres_virtual, 
	       var->xoffset, var->yoffset, var->bits_per_pixel, var->height, var->width);


	w=var->xres;
	h=var->yres;
	if (var->bits_per_pixel==16)
		xm=0x25;
	else
		xm=0x40;

	set_pic_mode(info, w, h, xm);
	hdefb.fb_info.fix.line_length = var->xres_virtual * var->bits_per_pixel / 8;
	return 0;
} // dev_set_par

//---------------------------------------------------------------------------

int dev_setcolreg (unsigned regno, unsigned red, unsigned green, unsigned blue, unsigned transp, struct fb_info *info) {
	hd_fb_dbg("dev_setcolreg");
	return 0;
} // dev_setcolreg

int dev_setcmap (struct fb_cmap *cmap, struct fb_info *info) {
//	hd_fb_dbg("dev_setcmap");
	return 0;
} // dev_setcmap

int dev_blank (int blank, struct fb_info *info) {
#if 0
	hd_fb_dbg("dev_blank %i",blank);
	if (blank) {
		hdefb.blank_w=hdefb.oldw;
		hdefb.blank_h=hdefb.oldh;
		hdefb.blank_m=hdefb.oldm;
		set_pic_mode(info,720,576,0x41);
	}
	else {
		set_pic_mode(info,hdefb.blank_w,hdefb.blank_h,hdefb.blank_m);
	}
#endif	
	return 0;
} // dev_blank

int dev_pan_display (struct fb_var_screeninfo *var, struct fb_info *info) {
//	hd_fb_dbg("dev_pan_display");
//	hd_fb_dbg("pan  w %d h %d vw %d vh xo %d yo %d bps %d %d ww %d hh %d", 
//	       var->xres, var->yres, var->xres_virtual, var->yres_virtual, var->xoffset, var->yoffset, var->bits_per_pixel, var->height, var->width);
	return 0;
} // dev_pan_display
 
void dev_fillrect (struct fb_info *info, const struct fb_fillrect *rect) {
	hd_fb_dbg("dev_fillrect");
	return cfb_fillrect(info, rect);
} // dev_fillrect

void dev_copyarea (struct fb_info *info, const struct fb_copyarea *region) {
	hd_fb_dbg("dev_copyarea");
	return cfb_copyarea(info, region);
} // dev_copyarea

void dev_imageblit (struct fb_info *info, const struct fb_image *image) {
	//hd_fb_dbg("dev_imageblit");
	return cfb_imageblit(info, image);
} // dev_imageblit

int dev_cursor (struct fb_info *info, struct fb_cursor *cursor) {
	hd_fb_dbg("dev_cursor");
	return 0;
} // dev_cursor

void dev_rotate(struct fb_info *info, int angle) {
	hd_fb_dbg("dev_rotate");
} // dev_rotate

int dev_sync(struct fb_info *info) {
	hd_fb_dbg("dev_sync");
	return 0;
} // dev_sync

int dev_ioctl (struct fb_info *info, unsigned int cmd, unsigned long arg) {
	int ret=0;
	hd_fb_dbg("dev_ioctl %x", cmd);
	return ret;
} // dev_ioctl

int dev_compat_ioctl (struct fb_info *info, unsigned cmd, unsigned long arg) {
	hd_fb_dbg("dev_compat_ioctl %x", cmd);
	return 0;
} // dev_compat_ioctl

int dev_mmap(struct fb_info *info, struct vm_area_struct *vma) {
	size_t size;
	hd_fb_dbg("dev_mmap");
	if (!vma || !info) {
		hd_fb_err("mmap: invalid args %p %p", info, vma);
		return -EINVAL;
	} // if
	if (!(vma->vm_flags & VM_SHARED)) {
		hd_fb_err("mmap: only support VM_SHARED mapping");
		return -EINVAL; 
	} // if
 	if (vma->vm_pgoff != 0) {
		hd_fb_err("mmap: only support offset=0 (%ld)", vma->vm_pgoff);
		return -EINVAL; 
	} // if
	size = vma->vm_end - vma->vm_start;
	if (size > info->fix.smem_len) {
		hd_fb_err("mmap: requested size to big 0x%x (0x%x)", size, info->fix.smem_len);
		return -EINVAL; 
	} // if
	hd_fb_dbg("remap_pfn_range> 0x%lx, 0x%lx, %d (0x%x)", vma->vm_start, info->fix.smem_start, size, size);
	if(remap_pfn_range(vma, vma->vm_start, info->fix.smem_start >> PAGE_SHIFT, size, vma->vm_page_prot)) //pgprot_noncached(vma->vm_page_prot)))
		return -EAGAIN;
	hd_fb_dbg("dev_mmap SUCCESS");
	return 0;
} // dev_mmap

void dev_save_state (struct fb_info *info) {
	hd_fb_dbg("dev_save_state");
} // dev_save_state

void dev_restore_state (struct fb_info *info) {
	hd_fb_dbg("dev_restore_state");
} // dev_restore_state 

/*****************************************************************************/
/*
	Module interface
*/
//xxxfb_poll
static struct fb_ops hde_fb_fops = {
	.owner            = THIS_MODULE,
	.fb_open          = dev_open, //
	.fb_release       = dev_release, //
	.fb_check_var     = dev_check_var, //
	.fb_set_par       = dev_set_par, //
//	.fb_setcolreg     = dev_setcolreg, //
	.fb_setcmap       = dev_setcmap, //! 
	.fb_blank         = dev_blank,
	.fb_pan_display   = dev_pan_display, //!
	.fb_fillrect      = dev_fillrect, //!
	.fb_copyarea      = dev_copyarea, //!
	.fb_imageblit     = dev_imageblit, //!
//	.fb_cursor        = dev_cursor,
//	.fb_rotate        = dev_rotate,
//	.fb_sync          = dev_sync,
// 	.fb_ioctl         = dev_ioctl,
//	.fb_compat_ioctl  = dev_compat_ioctl,
	.fb_mmap          = dev_mmap,
//	.fb_save_state    = dev_save_state, //
//	.fb_restore_state = dev_restore_state, // 
}; // hde_fb_fops

int hdfb_init(void) {
	int ret;
	hd_fb_dbg("init %d", has_fb);
	if (!has_fb)
		return 0;
	ret = init_fb_info();
	if(ret < 0) {
		hd_fb_err("Unable to init fb_info: %d", ret);
        return ret;
	} // if
	hdefb.fb_info.fbops = &hde_fb_fops;
	fb_alloc_cmap(&hdefb.fb_info.cmap, MAX_PALETTE_NUM_ENTRIES, 0);
	
	ret = register_framebuffer(&hdefb.fb_info);
	if(ret < 0) {
		hd_fb_err("Unable to register framebuffer: %d", ret);
        return -EINVAL;
	} // if
	return ret;
} // hdfb_init

void hdfb_exit(void) {
	hd_fb_dbg("exit");
	unregister_framebuffer(&hdefb.fb_info);
} // hdfb_exit
