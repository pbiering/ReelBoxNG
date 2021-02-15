/***************************************************************************
 * bspshm_user_structs.h --  Defines and structures for bspd/BSP
 *
 ***************************************************************************/

#ifndef _BSHSHM_USER_STRUCTS_H
#define _BSHSHM_USER_STRUCTS_H

/*--------------------------------------------------------------------------*/
//        Data structures contained in bspd_data_t
/*--------------------------------------------------------------------------*/

// Status from BSP for each player (coming from (MPEG)-stream decoder)
typedef struct {
	int frameWidth;
	int frameHeight;
	int imgAspectRatioX; 
	int imgAspectRatioY; 
	unsigned int fpsc;               // Coded fps (0...8)
	int interlace;
	int tff;                // top field first
	int changed;
} bsp_video_attributes_t;


// DRC Video status from BSP
typedef struct {
	int active;
	int width,height;
	int changed;
} bsp_drc_status_t;

/*--------------------------------------------------------------------------*/

// Commands to BSP
typedef struct {
    // video stream channel
	int channel;

    // playback control
    unsigned int idle;                // nonzero if bsp player is idle
    unsigned int av_sync;             // av sync phase countdown.
    unsigned int stc;                 // mpeg system time counter
    unsigned int stc_valid;           // indicates wether the stc field is valid
    unsigned int still_picture_id;    // last still picture id displayed by the player
    unsigned int stream_generation;   // increased to clear buffers
    unsigned int stream_pos;          // stream position in bytes up to wich the player requests video data
	unsigned int trickspeed;          // number of times to display each trickframe, = 0 if normal playback

    // Video plane settings
    int pip;
	int aspect[2];  // auto scaling
	int use_xywh;   // manual scaling
	int x,y,w,h;    // position, width, height
	int changed;
} bsp_video_player_t;

typedef struct {
	
} bsp_video_player_status_t;

// Commands to BSP
typedef struct {
	int enable;
	int running;
	int framebuffers;
	int width;
	int x;
	int y;
	int height;
	int format;        // 0: RGBA8888, 1: RGBA4444
} bsp_osd_attributes_t;

// Commands to BSP
typedef struct {
	int matrix[3][4];        // Color conversion matrix
	int matrix_changed;
	int gamma_table[3][64];  // Lookuptable	
	int gamma_changed;
	int hor_filter_luma[8][5];
	int hor_filter_chroma[8][5];
	int ver_filter_luma[8][4];
	int ver_filter_chroma[8][4];
	int filter_dither;
	int filter_changed;
} bsp_drc_signal_attributes_t;

// Commands to bspd (FS453 cannot handle BCC in all modes)
typedef struct {
	int values[6];      // brightness, contrast, colour, sharpness, gamma, flicker
	                    //  0-1000, 0-1000, 0-1000, 0-255, 0-255, 0-16
	int changed;
} bsp_picture_attributes_t;


/*--------------------------------------------------------------------------*/

#define BSP_HW_VM_OFF    0
#define BSP_HW_VM_PAL    1
#define BSP_HW_VM_CUSTOM 2

// Commands to BSP
typedef struct {
	int set;            // -> BSP
	int mode;           // 0: Off, 1: PAL, 2: Custom
	int settings[32];
} bsp_hw_video_mode_t;

/*--------------------------------------------------------------------------*/

// Main Video Modes
#define BSP_VMM_SD   0	// SDTV only
#define BSP_VMM_SDHD 1	// SDTV+HDTV
#define BSP_VMM_PC   2	// PC/VGA output

// Sub video modes (output mode)
#define BSP_VSM_YUV  0
#define BSP_VSM_RGB  1   
#define BSP_VSM_CVBS 2   // allowed only for SD
#define BSP_VSM_YC   3   // allowed only for SD

// Option Display Type
#define BSP_VM_DISPLAY_43      0
#define BSP_VM_DISPLAY_169     1

// Option AspectAdaption (SD/SDHC/PC)
#define BSP_VM_ASPECT_WSS      0  // always fit screen, WSS only for SD
#define BSP_VM_ASPECT_AUTO     1  // Adapt automatically to display type
#define BSP_VM_ASPECT_MANUAL   2  // switch manually depending on display type

