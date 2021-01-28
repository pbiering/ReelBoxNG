#ifndef _RB_CONTROL_H
#define _RB_CONTROL_H

typedef struct
{
        int code;
        int flags;
        const char *fn;
	const char *shift_fn;	// LONG PRESS, not shift-key!
	int event;
	int shift_event; // LONG PRESS, not shift-key!
} kbdx_t;

typedef struct {
	int source;            // 0: Frontpanel, 1: USB
        int cmd;
        int device;            // 1: Keyboard, else REEL_IR_STATE_*-codes
	int repeat;            // 0: initial cmd, 1: repeat
        unsigned long long timestamp;
        char name[32];
        int flags;
        int state;
	int shift;
	kbdx_t *kbdx;
} ir_cmd_t;

typedef struct {
	int state;                       // state of shift detection
	unsigned long long ts_reel_code; // >1: timestamp of vendor code packet, 1: implicitely ok (ruwido)
	unsigned long long ts_last;      // timestamp of last key
	unsigned long long ts_start_shift;  // timestamp of shift key wait
	int cmd_start_shift;             // code of initial shift key
	int cmd_last;
	int device_id;
	int available;
} ir_state_t;

// from rb_control.c, used by frontpanel_*-handlers

extern void handle_fan_status(unsigned char * buf);
extern void handle_raw_ir(unsigned char *buf);
extern void handle_key_timing(ir_state_t *state, ir_cmd_t *cmd);
extern int handle_ir_key(ir_state_t *state, ir_cmd_t *cmd, unsigned char * buf, unsigned long long ts);

void put_code(ir_cmd_t cmd);
unsigned long long get_timestamp(void);
void* external_command_thread(void* para);

// from external_ctrl.c
extern void standby(int off);
extern void poweroff(void);

#define STANDBYFILE "/tmp/vdr.standby"

#define DDD(x...)    printf("reelbox-ctrld %s(): ", __PRETTY_FUNCTION__) ; printf(x); printf("\n");


#endif
