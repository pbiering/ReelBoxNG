/***************************************************************************
 * hdshm_user_structs.h --  Defines and structures for hdplayer
 *
 ***************************************************************************/

#ifndef HDSHM_USER_STRUCTS_H_INCLUDED
#define HDSHM_USER_STRUCTS_H_INCLUDED

/*--------------------------------------------------------------------------*/
//        Data structures contained in hd_data_t
/*--------------------------------------------------------------------------*/

typedef struct {
	int format;
	int channels;
	int samplerate;
	int bitrate;
	int bpf;
	int generation;
	int extra;
	int reserved08;
	int reserved09;
	int reserved10;
	int reserved11;
	int reserved12;
	int reserved13;
	int reserved14;
	int reserved15;
	int reserved16;
} hd_adec_config_t;

typedef struct {
	int pixelformat;
	int pic_width;
	int pic_height;
	int asp_width;
	int asp_height;
	int rate_num;
	int rate_den;
	int generation;
	int av_sync;
	int reserved10;
	int reserved11;
	int reserved12;
	int reserved13;
	int reserved14;
	int reserved15;
	int reserved16;
} hd_vdec_config_t;

struct hd_player_s
{
	// playback control
	unsigned int data_generation;     // increased to discard buffered audio/video packets.

	// Legacy start
	//start by Klaus
	unsigned int clock_high;
	unsigned int clock_low;
	unsigned int correction_base_high;
	unsigned int correction_base_low;
	int correction_diff;
	unsigned int current_frame;
	unsigned int video_delay;
	unsigned int pts; 
	int pts_shift;
	//End by Klaus

	int stc_valid;
	int stc;
	// Legacy end

	// Flow control
	int throttle;
	
	int pause;
	int trickmode;
	int trickspeed;

	int hde_stc;
	int pts_offset;
	int ac3_pts_shift;
	int decoder_frames;
	int hde_stc_base_low;
	int hde_stc_base_high;
	int hde_stc_offset_low;
	int hde_stc_offset_high;
	int hde_last_pts;
	int dummy0; 
//	dummy must be 64bit aligned? (even int count)
	int dummy[22]; // Fill to a total of 48 int
	hd_vdec_config_t vdec_cfg;
        hd_adec_config_t adec_cfg;
};


typedef struct hd_player_s hd_player_t;


#define HDOSD_MAX_CACHED_FONTS  2
#define HDOSD_MAX_IMAGES       128

/*--------------------------------------------------------------------------*/
#define HD_VM_NORM_PAL 0
#define HD_VM_NORM_NTSC 1

#define HD_VM_SCALE_F2S 0   // Fill to screen
#define HD_VM_SCALE_F2A 1   // Fill to aspect
#define HD_VM_SCALE_C2F 2   // Crop to fill
#define HD_VM_SCALE_DBD 3   // Dot by dot
#define HD_VM_SCALE_VID 0x0f

#define HD_VM_SCALE_FB_FIT 0x0
#define HD_VM_SCALE_FB_DBD 0x1

// Output mode for HDMI
#define HD_VM_OUTD_HDMI  0
#define HD_VM_OUTD_DVI   1   // No sound included
#define HD_VM_OUTD_OFF   255

// Output mode for digital audio
#define HD_VM_OUTDA_PCM  0
#define HD_VM_OUTDA_AC3  1  // no AC3 decoding, simple passthrough

// Output mode for FS453
#define HD_VM_OUTA_YUV 0
#define HD_VM_OUTA_RGB 1
#define HD_VM_OUTA_YC  2
#define HD_VM_OUTA_AUTO 3        // RGB/YUV depending on connection
#define HD_VM_OUTA_OFF 255

// Output flags
#define HD_VM_OUTA_PORT_AUTO 0
#define HD_VM_OUTA_PORT_SCART (1<<8)
#define HD_VM_OUTA_PORT_MDIN  (2<<8)
#define HD_VM_OUTA_PORT_SCART_V1 (3<<8) // Switch voltage always 12V
#define HD_VM_OUTA_PORT_SCART_V0 (4<<8) // Switch voltage always off

// Deinterlacer
#define HD_VM_DEINT_OFF  0
#define HD_VM_DEINT_AUTO 1
#define HD_VM_DEINT_M1   2
#define HD_VM_DEINT_M2   3

