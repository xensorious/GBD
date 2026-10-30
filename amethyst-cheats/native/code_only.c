/* Amethyst v1.4.0 Delta-only runtime; ARM7TDMI Thumb with one ARM entry.
 * This is separate from the ROM-patch runtime. The cheat installer supplies the
 * RAM table and thunks at the InitHeap master-code point, outside the save
 * scratch-copy window. ReadKeys invokes a zero-argument heap call to refresh.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef void (*Fn)(void);
#define V8(a) (*(volatile u8 *)(a))
#define V16(a) (*(volatile u16 *)(a))
#define V32(a) (*(volatile u32 *)(a))
#define INSTALL_MAGIC 0x414d5954u
#define RUNTIME_MAGIC 0x52554e31u
struct State {
    u32 install, initialized, walk_patch;
    u16 previous, age, history[10];
    u8 count, release, walk, shiny, invulnerable, alive;
    u16 last_hp[4];
    u32 activations[6], item_success[3], frames, pulse;
    u8 listening, shiny_touched, shiny_original, was_battle;
    u16 last_party[4];
    u32 patch_request;
    u8 entry_mode, entry_hold, entry_latch, entry_block;
    u32 entry_callbacks[2];
};
#define S ((volatile struct State *)0x02010c00u)
#define SCRIPT (*(u8 * volatile *)0x02023d74u)

static int HooksLive(void) {
    return V32(0x080005ecu) == 0x0200cecdu && V16(0x08002b84u) == 0x4710;
}

#define ENTRY_FN __attribute__((section(".entrymode")))
static void entry_gate(void);
/* Callbacks are frozen while keys are captured; the main ReadKeys loop and
 * interrupt/audio processing continue, so the exit chord always remains live.
 */
static ENTRY_FN void entry_callbacks(u32 capture) {
    u32 i, gate = (u32)entry_gate;
    for (i = 0; i < 2; ++i) {
        u32 address = 0x030030f0u + i * 4, current = V32(address);
        if (capture && current != gate) {
            S->entry_callbacks[i] = current;
            V32(address) = gate;
        } else if (!capture && current == gate)
            V32(address) = S->entry_callbacks[i];
    }
}
static ENTRY_FN void entry_gate(void) {
    if (!HooksLive()) {
        S->entry_mode = S->entry_block = S->entry_hold = S->entry_latch = 0;
        entry_callbacks(0);
    }
}
static ENTRY_FN u32 entry_keys(u32 mask) {
    u32 suppress = S->entry_mode || S->entry_block;
    if (mask == 768) { /* L + R */
        suppress = S->entry_block = 1;
        if (!S->entry_latch && ++S->entry_hold >= 30) {
            S->entry_mode ^= 1;
            S->entry_latch = 1;
            S->count = S->previous = S->release = S->age = 0;
        }
    } else {
        S->entry_hold = 0;
        if (!mask) S->entry_latch = S->entry_block = 0;
    }
    return suppress || S->entry_mode;
}

static int player(u32 bank) {
    return bank < 4 && !(V8(0x02023bd6u + bank) & 1);
}
static u32 script_bank(void) {
    return ((u8 (*)(u8))0x088d2a19u)(SCRIPT[1]);
}
static void advance(u32 n) { SCRIPT += n; }
static void guard_dispatch(void) {
    ((Fn)V32(0x0200b960u + (u32)SCRIPT[0] * 4u))();
}
#include "code_only_dispatch.h"
static void hp_bar_guard(void) {
    if (S->listening && HooksLive() && S->invulnerable && V32(0x02023bc8u) == 0) {
        u32 block = V32(0x0203e038u), bank;
        if (block >= 0x02000000u && block <= 0x0203fe60u)
            for (bank = 0; bank < 4; ++bank)
                if (player(bank) && (s32)V32(block + 0x188u + bank * 4u) > 0)
                    V32(block + 0x188u + bank * 4u) = 0;
        if (player(script_bank()) && (s32)V32(0x02023d50u) >= 0) {
            if (((u8 (*)(void))0x088fc525u)()) ((Fn)0x088fc851u)();
            advance(2);
            return;
        }
    }
    ((Fn)0x088fd309u)();
}
static void hp_data_guard(void) {
    u32 bank;
    if (S->listening && HooksLive() && S->invulnerable && V32(0x02023bc8u) == 0 &&
        player(bank = script_bank()) && (s32)V32(0x02023d50u) >= 0) {
        V8(0x02023bc4u) = bank;
        V32(0x02023d54u) = 0;
        advance(2);
        return;
    }
    ((Fn)0x088fd4e5u)();
}
static void hp_zero_guard(void) {
    if (S->listening && HooksLive() && S->invulnerable && V32(0x02023bc8u) == 0 &&
        player(V8(0x02023d6bu))) {
        advance(1);
        return;
    }
    ((Fn)0x080271c1u)();
}
static void hp_subtract_guard(void) {
    if (S->listening && HooksLive() && S->invulnerable && player(V8(0x02023d6bu)) &&
        (s32)V32(0x02023d50u) >= 0) {
        advance(1);
        return;
    }
    ((Fn)0x0802dfbdu)();
}
static void faint_guard(void) {
    u32 bank;
    if (S->listening && HooksLive() && S->invulnerable) {
        for (bank = 0; bank < 4; ++bank) {
            if (player(bank) && (S->alive & (1u << bank)) &&
                V16(0x02023bceu + bank * 2u) == S->last_party[bank] &&
                V16(0x02023be4u + bank * 0x58u + 0x28u) == 0) {
                u16 hp = S->last_hp[bank];
                u16 index = V16(0x02023bceu + bank * 2u);
                V16(0x02023be4u + bank * 0x58u + 0x28u) = hp ? hp : 1;
                if (index < 6)
                    V16(0x02024284u + index * 100u + 86u) = hp ? hp : 1;
            }
        }
    }
    ((Fn)0x088fe135u)();
}

