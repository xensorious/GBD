/* Career is a browsing foundation: test every event and protect real saves. */
#include "career.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"campaign:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void press(Game*g,unsigned key){Input in={key,key};game_update(g,&in);}
static struct {unsigned before;uint16_t top[256*192],bottom[256*192];unsigned after;} screens;
int main(void){
 Game g;game_init(&g,123);Save original=g.save;
 g.mode=M_DRIVE;g.cursor=0;press(&g,K_DOWN);CHECK(g.cursor==2);press(&g,K_A);CHECK(g.mode==M_STORY);
 for(int c=0;c<5;c++)for(int e=0;e<25;e++){
  int x,y,xx,yy;career_point(c,e,&x,&y);career_point(c,e,&xx,&yy);
  CHECK(x==xx&&y==yy&&x>=18&&x<=228&&y>=35&&y<=147);
  CHECK(career_event_name(c,e)[0]&&career_event_format(c,e)[0]);
  CHECK(career_event_kind(e)==(e==24?2:e==11?1:0));
  for(int j=0;j<e;j++){career_point(c,j,&xx,&yy);CHECK((x-xx)*(x-xx)+(y-yy)*(y-yy)>=225);CHECK(strcmp(career_event_name(c,e),career_event_name(c,j)));}
  for(int n=0;n<=25;n++){campaign.preview_done[c]=(uint8_t)n;CHECK(career_status(c,e)==(e<n?2:e==n?1:0));}
 }
 career_reset();career_enter(&g);
 for(int c=0;c<5;c++){
  CHECK(campaign.chapter==c);g.cursor=1;
  for(int e=0;e<25;e++){CHECK(campaign.event==e);press(&g,K_A);CHECK(campaign.modal==1);for(int page=0;page<3;page++){CHECK(campaign.page==page);press(&g,K_A);}press(&g,K_B);CHECK(!campaign.modal);press(&g,K_RIGHT);}
  CHECK(!campaign.event);g.cursor=0;press(&g,K_RIGHT);
 }
 CHECK(!campaign.chapter);CHECK(!memcmp(&g.save,&original,sizeof original));
 for(int tab=0;tab<5;tab++){
  CHECK(campaign.tab==tab);
  for(int row=0;row<career_rows(tab);row++){g.cursor=(uint8_t)row;press(&g,K_DOWN);CHECK(g.cursor==(row+1)%career_rows(tab));press(&g,K_UP);CHECK(g.cursor==row);}
  press(&g,K_R);
 }
 CHECK(!campaign.tab);press(&g,K_L);CHECK(campaign.tab==4);g.cursor=1;
 for(int i=1;i<=25;i++){press(&g,K_RIGHT);CHECK(campaign.preview_done[0]==i);CHECK(campaign.event==(i<25?i:24));}
 g.cursor=2;press(&g,K_A);CHECK(!campaign.preview_done[0]&&!campaign.event);
 g.cursor=0;press(&g,K_A);CHECK(g.mode==M_GARAGE&&campaign.garage_return);press(&g,K_B);CHECK(g.mode==M_STORY&&!campaign.garage_return&&campaign.tab==4);
 CHECK(!memcmp(&g.save,&original,sizeof original));
 /* Draw all chapters, all tabs, all reading pages in every shipped theme. */
 screens.before=screens.after=0xa5a5a5a5u;
 for(int skin=SKIN_FUSION;skin<SKIN_STORAGE_LIMIT;skin++)for(int c=0;c<5;c++)for(int tab=0;tab<5;tab++){
  g.save.skin=(uint8_t)skin;campaign.chapter=(uint8_t)c;campaign.tab=(uint8_t)tab;
  for(int row=0;row<career_rows(tab);row++)for(int page=-1;page<3;page++){
   g.cursor=(uint8_t)row;campaign.modal=page<0?0:(uint8_t)(tab==0?1:tab+1);campaign.page=(uint8_t)(page<0?0:page);campaign.event=(uint8_t)(row==1?11:row==2?24:row);campaign.revision++;g.frame+=2;
   render_game(&g,screens.top,screens.bottom);CHECK(screens.before==0xa5a5a5a5u&&screens.after==0xa5a5a5a5u);
  }
 }
 campaign.modal=0;press(&g,K_B);CHECK(g.mode==M_DRIVE&&g.cursor==2);press(&g,K_UP);CHECK(g.cursor<2);
 g.cursor=0;press(&g,K_A);CHECK(g.mode==M_RUN);
 uint8_t disk[SAVE_DISK_SIZE];CHECK(game_encode_save(&g,disk,sizeof disk));Game restored;game_init(&restored,9);CHECK(game_load_bytes(&restored,disk,sizeof disk));CHECK(!campaign.preview_done[0]&&!campaign.modal);CHECK(restored.save.owned==original.owned&&restored.save.cash==original.cash);
 printf("campaign: %u checks; 125 events, 5 chapters, 6 themes, button navigation and unchanged v6 save layout pass\n",checks);return 0;
}