// Auto format Mode
#define HD_VM_AUTO_OFF 0
#define HD_VM_AUTO_ON  1

typedef struct {
	int w;
	int h;
	int scale;
	int overscan;
	int automatic;
	int dummy[10];
	int changed;
} hd_aspect_t;

typedef struct {
	int width;
	int height;
	int interlace;
	int framerate;
	int norm;           // PAL/NTSC
	int outputd;        // digital video output
	int outputa;        // analog video output
	int outputda;       // digital audio output
	int changed;
	int deinterlacer;
	int auto_format;
	int current_w;
	int current_h;
	int dummy[4];
} hd_video_mode_t;

typedef struct {
	int w,h;              // Media pixel size
	int asp_x, asp_y;     // Media pixel aspect
//	int dummy[12];
} hd_player_status_t;

typedef struct {
	int changed;
	int audiomix;        // 1: Mix analog HDE and MB sound
	int dummy[14];
} hd_hw_control_t;

typedef struct {
	int changed;
	int dummy[15];      // Reserve
} hd_hw_status_t;

typedef struct {
	int changed;
	int volume;
	int volume1;     // for player 1
	int dummy[29];   // more for downmix control
} hd_audio_control_t;

typedef struct {
	int changed;
	int brightness;       // 0-255, default 128
	int contrast;         // 0-255, default 128
	int gamma;            // 0-255 (0-2.55), default 100
	int color_conv[10];   // reserved
	int sharpness;        // 0-255, default 100
	int x,y;              // absolute position
	int w,h;
	int zorder;
	int alpha;            // 0: transparent, 255: full
	int mode;
	int ws,hs;            // scaled to: ws=0->Fill, -1->DBD-centered
	int enable;
	int osd_scale;        
	int vx,vy,vw,vh;      // Virtual dimensions
	int dummy[3];
} hd_plane_config_t;

#define HD_PLAYER_MODE_LIVE 0
#define HD_PLAYER_MODE_ERF  1
#define HD_PLAYER_MODE_RPC  2

struct hd_data_s
{
	// Status
	int hdc_running; // hdctrld running
	int hdp_running; // hdplayer running
	int event;

	// commands
	int hdp_enable;
	int hdp_terminate;    // read by hdplayer
	int hd_shutdown; // 1=Power down decoder

	// video mode setting
	hd_video_mode_t video_mode;

	// aspect setting
	hd_aspect_t aspect; // readonly for hdplayer
	
	// FIXME detected EDID values

    hd_aspect_t source_aspect; // read by reelbox-plugin

	// Players
	hd_player_t player[2];

	// Cached fonts of the osd. Nonzero entry: Font is currently cached.
	int osd_cached_fonts[HDOSD_MAX_CACHED_FONTS];
	// Cached images of the osd. Nonzero entry: Font is currently cached.
	int osd_cached_images[HDOSD_MAX_IMAGES];

	int osd_flush_count;
	int osd_hidden;
        // Set to 1 if VDR shouldn't do anything with the OSD
        int osd_dont_touch;

	hd_player_status_t player_status[2];
	hd_hw_control_t    hw_control;
	hd_hw_status_t     hw_status;
	hd_audio_control_t audio_control;

        int dummy;  // was active_player
	
	hd_plane_config_t plane[4];  // 0: main, 1: alternate, 2: fb
	int active_player[2];
};

typedef struct hd_data_s hd_data_t;

/*--------------------------------------------------------------------------*/

struct hd_packet_header_s
{
    unsigned int magic;
    unsigned int seq_nr;
    unsigned int type;
    unsigned int packet_size;
};

typedef struct hd_packet_header_s hd_packet_header_t;

struct hd_packet_es_data_s
{
    hd_packet_header_t header;
    unsigned int generation;
    unsigned int stream_id;
    unsigned int substream_id;
    unsigned int timestamp;
    unsigned int still_frame;
};

typedef struct hd_packet_es_data_s hd_packet_es_data_t;

struct hd_packet_pes_data_s
{
    hd_packet_header_t header;
    unsigned int generation;
    unsigned int av;
    unsigned int dummy[2];    
};

