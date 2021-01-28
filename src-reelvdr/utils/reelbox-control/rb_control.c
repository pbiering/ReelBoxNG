/*
 * VDR control for Reelbox
 * V2: LIRC
 *
 * (c) Georg Acher, BayCom GmbH, http://www.baycom.de
 *
 * V3: using linux uinput instead of LIRC
 * by Tobias Bratfisch
 *
 * #include <gpl-header.h>
 *
*/
#include <stdlib.h>
#include <linux/cdrom.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <pthread.h>
#include <getopt.h>
#include <signal.h>

#include "frontpanel.h"
#include "daemon.h"

int daemon_debug = 0;

// State file for Avantgarde
#define POWERCTRL_STATE_FILE "/tmp/powerctrl.state"
#define PULSE_BIT  0x8000
#define PULSE_MASK 0x7fff
static int raw_ir = 0;
static int lircudp = -1;
static struct sockaddr_in lircdest;
static struct timeval lirc_tv;
const char *fp_device = FP_DEVICE;
const char * fp_ice_device = "/dev/ttyS2";

static frontpanel_registry fpr;

#define UINPUT 1 // TB: uncomment to use linux input-driver
#ifdef UINPUT
#include <linux/input.h>
#include <linux/uinput.h>
struct uinput_user_dev uinp;
struct input_event event;
#else
#warn "UINPUT not defined"
#endif

#define KEY_REPEAT 1
#define KEY_REPEAT_FAST 2
#define KEY_SHIFT 4
#define SHIFT_THRESHOLD 8
#define REPEAT_DELAY 250000        // delay before the first repeat is sent
#define REPEAT_TIMEOUT (500*1000)  // 300ms, IR repeat is about 120ms

// beware of VDR`s #define KEYPRESSDELAY 150 (lirc.c) //


#include "rb_control.h"

unsigned long long TimeDiff(unsigned long long *timeVar, int update)
{
	unsigned long long diff_time = get_timestamp() - *timeVar;
	if (update) *timeVar += diff_time;
	return diff_time;
}

void print_diff(unsigned long long *timeVar, const char* prefix)
{
	printf("%s%llu us\033[0m\n", prefix, TimeDiff(timeVar,1));
}

#define KSH (1<<16)

