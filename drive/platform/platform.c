#include "platform.h"
#include <nds.h>
#include <nds/fifomessages.h>
#include <string.h>

#define SAVE_CAPACITY 8192u
static int save_bus_ready;
static int music_channel=-1;
static int8_t click_samples[4][1024] __attribute__((aligned(4)));
static int clicks_ready;
static int audio_ready;
static unsigned top_front_bank;
static volatile unsigned lcd_frames;
static unsigned frame_deadline;
static int paced_frames;
static int world_active;
static unsigned world_epoch;
static void vblank_tick(void) { ++lcd_frames; }
static void wait_lcd_vblank(void) {
    while (REG_VCOUNT >= 192) {}
    while (REG_VCOUNT < 192) {}
}

void ds_init(void) {
    powerOn(POWER_ALL_2D);
    lcdMainOnTop();
    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_BG_0x06020000);
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_LCD);
    vramSetBankE(VRAM_E_LCD);
    REG_BG3CNT = BG_BMP16_256x256 | BG_BMP_BASE(0);
    REG_BG3CNT_SUB = BG_BMP16_256x256 | BG_BMP_BASE(0);
    REG_BG3PA = REG_BG3PD = 256;
    REG_BG3PB = REG_BG3PC = 0;
    REG_BG3X = REG_BG3Y = 0;
    REG_BG3PA_SUB = REG_BG3PD_SUB = 256;
    REG_BG3PB_SUB = REG_BG3PC_SUB = 0;
    REG_BG3X_SUB = REG_BG3Y_SUB = 0;
    dmaFillHalfWords(0x8000, BG_BMP_RAM(0), 256*256*2);
    dmaFillHalfWords(0x8000, BG_BMP_RAM(8), 256*256*2);
    dmaFillHalfWords(0x8000, BG_BMP_RAM_SUB(0), 256*256*2);
    irqSet(IRQ_VBLANK, vblank_tick);
    irqEnable(IRQ_VBLANK);
    /* ARM7 may still be installing its sound handler when ARM9 finishes
     * clearing a small program's BSS. Start audio only after its ready reply. */
    for (int frame=0; frame<120 && !fifoCheckValue32(FIFO_USER_01); ++frame)
        ds_vblank();
    if (fifoCheckValue32(FIFO_USER_01))
        audio_ready=fifoGetValue32(FIFO_USER_01)==0x41535452;
    if (audio_ready) soundEnable();
    top_front_bank=0;
    TIMER0_DATA=0;
    TIMER0_CR=TIMER_ENABLE|TIMER_DIV_64;
    scanKeys();
}
unsigned ds_profile_clock(void) { return TIMER0_DATA; }

void ds_vblank(void) {
    /* Synchronize directly to the LCD counter. FIFO interrupts remain enabled;
     * this also works with frontend-provided replacement DS BIOS routines. */
    wait_lcd_vblank();
    scanKeys();
}
void ds_frame_begin(void) {
    /* One game update owns two LCD frames. Rendering may use both of them;
       a second unconditional input wait would discard that available time. */
    if (!paced_frames) { wait_lcd_vblank(); frame_deadline=lcd_frames; paced_frames=1; }
    frame_deadline+=2;
    /* A single cold sprite or save transaction must not shift every later
       update. Retain the cadence, but bound catch-up after a long stall. */
    if ((int32_t)(lcd_frames-frame_deadline)>4) frame_deadline=lcd_frames+2;
    scanKeys();
}
uint32_t ds_keys_held(void) { return keysHeld(); }
uint32_t ds_keys_down(void) { return keysDown(); }
int ds_touch(int *x, int *y) {
    touchPosition p;
    if (!(keysHeld() & KEY_TOUCH)) return 0;
    touchRead(&p);
    *x = p.px; *y = p.py;
    return 1;
}

