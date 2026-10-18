/* Small headless interface to the real mGBA core, used only by tests. */
#include <mgba/flags.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/blip_buf.h>
#include <mgba-util/vfs.h>
#include <stdlib.h>
#include <stdint.h>
static struct mCore *core;
static color_t pixels[240*160];
static unsigned audioNonzero;
static void quiet(struct mLogger*l,int c,enum mLogLevel v,const char*f,va_list a){(void)l;(void)c;(void)v;(void)f;(void)a;}
static struct mLogger logger={.log=quiet};
int emu_open(const char *path){
 mLogSetDefaultLogger(&logger);core=mCoreFind(path);if(!core||!core->init(core))return 0;
 mCoreInitConfig(core,NULL);
 mCoreConfigSetDefaultIntValue(&core->config,"volume",256);
 mCoreConfigSetDefaultIntValue(&core->config,"sampleRate",32768);
 mCoreConfigSetDefaultIntValue(&core->config,"audioBuffers",4096);
 mCoreLoadForeignConfig(core,&core->config);core->setVideoBuffer(core,pixels,240);
 core->setAudioBufferSize(core,4096);audioNonzero=0;
 if(!mCoreLoadFile(core,path))return 0;core->reset(core);
 blip_set_rates(core->getAudioChannel(core,0),core->frequency(core),32768);
 blip_set_rates(core->getAudioChannel(core,1),core->frequency(core),32768);
 return 1;
}
void emu_close(void){if(core){core->deinit(core);core=NULL;}}
void emu_frame(int keys,int frames){
 core->setKeys(core,keys);
 for(int i=0;i<frames;i++){
  core->runFrame(core);
  for(int ch=0;ch<2;ch++){
   short samples[4096];struct blip_t*b=core->getAudioChannel(core,ch);
   int n=blip_read_samples(b,samples,4096,0);
   for(int j=0;j<n;j++)if(samples[j])audioNonzero++;
  }
 }
}
unsigned emu_read(unsigned addr,int size){if(size==1)return core->busRead8(core,addr);if(size==2)return core->busRead16(core,addr);return core->busRead32(core,addr);}
const void *emu_pixels(void){return pixels;}
int emu_save(const char *path){void *save=NULL;size_t n=core->savedataClone(core,&save);FILE*f=fopen(path,"wb");if(!f||!save)return 0;fwrite(save,1,n,f);fclose(f);free(save);return n>0;}
int emu_load_save(const char*path){return mCoreLoadSaveFile(core,path,false);}
unsigned emu_pc(void){uint32_t out=0;core->readRegister(core,"pc",&out);return out;}
unsigned emu_frames(void){return core->frameCounter(core);}
int emu_audio_nonzero(void){
 return audioNonzero;
}
