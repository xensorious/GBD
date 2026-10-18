#include "platform.h"
#include "game.h"
#include "render.h"
#include "assets.h"
#include "gfx.h"

Game game;
static uint16_t bottom_buffer[256*192] __attribute__((aligned(32)));
static uint16_t top_buffer[256*192] __attribute__((aligned(32)));
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
 Save saved;
 if(ds_save_read(&saved,sizeof(saved)))game_load(&game,&saved);
 ds_audio_start(music_pcm,MUSIC_BYTES,MUSIC_RATE);
 ds_drive_audio(0,0,0,0,0);
 for(;;){
  ds_frame_begin();
  Input input={map_keys(ds_keys_held()),map_keys(ds_keys_down())};
  unsigned old_mode=game.mode,health=game.health,coins=game.run.count[B_COIN];
  game_update(&game,&input);
  if(old_mode!=M_RUN&&(input.pressed&(K_A|K_B|K_LEFT|K_RIGHT)))ds_audio_click(0);
  if(game.mode==M_RECEIPT&&old_mode!=M_RECEIPT)ds_audio_click(2);
  else if(game.mode==M_RUN&&game.health<health)ds_audio_click(2);
  else if(game.run.count[B_COIN]>coins)ds_audio_click(3);
  if(!(game.frame&1u))ds_drive_audio(game.speed_q8/Q,!!(input.held&K_R),game.drifting,game.horn_timer,game.mode==M_RUN);
  if(game.save_requested){game_make_save(&game,&saved);game_save_result(&game,ds_save_write(&saved,sizeof(saved)));}
  int changed=render_game(&game,top_buffer,bottom_buffer);
  ds_present(changed&1?top_buffer:0,changed&2?bottom_buffer:0);
 }
}
