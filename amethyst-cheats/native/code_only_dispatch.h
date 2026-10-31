/* Exact Amethyst v1.4.0 dispatcher glue.
 * Target SHA256: 89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430
 * Include AFTER declaring/defining void guard_dispatch(void).
 * guard_dispatch must select shadow_commands[*gBattlescriptCurrInstr]().
 * The code and both shadow tables MUST already occupy reserved RAM.
 * These helpers do not reserve RAM or install any ROM patches themselves.
 */
#ifndef AMETHYST_DISPATCH_WRAPPERS_H
#define AMETHYST_DISPATCH_WRAPPERS_H

typedef void (*AmethystBattleCallback)(void);

#define AMETHYST_MAIN_FUNC (*(volatile AmethystBattleCallback *)0x03004F84u)
#define AMETHYST_ACTION_ID (*(volatile unsigned char *)0x02023BE3u)
#define AMETHYST_EXEC_FLAGS (*(volatile unsigned int *)0x02023BC8u)

static void execute_actions_guard(void);

/* The fourth AR slot alternates between selection-script dispatch and the
 * original ExecuteActions action table. Refresh only at a phase change.
 */
static unsigned int BattlePatchRequest(void)
{
    AmethystBattleCallback current = AMETHYST_MAIN_FUNC;
    return current == execute_actions_guard ||
           current == (AmethystBattleCallback)0x080155c9u ||
           current == (AmethystBattleCallback)0x088ce0cdu ?
           0x4143544eu : 0x4241544cu;
}

static void RequestBattlePatch(unsigned int request)
{
    if (HooksLive() && S->patch_request != request) {
        S->patch_request = request;
        ((void (*)(void *,unsigned int))0x08002b81u)((void *)0,0);
    }
}

static void execute_actions_guard(void)
{
    if (!HooksLive() && AMETHYST_MAIN_FUNC == execute_actions_guard)
        AMETHYST_MAIN_FUNC = (AmethystBattleCallback)0x080155c9u;
    RequestBattlePatch(0x4143544eu);
    ((AmethystBattleCallback)0x080155c9u)();
}

/* Same controller wait used by all three original dispatchers. */
static void amethyst_run_commands(void)
{
    if (!HooksLive()) {
        if (AMETHYST_MAIN_FUNC == amethyst_run_commands)
            AMETHYST_MAIN_FUNC = (AmethystBattleCallback)0x08015c75u;
        ((AmethystBattleCallback)0x08015c75u)();
        return;
    }
    if (AMETHYST_EXEC_FLAGS == 0)
        guard_dispatch();
}

/* 0x08015910: HandleEndTurn_FinishBattle. Its finish branch contains custom
 * Amethyst/CFRU behavior and must execute in the original, not a C replica.
 * For every other action id its only behavior is the guarded dispatch.
 */
static void amethyst_finish_commands(void)
{
    if (!HooksLive()) {
        if (AMETHYST_MAIN_FUNC == amethyst_finish_commands)
            AMETHYST_MAIN_FUNC = (AmethystBattleCallback)0x08015911u;
        ((AmethystBattleCallback)0x08015911u)();
        return;
    }
    unsigned char action = AMETHYST_ACTION_ID;
    if (action == 11 || action == 12)
        ((AmethystBattleCallback)0x08015911u)();
    else
        amethyst_run_commands();
}

/* 0x08015C00: RunBattleScriptCommands_PopCallbacksStack. The original finish
 * branch decrements the stack and replaces gBattleMainFunc. Keep that write.
 */
static void amethyst_pop_commands(void)
{
    if (!HooksLive()) {
        if (AMETHYST_MAIN_FUNC == amethyst_pop_commands)
            AMETHYST_MAIN_FUNC = (AmethystBattleCallback)0x08015c01u;
        ((AmethystBattleCallback)0x08015c01u)();
        return;
    }
    unsigned char action = AMETHYST_ACTION_ID;
    if (action == 11 || action == 12)
        ((AmethystBattleCallback)0x08015C01u)();
    else
        amethyst_run_commands();
}

/* Called after ReadKeys and before every ordinary or fast-loop battle CB1.
 * Unique wrapper identities avoid capturing/chaining our own wrapper.
 * A callback-stack entry containing an original pointer is replaced only after
 * the original stack-pop routine has returned normally.
 */