// Option Framerate (only PC)
#define BSP_VM_FRATE_AUTO1     0   // 75 for 50, 60 for 60
#define BSP_VM_FRATE_AUTO2     1   // 50 for 50, 60 for 60  NON-VESA
#define BSP_VM_FRATE_AUTO3     2   // 67 for 50, 60 for 60  NON-VESA
#define BSP_VM_FRATE_50        3   // NON-VESA
#define BSP_VM_FRATE_60        4
#define BSP_VM_FRATE_67        5   // NON-VESA
#define BSP_VM_FRATE_75        6


// Option Norm (only SD)
#define BSP_VM_NORM_PAL50_60   0   // autoswitch between PAL-50 and PAL-60
#define BSP_VM_NORM_PAL50      1   // PAL with 50Hz only
#define BSP_VM_NORM_NTSC       2   // NTSC with 60Hz only

// Option Resolution (only PC)
#define BSP_VM_RESOLUTION_640x480   0
#define BSP_VM_RESOLUTION_800x600   1
#define BSP_VM_RESOLUTION_1024x768  2
#define BSP_VM_RESOLUTION_1280x720  3
//...

// Option Resolution (only SDHD)
#define BSP_VM_RESOLUTION_AUTO      0
#define BSP_VM_RESOLUTION_1080      1
#define BSP_VM_RESOLUTION_720       2

// Option Flags
#define BSP_VM_FLAGS_SCART_MASK    3
#define BSP_VM_FLAGS_SCART_SHIFT   0
#define BSP_VM_FLAGS_SCART_ON      0
#define BSP_VM_FLAGS_SCART_STARTUP 1
#define BSP_VM_FLAGS_SCART_OFF     2

#define BSP_VM_FLAGS_DEINT_MASK    (4+8)
#define BSP_VM_FLAGS_DEINT_SHIFT   2
#define BSP_VM_FLAGS_DEINT_OFF     0
#define BSP_VM_FLAGS_DEINT_AUTO    4
#define BSP_VM_FLAGS_DEINT_ON      8

typedef struct {
	int last_changed;  // == .changed when major mode switch done
	int main_mode;   // SDTV, SD+HD, PC
	int sub_mode;    
	int display_type; 
	int aspect;
	int framerate;
	int norm;
	int resolution;
	int flags;
	int reserved[3];
	int changed;
} bsp_video_mode_t;

/*--------------------------------------------------------------------------*/
//        bspd (+BSP status)                         
/*--------------------------------------------------------------------------*/

#define BSPOSD_MAX_CACHED_FONTS   2
#define BSPOSD_MAX_IMAGES       128

typedef struct {
	// Status
	int event;
	int status;           // 1: bsp running
	int generation;       // "run" generation of BSP15 executable

	// External commands
	int bsp_enable;      // 0: don't run BSP15
	int bsp_auto_restart;// 1; Automatically restart BSP15 after kill
	int bsp_kill;        // 1: Kill running BSP (automatically cleared)
	int bspd_shutdown;   // 1: Shutdown BSP *and* bspd

	// BSP Watchdog
	int bsp_watchdog;    // incremented by BSP15

	// Display mode
	int display_type;

	// Old Player Ports
	int BspPortFocusSync;
	
	// 
	int need_focus_sync;
	int demo_running;
	int demo_stop;
	int demo_start;

	// Picture settings (set by vdr)
	bsp_picture_attributes_t picture;
	
	// Requested video mode settings (set by vdr)
	bsp_video_mode_t video_mode;

	// HW video mode settings (read by BSP15)
	bsp_hw_video_mode_t hw_video_mode;

	// Video attributes (Aspect Ratio)
	bsp_video_attributes_t video_attributes[2];

	// Video Players
	bsp_video_player_t video_player[2];

	// OSD settings
	bsp_osd_attributes_t osd;

	// DRC signal conversion settings
	bsp_drc_signal_attributes_t drc;

	// DRC video status
	bsp_drc_status_t drc_status;

    // Cached fonts of the osd. Nonzero entry: Font is currently cached.
    int osd_cached_fonts[BSPOSD_MAX_CACHED_FONTS];

    // Cached images of the osd. Nonzero entry: Font is currently cached.
    int osd_cached_images[BSPOSD_MAX_IMAGES];

    int osd_flush_count;
	int dummy;
} bspd_data_t;


