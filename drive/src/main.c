#include "platform.h"
#include "game.h"
#include "render.h"
#include "assets.h"
#include "gfx.h"

Game game;
volatile uint32_t update_profile;
static uint16_t bottom_buffer[256*192] __attribute__((aligned(32)));
_Static_assert(SAVE_DISK_SIZE<=512,"Expanded saves must fit existing batteries");
static uint32_t map_keys(uint32_t keys){
 uint32_t k=0;
 if(keys&DS_UP)k|=K_UP;
 if(keys&DS_DOWN)k|=K_DOWN;
 if(keys&DS_LEFT)k|=K_LEFT;
 if(keys&DS_RIGHT)k|=K_RIGHT;
 if(keys&DS_A)k|=K_A;
 if(keys&DS_B)k|=K_B;
 if(keys&DS_L)k|=K_L;
 if(keys&DS_R)k|=K_R;
 if(keys&DS_START)k|=K_START;
 if(keys&DS_SELECT)k|=K_SELECT;
 return k;
}
int main(void){
 ds_init();gfx_init();game_init(&game,0x434f4153u);
 /* Keep the on-cartridge image inside the original 4 Kbit EEPROM so an
    imported battery save never needs a different physical chip geometry. */
 uint8_t saved[512];
 if(ds_save_read(saved,sizeof(saved)))game_load_bytes(&game,saved,sizeof(saved));
 ds_audio_settings(game.save.music_on,game.save.music_volume,game.save.sfx_on,game.save.sfx_volume);
 ds_audio_start(music_pcm,MUSIC_BYTES,MUSIC_RATE);
 ds_drive_audio(0,0,0,0,0);
 for(;;){
  ds_frame_begin();
  Input input={map_keys(ds_keys_held()),map_keys(ds_keys_down())};
  unsigned old_mode=game.mode,health=game.health,coins=game.run.count[B_COIN];
  unsigned update_start=ds_profile_clock();
  game_update(&game,&input);
  update_profile=(ds_profile_clock()-update_start)&65535u;
  ds_audio_settings(game.save.music_on,game.save.music_volume,game.save.sfx_on,game.save.sfx_volume);
  if(old_mode!=M_RUN&&(input.pressed&(K_A|K_B|K_LEFT|K_RIGHT)))ds_audio_click(0);
  if(game.mode==M_RECEIPT&&old_mode!=M_RECEIPT)ds_audio_click(2);
  else if(game.mode==M_RUN&&game.health<health)ds_audio_click(2);
  else if(game.run.count[B_COIN]>coins)ds_audio_click(3);
  if(!(game.frame&1u))ds_drive_audio(game.speed_q8/Q,
      !!(input.held&K_R)&&!(input.held&K_L),game.drifting,game.horn_timer,game.mode==M_RUN);
  if(game.save_requested){
   int encoded=game_encode_save(&game,saved,sizeof(saved));
   game_save_result(&game,encoded && ds_save_write(saved,SAVE_DISK_SIZE));
  }
  /* The platform already owns two VRAM pages for the upper screen. Draw
     directly into the hidden page instead of retaining a third RAM copy. */
  uint16_t *top_buffer=ds_top_buffer();
  int changed=render_game(&game,top_buffer,bottom_buffer);
  ds_present(changed&1?top_buffer:0,changed&2?bottom_buffer:0);
 }
}
