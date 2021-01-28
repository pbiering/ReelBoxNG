#define PIDFILE "/var/run/reelbox-ctrld.pid"

void daemon_setup(void);
void daemon_cleanup(void);

void pabort (const char *fmt, ...);

void perr (const char *fmt, ...);