uint16_t *ds_top_buffer(void) {
    if (world_active) return BG_BMP_RAM(8);
    return BG_BMP_RAM((top_front_bank^1u)*8);
}
unsigned ds_world_epoch(void) { return world_epoch; }
void ds_world_upload_begin(int all_banks) {
    /* A prepared player pose takes only a small row transfer. Reuse the
     * current blank if at least 43 blank scanlines remain; never wait an
     * extra whole LCD frame just because input was sampled during vblank. */
    if(REG_VCOUNT<192||REG_VCOUNT>=220)wait_lcd_vblank();
    if(all_banks){vramSetBankA(VRAM_A_LCD);vramSetBankE(VRAM_E_LCD);}
    vramSetBankD(VRAM_D_LCD);
}
void ds_world_upload_end(int all_banks) {
    if(all_banks)vramSetBankA(VRAM_A_TEXTURE_SLOT0);
    vramSetBankD(VRAM_D_TEXTURE_SLOT1);
    if(all_banks)vramSetBankE(VRAM_E_TEX_PALETTE);
}
void ds_world_mode(int active) {
    active=!!active;if (active==world_active) return;
    wait_lcd_vblank();
    if (active) {
        powerOn(POWER_3D_CORE|POWER_MATRIX);
        while(GFX_STATUS&(1<<27)) {}
        GFX_STATUS|=1<<29;
        glResetMatrixStack();
        GFX_CONTROL=GL_TEXTURE_2D|GL_BLEND|GL_ANTIALIAS;
        GFX_CLEAR_COLOR=RGB15(5,16,19)|(31u<<16)|(63u<<24);
        GFX_CLEAR_DEPTH=0x7fff;
        GFX_TEX_FORMAT=0;GFX_POLY_FORMAT=0;
        glViewport(0,0,255,191);
        glFlush(0);
        ds_world_upload_end(1);
        dmaFillHalfWords(0,BG_BMP_RAM(8),256*256*2);
        REG_BG0CNT=BG_PRIORITY(0);
        videoSetMode(MODE_0_3D);
        ++world_epoch;
    } else {
        while(GFX_STATUS&(1<<27)) {}
        videoSetMode(MODE_5_2D|DISPLAY_BG3_ACTIVE);
        vramSetBankA(VRAM_A_MAIN_BG);
        vramSetBankD(VRAM_D_LCD);vramSetBankE(VRAM_E_LCD);
        top_front_bank=1;
        REG_BG3CNT=BG_BMP16_256x256|BG_BMP_BASE(8);
    }
    world_active=active;
}
/* D (128 KiB) and E (64 KiB) are contiguous LCDC memory: one tall movie
 * frame without a second full-screen allocation in the DS's main RAM. */
uint16_t *ds_cinematic_buffer(void) { return (uint16_t*)0x06860000; }
void ds_present(const uint16_t *top, const uint16_t *bottom) {
    unsigned next=top_front_bank^1u;
    uint16_t *back=BG_BMP_RAM(next*8);
    if (top && top!=back && !world_active) {
        /* RAM framebuffer fallback, used by simple platform test programs. */
        DC_FlushRange(top, 256*192*2);
        dmaCopyWords(0, top, back, 256*192*2);
    }
    /* Both screens share the deadline. Input is sampled once at frame begin. */
    if (paced_frames) {
        while ((int32_t)(lcd_frames-frame_deadline)<0) wait_lcd_vblank();
        if (REG_VCOUNT<192) wait_lcd_vblank();
    } else wait_lcd_vblank();
    if (top && !world_active) {
        REG_BG3CNT=BG_BMP16_256x256 | BG_BMP_BASE(next*8);
        top_front_bank=next;
    }
    if (bottom) {
        DC_FlushRange(bottom, 256*192*2);
        dmaCopyWords(0, bottom, BG_BMP_RAM_SUB(0), 256*192*2);
    }
}

/* A real 8 KiB cartridge EEPROM, rather than a host filesystem. The emulator
 * owns its persistence just as it does for an ordinary DS cartridge. */