typedef struct hd_packet_pes_data_s hd_packet_pes_data_t;

struct hd_packet_ts_data_s
{
    hd_packet_header_t header;
    unsigned int generation;
    unsigned int vpid;
    unsigned int apid;
};

typedef struct hd_packet_ts_data_s hd_packet_ts_data_t;

//Start by Klaus
struct hd_packet_af_data_s
{
    hd_packet_header_t header;
    unsigned long long frame_nr;
    unsigned int sampleRate;
    unsigned int pts;
};

typedef struct hd_packet_af_data_s hd_packet_af_data_t;
//End by Klaus
//Start by dl
struct hd_packet_erf_init_s
{
    hd_packet_header_t header;
};
typedef struct hd_packet_erf_init_s hd_packet_erf_init_t;

struct hd_packet_erf_play_s
{
    hd_packet_header_t header;
    char file_name[1024];
    char lang[64];
    unsigned long flags;
    unsigned long res;
};
typedef struct hd_packet_erf_play_s hd_packet_erf_play_t;

struct hd_packet_erf_cmd_s
{
    hd_packet_header_t header;
    unsigned int cmd;
    int speed;
    long long offset;
    unsigned long id;
    unsigned long res;
};
typedef struct hd_packet_erf_cmd_s hd_packet_erf_cmd_t;

#define ERF_CMD_NONE  0
#define ERF_CMD_REL   1
#define ERF_CMD_ABS   2
#define ERF_CMD_PROG  3
#define ERF_CMD_VIDEO 4
#define ERF_CMD_AUDIO 5

struct hd_packet_erf_done_s
{
    hd_packet_header_t header;
};
typedef struct hd_packet_erf_done_s hd_packet_erf_done_t;


struct hd_packet_erf_info_s
{
    hd_packet_header_t header;
    int state;
    int speed;
    long error;
    long flags;
    long long pos;
    long long size;
};
typedef struct hd_packet_erf_info_s hd_packet_erf_info_t;
#define ERF_STATE_UNKNOWN     0
#define ERF_STATE_INIT        1
#define ERF_STATE_STOPPED     2
#define ERF_STATE_LOADING     3
#define ERF_SEEK_PENDING      4
#define ERF_SEEK_READY        5
#define ERF_STATE_PLAYING     6
#define ERF_STATE_FINISHED    7
#define ERF_STATE_DEMUX_ERROR 8
#define ERF_STATE_DEC_ERROR   9
#define ERF_STATE_DONE        10

struct hd_packet_erf_req_s
{
    hd_packet_header_t header;
    int mode;
    int size;
    long long offset;
};
typedef struct hd_packet_erf_req_s hd_packet_erf_req_t;
#define ERF_REQ_SIZE 1
#define ERF_REQ_DATA 2

struct hd_packet_erf_sinfo_data_s
{
    unsigned long id;
    unsigned long format;
    unsigned long flags;
    unsigned long res;
    char descr[32];
};
typedef struct hd_packet_erf_sinfo_data_s hd_packet_erf_sinfo_data_t;
#define ERF_SINFO_SET     (1<<0)
#define ERF_SINFO_PLAYING (1<<1)
#define ERF_SINFO_ENDED   (1<<2)

#define ERF_MAX_PROGS 32
#define ERF_MAX_VIDEO 4
#define ERF_MAX_AUDIO 28
struct hd_packet_erf_sinfo_s
{
    hd_packet_header_t header;
    hd_packet_erf_sinfo_data_t prog[ERF_MAX_PROGS];
    hd_packet_erf_sinfo_data_t video[ERF_MAX_VIDEO];
    hd_packet_erf_sinfo_data_t audio[ERF_MAX_AUDIO];
};
typedef struct hd_packet_erf_sinfo_s hd_packet_erf_sinfo_t;

//End by dl

struct hd_packet_rpc_init_s
{
    hd_packet_header_t header;
};
typedef struct hd_packet_rpc_init_s hd_packet_rpc_init_t;

struct hd_packet_rpc_done_s
{
    hd_packet_header_t header;
};
typedef struct hd_packet_rpc_done_s hd_packet_rpc_done_t;

