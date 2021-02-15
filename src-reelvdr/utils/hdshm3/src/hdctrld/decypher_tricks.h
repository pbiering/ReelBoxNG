#ifndef INCLUDE_DECYPHER_TRICKS
#define INCLUDE_DECYPHER_TRICKS

void decypher_set_vfilter(int mode, int val);
void decypher_set_bcg(double gamma_val, int brightness, double contrast);
int init_decypher_tricks(void);
void spdif_mute(void);
#endif

