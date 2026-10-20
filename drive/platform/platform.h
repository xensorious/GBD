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
uint16_t *ds_cinematic_buffer(void);
/* Free Roam owns the 3D engine and texture banks A/D/E. */
void ds_world_mode(int active);
unsigned ds_world_epoch(void);
void ds_world_upload_begin(int all_banks);
void ds_world_upload_end(int all_banks);
void ds_present(const uint16_t *top, const uint16_t *bottom);
int ds_story_read(unsigned offset,void *data,unsigned size);
int ds_save_read(void *data, unsigned size);
int ds_save_write(const void *data, unsigned size);
int ds_save_write_at(unsigned address,const void *data,unsigned size);
int ds_play_sample(const int8_t *data, unsigned samples, unsigned rate,
                   int volume, int loop);
void ds_stop_sample(int channel);
void ds_audio_start(const int8_t *data, unsigned bytes, int frequency);
/* Independent live mixers; muted loops keep their phase and need no restart. */
void ds_audio_settings(int music_on,int music_volume,int sfx_on,int sfx_volume);
void ds_audio_click(int kind);
void ds_drive_audio(int speed,int accelerating,int drift,int horn,int active);
unsigned ds_profile_clock(void);

#endif