#define HDE_FLAG_FRAME_START_PTS  0x8000000000000000ULL
#define HDE_FLAG_FRAME_END_PTS    0x4000000000000000ULL
#define HDE_FLAG_STILLFRAME_PTS   0x2000000000000000ULL
#define HDE_UNSPEC_PTS            0x0001000000000000ULL
#define HDE_VALID_PTS             0x00000001FFFFFFFFULL

#define RPC_CMD_ACFG     1
#define RPC_CMD_ADATA    2
#define RPC_CMD_VCFG     3
#define RPC_CMD_VDATA    4

struct hd_packet_rpc_cmd_s
{
    hd_packet_header_t header;
    unsigned int cmd;
    unsigned int data_size;
    unsigned int param1;
    unsigned int param2;
    int reserved5;
    int reserved6;
    int reserved7;
    int reserved8;
};
typedef struct hd_packet_rpc_cmd_s hd_packet_rpc_cmd_t;

struct hd_packet_trickmode_s
{
    hd_packet_header_t header;
    unsigned int trickspeed;
    unsigned int i_frames_only;
};

typedef struct hd_packet_trickmode_s hd_packet_trickmode_t;

struct hd_packet_off_s
{
    hd_packet_header_t header;
};

typedef struct hd_packet_off_s hd_packet_off_t;

struct hd_packet_clear_s
{
    hd_packet_header_t header;
};

typedef struct hd_packet_clear_s hd_packet_clear_t;

struct hd_packet_freeze_s
{
    hd_packet_header_t header;
};

typedef struct hd_packet_freeze_s hd_packet_freeze_t;

//Start by Klaus
#define HD_MAX_DGRAM_SIZE 16384
//End by Klaus
//#define HD_MAX_DGRAM_SIZE 4096

#define HD_PACKET_ES_DATA    1
#define HD_PACKET_TS_DATA    2
#define HD_PACKET_TRICKMODE  3
#define HD_PACKET_OFF        4
#define HD_PACKET_OSD        5
//Start by Klaus
#define HD_PACKET_AF_DATA 6
#define HD_PACKET_AC3_DATA 7
//End by Klaus
//Start by dl
#define HD_PACKET_ERF_INIT  8
#define HD_PACKET_ERF_PLAY  9
#define HD_PACKET_ERF_CMD   10
#define HD_PACKET_ERF_DONE  11
#define HD_PACKET_ERF_INFO  12
#define HD_PACKET_ERF_REQ   13
#define HD_PACKET_ERF_SINFO 14
//End by dl
#define HD_PACKET_CLEAR 15
#define HD_PACKET_FREEZE 16

#define HD_PACKET_RPC_INIT  20
#define HD_PACKET_RPC_CMD   21
#define HD_PACKET_RPC_DONE  22

#define HD_PACKET_PES_DATA 32

#define HD_PACKET_MAGIC 0x5245454C


// ID for area containing the hd_data_t structure

#define HDID_HDA 0x01

#define HDCH_XINE_OSD 9
#define HDCH_XINE_OSD_SIZE 0x40000

#define HDCH_OSD 10
#define HDCH_OSD_SIZE 0x8000

#define HDCH_STREAM1 12
#define HDCH_STREAM1_SIZE 0x40000

//Start by dl
#define HDCH_STREAM1_INFO 8
#define HDCH_STREAM1_INFO_SIZE 0x10000
//End by dl

#define HDCH_STREAM2 13
#define HDCH_STREAM2_SIZE 0x10000

#define HDCH_STREAM2_INFO 14
#define HDCH_STREAM2_INFO_SIZE 0x1000

/*--------------------------------------------------------------------------*/
// OSD Commands
/*--------------------------------------------------------------------------*/

#define HDCMD_OSD_PALETTE       1
#define HDCMD_OSD_DRAW8         2
#define HDCMD_OSD_CLEAR         3
#define HDCMD_OSD_DRAW_RECT     4
#define HDCMD_OSD_DRAW_ELLIPSE  5
#define HDCMD_OSD_CACHE_FONT    6
#define HDCMD_OSD_DRAW_TEXT     7
#define HDCMD_OSD_CACHE_IMAGE   8
#define HDCMD_OSD_DRAW_IMAGE    9
#define HDCMD_OSD_FLUSH        10
#define HDCMD_OSD_DRAW_RECT2   11
#define HDCMD_OSD_DRAW8_OVERLAY 12
#define HDCMD_OSD_DRAW_CROP_IMAGE 13
#define HDCMD_OSD_SAVE_REGION 14
#define HDCMD_OSD_RESTORE_REGION 15