static void save_prepare(void) {
    sysSetCartOwner(BUS_OWNER_ARM9);
    if (!save_bus_ready) {
        uint8_t probe;
        /* A complete one-byte read identifies the address width for emulators
         * that infer cartridge EEPROM geometry from the first transaction. */
        /* Explicit HOLD release is required by both real SPI devices and
         * older emulators which infer geometry on the chip-select edge. */
        REG_AUXSPICNT=0xa040;
        REG_AUXSPIDATA=0x03; eepromWaitBusy();
        REG_AUXSPIDATA=0; eepromWaitBusy();
        REG_AUXSPIDATA=0; eepromWaitBusy();
        REG_AUXSPICNT=0xa000;
        REG_AUXSPIDATA=0; eepromWaitBusy();
        probe=REG_AUXSPIDATA;
        REG_AUXSPICNT=0;
        (void)probe;
        save_bus_ready=1;
    }
}
static uint8_t save_transfer(uint8_t value) {
    REG_AUXSPIDATA=value;
    eepromWaitBusy();
    return REG_AUXSPIDATA;
}
static void save_read_at(unsigned address, uint8_t *data, unsigned size) {
    if (!size) return;
    REG_AUXSPICNT=0xa040;
    save_transfer(0x03);
    save_transfer(address>>8);
    save_transfer(address);
    for (unsigned i=0; i<size; ++i) {
        if (i+1==size) REG_AUXSPICNT=0xa000;
        data[i]=save_transfer(0);
    }
    REG_AUXSPICNT=0;
}
int ds_save_read(void *data, unsigned size) {
    if (size > SAVE_CAPACITY) return 0;
    save_prepare();
    save_read_at(0, data, size);
    return 1;
}
int ds_save_write(const void *data, unsigned size) {
    return ds_save_write_at(0,data,size);
}
int ds_save_write_at(unsigned start,const void *data,unsigned size) {
    if (start>SAVE_CAPACITY || size>SAVE_CAPACITY-start) return 0;
    save_prepare();
    const uint8_t *bytes=data;
    for (unsigned address=start; address<start+size;) {
        unsigned count=32-(address&31);
        if (count>start+size-address) count=start+size-address;
        REG_AUXSPICNT=0xa000;
        save_transfer(0x06); /* write enable */
        REG_AUXSPICNT=0;
        REG_AUXSPICNT=0xa040;
        save_transfer(0x02);
        save_transfer(address>>8);
        save_transfer(address);
        for (unsigned i=0; i<count; ++i) {
            if (i+1==count) REG_AUXSPICNT=0xa000;
            save_transfer(bytes[address-start+i]);
        }
        REG_AUXSPICNT=0;
        int ready=0;
        for (unsigned retry=0; retry<4096; ++retry) {
            REG_AUXSPICNT=0xa040;
            save_transfer(0x05);
            REG_AUXSPICNT=0xa000;
            uint8_t status=save_transfer(0);
            REG_AUXSPICNT=0;
            if (status==0xff) return 0;
            if (!(status&1)) { ready=1; break; }
        }
        if (!ready) return 0;
        uint8_t check[32];
        save_read_at(address, check, count);
        if (memcmp(check, bytes+address-start, count)) return 0;
        address+=count;
    }
    return 1;
}

int ds_play_sample(const int8_t *data, unsigned samples, unsigned rate,
                   int volume, int loop) {
    if (!audio_ready) return -1;
    DC_FlushRange(data, samples);
    FifoMessage message={0};
    message.type=SOUND_PLAY_MESSAGE;
    message.SoundPlay.data=data;
    message.SoundPlay.freq=rate;
    message.SoundPlay.volume=volume;
    message.SoundPlay.pan=64;
    message.SoundPlay.loop=loop!=0;
    message.SoundPlay.format=SoundFormat_8Bit;
    message.SoundPlay.dataSize=samples>>2;
    fifoSendDatamsg(FIFO_SOUND, sizeof(message), (uint8_t *)&message);
    /* ARM7 normally replies within a few microseconds. Let its FIFO IRQ run
     * before yielding a whole LCD frame; otherwise every road-coin sound
     * spends 16.7 ms waiting even when the reply is already on its way. Keep
     * the existing bounded frame fallback for an unusually busy ARM7. */
    for (unsigned spin=0;spin<8192 && !fifoCheckValue32(FIFO_SOUND);++spin)
        __asm__ volatile("nop");
    for (int frame=0; frame<60 && !fifoCheckValue32(FIFO_SOUND); ++frame)
        ds_vblank();
    if (!fifoCheckValue32(FIFO_SOUND)) { audio_ready=0; return -1; }
    return (int)fifoGetValue32(FIFO_SOUND);
}
void ds_stop_sample(int channel) { if (channel >= 0) soundKill(channel); }