static void RestoreBattleMain(void)
{
    AmethystBattleCallback current = AMETHYST_MAIN_FUNC;
    if (current == amethyst_finish_commands)
        AMETHYST_MAIN_FUNC = (AmethystBattleCallback)0x08015911u;
    else if (current == amethyst_pop_commands)
        AMETHYST_MAIN_FUNC = (AmethystBattleCallback)0x08015c01u;
    else if (current == amethyst_run_commands)
        AMETHYST_MAIN_FUNC = (AmethystBattleCallback)0x08015c75u;
    else if (current == execute_actions_guard)
        AMETHYST_MAIN_FUNC = (AmethystBattleCallback)0x080155c9u;
}

static void PatchBattleMain(void)
{
    if (!HooksLive()) {
        RestoreBattleMain();
        return;
    }
    AmethystBattleCallback current = AMETHYST_MAIN_FUNC;
    if (current == (AmethystBattleCallback)0x08015911u)
        AMETHYST_MAIN_FUNC = amethyst_finish_commands;
    else if (current == (AmethystBattleCallback)0x08015C01u)
        AMETHYST_MAIN_FUNC = amethyst_pop_commands;
    else if (current == (AmethystBattleCallback)0x08015C75u)
        AMETHYST_MAIN_FUNC = amethyst_run_commands;
    else if (current == (AmethystBattleCallback)0x080155c9u ||
             current == (AmethystBattleCallback)0x088ce0cdu)
        AMETHYST_MAIN_FUNC = execute_actions_guard;
}

/* Both the normal callback1 route and CFRU's fast-loop clone use this guard.
 * Keep the public callback2 identity visible while original game code runs:
 * numerous animation, party and synchronization queries compare it directly.
 */
static void battle_callback2_shim(void);

static void battle_main_guard(void)
{
    volatile AmethystBattleCallback *cb2 = (volatile AmethystBattleCallback *)0x030030f4u;
    unsigned int restore_identity = *cb2 == battle_callback2_shim;
    if (restore_identity) *cb2 = (AmethystBattleCallback)0x08011101u;
    PatchBattleMain();
    RequestBattlePatch(BattlePatchRequest());
    if (!HooksLive() && *(volatile AmethystBattleCallback *)0x030030f0u == battle_main_guard)
        *(volatile AmethystBattleCallback *)0x030030f0u = (AmethystBattleCallback)0x080123e5u;
    ((AmethystBattleCallback)0x080123e5u)();
    if (restore_identity && HooksLive() && *cb2 == (AmethystBattleCallback)0x08011101u)
        *cb2 = battle_callback2_shim;
}

static void battle_callback2_shim(void)
{
    volatile AmethystBattleCallback *cb2 = (volatile AmethystBattleCallback *)0x030030f4u;
    if (*cb2 == battle_callback2_shim) *cb2 = (AmethystBattleCallback)0x08011101u;
    ((AmethystBattleCallback)(HooksLive() ? 0x0201be61u : 0x08011101u))();
    if (HooksLive() && *cb2 == (AmethystBattleCallback)0x08011101u)
        *cb2 = battle_callback2_shim;
}

static void PatchBattleCallbacks(void)
{
    if (*(volatile AmethystBattleCallback *)0x030030f0u == (AmethystBattleCallback)0x080123e5u)
        *(volatile AmethystBattleCallback *)0x030030f0u = battle_main_guard;
    if (*(volatile AmethystBattleCallback *)0x030030f4u == (AmethystBattleCallback)0x08011101u)
        *(volatile AmethystBattleCallback *)0x030030f4u = battle_callback2_shim;
}

/* Shadow the original fourteen-entry action table, preserving every ordinary
 * move/item/switch/safari/end-action handler. Only index 10 (EXEC_SCRIPT) is
 * replaced. ExecuteActions uses this table while its phase owns the shared
 * ROM patch slot. The original CFRU pre/post-action logic runs in ROM.
 */
static void InitActionShadow(void)
{
    const unsigned int *source = (const unsigned int *)0x08250038u;
    volatile unsigned int *target = (volatile unsigned int *)0x02010038u;
    unsigned int i;
    for (i = 0; i < 14; ++i)
        target[i] = source[i];
    target[10] = (unsigned int)amethyst_run_commands;
}

#undef AMETHYST_MAIN_FUNC
#undef AMETHYST_ACTION_ID
#undef AMETHYST_EXEC_FLAGS
#endif