#define HDCMD_OSD_XINE_START   20
#define HDCMD_OSD_XINE_TILE    21
#define HDCMD_OSD_XINE_END     22

typedef struct {
        int cmd;
        int count;
        unsigned int palette[];
} hdcmd_osd_palette_t;

typedef struct {
        int cmd;
        int x;
        int y;
        int w;
        int h;
    int scale;
        unsigned int data[];
} hdcmd_osd_draw8_t;

typedef struct {
    int cmd;
    int x;
    int y;
    int x1;
    int y1;
} hdcmd_osd_clear_t;

typedef struct {
    int          cmd;
    unsigned int l;
    unsigned int t;
    unsigned int r;
    unsigned int b;
    unsigned int color;
} hdcmd_osd_draw_rect;

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
} hdcmd_osd_draw_rect2;

typedef struct {
    int          cmd;
    unsigned int l;
    unsigned int t;
    unsigned int r;
    unsigned int b;
    unsigned int color;
    int          quadrants;
} hdcmd_osd_draw_ellipse;

typedef struct {
    int          cmd;
    int          fontIndex;
} hdcmd_osd_cache_font;

typedef struct {
    int          cmd;
    int          x, y;
    unsigned int colorFg, colorBg;
    int          fontIndex;
    int          width;
    int          height;
    int          alignment;
} hdcmd_osd_draw_text;

typedef struct {
    int          cmd;
    unsigned int image_id;
    int          width, height;
} hdcmd_osd_cache_image;

typedef struct {
    int          cmd;
    unsigned int image_id;
    int          x, y;
    int          blend;
    int          hor_repeat;
    int          vert_repeat;
} hdcmd_osd_draw_image;

typedef struct {
    int          cmd;
    unsigned int image_id;
    int          x, y;
    int          x0, y0;
    int          x1, y1;
    int          blend;
} hdcmd_osd_draw_crop_image;

typedef struct {
    int    cmd;
    int    flush_count;
} hdcmd_osd_flush;

typedef struct {
    int cmd;
}  hdcmd_osd_xine_start;

#define OVL_PALETTE_SIZE 256
typedef struct {
    unsigned short len;
    unsigned short color;
} hdcmd_osd_rle_elem;

typedef struct {
    hdcmd_osd_rle_elem *rle;           /* rle code buffer                  */
    int                 data_size;     /* useful for deciding realloc      */
    int                 num_rle;       /* number of active rle codes       */
    int                 x;             /* x start of subpicture area       */
    int                 y;             /* y start of subpicture area       */
    int                 width;         /* width of subpicture area         */
    int                 height;        /* height of subpicture area        */
    unsigned long       color[OVL_PALETTE_SIZE];  /* color lookup table     */
    unsigned char       trans[OVL_PALETTE_SIZE];  /* mixer key table        */
    int                 rgb_clut;      /* true if clut was converted to rgb */
    int                 hili_top; /* define a highlight area with different colors */
    int                 hili_bottom;
    int                 hili_left;
    int                 hili_right;
    unsigned long       hili_color[OVL_PALETTE_SIZE];
    unsigned char       hili_trans[OVL_PALETTE_SIZE];
    int                 hili_rgb_clut; /* true if clut was converted to rgb */
    int                 unscaled;      /* true if it should be blended unscaled */
} hdcmd_osd_overlay;

typedef struct {
    hdcmd_osd_overlay overlay;
    int img_width;
    int img_height;
    int offset_x;
    int offset_y;
    int dst_width;
    int dst_height;
}  hdcmd_osd_tile;

typedef struct {
    int cmd;
    hdcmd_osd_tile tile;
}  hdcmd_osd_xine_tile;

typedef struct {
    int cmd;
}  hdcmd_osd_xine_end;

/*--------------------------------------------------------------------------*/
//   Communication to controlling process
/*--------------------------------------------------------------------------*/



#endif // def HDSHM_USER_STRUCTS_H_INCLUDED
