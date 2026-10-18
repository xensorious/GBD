#include "platform.h"
#include "game.h"
#include "world.h"
#include "render.h"
#include "assets.h"
#include <string.h>

Game game;
static uint16_t bottom_buffer[256*192] __attribute__((aligned(32)));
static uint32_t map_keys(uint32_t k){
 uint32_t out=0;
 if(k&DS_A)out|=GK_A;
 if(k&DS_B)out|=GK_B;
 if(k&DS_SELECT)out|=GK_SELECT;
 if(k&DS_START)out|=GK_START;
 if(k&DS_UP)out|=GK_UP;
 if(k&DS_DOWN)out|=GK_DOWN;
 if(k&DS_LEFT)out|=GK_LEFT;
 if(k&DS_RIGHT)out|=GK_RIGHT;
 if(k&DS_L)out|=GK_L;
 if(k&DS_R)out|=GK_R;
 if(k&DS_X)out|=GK_X;
 if(k&DS_Y)out|=GK_Y;
 return out;
}
int main(void){
 ds_init();game_init(&game,0x41535452);game_set_world(&game,&aster_world);
 GameSave saved;
 if(ds_save_read(&saved,sizeof(saved)))game_load(&game,&saved);
 ds_audio_start(music_pcm,MUSIC_BYTES,MUSIC_RATE);
 for(;;){
  ds_vblank();
  GameInput in={0};in.held=map_keys(ds_keys_held());in.pressed=map_keys(ds_keys_down());
  GameMode previous=game.mode;unsigned before_count=game.save.party_count,before_flags=game.save.flags;
  game_update(&game,&in);
  if(in.pressed&(GK_A|GK_SELECT|GK_L|GK_R))ds_audio_click(0);
  if(game.save.party_count>before_count||(game.save.flags&PF_TRIAL_WON)!=(before_flags&PF_TRIAL_WON))ds_audio_click(3);
  else if(game.mode==GM_BATTLE&&previous!=GM_BATTLE)ds_audio_click(1);
  if(game.save_requested){game_make_save(&game,&saved);int okay=ds_save_write(&saved,sizeof(saved));game_save_result(&game,okay);}
  uint16_t *top_buffer=ds_top_buffer();
  int changed=render_game(&game,top_buffer,bottom_buffer);
  ds_present(changed&1?top_buffer:0,changed&2?bottom_buffer:0);
 }
}
