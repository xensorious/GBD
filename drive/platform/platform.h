#ifndef ASTERWILD_PLATFORM_H
#define ASTERWILD_PLATFORM_H
#include <stdint.h>

/* Native Nintendo DS hardware boundary. RGB555 pixels include bit 15. */
enum {
    DS_A=1, DS_B=2, DS_SELECT=4, DS_START=8,
    DS_RIGHT=16, DS_LEFT=32, DS_UP=64, DS_DOWN=128,
    DS_R=256, DS_L=512, DS_X=1024, DS_Y=2048, DS_TOUCH=4096
};
void ds_init(void);
void ds_vblank(void);
void ds_frame_begin(void);
uint32_t ds_keys_held(void);
uint32_t ds_keys_down(void);
int ds_touch(int *x, int *y);
/* The inactive main-screen VRAM page. Draw using 16/32-bit stores only.
 * ds_present() flips it at the next LCD vblank. A NULL screen is unchanged;
 * presentation still waits for vblank, and does not rescan button edges. */
uint16_t *ds_top_buffer(void);
void ds_present(const uint16_t *top, const uint16_t *bottom);
int ds_save_read(void *data, unsigned size);
int ds_save_write(const void *data, unsigned size);
int ds_play_sample(const int8_t *data, unsigned samples, unsigned rate,
                   int volume, int loop);
void ds_stop_sample(int channel);
void ds_audio_start(const int8_t *data, unsigned bytes, int frequency);
void ds_audio_click(int kind);
void ds_drive_audio(int speed,int accelerating,int drift,int horn,int active);
unsigned ds_profile_clock(void);

#endif