static void initialize(void) {
    S->shiny = ((u8 (*)(u16))0x0806e6d1u)(0x0913);
    S->initialized = RUNTIME_MAGIC;
}

static void activate(u32 cheat) {
    ++S->activations[cheat];
    if (cheat < 3) {
        static const u16 items[3] = {1, 25, 68};
        /* The inventory does not exist before a save/new game is initialized. */
        u32 pocket = V32(0x0203988cu);
        if (pocket >= 0x02000000u && pocket < 0x02040000u &&
            ((u8 (*)(u16,u16))0x0809a085u)(items[cheat], 1))
            ++S->item_success[cheat];
    } else if (cheat == 3) {
        S->walk ^= 1;
        S->walk_patch = S->walk ? 0x57414c4bu : 0;
    } else if (cheat == 4) {
        if (!S->shiny_touched) {
            S->shiny_original = ((u8 (*)(u16))0x0806e6d1u)(0x0913);
            S->shiny_touched = 1;
        }
        S->shiny ^= 1;
        ((void (*)(u16))(S->shiny ? 0x0806e681u : 0x0806e6a9u))(0x0913);
    } else {
        S->invulnerable ^= 1;
        S->alive = 0;
    }
}

static void input(void) {
    static const u16 prefix[8] = {512,256,512,256,32,16,64,128};
    u16 mask = (~V16(0x04000130u)) & 1023;
    u32 i, j, valid, count;
    ++S->frames;
    if (S->age < 65535) ++S->age;
    if (S->age > 120) S->count = 0;
    if (!mask) { S->previous = 0; S->release = 0; return; }
    if (mask & (mask - 1)) { S->count = 0; S->release = 1; S->previous = mask; return; }
    if (S->release || S->previous == mask) return;
    S->previous = mask;
    S->age = 0;
    count = S->count;
    S->history[count++] = mask;
    for (;;) {
        valid = 1;
        for (i = 0; i < count && i < 8; ++i)
            if (S->history[i] != prefix[i]) valid = 0;
        if (count > 8 && S->history[8] != 1 && S->history[8] != 2 &&
            S->history[8] != 256 && S->history[8] != 512) valid = 0;
        if (count == 10) {
            u16 first = S->history[8], last = S->history[9];
            if (valid && first == 1 && last == 1) activate(0);
            else if (valid && first == 2 && last == 2) activate(1);
            else if (valid && first == 1 && last == 2) activate(2);
            else if (valid && first == 2 && last == 1) activate(3);
            else if (valid && first == 512 && last == 512) activate(4);
            else if (valid && first == 256 && last == 256) activate(5);
            else valid = 0;
            if (valid) { S->count = 0; return; }
        } else if (valid) { S->count = count; return; }
        if (!count) { S->count = 0; return; }
        for (j = 1; j < count; ++j) S->history[j-1] = S->history[j];
        --count;
    }
}

