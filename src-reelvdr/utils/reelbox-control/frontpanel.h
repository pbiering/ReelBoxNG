/*
 * Frontpanel communication for Reelbox PVR1100
 *
 * (c) Georg Acher, acher (at) baycom dot de
 *     BayCom GmbH, http://www.baycom.de
 *
 * #include <gpl-header.h>
 *
 * $Id: vdr_control.c,v 1.1 2004/09/28 00:19:09 acher Exp $
 *
 * Defined User Keys:
 * User1 IR_TEXT
 * User2 IR_PIP
 * Audio: Shift Less
 * User3 Shift Stop (eject)
 * User4 Shift Help (epg actual
 * User5 IR_DVD
 * User6 IR_DVB
 * User7 IR_PVR
 * User8 IR_REEL
*/

#ifndef _REEL_FRONTPANEL_H
#define _REEL_FRONTPANEL_H

#include "rb_control.h"
#include <termios.h>

// remote key codes
#define REEL_IR_NONE 	0x1000  // no key pressed
#define REEL_IR_GT	0x5500  // > at numberblock
#define REEL_IR_LT	0x5600  // < at numberblock
#define REEL_IR_0 	0x0000
#define REEL_IR_1 	0x0100
#define REEL_IR_2 	0x0200
#define REEL_IR_3 	0x0300
#define REEL_IR_4 	0x0400
#define REEL_IR_5 	0x0500
#define REEL_IR_6 	0x0600
#define REEL_IR_7 	0x0700
#define REEL_IR_8 	0x0800
#define REEL_IR_9 	0x0900
#define REEL_IR_VOLMINUS 0x0a00
#define REEL_IR_VOLPLUS 0x0b00
#define REEL_IR_MUTE	0x0c00
#define REEL_IR_PROGMINUS 0x0d00
#define REEL_IR_PROGPLUS 0x0e00
#define REEL_IR_STANDBY 0x0f00
#define REEL_IR_MENU	0x2000
#define REEL_IR_UP	0x2100
#define REEL_IR_DOWN	0x2200
#define REEL_IR_LEFT	0x2300
#define REEL_IR_RIGHT	0x2400
#define REEL_IR_OK	0x2500
#define REEL_IR_EXIT	0x2600
// Player Keys
#define REEL_IR_REV	0x3000
#define REEL_IR_PLAY	0x3100
#define REEL_IR_PAUSE	0x3200
#define REEL_IR_FWD	0x3300
#define REEL_IR_STOP 	0x3400
#define REEL_IR_REC	0x3500
#define REEL_IR_EJECT	0x3600 // new on remote keyboard
// Color Keys
#define REEL_IR_RED	0x4000
#define REEL_IR_GREEN	0x4100
#define REEL_IR_YELLOW	0x4200
#define REEL_IR_BLUE	0x4300
// New keys
#define REEL_IR_GUIDE		0x4600     //
#define REEL_IR_MULTIMEDIA	0x4700     //
#define REEL_IR_AUDIO		0x4800     // | Aspect
#define REEL_IR_SEARCH		0x4900     //
#define REEL_IR_FAVORITES       0x4a00     // Heart-logo
#define REEL_IR_SUBTITLE	0x4b00     // | Subtitles
#define REEL_IR_RADIO	        0x4c00     // | Fav+
// Device Keys
#define REEL_IR_DVB     0x5000
#define REEL_IR_TV      0x5100
#define REEL_IR_DVD     0x5200
#define REEL_IR_PVR     0x5300
#define REEL_IR_REEL    0x5400
// Object Keys
#define REEL_IR_SETUP 	0x5700
#define REEL_IR_PIP	0x5800
#define REEL_IR_TEXT	0x5900
#define REEL_IR_HELP	0x5a00
#define REEL_IR_TIMER	0x5b00
#define REEL_IR_INTERNET  0x5c00
#define REEL_IR_NIXXX   0x5d00
// Special keys for other remote controls
#define REEL_IR_POWER_OFF 0x6000  // Go into DS
#define REEL_IR_POWER_ON  0x6100  // Wakup from DS or Standby
#define REEL_IR_GO_STANDBY  0x6200  // Go only into Standby
// remote key codes end


// IR device codes
#define REEL_IR_VENDOR 	0x4428
#define REEL_IR_STATE_KEYBOARD  0x0001   // Frontpanel keys
#define REEL_IR_STATE_RUWIDO    0x0002   // Ruwido remote control
#define REEL_IR_STATE_DVB	0x1d1f
#define REEL_IR_STATE_DVD	0x1b3f
#define REEL_IR_STATE_TV	0x1c2f
#define REEL_IR_STATE_PVR	0x1a4f
#define REEL_IR_STATE_REEL	0x195f

