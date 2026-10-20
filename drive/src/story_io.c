/* Art/dialogue stay on the cartridge rather than consuming the DS's 4 MiB RAM. */
#include "story.h"
#ifdef ARM9
#include "platform.h"
int story_read(unsigned offset,void*destination,unsigned bytes){return ds_story_read(offset,destination,bytes);}
#else
#include <stdio.h>
int story_read(unsigned offset,void*destination,unsigned bytes){
 FILE*f=fopen("build/nitro/story.bin","rb");if(!f)return 0;
 int okay=!fseek(f,(long)offset,SEEK_SET)&&fread(destination,1,bytes,f)==bytes;fclose(f);return okay;
}
#endif
int story_asset(int id,uint8_t*destination,uint16_t*palette,unsigned capacity,int*w,int*h){
 uint32_t header[8],entry[4];if(id<0||id>=STORY_ASSETS||!story_read(0,header,sizeof header))return 0;
 if(header[0]!=0x4f545343u||header[1]!=0x32305952u||header[5]!=STORY_ASSETS)return 0;
 if(!story_read(header[4]+(unsigned)id*16,entry,sizeof entry))return 0;
 *w=(int)(entry[1]&65535u);*h=(int)(entry[1]>>16);
 if(!*w||!*h||entry[2]!=(unsigned)(*w)*(unsigned)(*h)||entry[2]>capacity)return 0;
 return story_read(entry[0],palette,512)&&story_read(entry[0]+512,destination,entry[2]);
}