// Codes vs. flags vs. long pressed keys vs. event sequence
kbdx_t ir_kbdx[]={
//	IR-code			Flags		Name		Shift-Name	Event		Shift-Event
	{REEL_IR_NONE,		0,		"None",		"",		0,		0},
	{REEL_IR_0,		0,		"0",		"",		KEY_0,		0},
	{REEL_IR_1,		0,		"1",		"",		KEY_1,		0},
	{REEL_IR_2,		0,		"2",		"",		KEY_2,		0},
	{REEL_IR_3,		0,		"3",		"",		KEY_3,		0},
	{REEL_IR_4,		0,		"4",		"",		KEY_4,		0},
	{REEL_IR_5,		0,		"5",		"",		KEY_5,		0},
	{REEL_IR_6,		0,		"6",		"",		KEY_6,		0},
	{REEL_IR_7,		0,		"7",		"",		KEY_7,		0},
	{REEL_IR_8,		0,		"8",		"",		KEY_8,		0},
	{REEL_IR_9,		0,		"9",		"",		KEY_9,		0},
	{REEL_IR_MENU,		0,		"Menu",		"",		KEY_MENU,	0},
	{REEL_IR_UP,		KEY_REPEAT,	"Up",		"",		KEY_UP,		0},
	{REEL_IR_DOWN,		KEY_REPEAT,	"Down",		"",		KEY_DOWN,	0},
	{REEL_IR_LEFT,		KEY_REPEAT,	"Left",		"",		KEY_LEFT,	0},
	{REEL_IR_RIGHT,		KEY_REPEAT,	"Right",	"",		KEY_RIGHT,	0},
	{REEL_IR_OK,		0,		"Ok",		"",		KEY_OK,		0},
	{REEL_IR_EXIT,		0,		"Back",		"",		KEY_EXIT,	0},
	{REEL_IR_RED,		KEY_REPEAT,	"Red",		"",		KEY_RED,	0},
	{REEL_IR_GREEN,		KEY_REPEAT,	"Green",	"",		KEY_GREEN,	0},
	{REEL_IR_YELLOW,	KEY_REPEAT,	"Yellow",	"",		KEY_YELLOW,	0},
	{REEL_IR_BLUE,		KEY_REPEAT,	"Blue",		"",		KEY_BLUE,	0},
	{REEL_IR_SETUP,		0,		"Setup",	"",		KEY_SETUP,	0},
	{REEL_IR_PROGPLUS,	KEY_REPEAT,	"Channel+",	"",		KEY_CHANNELUP,	0},
	{REEL_IR_PROGMINUS,	KEY_REPEAT,	"Channel-",	"",		KEY_CHANNELDOWN,0},
	{REEL_IR_MUTE,		0,		"Mute",		"",		KEY_MUTE,	0},
	{REEL_IR_VOLPLUS,	KEY_REPEAT,	"Volume+",	"",		KEY_VOLUMEUP,	0},
	{REEL_IR_VOLMINUS,	KEY_REPEAT,	"Volume-",	"",		KEY_VOLUMEDOWN,	0},
	{REEL_IR_STANDBY,	0,		"Power",	"",		KEY_POWER,	0},
	{REEL_IR_HELP,		KEY_SHIFT,	"Info",		"Help",		KEY_INFO,	KEY_HELP},   //shift: Help
	{REEL_IR_TEXT,		KEY_SHIFT,	"TT",		"User5",	KEY_TEXT,	KEY_F5},     //shift: User5
	{REEL_IR_PIP,		KEY_SHIFT,	"PiP",		"User6",	KEY_MODE,	KEY_F6},     //shift: User6
	{REEL_IR_GT,		KEY_SHIFT,	"Greater",	"2digit",	KEY_RIGHTBRACE,	KEY_DIGITS}, //shift: 2digit
	{REEL_IR_LT,		KEY_SHIFT,	"Audio",	"Aspect",	KEY_AUDIO,	KEY_SWITCHVIDEOMODE}, //shift: Aspect
	{REEL_IR_DVB,		KEY_SHIFT,	"DVB",		"User1",	KEY_SAT,	KEY_F1},     //shift: User1
	{REEL_IR_DVD,		KEY_SHIFT,	"DVD",		"User2",	KEY_DVD,	KEY_F2},     //shift: User2
	{REEL_IR_PVR,		KEY_SHIFT,	"PVR",		"User3",	KEY_PVR,	KEY_F3},     //shift: User3
	{REEL_IR_REEL,		KEY_SHIFT,	"Reel",		"User4",	KEY_VENDOR,	KEY_F4},     //shift: User4
	{REEL_IR_PLAY,		0,		"Play",		"",		KEY_PLAY,	0},
	{REEL_IR_PAUSE,		0,		"Pause",	"",		KEY_PAUSE,	0},
	{REEL_IR_STOP,		KEY_SHIFT,	"Stop",		"Eject",	KEY_STOP,	KEY_EJECTCD}, //shift: Eject
	{REEL_IR_REC,		0,		"Record",	"",		KEY_RECORD,	0},
	{REEL_IR_EJECT,		0,		"Eject",	"",		KEY_EJECTCD,	0},
	{REEL_IR_REV,		0,		"FastRwd",	"",		KEY_REWIND,	0},
	{REEL_IR_FWD,		0,		"FastFwd",	"",		KEY_FORWARD,	0},
	{REEL_IR_TIMER,		KEY_SHIFT,	"Timers",	"Searchtimers",	KEY_TIME,	KEY_QUESTION}, //shift: Searchtimers
	{REEL_IR_POWER_OFF,  	0,		"Poweroff", 	"",		0,		0},
	{REEL_IR_POWER_ON,   	0,		"Startup",  	"",		0,		0},
	{REEL_IR_GO_STANDBY, 	0,		"Standby",  	"",		0,		0},

	// New keys
	{REEL_IR_INTERNET,   	0,		"Internet", 	"",		KEY_WWW,	0},
	{REEL_IR_GUIDE,		0,		"Guide", 	"",		KEY_EPG,	0},
	{REEL_IR_MULTIMEDIA,	0,		"Multimedia",	"",		KEY_MEDIA,	0},
	{REEL_IR_AUDIO,		KEY_SHIFT,	"Audio",	"Aspect",	KEY_AUDIO,	KEY_SWITCHVIDEOMODE},
	{REEL_IR_SEARCH,	KEY_SHIFT,	"Search", 	"",		KEY_QUESTION,	0},
	{REEL_IR_FAVORITES,	0,		"Favorites",	"",		KEY_FAVORITES,	0},
	{REEL_IR_SUBTITLE,	0,		"Subtitle",	"",		KEY_SUBTITLE,	0},
	{REEL_IR_RADIO,		KEY_SHIFT,	"Radio",	"Favorites+",	KEY_RADIO,	KEY_XFER},

	// New keys keyboard
	{REEL_IR_BACKSPACE,	0,		"BS", 		"",		KEY_BACKSPACE,	0},
	{REEL_IR_TABULATOR,	0,		"TAB", 		"",		KEY_TAB,	0},
	{REEL_IR_ENTER,		0,		"ENTER", 	"",		KEY_ENTER,	0},

	{REEL_IR_RING,		0,		"ring", 	"",		0,		0},
	{REEL_IR_CAP_UML_A,	0,		"uml_A", 	"",		KEY_APOSTROPHE,		0},
	{REEL_IR_CAP_UML_O,	0,		"uml_O", 	"",		KEY_SEMICOLON,		0},
	{REEL_IR_CAP_UML_U,	0,		"uml_U", 	"",		KEY_LEFTBRACE,		0},
	{REEL_IR_SML_UML_A,	0,		"uml_a", 	"",		KEY_APOSTROPHE,		0},
	{REEL_IR_SML_UML_O,	0,		"uml_o", 	"",		KEY_SEMICOLON,		0},
	{REEL_IR_SML_UML_U,	0,		"uml_u", 	"",		KEY_LEFTBRACE,		0},
	{REEL_IR_MICRO,		0,		"micro", 	"",		KEY_EURO,	0},
	{REEL_IR_EURO,		0,		"euro", 	"",		0,		0},
	{REEL_IR_PARAGRAPH,	0,		"paragraph", 	"",		KEY_3,		0},
	{REEL_IR_SZ,		0,		"sz", 		"",		0,		0},
	{REEL_IR_APOSTROPHE,	0,		"apostrophe", 	"",		0,		0},
	{REEL_IR_HIGH_1,	0,		"high_1", 	"",		0,		0},
	{REEL_IR_HIGH_2,	0,		"high_2", 	"",		0,		0},
	{REEL_IR_HIGH_3,	0,		"high_3", 	"",		0,		0},
	{REEL_IR_QUARTER,	0,		"quarter", 	"",		0,		0},
	{REEL_IR_HALF,		0,		"half", 	"",		0,		0},
	{REEL_IR_SPACE,		0,		"SPACE", 	"",		0,		0},

	{REEL_IR_EXCLAMATION,   0,		"exclamationmark","",		KEY_1+KSH,		0},
	{REEL_IR_DOUBLEQUOTE,	0,		"doublequote", 	"",		KEY_2+KSH,		0},
	{REEL_IR_HASH,		0,		"#", 		"",		KEY_BACKSLASH+KSH,	0},
	{REEL_IR_DOLLAR,	0,		"$", 		"",		KEY_4+KSH,		0},
	{REEL_IR_PERCENT,	0,		"%", 		"",		KEY_5+KSH,		0},
	{REEL_IR_AMPERSAND,	0,		"ampersand", 	"",		KEY_6+KSH,		0},
	{REEL_IR_ACUTE,		0,		"acute", 	"",		KEY_BACKSLASH,		0},
	{REEL_IR_LEFT_PARENTHE,	0,		"left_parenthesis","",		KEY_8+KSH,		0},
	{REEL_IR_RIGHT_PARENTHE,0,       	"right_parenthesis","",		KEY_9,			0},
	{REEL_IR_ASTERISK,	0,		"asterisk", 	"",		KEY_RIGHTBRACE,		0},
	{REEL_IR_PLUS,		0,		"plus", 	"",		KEY_RIGHTBRACE,		0},
	{REEL_IR_COMMA,		0,		"comma", 	"",		KEY_COMMA,		0},
	{REEL_IR_MINUS,		0,		"minus", 	"",		KEY_SLASH,		0},
	{REEL_IR_POINT,		0,		"point", 	"",		KEY_DOT,		0},
	{REEL_IR_SLASH,		0,		"slash", 	"",		KEY_7+KSH,		0},

	{REEL_IR_COLON,         0,		"colon", 	"",		KEY_DOT,		0},
	{REEL_IR_SEMICOLON,     0,		"semicolon", 	"",		KEY_COMMA,		0},
	{REEL_IR_LEFT_ANGLE_BR, 0,		"left_angle_bracket", "",	KEY_8+KSH,		0},
	{REEL_IR_EQUAL,         0,		"=", 		"",		KEY_0+KSH,		0},
	{REEL_IR_RIGHT_ANGLE_BR,0,	 	"right_angle_bracket","",	KEY_9+KSH,		0},
	{REEL_IR_QUESTIONMARK,  0,		"questionmark", "",		KEY_MINUS,		0},
	{REEL_IR_AT,		0,		"@", 		"",		KEY_Q,			0},

	{REEL_IR_CAP_A,		0,		"A", 		"",		KEY_A+KSH,		0},
	{REEL_IR_CAP_B,		0,		"B", 		"",		KEY_B+KSH,		0},
	{REEL_IR_CAP_C,		0,		"C", 		"",		KEY_C+KSH,		0},
	{REEL_IR_CAP_D,		0,		"D", 		"",		KEY_D+KSH,		0},
	{REEL_IR_CAP_E,		0,		"E", 		"",		KEY_E+KSH,		0},
	{REEL_IR_CAP_F,		0,		"F", 		"",		KEY_F+KSH,		0},
	{REEL_IR_CAP_G,		0,		"G", 		"",		KEY_G+KSH,		0},
	{REEL_IR_CAP_H,		0,		"H", 		"",		KEY_H+KSH,		0},
	{REEL_IR_CAP_I,		0,		"I", 		"",		KEY_I+KSH,		0},
	{REEL_IR_CAP_J,		0,		"J", 		"",		KEY_J+KSH,		0},
	{REEL_IR_CAP_K,		0,		"K", 		"",		KEY_K+KSH,		0},
	{REEL_IR_CAP_L,		0,		"L", 		"",		KEY_L+KSH,		0},
	{REEL_IR_CAP_M,		0,		"M", 		"",		KEY_M+KSH,		0},
	{REEL_IR_CAP_N,		0,		"N", 		"",		KEY_N+KSH,		0},
	{REEL_IR_CAP_O,		0,		"O", 		"",		KEY_O+KSH,		0},
	{REEL_IR_CAP_P,		0,		"P", 		"",		KEY_P+KSH,		0},
	{REEL_IR_CAP_Q,		0,		"Q", 		"",		KEY_Q+KSH,		0},
	{REEL_IR_CAP_R,		0,		"R", 		"",		KEY_R+KSH,		0},
	{REEL_IR_CAP_S,		0,		"S", 		"",		KEY_S+KSH,		0},
	{REEL_IR_CAP_T,		0,		"T", 		"",		KEY_T+KSH,		0},
	{REEL_IR_CAP_U,		0,		"U", 		"",		KEY_U+KSH,		0},
	{REEL_IR_CAP_V,		0,		"V", 		"",		KEY_V+KSH,		0},
	{REEL_IR_CAP_W,		0,		"W", 		"",		KEY_W+KSH,		0},
	{REEL_IR_CAP_X,		0,		"X", 		"",		KEY_X+KSH,		0},
	{REEL_IR_CAP_Y,		0,		"Y", 		"",		KEY_Z+KSH,		0},
	{REEL_IR_CAP_Z,		0,		"Z", 		"",		KEY_Y+KSH,		0},

	{REEL_IR_LEFT_SQUARE_BRAC, 0,		"[", 		"",		0,		0},
	{REEL_IR_BACKSLASH,	0,		"BACKSLASH", 	"",		0,		0},
	{REEL_IR_RIGHT_SQUARE_BRAC, 0,		"]",	 	"",		0,		0},
	{REEL_IR_CIRCUMFLEX,	0,		"^", 		"",		0,		0},
	{REEL_IR_UNDERSCORE,	0,		"_", 		"",		0,		0},
	{REEL_IR_GRAVE,		0,		"grave", 	"",		0,		0},

	{REEL_IR_SML_A,		0,		"a", 		"",		KEY_A,		0},
	{REEL_IR_SML_B,		0,		"b", 		"",		KEY_B,		0},
	{REEL_IR_SML_C,		0,		"c", 		"",		KEY_C,		0},
	{REEL_IR_SML_D,		0,		"d", 		"",		KEY_D,		0},
	{REEL_IR_SML_E,		0,		"e", 		"",		KEY_E,		0},
	{REEL_IR_SML_F,		0,		"f", 		"",		KEY_F,		0},
	{REEL_IR_SML_G,		0,		"g", 		"",		KEY_G,		0},
	{REEL_IR_SML_H,		0,		"h", 		"",		KEY_H,		0},
	{REEL_IR_SML_I,		0,		"i", 		"",		KEY_I,		0},
	{REEL_IR_SML_J,		0,		"j", 		"",		KEY_J,		0},
	{REEL_IR_SML_K,		0,		"k", 		"",		KEY_K,		0},
	{REEL_IR_SML_L,		0,		"l", 		"",		KEY_L,		0},
	{REEL_IR_SML_M,		0,		"m", 		"",		KEY_M,		0},
	{REEL_IR_SML_N,		0,		"n", 		"",		KEY_N,		0},
	{REEL_IR_SML_O,		0,		"o", 		"",		KEY_O,		0},
	{REEL_IR_SML_P,		0,		"p", 		"",		KEY_P,		0},
	{REEL_IR_SML_Q,		0,		"q", 		"",		KEY_Q,		0},
	{REEL_IR_SML_R,		0,		"r", 		"",		KEY_R,		0},
	{REEL_IR_SML_S,		0,		"s", 		"",		KEY_S,		0},
	{REEL_IR_SML_T,		0,		"t", 		"",		KEY_T,		0},
	{REEL_IR_SML_U,		0,		"u", 		"",		KEY_U,		0},
	{REEL_IR_SML_V,		0,		"v", 		"",		KEY_V,		0},
	{REEL_IR_SML_W,		0,		"w", 		"",		KEY_W,		0},
	{REEL_IR_SML_X,		0,		"x", 		"",		KEY_X,		0},
	{REEL_IR_SML_Y,		0,		"y", 		"",		KEY_Z,		0},
	{REEL_IR_SML_Z,		0,		"z", 		"",		KEY_Y,		0},

	{REEL_IR_LEFT_BRACE,	0,		"left_brace", 	"",		0,		0},
	{REEL_IR_PIPE,		0,		"pipe", 	"",		0,		0},
	{REEL_IR_RIGHT_BRACE,	0,		"right_brace", 	"",		0,		0},
	{REEL_IR_TILDE,		0,		"~", 		"",		0,		0},
	{REEL_IR_DELETE,	0,		"DEL", 		"",		0,		0},

	{0xffff,0,"","",0,0}
};