// Frontpanel Keys
#define REEL_KBD_BT0	0x0008 // Standby     NC: up
#define REEL_KBD_BT1	0x0002 // Red         NC: down
#define REEL_KBD_BT2	0x0200 // Green
#define REEL_KBD_BT3	0x0020 // Yellow      NC: Power
#define REEL_KBD_BT4	0x0400 // Blue
#define REEL_KBD_BT5	0x0001 // Eject       NC: left
#define REEL_KBD_BT6	0x0004 // Play        NC: right
#define REEL_KBD_BT7	0x0080 // Pause
#define REEL_KBD_BT8	0x0800 // Stop
#define REEL_KBD_BT9	0x0040 // Rev
#define REEL_KBD_BT10	0x0010 // Fwd         NC: OK
#define REEL_KBD_BT11	0x0100 // Rec
#define REEL_KBD_BT12	0x1000 // Menu        (NC2)

//New keys keyboard
#define REEL_IR_BACKSPACE	0x8800
#define REEL_IR_TABULATOR	0x8900
#define REEL_IR_ENTER		0x8a00

#define REEL_IR_RING		0x8f00 // °
#define REEL_IR_CAP_UML_A	0x9000 // Ä
#define REEL_IR_CAP_UML_O	0x9100 // Ö
#define REEL_IR_CAP_UML_U	0x9200 // Ü
#define REEL_IR_SML_UML_A	0x9300 // ä
#define REEL_IR_SML_UML_O	0x9400 // ö
#define REEL_IR_SML_UML_U	0x9500 // ü
#define REEL_IR_MICRO		0x9600 // µ
#define REEL_IR_EURO		0x9700 // €
#define REEL_IR_PARAGRAPH	0x9800 // %
#define REEL_IR_SZ		0x9900 // ß
#define REEL_IR_APOSTROPHE	0x9a00 // '
#define REEL_IR_HIGH_1		0x9b00 // ¹
#define REEL_IR_HIGH_2		0x9c00 // ²
#define REEL_IR_HIGH_3		0x9d00 // ³
#define REEL_IR_QUARTER		0x9e00 // ¼
#define REEL_IR_HALF		0x9f00 // ½
#define REEL_IR_SPACE		0xa000 // ' '

#define REEL_IR_EXCLAMATION	0xa100 // !
#define REEL_IR_DOUBLEQUOTE	0xa200 // "
#define REEL_IR_HASH		0xa300
#define REEL_IR_DOLLAR		0xa400
#define REEL_IR_PERCENT		0xa500
#define REEL_IR_AMPERSAND	0xa600 // &
#define REEL_IR_ACUTE		0xa700 // á
#define REEL_IR_LEFT_PARENTHE	0xa800 // (
#define REEL_IR_RIGHT_PARENTHE	0xa900 // )
#define REEL_IR_ASTERISK	0xaa00 // *
#define REEL_IR_PLUS		0xab00
#define REEL_IR_COMMA		0xac00
#define REEL_IR_MINUS		0xad00
#define REEL_IR_POINT		0xae00
#define REEL_IR_SLASH		0xaf00
// b0-b9: reserved 0-9
#define REEL_IR_COLON		0xba00
#define REEL_IR_SEMICOLON	0xbb00
#define REEL_IR_LEFT_ANGLE_BR	0xbc00 // <
#define REEL_IR_EQUAL		0xbd00 // =
#define REEL_IR_RIGHT_ANGLE_BR	0xbe00 // >
#define REEL_IR_QUESTIONMARK	0xbf00

