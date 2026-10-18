/* Original ARM7 companion. libnds supplies calibrated touch, FIFO and PCM. */
#include <nds.h>

static void input_vcount(void) { inputGetAndSend(); }
static void idle_vblank(void) {}

int main(void) {
    readUserSettings();
    irqInit();
    initClockIRQ();
    fifoInit();
    SetYtrigger(80);
    installSoundFIFO();
    installSystemFIFO();
    irqSet(IRQ_VCOUNT, input_vcount);
    irqSet(IRQ_VBLANK, idle_vblank);
    irqEnable(IRQ_VBLANK | IRQ_VCOUNT);
    fifoSendValue32(FIFO_USER_01, 0x41535452);
    while (1) swiWaitForVBlank();
    return 0;
}