// Frontpanel Keys
#ifdef RBMINI
kbdx_t kb_kbdx[]={
	{REEL_KBD_BT0,		KEY_REPEAT,	"Up",		"",		KEY_UP,		0},
	{REEL_KBD_BT1,		KEY_REPEAT,	"Down",		"",		KEY_DOWN,	0},
	{REEL_KBD_BT3,		0,		"Power",	"",		KEY_POWER,	0},
	{REEL_KBD_BT5,		0,		"Back",		"",		KEY_EXIT,	0},
	{REEL_KBD_BT6,		0,		"Menu",		"",		KEY_MENU,	0},
	{REEL_KBD_BT10,		0,		"Ok",		"",		KEY_OK,		0},
	{0xffff,0,"","",0,0}
};
#else // ! RB MINI
kbdx_t kb_kbdx[]={
	{REEL_KBD_BT0,		0,		"Power",	"",		KEY_POWER,	0},
	{REEL_KBD_BT1,		0,		"Ok",		"",		KEY_OK,		0},
	{REEL_KBD_BT2,		KEY_REPEAT,	"Up",		"",		KEY_UP,		0},
	{REEL_KBD_BT3,		KEY_REPEAT,	"Down",		"",		KEY_DOWN,	0},
	{REEL_KBD_BT4,		0,		"Back",		"",		KEY_EXIT,	0},
	{REEL_KBD_BT5,		0,		"Eject",	"",		KEY_EJECTCD,	0},
	// BT6 is the "Play" key but sends "DVD" to start Disc player on RB Lite
#if 1 //def RBLITE
	{REEL_KBD_BT6,		0,		"DVD",		"",		KEY_DVD,	0},
#else
	{REEL_KBD_BT6,		0,		"Play",		"",		KEY_PLAY,	0},
#endif //RBLITE
	{REEL_KBD_BT7,		0,		"Pause",	"",		KEY_PAUSE,	0},
	{REEL_KBD_BT8,		0,		"Stop",		"",		KEY_STOP,	0},
	{REEL_KBD_BT9,		KEY_REPEAT_FAST,"FastRwd",	"",		KEY_REWIND,	0},
	{REEL_KBD_BT10,		KEY_REPEAT_FAST,"FastFwd",	"",		KEY_FORWARD,	0},
	{REEL_KBD_BT11,		0,		"Record",	"",		KEY_RECORD,	0},

// Client2 internal KBD
	{REEL_KBD_BT12,		0,              "Menu",		"",		KEY_MENU,	0},
	{0xffff,0,"","",0,0}
};
#endif // RBMINI

#define QUEUELEN 256
ir_cmd_t code_queue[QUEUELEN];
volatile int code_w;
//volatile int vdr_is_up = 0;

int fd_in_use = 0;
int twodevices = 0;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

int power_state=1;

// 0 = Enable XMP-1 and rstep
// 1 = Enable only rstep
// 2 = Enable only XMP-1
int ir_filter_mode=0;

#if ! defined RBLITE && ! defined RBMINI
extern int fan_set[2];
extern int fan_min[2];
#endif

/*-------------------------------------------------------------------------*/
void put_code(ir_cmd_t cmd)
{
        pthread_mutex_lock( &mutex1 );
#if 1//def DEBUG
	if (daemon_debug == 1)
	printf("PUT %s\n", cmd.name);
#endif
	code_queue[code_w]=cmd;
	code_w=(code_w+1)%QUEUELEN;
        pthread_mutex_unlock( &mutex1 );
}
/*-------------------------------------------------------------------------*/
int get_code(int* rp, ir_cmd_t *cmd)
{
	*rp=*rp%QUEUELEN;
	if (code_w!=*rp)
	{
		*cmd=code_queue[(*rp)++];
		return 0;
	}
	return -1;
}
/*-------------------------------------------------------------------------*/
void setnolinger(int sock)
{
	static struct linger  linger = {0, 0};
	int l  = sizeof(struct linger);
	setsockopt(sock, SOL_SOCKET, SO_LINGER, (void *)&linger, l);
}