static int music_mix=70,sfx_mix=75,music_enabled=1,sfx_enabled=1;
static int mix_volume(int volume,int mix,int enabled) {
    if (!enabled) return 0;
    int scaled=volume*mix/100;
    return scaled<0?0:scaled>127?127:scaled;
}
void ds_audio_settings(int music_on,int music_volume,int sfx_on,int sfx_volume) {
    int changed=music_enabled!=!!music_on || music_mix!=music_volume;
    music_enabled=!!music_on; sfx_enabled=!!sfx_on;
    music_mix=music_volume<0?0:music_volume>100?100:music_volume;
    sfx_mix=sfx_volume<0?0:sfx_volume>100?100:sfx_volume;
    if (changed && music_channel>=0)
        soundSetVolume(music_channel,(uint8_t)mix_volume(58,music_mix,music_enabled));
}
void ds_audio_start(const int8_t *data, unsigned bytes, int frequency) {
    if (music_channel >= 0) soundKill(music_channel);
    music_channel=-1;
    if (data && bytes >= 4 && frequency > 0)
        music_channel=ds_play_sample(data, bytes & ~3u, frequency,
                                    mix_volume(58,music_mix,music_enabled),1);
}
void ds_audio_click(int kind) {
    if (!sfx_enabled || !sfx_mix) return;
    if (!clicks_ready) {
        for (int k=0; k<4; ++k) {
            unsigned phase=0;
            for (int i=0; i<1024; ++i) {
                unsigned hz=(unsigned[]){660,880,210,1100}[k];
                if (k==2) hz+=(1024-i)/4;
                if (k==3) hz+=i/2;
                phase+=hz*65536u/16000;
                int p=(phase>>8)&255;
                int triangle=p<128 ? p*2-128 : 383-p*2;
                click_samples[k][i]=(triangle*(1024-i)*3)/(1024*5);
            }
        }
        clicks_ready=1;
    }
    ds_play_sample(click_samples[kind&3],1024,16000,mix_volume(85,sfx_mix,sfx_enabled),0);
}

/* Three persistent channels avoid waiting on the ARM7 for every rev change. */
void ds_drive_audio(int speed,int accelerating,int drift,int horn,int active) {
    static int ready,engine_channel=-1,road_channel=-1,horn_channel=-1;
    static int old_rate=-1,old_engine=-1,old_road=-1,old_horn=-1;
    static int8_t engine[512] __attribute__((aligned(4)));
    static int8_t air[1024] __attribute__((aligned(4)));
    static int8_t beep[256] __attribute__((aligned(4)));
    if (!audio_ready) return;
    if (!ready) {
        uint32_t noise=0x434f4153;
        for (int i=0;i<1024;i++) {
            noise=noise*1664525u+1013904223u;
            air[i]=(int8_t)((int)(noise>>25)-64);
        }
        for (int i=0;i<512;i++) {
            int p=i&63,triangle=p<32?p*4-64:191-p*4;
            int pulse=(i&15)<7?20:-20;
            engine[i]=(int8_t)(triangle/2+pulse);
        }
        for(int i=0;i<256;i++) {
            int a=(i*8)&255,b=(i*10)&255;
            a=a<128?a-64:191-a;b=b<128?b-64:191-b;
            beep[i]=(int8_t)((a+b)/2);
        }
        engine_channel=ds_play_sample(engine,512,6000,0,1);
        road_channel=ds_play_sample(air,1024,12000,0,1);
        horn_channel=ds_play_sample(beep,256,12000,0,1);
        ready=1;
    }
    if (speed<0) speed=-speed;
    int rate=5000+(speed%45)*130+(accelerating?1600:0);
    int ev=active?18+(accelerating?16:0)+speed/18:0;
    int rv=active?(drift?38:speed/15):0,hv=active&&horn?72:0;
    ev=mix_volume(ev,sfx_mix,sfx_enabled);
    rv=mix_volume(rv,sfx_mix,sfx_enabled);
    hv=mix_volume(hv,sfx_mix,sfx_enabled);
    if(engine_channel>=0){
        if(rate!=old_rate)soundSetFreq(engine_channel,(uint16_t)rate);
        if(ev!=old_engine)soundSetVolume(engine_channel,(uint8_t)ev);
    }
    if(road_channel>=0&&rv!=old_road)soundSetVolume(road_channel,(uint8_t)rv);
    if(horn_channel>=0&&hv!=old_horn)soundSetVolume(horn_channel,(uint8_t)hv);
    old_rate=rate;old_engine=ev;old_road=rv;old_horn=hv;
}
