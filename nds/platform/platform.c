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
static void vblank_tick(void) {}
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
    scanKeys();
}

void ds_vblank(void) {
    /* Synchronize directly to the LCD counter. FIFO interrupts remain enabled;
     * this also works with frontend-provided replacement DS BIOS routines. */
    wait_lcd_vblank();
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
    return BG_BMP_RAM((top_front_bank^1u)*8);
}
void ds_present(const uint16_t *top, const uint16_t *bottom) {
    unsigned next=top_front_bank^1u;
    uint16_t *back=BG_BMP_RAM(next*8);
    if (top && top!=back) {
        /* RAM framebuffer fallback, used by simple platform test programs. */
        DC_FlushRange(top, 256*192*2);
        dmaCopyWords(0, top, back, 256*192*2);
    }
    /* Even an unchanged frame consumes one presentation boundary. Gameplay's
     * outer ds_vblank samples input separately, preserving button edges. */
    wait_lcd_vblank();
    if (top) {
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
    if (size > SAVE_CAPACITY) return 0;
    save_prepare();
    const uint8_t *bytes=data;
    for (unsigned address=0; address<size;) {
        unsigned count=32-(address&31);
        if (count>size-address) count=size-address;
        REG_AUXSPICNT=0xa000;
        save_transfer(0x06); /* write enable */
        REG_AUXSPICNT=0;
        REG_AUXSPICNT=0xa040;
        save_transfer(0x02);
        save_transfer(address>>8);
        save_transfer(address);
        for (unsigned i=0; i<count; ++i) {
            if (i+1==count) REG_AUXSPICNT=0xa000;
            save_transfer(bytes[address+i]);
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
        if (memcmp(check, bytes+address, count)) return 0;
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
    for (int frame=0; frame<60 && !fifoCheckValue32(FIFO_SOUND); ++frame)
        ds_vblank();
    if (!fifoCheckValue32(FIFO_SOUND)) { audio_ready=0; return -1; }
    return (int)fifoGetValue32(FIFO_SOUND);
}
void ds_stop_sample(int channel) { if (channel >= 0) soundKill(channel); }

void ds_audio_start(const int8_t *data, unsigned bytes, int frequency) {
    if (music_channel >= 0) soundKill(music_channel);
    music_channel=-1;
    if (data && bytes >= 4 && frequency > 0)
        music_channel=ds_play_sample(data, bytes & ~3u, frequency, 58, 1);
}
void ds_audio_click(int kind) {
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
    ds_play_sample(click_samples[kind&3], 1024, 16000, 85, 0);
}