__attribute__((used)) void runtime_read_keys(void) {
    u32 bank, suppress;
    u16 watched = V16(0x03003124u);
    ((Fn)0x0891cecdu)(); /* Chain Amethyst's RTC/keypad implementation. */
    if (S->install != INSTALL_MAGIC) return;
    /* The master code fires at InitHeap, before any cold/reset heap entry can
     * execute its RAM trampoline. This no-op call also refreshes the payload
     * once per ReadKeys without allowing mid-save-scratch writes at VBlank.
     */
    S->patch_request = (V8(0x03003529u) & 2) ? 0x4241544cu :
                       (S->walk ? ((V8(0x02037078u) & 6) ? 0x42494b45u : 0x57414c4bu) : 0);
    if (HooksLive()) ((void (*)(void *,u32))0x08002b81u)((void *)0,0);
    if (S->initialized != RUNTIME_MAGIC) initialize();
    if (!HooksLive() || S->pulse != 0x43484541u) {
        if (S->listening) {
            if (S->shiny_touched)
                ((void (*)(u16))(S->shiny_original ? 0x0806e681u : 0x0806e6a9u))(0x0913);
            S->shiny = ((u8 (*)(u16))0x0806e6d1u)(0x0913);
            S->shiny_touched = S->walk = S->invulnerable = S->alive = 0;
            S->count = S->previous = S->release = S->age = 0;
        }
        S->listening = 0;
        S->entry_mode = S->entry_hold = S->entry_latch = S->entry_block = 0;
        entry_callbacks(0);
        return;
    }
    S->pulse = 0;
    if (!S->listening) S->shiny = ((u8 (*)(u16))0x0806e6d1u)(0x0913);
    S->listening = 1;
    InitActionShadow();
    PatchBattleMain();
    PatchBattleCallbacks();
    suppress = entry_keys((~V16(0x04000130u)) & 1023);
    if (S->entry_mode && !S->entry_latch) input();
    else S->count = S->previous = S->release = S->age = 0;
    entry_callbacks(suppress);
    if (suppress) {
        for (bank = 0; bank < 5; ++bank) V16(0x03003118u + bank * 2) = 0;
        V16(0x03003122u) = 40;
        V16(0x03003124u) = watched;
    }
    /* Battle cleanup clears the game's temporary shiny-creation flag. Keep
     * the user's explicit toggle active across encounters, without promoting
     * an untouched game flag (e.g. a temporary encounter bonus) to a cheat.
     */
    if (S->shiny_touched && S->shiny &&
        !((u8 (*)(u16))0x0806e6d1u)(0x0913))
        ((void (*)(u16))0x0806e681u)(0x0913);
    if (!(V8(0x03003529u) & 2)) S->alive = S->was_battle = 0;
    if (S->invulnerable && (V8(0x03003529u) & 2)) {
        if (!S->was_battle) S->alive = 0;
        S->was_battle = 1;
        for (bank = 0; bank < V8(0x02023bccu) && bank < 4; ++bank) {
            u16 hp = V16(0x02023be4u + bank * 0x58u + 0x28u);
            u16 party = V16(0x02023bceu + bank * 2u);
            if (S->last_party[bank] != party) S->alive &= ~(1u << bank);
            S->last_party[bank] = party;
            if (player(bank) && hp) {
                S->alive |= 1u << bank;
                S->last_hp[bank] = hp;
            }
        }
    }
}

/* Reserve only the payload intervals, each preceded by an allocated header.
 * The master-code installer refreshes the payload before this wrapper runs,
 * including cold boot. Ordinary heap resets preserve the installed state.
 */
static void heap_header(u32 address, u16 allocated, u32 size, u32 prev, u32 next) {
    V16(address) = allocated;
    V16(address + 2) = 0xa3a3;
    V32(address + 4) = size;
    V32(address + 8) = prev;
    V32(address + 12) = next;
}

static void install_state(void) {
    u32 i;
    if (S->install != INSTALL_MAGIC) {
        for (i = 0; i < 512; i += 4) V32(0x02010c00u + i) = 0;
        S->install = INSTALL_MAGIC;
    }
}

__attribute__((used)) void runtime_init_heap(void *start, u32 size) {
    if (!start && !size) return;
    V32(0x03000a38u) = (u32)start;
    V32(0x03000a3cu) = size;
    if ((u32)start == 0x02000000u && size == 0x1c000u) {
        static const u32 intervals[8][2] = {
            {0x0200b950u, 0x0200bd60u}, {0x0200cebcu, 0x0200ced4u},
            {0x0200d0f0u, 0x0200eaf0u}, {0x0200eff0u, 0x0200f200u},
            {0x02010028u, 0x02010070u}, {0x02010a28u, 0x02010a48u},
            {0x02010af0u, 0x02010b28u}, {0x02010bf0u, 0x02010e00u}
        };
        u32 current = (u32)start, previous = current, i;
        for (i = 0; i < 8; ++i) {
            u32 reserved = intervals[i][0], next = intervals[i][1];
            heap_header(current, 0, reserved - current - 16, previous, reserved);
            heap_header(reserved, 1, next - reserved - 16, current, next);
            previous = reserved;
            current = next;
        }
        heap_header(current, 0, (u32)start + size - current - 16, previous, (u32)start);
        install_state();
    } else ((void (*)(void *,u32))0x08002949u)(start,size);
}

__attribute__((section(".readkeys"), naked, used))
void read_keys_entry(void) {
    __asm__ volatile("ldr r0, 1f\n bx r0\n .align 2\n1: .word runtime_read_keys\n");
}

/* ROM InitHeap has already executed push {lr}. The two-halfword gadget
 * loads the even ARM address 02010A38 and branches through r2. Recover the saved
 * return address before tail-entering the normal Thumb C function.
 */
__attribute__((section(".heapentry"), naked, used))
void init_heap_entry(void) {
    __asm__ volatile(
        ".arm\n"
        "ldmia sp!, {lr}\n"
        "ldr ip, 1f\n"
        "bx ip\n"
        ".align 2\n"
        "1: .word runtime_init_heap\n"
        ".thumb\n");
}

/* Export original-table replacements without relying on a linker symbol table. */
__attribute__((section(".guards"), used))
Fn const guards[5] = {hp_bar_guard,hp_data_guard,faint_guard,hp_zero_guard,hp_subtract_guard};

__attribute__((section(".callbacks"), used))
Fn const callbacks[2] = {battle_main_guard,battle_callback2_shim};