#define REEL_IR_AT		0xc000
#define REEL_IR_CAP_A		0xc100
#define REEL_IR_CAP_B		0xc200
#define REEL_IR_CAP_C		0xc300
#define REEL_IR_CAP_D		0xc400
#define REEL_IR_CAP_E		0xc500
#define REEL_IR_CAP_F		0xc600
#define REEL_IR_CAP_G		0xc700
#define REEL_IR_CAP_H		0xc800
#define REEL_IR_CAP_I		0xc900
#define REEL_IR_CAP_J		0xca00
#define REEL_IR_CAP_K		0xcb00
#define REEL_IR_CAP_L		0xcc00
#define REEL_IR_CAP_M		0xcd00
#define REEL_IR_CAP_N		0xce00
#define REEL_IR_CAP_O		0xcf00
#define REEL_IR_CAP_P		0xd000
#define REEL_IR_CAP_Q		0xd100
#define REEL_IR_CAP_R		0xd200
#define REEL_IR_CAP_S		0xd300
#define REEL_IR_CAP_T		0xd400
#define REEL_IR_CAP_U		0xd500
#define REEL_IR_CAP_V		0xd600
#define REEL_IR_CAP_W		0xd700
#define REEL_IR_CAP_X		0xd800
#define REEL_IR_CAP_Y		0xd900
#define REEL_IR_CAP_Z		0xda00
#define REEL_IR_LEFT_SQUARE_BRAC	0xdb00 // [
#define REEL_IR_BACKSLASH		0xdc00 /* \ */
#define REEL_IR_RIGHT_SQUARE_BRAC	0xdd00 // ]
#define REEL_IR_CIRCUMFLEX	0xde00 // ^
#define REEL_IR_UNDERSCORE	0xdf00 // _

#define REEL_IR_GRAVE		0xe000 // `
#define REEL_IR_SML_A           0xe100
#define REEL_IR_SML_B           0xe200
#define REEL_IR_SML_C           0xe300
#define REEL_IR_SML_D           0xe400
#define REEL_IR_SML_E           0xe500
#define REEL_IR_SML_F           0xe600
#define REEL_IR_SML_G           0xe700
#define REEL_IR_SML_H           0xe800
#define REEL_IR_SML_I           0xe900
#define REEL_IR_SML_J           0xea00
#define REEL_IR_SML_K           0xeb00
#define REEL_IR_SML_L           0xec00
#define REEL_IR_SML_M           0xed00
#define REEL_IR_SML_N           0xee00
#define REEL_IR_SML_O           0xef00
#define REEL_IR_SML_P           0xf000
#define REEL_IR_SML_Q           0xf100
#define REEL_IR_SML_R           0xf200
#define REEL_IR_SML_S           0xf300
#define REEL_IR_SML_T           0xf400
#define REEL_IR_SML_U           0xf500
#define REEL_IR_SML_V           0xf600
#define REEL_IR_SML_W           0xf700
#define REEL_IR_SML_X           0xf800
#define REEL_IR_SML_Y           0xf900
#define REEL_IR_SML_Z           0xfa00
#define REEL_IR_LEFT_BRACE	0xfb00 // {
#define REEL_IR_PIPE		0xfc00 // |
#define REEL_IR_RIGHT_BRACE	0xfd00 // }
#define REEL_IR_TILDE		0xfe00 // ~
#define REEL_IR_DELETE		0xff00 // DEL

int u_sleep(long long nsec);

extern const char *fp_device;

/*-------------------------------------------------------------------------*/
// Base class dummy, does nothing
/*-------------------------------------------------------------------------*/

#define CAP_ICE "/dev/.fpctl.ice"
#define CAP_NOICE "/dev/.fpctl.noice"

#define CAP_AVR "/dev/.fpctl.avr"
#define CAP_NOAVR "/dev/.fpctl.noavr"

class frontpanel {
public:
//	frontpanel(const char *device) {};
//	~frontpanel(void) {};
	
	static int fp_available(const char *device, int force=0) {return 0;};
	virtual const char* fp_get_name(void) {return "DUMMY";};

	virtual int fp_get_fd(void){return -1;};
	virtual void fp_start_handler(void){};
	virtual void fp_noop(void) {};
	virtual void fp_get_version(void) {};
	virtual void fp_enable_messages(int n) {};
	virtual void fp_display_brightness(int n){};
	virtual void fp_display_contrast(int n){};
	virtual void fp_clear_display(void){};
	virtual void fp_write_display(unsigned char *data, int datalen){};
	virtual void fp_display_data(char *data, int l){};
	virtual void fp_set_leds(int blink, int state){};
	virtual void fp_set_clock(void){};
	virtual void fp_set_wakeup(time_t t){};
	virtual void fp_set_displaymode(int m){};
	virtual void fp_set_switchcpu(int timeout, int mode){};
	virtual void fp_get_clock(void){};
	virtual void fp_set_clock_adjust(int v1, int v2){};
	virtual void fp_do_display(void){};
	virtual int  fp_read_msg(unsigned char *msg, unsigned long long *ts){return 0;};
	