/*-------------------------------------------------------------------------*/

int lircudp_open(void)
{
	int f;
	if ((f = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
	  perror("Can't get socket");
          return -1;
        }
        struct sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_port = htons(9999);
        sin.sin_addr.s_addr = INADDR_ANY;
        if(bind(f, (struct sockaddr *)&sin, sizeof(sin))<0){
	  perror("Can't bind socket");
        }
        lircdest.sin_family = AF_INET;
        lircdest.sin_port = htons(8765);
        lircdest.sin_addr.s_addr = inet_addr("127.0.0.1");

        gettimeofday(&lirc_tv, NULL);
	return f;
}

/*-------------------------------------------------------------------------*/

#ifndef UINPUT
int lirc_open(void)
{
	int f;
	struct sockaddr_un addr;

	unlink(LIRC_DEVICE);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, LIRC_DEVICE);
	if ((f = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0) {
		if(bind(f,(struct sockaddr *)&addr,sizeof(addr))) {
			perror("bind error");
			close(f);
			return -1;
		}

		setnolinger(f);
//		flags=fcntl(fd,F_GETFL,0);

	}
	return f;
}

#else

int fd[2];
int setup_uinput(int fd, const char* name);

int uinput_open(void)
{

        fd[0] = open(USER_INPUT_DEVICE,O_WRONLY | O_NDELAY);
        if (fd[0] <= 0)
        {
          printf("No uinput driver loaded!\n");
          return -1;
        } else
          printf("uinput driver openend\n");

	fd_in_use = fd[0];
        setup_uinput(fd[0], "Reelbox Frontpanel Part I (VDR-Mode)");

	if(twodevices){
          fd[1] = open(USER_INPUT_DEVICE,O_WRONLY | O_NDELAY);
          if (fd[1] <= 0)
          {
            printf("No uinput driver loaded!\n");
            return -1;
          } else
            printf("uinput driver openend\n");
          setup_uinput(fd[1], "Reelbox Frontpanel Part II (XINE-Mode)");
	}

	return 0;
}

int setup_uinput(int fd, const char* name) {
     memset(&uinp, 0, sizeof(uinp));

     strncpy(uinp.name, name, UINPUT_MAX_NAME_SIZE - 1);
     uinp.id.version = 1;
     uinp.id.bustype = BUS_XTKBD;

     int res = ioctl (fd, UI_SET_EVBIT, EV_KEY);
     res += ioctl (fd, UI_SET_EVBIT, EV_REP);

     /* register all key events */
     int n=0;
     while(ir_kbdx[n].code!=0xffff) {
	     if (ir_kbdx[n].event)
		     res+=ioctl(fd, UI_SET_KEYBIT, ir_kbdx[n].event);
	     if (ir_kbdx[n].shift_event)
		     res+=ioctl(fd, UI_SET_KEYBIT, ir_kbdx[n].shift_event);
	     n++;
     }

     // register modifier keys
     res+=ioctl(fd, UI_SET_KEYBIT, KEY_LEFTSHIFT);
     res+=ioctl(fd, UI_SET_KEYBIT, KEY_LEFTALT);
     res+=ioctl(fd, UI_SET_KEYBIT, KEY_RIGHTALT);

     int written = write(fd, &uinp, sizeof(uinp));

     if(ioctl(fd, UI_DEV_CREATE))
     {
          printf("Unable to create uinput device\n");
          return -1;
     } else
       printf("uinput device created - bytes written: %i\n", written);
     return res;
}
#endif

/*-------------------------------------------------------------------------*/
int write_socket(int fd, char *buf, int l)
{
        int written,todo=l;

        while(todo)
        {
                written=write(fd,buf,todo);
                if(written<=0)
			return(written);
                buf+=written;
                todo-=written;
        }
        return(l);
}
/*-------------------------------------------------------------------------*/
unsigned long long get_timestamp(void)
{
        struct  timeval ti;
        struct timezone tzp;
        gettimeofday(&ti,&tzp);
        return ti.tv_usec+1000000LL*ti.tv_sec;
}

/*-------------------------------------------------------------------------*/
kbdx_t * find_ir_key(int ir_code)
{
	int n=0;

	while(ir_kbdx[n].code!=0xffff) {
		if (ir_code==ir_kbdx[n].code) {
			return &ir_kbdx[n];
		}
		n++;
	}
	return NULL;
}
/*-------------------------------------------------------------------------*/
kbdx_t * find_kbd_key(int ir_code)
{
	int n=0;

	while(kb_kbdx[n].code!=0xffff) {
		if (ir_code==kb_kbdx[n].code) {
		    return &kb_kbdx[n];
		}
		n++;
	}
	return NULL;
}
/*-------------------------------------------------------------------------*/
kbdx_t * find_named_key(char *name, int *shifted)
{
	int n=0;

	while(ir_kbdx[n].code!=0xffff) {
		if (!strcasecmp(name,ir_kbdx[n].fn)) {
			*shifted=0;
			return &ir_kbdx[n];
		}
		if (!strcasecmp(name,ir_kbdx[n].shift_fn)) {
			*shifted=1;
			return &ir_kbdx[n];
		}
		n++;
	}
	return NULL;
}
/*-------------------------------------------------------------------------*/
#ifndef UINPUT
int send_command(int sock, char *string)
{
	int l;
	fpr.fp_set_leds(0x10,2);
	l=write_socket(sock,string,strlen(string));
	u_sleep(20000);
	fpr.fp_set_leds(0x20,2);
	return l;
}

#else

// GA: Preliminary code until event-remapping integrated in input-PI

typedef struct SimulatedKeys {
    int Key;
    int repeatCount; // number of times 'Key' has to be repeated
} SimulatedKeys_t;


int SimulateKey(const char* key_name, SimulatedKeys_t* sim) {
    if (!key_name || !sim) return 0;

#define ISKEY(x) else if(strcasecmp(key_name, x)==0)

    int key=0, count = 0;

    if (strcasecmp(key_name,"a")==0) { key = KEY_2; count = 1; }
    ISKEY("b") { key = KEY_2; count = 2; }
    ISKEY("c") { key = KEY_2; count = 3; }

    ISKEY("uml_a") { key = KEY_2; count = 4; }
    ISKEY("uml_A") { key = KEY_2; count = 4; }

    ISKEY("d") { key = KEY_3; count = 1; }
    ISKEY("e") { key = KEY_3; count = 2; }
    ISKEY("f") { key = KEY_3; count = 3; }

    ISKEY("g") { key = KEY_4; count = 1; }
    ISKEY("h") { key = KEY_4; count = 2; }
    ISKEY("i") { key = KEY_4; count = 3; }

    ISKEY("j") { key = KEY_5; count = 1; }
    ISKEY("k") { key = KEY_5; count = 2; }
    ISKEY("l") { key = KEY_5; count = 3; }

    ISKEY("m") { key = KEY_6; count = 1; }
    ISKEY("n") { key = KEY_6; count = 2; }
    ISKEY("o") { key = KEY_6; count = 3; }

    ISKEY("uml_o") { key = KEY_6; count = 4; } // ö
    ISKEY("uml_O") { key = KEY_6; count = 4; } // Ö

    ISKEY("p") { key = KEY_7; count = 1; }
    ISKEY("q") { key = KEY_7; count = 2; }
    ISKEY("r") { key = KEY_7; count = 3; }
    ISKEY("s") { key = KEY_7; count = 4; }
    ISKEY("sz") { key = KEY_7; count = 5; } // ß

    ISKEY("t") { key = KEY_8; count = 1; }
    ISKEY("u") { key = KEY_8; count = 2; }
    ISKEY("v") { key = KEY_8; count = 3; }

    ISKEY("uml_u") { key = KEY_8; count = 4; } // ü
    ISKEY("uml_U") { key = KEY_8; count = 4; } // Ü

    ISKEY("w") { key = KEY_9; count = 1; }
    ISKEY("x") { key = KEY_9; count = 2; }
    ISKEY("y") { key = KEY_9; count = 3; }
    ISKEY("z") { key = KEY_9; count = 4; }

    ISKEY("@") { key = KEY_0; count = 3; }
    ISKEY("plus") { key = KEY_0; count = 4; }
    ISKEY("minus") { key = KEY_0; count = 5; }

    ISKEY("point")                  { key = KEY_1; count = 1; }
    ISKEY("questionmark")           { key = KEY_1; count = 2; }
    ISKEY("exclamationmark")        { key = KEY_1; count = 3; }
    ISKEY("comma")                  { key = KEY_1; count = 4; }
//    ISKEY("1")                      { key = KEY_1; count = 5; }
    ISKEY("#")                      { key = KEY_1; count = 6; }
    ISKEY("~")                      { key = KEY_1; count = 7; }
    ISKEY("BACKSLASH")              { key = KEY_1; count = 8; }
    ISKEY("^")                      { key = KEY_1; count = 9; }
    ISKEY("$")                      { key = KEY_1; count = 10; }
    ISKEY("[")                      { key = KEY_1; count = 11; }
    ISKEY("]")                      { key = KEY_1; count = 12; }
    ISKEY("pipe")                   { key = KEY_1; count = 13; }
    ISKEY("left_parenthesis")       { key = KEY_1; count = 14; } // (
    ISKEY("right_parenthesis")      { key = KEY_1; count = 15; } // )
    ISKEY("asterisk")               { key = KEY_1; count = 16; }
    ISKEY("left_brace")             { key = KEY_1; count = 17; } // {
    ISKEY("right_brace")            { key = KEY_1; count = 18; } // }
    ISKEY("slash")                  { key = KEY_1; count = 19; } // /
    ISKEY("colon")                  { key = KEY_1; count = 20; }
    ISKEY("%")                      { key = KEY_1; count = 21; }
    ISKEY("ampersand")              { key = KEY_1; count = 22; }

    ISKEY("BS")                     { key = KEY_YELLOW; count = 1; } // Backspace
    ISKEY("DEL")                    { key = KEY_YELLOW; count = 1; } // Delete

    ISKEY("SPACE")                  { key = KEY_0;      count = 1; } // spacebar
    ISKEY("ENTER")                  { key = KEY_OK;     count = 1; }

    if (daemon_debug == 1)
    printf("Simulating Key: %s --> with %d keys\n", key_name, count);
    sim->Key = key;
    sim->repeatCount = count;

    return 1;

}

/*
  returns number of keys needed to simulate the key represented by 'cmd'
*/
int CanSimulateKey(ir_cmd_t* cmd, int * const simulated_array) {

    if (!cmd || !simulated_array) {
        if (daemon_debug == 1)
        printf("Simulate key: returning 0 because  cmd %p, array %p\n", cmd, simulated_array);
        return 0;
    }

    if (daemon_debug == 1)
    printf("simulating key '%s'\n", cmd->name);

    int i;

    // number of keys need to simulate the given 'cmd' key
    int n = 0;

    SimulatedKeys_t sim;
    // calculate number of keys required to simulate 'cmd->name' key
    n = SimulateKey(cmd->name, &sim)? sim.repeatCount:0;

    // copy these keys into 'simulated_array'
    for (i=0; i<n; ++i) simulated_array[i] = sim.Key;

    // if key is a letter, for faster typing add 'kRight' at the end.
    if (n && strcmp(cmd->name, "BS") && strcmp(cmd->name, "ENTER") && strcmp(cmd->name, "DEL")) {
            simulated_array[n++] = KEY_RIGHT;
    }

    return n;
}

/*-------------------------------------------------------------------------*/

int sendInputEvent(int unused, ir_cmd_t *cmd, int rep_count)
{
    // variables for simulating character keys
    int countSimulatedKeys; // number of number keys needed to simulate a given character key
    int simulated_keys_array[32];  // simulate character key with keys in this array

     int blink=0;

     if ((rep_count&3)==0)
	     blink=1;


     static unsigned long long send_time = 0;
     if (daemon_debug == 1)
     printf("\tmakeInputEvent: cmd->name: %s, rep_count: %i", cmd->name, rep_count);
     if (daemon_debug == 1)
     print_diff(&send_time, " \t");


     if (blink)
	     fpr.fp_set_leds(0x10,2);

//     gettimeofday(&event.time, NULL);

     //dirty:
     event.value = 1; // 1 is keypress, 0 is release, 2 is autorepeat
     event.type = EV_KEY;

     if (rep_count > 0 && (rep_count <= 3 || !(cmd->flags&KEY_REPEAT))) { //adjust here for delay of first key repeat
          u_sleep(10*1000);
          if (blink)
		  fpr.fp_set_leds(0x20,2);
          return -1; // ignore first repeat-keys, too fast
     } else if (rep_count > 2){
          event.value=2;
     }

     switch(cmd->state){
          /* DVB+PVR - use device for VDR */
          case REEL_IR_STATE_DVB:
          case REEL_IR_STATE_PVR:
            if(fd_in_use != fd[0]){
              if (daemon_debug == 1)
              printf("DDD: switching to VDR-Mode\n");
              fd_in_use = fd[0];
	    }
            break;
          /* DVD, REEL, TV - use the other device */
          case REEL_IR_STATE_TV:
          case REEL_IR_STATE_DVD:
          case REEL_IR_STATE_REEL:
          case REEL_IR_STATE_RUWIDO:
            if(twodevices && (fd_in_use != fd[1])){
              if (daemon_debug == 1)
              printf("DDD: switching to XINE-Mode\n");
              fd_in_use = fd[1];
	    }
            break;
          default:
            if (daemon_debug == 1)
            printf("DDDD: %0#4x\n", cmd->state);
     }


     if (cmd->shift)
	     event.code=cmd->kbdx->shift_event;
     else
	     event.code=cmd->kbdx->event;

     // Preliminary: Emulate alpha keys
     /* Can this unhandled key be simulated? Eg. key from keyboard 'a' - 'z' */
     countSimulatedKeys = CanSimulateKey(cmd, simulated_keys_array);

     if (daemon_debug == 1)
     printf("EVENT %i\n",event.code);


#if 0
     else {
	     printf("ERROR: unknown keystroke - shouldn't happen! %i %i\n",fd_in_use,fd[0]);
           u_sleep(200);
           if (blink)
		   fpr.fp_set_leds(0x20,2);
           return -1;
     }
#endif

     if (countSimulatedKeys) {
         if (daemon_debug == 1)
         printf("\nSimulating '%s' with %d keys\n", cmd->name, countSimulatedKeys);
     }

     int i = 0;

     do {
         if (countSimulatedKeys > 0) {
             if (daemon_debug == 1)
             printf("simulated by key %i: '%d'\n", i, simulated_keys_array[i]);
             event.code = simulated_keys_array[i];
             event.type = EV_KEY;
             event.value = 1; // simulate key presses

             // do not send in too many keys at once. Some keys are ignored.
             if (i && i%10==0)
                 u_sleep(5); // delay between simulated keys
         }

	 if (fd_in_use>0) {
		 int ret;
		 ret=write(fd_in_use, &event, sizeof(event));
		 if (ret == 0) { }; // dummy

		 int event_code_cp = event.code;
		 int event_value_cp = event.value;

		 memset(&event, 0, sizeof(event));
		 gettimeofday(&event.time, NULL);
		 event.type = EV_SYN;
		 event.code = SYN_REPORT;
		 event.value = 0;
		 ret=write(fd_in_use, &event, sizeof(event));

		 if(event_value_cp == 1){ /* if there was a key-press-event */
			 /* send key-release-event */
			 memset(&event, 0, sizeof(event));
			 gettimeofday(&event.time, NULL);
			 event.type = EV_KEY;
			 event.code = event_code_cp;
			 event.value = 0;
			 ret=write(fd_in_use, &event, sizeof(event));

			 memset(&event, 0, sizeof(event));
			 gettimeofday(&event.time, NULL);
			 event.type = EV_SYN;
			 event.code = SYN_REPORT;
			 event.value = 0;
			 ret=write(fd_in_use, &event, sizeof(event));
		 }
	 } // if (fd_in_use>0)
     }  while (++i < countSimulatedKeys);

     u_sleep(5*1000);
     if (blink)
	     fpr.fp_set_leds(0x20,2);

     return 1;
}
/*-------------------------------------------------------------------------*/
// Floh: This function checks if vdr is running by checking the presence of the file vdr.standby
int running_vdr()
{
	struct stat statbuf;
	int ret=stat("/tmp/vdr.standby", &statbuf);

	if (ret<0)
		return 1;	//1 = vdr is running
	else
		return 0;	//0 = vdr is not running
}

void shutdown_reelbox()
{
	fpr.fp_set_leds(0x10,2);
	printf("initiate shutdown - touch /tmp/vdr.deepstandby\n");
	if (system("touch /tmp/vdr.deepstandby")); // ARGL... avoid warning due to attribute 'warn_unused_result'
	unlink(STANDBYFILE);
	u_sleep(1000*1000);
	fpr.fp_set_leds(0x20,2);
}

#endif
/*-------------------------------------------------------------------------*/
int send_code(ir_cmd_t cmd, int rep_count)
{
	//char string[256];
	int died=0;

#ifndef UINPUT
	int l;
	sprintf(string,"%x %i LIRC.%s reel ",cmd.cmd,rep_count,cmd.name); // workaround for 2 digits (_blank after reel)
	l=send_command(sock,string);
	if (l<0)
		died=1;
#else
	sendInputEvent(0, &cmd, rep_count) ;
#endif
	power_state = running_vdr();
	return died;
}
/*-------------------------------------------------------------------------*/
void cdeject(void)
{
	if (system("/usr/sbin/mount.sh eject"));
}
/*-------------------------------------------------------------------------*/

struct fake_rep_cmd_t
{
	ir_cmd_t cmd;
	unsigned long long timestamp;
	int count;
};

void reset_fake_repeat(struct fake_rep_cmd_t* fake_rep, const ir_cmd_t cmd)
{
	fake_rep->count = 0;			// reset count
	fake_rep->timestamp = get_timestamp(); 	// timestamp is now
	fake_rep->cmd = cmd;			// cmd is the supplied one
}

#define SOURCE_TIMEOUT 500*1000

void* ir_send_thread(void* para)
{
	int sock=(intptr_t)(para);
	ir_cmd_t cmd;
	int rp=code_w;
	//char string[256];
	int died=0;
	int rep_count=0;
	unsigned long long last_timestamp=0;
	//unsigned long long last_queue_empty=get_timestamp();
	//unsigned long long last_queue_notempty=get_timestamp();

        int avr_fd=-1,ice_fd=-1;
	int pollnum=1;
	struct pollfd pfd[3]=  {{sock, POLLERR|POLLHUP,0}};

#ifdef RB_MINI
	avr_fd=fpr.fp_get_fd("NCL1");
#else
        avr_fd=fpr.fp_get_fd("AVR");
	if (avr_fd>0) {
		struct pollfd pfdx={avr_fd, POLLIN|POLLERR|POLLHUP,0};
		pfd[pollnum++]=pfdx;
	}
        ice_fd=fpr.fp_get_fd("ICE");
	if (ice_fd>0) {
		struct pollfd pfdx={ice_fd, POLLIN|POLLERR|POLLHUP,0};
		pfd[pollnum++]=pfdx;
	}
#endif


	unsigned long long ts_source=0;
	int last_source=-1; // -1: take any, 0: take serial, 1: take USB

	puts("send thread started");
	power_state = 1;

	unsigned long long last_fake_rep_diff, tmp;
	struct fake_rep_cmd_t fake_rep;
	reset_fake_repeat(&fake_rep, cmd);

	while(!died) {
		unsigned long long now=get_timestamp();

		if (get_code(&rp,&cmd)==-1) {

#if 1
			u_sleep(10*1000);

			poll(pfd,pollnum,10);

			if (pfd[0].revents&(POLLERR|POLLHUP)) // socket
				break;

			// Special treatment for STOP/EJECT
			// FIXME: why?
			if (!pfd[1].revents&POLLIN && rep_count<15 && (cmd.cmd==REEL_IR_STOP))
			{
				died=send_code(cmd, rep_count);
				cmd.cmd=0;
			}

			//-------------- FAKE REPEAT -------------------
			last_fake_rep_diff = TimeDiff(&fake_rep.timestamp,0);
			if ( last_fake_rep_diff > 60000 /*in us*/
				&& (tmp = TimeDiff(&last_timestamp,0)) < 120000 /*in us*/
				&& rep_count > 5)
			// 60ms have passed before last fake repeat
			// AND the last "real cmd" was within the past 120ms since IR repeat is ~120ms
			// THEN do a fake repeat
			{
				//print_diff(&last_queue_empty, "\033[1;91m Q empty: \t");

				printf("fake");
				died=send_code(fake_rep.cmd,rep_count++);
				fake_rep.count++;
				fake_rep.timestamp = now;
			}
			// -------------end FAKE REPEAT -----------------
			continue;
		}
#endif

		if (now-ts_source>SOURCE_TIMEOUT) {
			ts_source=now;
			last_source=cmd.source;
		}
		else {
			if (last_source>=0 && cmd.source!=last_source) {// wrong source, ignore
				printf("IGN %i\n",cmd.source);
				continue;
			}
		}


		//print_diff(&last_queue_notempty, "\033[1;92m Q not empty: \t");

		/*copy 'cmd' into fake_rep and fake its repeat if queue is empty*/
		reset_fake_repeat(&fake_rep, cmd);

		// Reset repeat count
		// last timestamp > timeout || key was different than the last one!
		if (cmd.timestamp-last_timestamp > REPEAT_TIMEOUT || (cmd.device != 1 && !cmd.repeat))
			rep_count=0;

		died=send_code(cmd,rep_count);
#if 0
		// Faster repeat after a while
		if (rep_count>10) {
			rep_count++;
			u_sleep(60*1000);  // IR repeat is 120ms, place in between
			send_code(cmd,rep_count);
		}
#endif

		//DDD("power_state: %d, cmd.name: %s",  power_state, cmd.name);

		if (!strcmp(cmd.name,"Power") ) {
			fpr.fp_set_leds(0x10,2);
			if (!unlink(STANDBYFILE))
				power_state=1; // unlink successfull -> was in standby state
			else if (rep_count==0)
			{
				power_state=0;  // unlink unsuccessfull -> going into standby
						// start a shutdown watchdog on Power press
				fprintf(stderr, "POWER pressed - starting shutdownWD\n");
				if (system("shutdownwd.sh"));
			}

			u_sleep(20000);
			fpr.fp_set_leds(0x20,2);

		} else if (strcmp(cmd.name,"Poweron")==0) {
			DDD("Poweron");
			if (!unlink(STANDBYFILE))
				power_state=1; // unlink successfull -> was in standby state

		} else if (strcmp(cmd.name,"Poweroff")==0) {
			DDD("Poweroff");
			if (running_vdr()) {
				if (system("/etc/init.d/reelvdr stop"));
			}
			shutdown_reelbox();

		} else if (strcmp(cmd.name,"Standby")==0) {
			DDD("Standby");
			if (running_vdr()) {
				standby(1);
				DDD("sending cmd.name=Power");
				strcpy(cmd.name,"Power");
			}
			else {
				DDD("already in standby. nothing to do.");
			}
		} else if (strcmp(cmd.name,"Eject")==0
				&& (cmd.device == 1 || !cmd.repeat)
				&& !running_vdr()) {
			cdeject();
		} else if (!strcmp(cmd.name,"Back") && !running_vdr()) {
			shutdown_reelbox();
		}

		//if (rep_count == 0)
		//	u_sleep(REPEAT_DELAY);
		rep_count++;
		last_timestamp=cmd.timestamp;
	}

	close(sock);
	power_state = 0;
	puts("send thread exit");
	return 0;
}
/*-------------------------------------------------------------------------*/
void* server_thread(void* p)
{
	int sock=0; //,newsock;
	//socklen_t addrlen;
	//struct sockaddr_un peer_addr;
	pthread_t clientthread;
	do {
		u_sleep(100*1000);
#ifndef UINPUT
		sock=lirc_open();
#else
        	uinput_open();
#endif
	}  while(sock==-1);

        if(raw_ir) {
          lircudp=lircudp_open();
        }

	printf("Server Thread running!\n");

#ifndef UINPUT
	while(1) {
		listen(sock,5);
		newsock=accept(sock,(struct sockaddr *)&peer_addr,&addrlen);
		setnolinger(newsock);
		if (!pthread_create(&clientthread, NULL,ir_send_thread,(void*)newsock)){
				fprintf(stderr,"pthread_detach!\n");
			pthread_detach(clientthread);
		}
	}
#else
	if (!pthread_create(&clientthread, NULL,ir_send_thread,(void*)0)){
		fprintf(stderr,"pthread_detach!\n");
		pthread_detach(clientthread);
	}
#endif

	return 0;
}


/*-------------------------------------------------------------------------*/
// calculate fan RPMs from power control messages

#define RPM_CALC(x) ((x)!=0 && (x)!=0x7ff?60*(7800/(x))/4:0)

void handle_fan_status(unsigned char * buf)
{
#if ! defined RBLITE && ! defined RBMINI

	if (buf[2]==0xff) // ignore NCL2 PIC message
		return;
	FILE *statefd=fopen(POWERCTRL_STATE_FILE,"w");
	double temp, rpm1, rpm2,rpm1_set,rpm2_set;
	//printf("%x %x %x %x %x %x\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);
	if (statefd) {
		temp=buf[2];
		rpm1=(buf[3]<<4) | ((buf[4]>>4)&15);
		rpm2=((buf[4]&15)<<8 ) | (buf[5]);
		temp=25+ 1000*(((temp*2)*1.1/1024.0)-0.314)/1.07; // useless without calibration...
		rpm1=RPM_CALC(rpm1);
		rpm2=RPM_CALC(rpm2);
		rpm1_set=RPM_CALC(fan_set[1]);
		rpm2_set=RPM_CALC(fan_set[0]);

		fprintf(statefd,"%i %i %i %i %i %i\n",(int)time(0),(int)temp,(int)rpm1,(int)rpm2,(int)rpm1_set,(int)rpm2_set);
                if (daemon_debug == 1)
		printf("now: %i temp %i rpm(cpu) %i rpm(ps) %i rpm1_set %i rpm2_set %i\n",
                        (int)time(0),(int)temp,(int)rpm1,(int)rpm2,(int)rpm1_set,(int)rpm2_set);
		fclose(statefd);
	}
#endif
}
/*-------------------------------------------------------------------------*/
// Raw IR timing (when enabled with -r)

void handle_raw_ir(unsigned char *buf)
{
	struct timeval tv;
	int t0=buf[1], t1=buf[2];
	u_int8_t udp_buf[4];
	gettimeofday(&tv, NULL);

	if (t0<128)
		t0=t0*4*5787;
	else
		t0=128*4*5787+(t0-128)*4*4*4*5787;

	if (t1<128)
		t1=t1*4*5787;
	else
		t1=128*4*5787+(t1-128)*4*4*4*5787;

	t0/=1000;
	t1/=1000;

	if(!t0) {
		u_int64_t now  = tv.tv_sec*1000000 + tv.tv_usec;
		u_int64_t last = lirc_tv.tv_sec*1000000 + lirc_tv.tv_usec;
		t0=now-last;
		if(t0>1999937) {
			t0=1999937;
		}
	}

	//printf("%02x %02x % 5i % 5i\n", buf[2], buf[3],t0, t1);

	t0=((t0 * 256) / 15625)&0x7fff;

	t1=((t1 * 256) / 15625)&0x7fff;
	t0|=PULSE_BIT;

	//printf("space %i\npulse %i %02x %02x\n", ((t0&0x7fff)*15625)/256, ((t1&0x7fff)*15625)/256, buf[2], buf[3]);
	printf("space %i  %i \n", ((t0&0x7fff)*15625)/256, ((t1&0x7fff)*15625)/256);

	udp_buf[0]=t0&0xff;
	udp_buf[1]=(t0>>8)&0xff;

	udp_buf[2]=t1&0xff;
	udp_buf[3]=(t1>>8)&0xff;
	sendto(lircudp, udp_buf, 4, MSG_DONTWAIT, (struct sockaddr *)&lircdest, sizeof(lircdest));

	lirc_tv=tv;
}
/*-------------------------------------------------------------------------*/

int handle_ir_key(ir_state_t *state, ir_cmd_t *cmd, unsigned char * buf, unsigned long long ts)
{
	unsigned int xcmd;

	xcmd=(buf[2]<<24)| (buf[3]<<16)| (buf[4]<<8)|(buf[5]);

	if (!xcmd)
		return 1; // skip (possible decoding bug)

	state->available=0;

        if (daemon_debug == 1)
	printf("DDDD: xcmd: %x\n", xcmd);

	if (xcmd)
	{
		// map rstep to XMP-1
		if (buf[2]==0x00 && (buf[3]&0x7f)==0x59 && (buf[4]&0x7e)==0x00) {
			state->ts_reel_code=ts;
			xcmd=(buf[5]<<8);
			cmd->repeat=(buf[4]&0x01);
			cmd->source=0;
			cmd->timestamp=ts;
		}

		int key = buf[2] << 8 | buf[3];

		if (key == REEL_IR_STATE_DVB || key == REEL_IR_STATE_DVD
		    || key == REEL_IR_STATE_TV  || key == REEL_IR_STATE_PVR
		    || key == REEL_IR_STATE_REEL)
		{
			cmd->state = key;
			//printf("DDD: state: %x\n", state);
		}
	}

	if ((xcmd&0xffff)==REEL_IR_VENDOR) {
		state->ts_reel_code=ts;
		state->device_id=xcmd>>16;
	}
	else if (state->ts_reel_code || (buf[1]==0xf1)) {
		if (buf[1]==0xf1) {
			cmd->device=REEL_IR_STATE_KEYBOARD; // Kbd
			xcmd>>=16;
		}
		else if (buf[2]==0x00)
			cmd->device=REEL_IR_STATE_RUWIDO;
		else
			cmd->device=state->device_id; // IR

		if (cmd->device!=REEL_IR_STATE_RUWIDO ) {
			cmd->repeat=buf[3]&0x80;
		}
		cmd->cmd=xcmd&0xffff; // Strip mode
		cmd->timestamp=ts;
		state->device_id=0;
		state->available=1;

	}
	return 0;
}
/*-------------------------------------------------------------------------*/
#define SHIFT_TIMEOUT (1000LL*1000LL)      // us
#define VENDOR_ID_TIMEOUT (300LL*1000LL)

#define STATE_IDLE 0
#define STATE_WAIT_SHIFT 1
#define STATE_WAIT_RELEASE 2

void handle_key_timing(ir_state_t *state, ir_cmd_t *cmd)
{
	unsigned long long now=get_timestamp();

	cmd->shift=0;

	if (!state->available) {
		// handle timeouts

		// clear reel id state

		if (state->ts_reel_code && ( (now-state->ts_reel_code) > VENDOR_ID_TIMEOUT)) {
			state->ts_reel_code=0;
			if (state->state==STATE_WAIT_SHIFT)
				put_code(*cmd);
			state->state=STATE_IDLE;
		}
	}
	else {
		// handle shifts
		kbdx_t *found_key=NULL;
		int flags;

		state->ts_last=cmd->timestamp;

		if (cmd->device==REEL_IR_STATE_KEYBOARD)
			found_key=find_kbd_key(cmd->cmd);
		else
			found_key=find_ir_key(cmd->cmd);

		if (found_key) {

			strcpy(cmd->name,found_key->fn);
			flags=cmd->flags=found_key->flags;
			cmd->kbdx=found_key;

			switch(state->state) {
			case STATE_IDLE:
				if (flags==KEY_SHIFT) {
					state->state=STATE_WAIT_SHIFT;
					state->ts_start_shift=cmd->timestamp;
					state->cmd_start_shift=cmd->cmd&0xffff;
				}
				else {
					// Clear repeat state if initial packet gets lost
					if ((cmd->cmd&0xffff) != (state->cmd_last&0xffff))
						cmd->repeat=0;

					put_code(*cmd);
				}
				break;

			case STATE_WAIT_SHIFT:
				// is the command pressed long enough?
				if ((cmd->cmd&0xffff)==state->cmd_start_shift) {
					if ((now - state->ts_start_shift) > SHIFT_TIMEOUT) {

						if (strlen(found_key->shift_fn)) {
							cmd->repeat=0;
							strcpy(cmd->name,found_key->shift_fn);
							cmd->shift=1;
							put_code(*cmd);
							state->state=STATE_WAIT_RELEASE;
						}
					}
				}
				else { // command changed in-flight???
					put_code(*cmd);
					state->state=STATE_IDLE;
				}
				break;

			case STATE_WAIT_RELEASE:
				if ((cmd->cmd&0xffff)!=state->cmd_start_shift) { // command changed???
					put_code(*cmd);
					state->state=STATE_IDLE;
				}
				// let timeout above do the rest
				break;
			}
		}
		state->cmd_last=cmd->cmd;
	}
}
/*-------------------------------------------------------------------------*/
void signal_nop(int sig)
{
}

void usage(void)
{
    /* print usage and exit */
    fprintf(stderr, "reelbox-ctrld -- Remote control of the ReelBox by IR, serial or network  V4.0\n"
                    "Usage: reelbox-ctrld [options]\n"
		    "       -2, --twodevices    create 2 uinput devices\n"
                    "       -d, --device        input device to use, default /dev/ttyS2\n"
                    "       -D, --debug         debug-messages\n"
                    "       -f, --daemon        fork to background (daemon mode)\n"
                    "       -r, --raw           raw IR mode\n"
	            "       -F <mode>           Filter mode (0/1/2 = XMP+rstep/rstep/XMP\n"
                    "       -X <spec>, --external=<spec>\n"
                    "                           External comand input\n"
                    "       -h, --help          this text\n"
                    "Supported external specs: \n"
                    " rs232:<device>[:<baudrate>] RS232-IO, default 115200\n"
                    " udp:<port>                  UDP-IO, port >1023\n"
        );
    exit(EXIT_FAILURE);

}

//Start by Klaus
void mySigHandler(int sig)
{
    ir_cmd_t event =
    {
	REEL_IR_REEL,
	1,
	0,
	0,
	1,
	"Reel",
	REEL_IR_STATE_REEL
    };
    sendInputEvent(0, &event, 0);


    puts("-------------MySigHandler:Received Signal-----------\n");
}

//End by Klaus


/*-------------------------------------------------------------------------*/

#define STATE_IDLE 0
#define STATE_WAIT_SHIFT 1
#define STATE_WAIT_RELEASE 2

int main(int argc, char **argv)
{
	int n;
	if (n == 0) { }; // dummy
	// ir_cmd_t last_cmd;
	//int flags=0;
	//int last_flags=0;
	//int ir_avail=0;
	pthread_t servermain, externalio;
	//int send=0;
	//int rep_count=0;
        int run_as_daemon = 0;
        int c;
        //int state=STATE_IDLE;
	//int refresh_cnt=0;

        static struct option long_options[] = {
             {"help", 0, 0, 'h'},
             {"device", 0, 0, 'd'},
             {"debug", 0, 0, 'D'},
             {"daemon", 0, 0, 'f'},
             {"raw", 0, 0, 'r'},
	     {"twodevices", 0, 0, '2'},
	     {"external", 1, 0, 'X'},
	     {"filter", 1, 0, 'F'},
             {0,0,0,0}
        };
	//Start by Klaus
	struct sigaction mySigAction;
	mySigAction.sa_handler = mySigHandler;
	sigemptyset(&mySigAction.sa_mask);
	mySigAction.sa_flags = SA_RESTART;

	int signr = SIGUSR1;
	int ret = sigaction(signr, &mySigAction, NULL);
	if (ret == 0) { }; // dummy
	//printf("---------sigaction = %d------------\n", ret);
	//End by Klaus

        int option_index = 0;
	char ext_io_optarg[32] = {0};
	int start_external_io = 0;

    while ((c = getopt_long(argc, argv, "2d:fF:DhnrX:", long_options, &option_index)) != EOF) {
	switch (c) {
	case '2':
	    twodevices = 1;
	    break;
	case 'f':
	    run_as_daemon = 1;
	    break;
	case 'd':
	    fp_device = optarg;
	    break;
	case 'D':
	    daemon_debug = 1;
            break;
	case 'r':
	    raw_ir = 1;
	    break;
	case 'F':
	    ir_filter_mode=atoi(optarg)&3;
	    break;
        case 'X':
//             pthread_create(&externalio, NULL, external_command_thread, optarg);
	    // start thread after forking, save just the necessary parameter here
	    start_external_io = 1;
	    strncpy(ext_io_optarg, optarg, 31);
	    printf("optarg: '%s'\n", ext_io_optarg);
            break;
	case '?':
            usage();
	    pabort("unknown option");
	    break;
        case 'h':
	default:
	    usage();
	    exit(0);
	}
    }

#ifdef RBMINI
	frontpanel *f_main=new frontpanel_nc("");
	fpr.register_unit(f_main);
#else
	int have_avr=0,have_ice=0;

	// Detection
	if (frontpanel_avr::fp_available(fp_device))
		have_avr=1;

	if (frontpanel_ice::fp_available(fp_ice_device))
		have_ice=1;

	if (have_avr){
		frontpanel *f_main=new frontpanel_avr(fp_device);
		fpr.register_unit(f_main);
	}

	if (have_ice){
		frontpanel *f_ice=new frontpanel_ice(fp_ice_device);
		fpr.register_unit(f_ice);
	}
#endif


	frontpanel *f_usb=new frontpanel_usb("");
	fpr.register_unit(f_usb);

	signal(SIGPIPE,signal_nop);

	if(run_as_daemon)
           daemon_setup();

	// start external IO thread after fork()
	if (start_external_io && ext_io_optarg[0]){
		int ret_value = pthread_create(&externalio, NULL, external_command_thread, ext_io_optarg);
		printf("thread_create returned %d\n", ret_value);
	}

	n=0;
	if (raw_ir)
		fpr.fp_enable_messages(0x3f+(ir_filter_mode<<6));
	else
		fpr.fp_enable_messages(0x1f+(ir_filter_mode<<6));

	// last_cmd.cmd=0;
	// last_cmd.timestamp=get_timestamp();

	unlink(STANDBYFILE);

	pthread_create(&servermain, NULL, server_thread , 0);


	// start scanner threads
	fpr.fp_start_handler();

	while(1) {
		sleep(10);
	}
	if(run_as_daemon)
          daemon_cleanup();
        exit(EXIT_SUCCESS);

}
