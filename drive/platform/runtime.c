/* Small newlib integration for standard Debian ARM GCC. This deliberately
 * avoids the devkitPro-specific newlib syscall ABI.
 *
 * DS initialization adapted from libnds initSystem.c:
 * Copyright (C) 2007-2021 Dave Murphy (WinterMute).
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */
#include <nds.h>
#include <stddef.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef ARM9
#include <libnds_internal.h>
bool __dsimode;
char *fake_heap_start;
char *fake_heap_end;
void initSystem(void) {
    for (int i=0; i<4; ++i) {
        DMA_CR(i)=0; TIMER_CR(i)=0; TIMER_DATA(i)=0;
    }
    dmaFillWords(0, (void *)0x04000000, 0x56);
    dmaFillWords(0, (void *)0x04001008, 0x56);
    videoSetModeSub(0);
    vramDefault();
    VRAM_E_CR=VRAM_F_CR=VRAM_G_CR=VRAM_H_CR=VRAM_I_CR=0;
    irqInit();
    fifoInit();
    fifoSetValue32Handler(FIFO_SYSTEM, systemValueHandler, 0);
    fifoSetDatamsgHandler(FIFO_SYSTEM, systemMsgHandler, 0);
    __transferRegion()->buttons=0xffff;
    irqEnable(IRQ_VBLANK);
}
void build_argv(void) {}
void *_sbrk(ptrdiff_t increment) {
    static char *head;
    if (!head) head=fake_heap_start;
    char *old=head;
    if (head+increment > fake_heap_end) { errno=ENOMEM; return (void *)-1; }
    head+=increment;
    return old;
}
#else
void *_sbrk(ptrdiff_t increment) { (void)increment; return (void *)-1; }
#endif
void __attribute__((noreturn)) __libnds_exit(int status) { (void)status; while (1) swiWaitForVBlank(); }
void _init(void) {}
void _fini(void) {}
int _write(int fd, const void *buf, size_t size) { (void)fd;(void)buf;return size; }
int _read(int fd, void *buf, size_t size) { (void)fd;(void)buf;(void)size;return 0; }
int _close(int fd) { (void)fd;return -1; }
int _fstat(int fd, struct stat *s) { (void)fd;s->st_mode=S_IFCHR;return 0; }
int _isatty(int fd) { (void)fd;return 1; }
int _lseek(int fd, int off, int way) { (void)fd;(void)off;(void)way;return 0; }
void _exit(int status) { __libnds_exit(status); }
int _kill(int pid, int sig) { (void)pid;(void)sig;errno=EINVAL;return -1; }
int _getpid(void) { return 1; }