	virtual void fp_show_pic(unsigned char **LCD){};
	virtual void fp_get_wakeup(void){};
	virtual void fp_get_key(void){};
	virtual void fp_set_led_brightness(int v){};
	virtual void fp_get_flags(void){};
	virtual void fp_power_control(int v){};
	virtual void fp_eeprom_flash(int a, int d ){};
	virtual void fp_send_cec(unsigned char *buf){};
};

/*-------------------------------------------------------------------------*/
// Frontpanel base commands
/*-------------------------------------------------------------------------*/

class frontpanel_rs232 : public frontpanel {
public:
	frontpanel_rs232(const char* device);
	~frontpanel_rs232(void);

	static int fp_available(const char *device, int force=0){return 0;};
	static void * fp_serial_thread (void* p);
	virtual const char* fp_get_name(void);

	virtual int fp_get_fd(void);
	virtual void fp_start_handler(void);
	virtual void fp_noop(void);
	virtual void fp_get_version(void);
	virtual void fp_enable_messages(int n);
 	virtual void fp_display_brightness(int n);
	virtual void fp_display_contrast(int n);
	virtual void fp_clear_display(void);
	virtual void fp_write_display(unsigned char *data, int datalen);
	virtual void fp_display_data(char *data, int l);
	virtual void fp_set_leds(int blink, int state);
	virtual void fp_set_clock(void);
	virtual void fp_set_wakeup(time_t t);
	virtual void fp_set_displaymode(int m);
	virtual void fp_set_switchcpu(int timeout, int mode);
	virtual void fp_get_clock(void);
	virtual void fp_set_clock_adjust(int v1, int v2);
	virtual int  fp_read_msg(unsigned char *msg, unsigned long long *ts);
	
	virtual void fp_show_pic(unsigned char **LCD);
	virtual void fp_get_wakeup(void);
	virtual void fp_get_key(void);
	virtual void fp_set_led_brightness(int v);
	virtual void fp_get_flags(void);
	virtual void fp_power_control(int v);
	virtual void fp_eeprom_flash(int a, int d );

protected:		
	int fp_write(char* buf, int len);
	int fp_open_serial(const char* fp_device, int baudrate);
	int fp_open_serial_hw(const char* fp_device, int baudrate);
	void fp_close_serial(void);
	
private:
	void fp_display_cmd(char cmd);
	
	int get_answer_length(int cmd);
	size_t fp_read(unsigned char *b, size_t len);

	int fd;
	// rs232 assembly
	int state;
	int cmd_length;
	unsigned char buf[10];
	unsigned long long timestamp;
	char device[256];
	int baudrate;
	int old_baudrate;
};

/*-------------------------------------------------------------------------*/
// Avantgarde frontpanel (AVR via RS232)
/*-------------------------------------------------------------------------*/

class frontpanel_avr : public frontpanel_rs232 {
public:
	frontpanel_avr(const char* device) : frontpanel_rs232(device) 
	{
		fp_open_serial(device, B115200);
	};

	const char* fp_get_name(void) { return "AVR";};	

	static int fp_available(const char *device, int force=0);
};
/*-------------------------------------------------------------------------*/
// NetClient 2 (8051 SoC via virtual RS232, Reel FW, raw GPIO)
/*-------------------------------------------------------------------------*/

class frontpanel_ice: public frontpanel_rs232 {
public:
	frontpanel_ice(const char* device) : frontpanel_rs232(device) 
	{
		init_done=0;
		fp_open_serial(device, B9600); // virtual baud rate
	};
		
	const char* fp_get_name(void) { return "ICE";};	

	static int fp_available(const char *device, int force=0);
	virtual void fp_set_leds(int blink, int state);
	virtual void fp_send_cec(unsigned char *buf);

	// earth unsupported calls
 	virtual void fp_display_brightness(int n){};
	virtual void fp_display_contrast(int n){};
	virtual void fp_clear_display(void){};
	virtual void fp_write_display(unsigned char *data, int datalen){};
	virtual void fp_display_data(char *data, int l){};
	virtual void fp_set_displaymode(int m){};
	virtual void fp_show_pic(unsigned char **LCD){};
	virtual void fp_set_led_brightness(int v){};
	virtual void fp_get_flags(void){};
	virtual void fp_eeprom_flash(int a, int d ){};

private:
	void gpio_init(void);
	void gpio_set(int num, int val);
	int gpio_get(int num);
	int init_done;
};
/*-------------------------------------------------------------------------*/
// USB/OLED display
/*-------------------------------------------------------------------------*/
#define MAX_USB_HANDLES 16
#define MAX_CMDS 16

typedef struct {
	pthread_mutex_t lock;
	int state;
        struct usb_dev_handle *handle;
        struct usb_device *device;
} usb_display_t;