// ID for bspd, containing the bspd_data_t structure
#define BSPID_BSPD 0x01

/*--------------------------------------------------------------------------*/
// Management for BSP created channels
/*--------------------------------------------------------------------------*/

typedef struct {
	int max_id;
	int ids[];
} bsp_channel_list_t;

#define BSPID_CHANNELS 0x10

/*--------------------------------------------------------------------------*/
//  Framebuffer (Linear mapped RGBA)
/*--------------------------------------------------------------------------*/

#define BSPID_FRAMEBUFFER_RGBA_0 0x100
#define BSPID_FRAMEBUFFER_RGBA_1 0x101


/*--------------------------------------------------------------------------*/
// Framebuffer Video
/*--------------------------------------------------------------------------*/
#define BSPID_FRAMEBUFFER_VF1_0 0x200
#define BSPID_FRAMEBUFFER_VF1_1 0x201
#define BSPID_FRAMEBUFFER_VF1_2 0x202
#define BSPID_FRAMEBUFFER_VF1_3 0x203

#define BSPID_FRAMEBUFFER_VF2_0 0x208
#define BSPID_FRAMEBUFFER_VF2_1 0x209
#define BSPID_FRAMEBUFFER_VF2_2 0x20a
#define BSPID_FRAMEBUFFER_VF2_3 0x20b

/*--------------------------------------------------------------------------*/
// Channel definitions
/*--------------------------------------------------------------------------*/

// For VDR

#define BSPCH_OSD 10
#define BSPCH_CMD 11
#define BSPCH_PES1 12
#define BSPCH_PES2 13
#define BSPCH_PES3 14

/*--------------------------------------------------------------------------*/
// OSD Commands
/*--------------------------------------------------------------------------*/

#define BSPCMD_OSD_PALETTE       1
#define BSPCMD_OSD_DRAW8         2
#define BSPCMD_OSD_CLEAR         3
#define BSPCMD_OSD_DRAW_RECT     4
#define BSPCMD_OSD_DRAW_ELLIPSE  5
#define BSPCMD_OSD_CACHE_FONT    6
#define BSPCMD_OSD_DRAW_TEXT     7
#define BSPCMD_OSD_CACHE_IMAGE   8
#define BSPCMD_OSD_DRAW_IMAGE    9
#define BSPCMD_OSD_FLUSH        10
#define BSPCMD_OSD_DRAW_RECT2   11

typedef struct {
	int cmd;
	int count;
	unsigned int palette[];
} bspcmd_osd_palette_t;

typedef struct {
	int cmd;
	int x;
	int y;
	int w;
	int h;
    int scale;
	unsigned int data[];
} bspcmd_osd_draw8_t;

typedef struct {
    int cmd;
    int x;
    int y;
    int x1;
    int y1;
} bspcmd_osd_clear_t;

typedef struct {
    int          cmd;
    unsigned int l;
    unsigned int t;
    unsigned int r;
    unsigned int b;
    unsigned int color;
} bspcmd_osd_draw_rect;

typedef struct {
    int          cmd;
    unsigned int l;
    unsigned int t;
    unsigned int r;
    unsigned int b;
    unsigned int color;
    int alphaGradH;
    int alphaGradV;
    int alphaGradStepH;
    int alphaGradStepV;
} bspcmd_osd_draw_rect2;

typedef struct {
    int          cmd;
    unsigned int l;
    unsigned int t;
    unsigned int r;
    unsigned int b;
    unsigned int color;
    int          quadrants;
} bspcmd_osd_draw_ellipse;

typedef struct {
    int          cmd;
    int          fontIndex;
} bspcmd_osd_cache_font;

typedef struct {
    int          cmd;
    int          x, y;
    unsigned int colorFg, colorBg;
    int          fontIndex;
    int          width;
    int          height;
    int          alignment;    
} bspcmd_osd_draw_text;

typedef struct {
    int          cmd;
    unsigned int image_id;
    int          width, height;
} bspcmd_osd_cache_image;

typedef struct {
    int          cmd;
    unsigned int image_id;
    int          x, y;
    int          blend;
    int          hor_repeat;
    int          vert_repeat;
} bspcmd_osd_draw_image;

typedef struct {
    int    cmd;
    int    flush_count;
} bspcmd_osd_flush;

#endif