class frontpanel_usb: public frontpanel {
public:
	frontpanel_usb(const char *device);
	~frontpanel_usb(void);
	virtual const char* fp_get_name(void) {return "USB";};
	virtual void fp_display_brightness(int n);
	virtual void fp_set_led_brightness(int v);
	virtual void fp_set_leds(int blink, int state);
	virtual void fp_show_pic(unsigned char **LCD);
	virtual int fp_read_msg(unsigned char *msg, unsigned long long *ts);
	virtual void fp_start_handler(void);
	static int fp_available(const char *device, int force=0);
	void fp_refresh_usb(void);
private:

	int decode_rstep(unsigned int *ir_data, int len, unsigned int *c1);
	void decode_xmp(unsigned int *ir_data, unsigned int *c1, unsigned int *c2);
	void fp_read_ir(void);
	
	int num_handles;
	usb_display_t displays[MAX_USB_HANDLES];
	int brightness;
	int brightness_usb;
	unsigned int cmd[MAX_CMDS];
	int cmd_wp;
	int cmd_rp;
};

/*-------------------------------------------------------------------------*/
// NetClient (AVR via SPI, GPIO)
/*-------------------------------------------------------------------------*/

class frontpanel_nc: public frontpanel {
public:
	frontpanel_nc(const char *device);
	virtual int fp_get_fd(void);
	virtual const char* fp_get_name(void);
	virtual void fp_get_version(void);
	virtual void fp_enable_messages(int n);
	virtual void fp_set_leds(int blink, int state);
	virtual void fp_set_clock(void);
	virtual void fp_set_wakeup(time_t t);
	virtual void fp_set_switchcpu(int timeout, int mode);
	virtual void fp_get_clock(void);
	virtual void fp_get_clock_adjust(void);
	virtual void fp_set_clock_adjust(int v1, int v2);
	virtual void fp_get_wakeup(void);
	virtual void fp_get_key(void);
	virtual void fp_eeprom_flash(int a, int di);
	virtual int fp_read_msg(unsigned char *msg, unsigned long long *ts);
	virtual void fp_start_handler(void);

private:
	int fp_open_serial(void);
	void fp_close_serial(void);
	void send_cmd_sub4(char *din, char *dout);
	void send_cmd(char *d, char *r);
	int decode_ruwido(unsigned char *msg,  unsigned long long *ts, int diff, int bit);
	size_t fp_read(unsigned char *b, size_t len);
  	int xir_state,rbit_sr,lb;
	int xir_data;

	int ir_filter;
	int irstate;
	int hist[4];
	int cmd;
	int last_keyval;
	int rpt;
	int fd;
};


/*-------------------------------------------------------------------------*/
// FP registry, dispatch commands to all frontpanels

#define MAX_FRONTPANELS 16

class frontpanel_registry  {
public:
	frontpanel_registry(void);
	~frontpanel_registry(void);
	
	void register_unit(frontpanel *f); // no unregister for now
	
	int fp_get_fd(const char* name);
	frontpanel* fp_get_frontpanel(const char* name);
	void fp_start_handler(void);

	void fp_noop(void);
	void fp_get_version(void);
	void fp_enable_messages(int n);
	void fp_display_brightness(int n);
	void fp_display_contrast(int n);
	void fp_clear_display(void);
	void fp_write_display(unsigned char *data, int datalen);
	void fp_display_data(char *data, int l);
	void fp_set_leds(int blink, int state);
	void fp_set_clock(void);
	void fp_set_wakeup(time_t t);
	void fp_set_displaymode(int m);
	void fp_set_switchcpu(int timeout, int mode);
	void fp_get_clock(void);
	void fp_set_clock_adjust(int v1, int v2);
	void fp_do_display(void);
//	size_t fp_read(unsigned char *b, size_t l){return 0;};
	int  fp_read_msg(unsigned char *msg, unsigned long long *ts);
	
	void fp_show_pic(unsigned char **LCD);
	void fp_get_wakeup(void);
	void fp_get_key(void);
	void fp_set_led_brightness(int v);
	void fp_get_flags(void);
	void fp_power_control(int v);
	void fp_eeprom_flash(int a, int d );

	void force_ice(void); // Force ICE even on AVG3
private:
	frontpanel* frontpanels[MAX_FRONTPANELS]; // no full blown list needed
	int num_frontpanels;
};

extern int u_sleep(long long usec);


#endif // _REEL_FRONTPANEL_H

