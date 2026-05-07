#include "rakuzitu.h"

static volatile u16 g_logic_ticks = 0;
static volatile u32 g_timer_bios_accumulator = 0;
static void (__interrupt __far *g_old_timer_isr)(void);

static void __interrupt __far timer_handler(void)
{
    ++g_logic_ticks;
    g_timer_bios_accumulator += PIT_BASE_DIVISOR;

    if (g_timer_bios_accumulator >= PIT_BIOS_DIVISOR) {
        g_timer_bios_accumulator -= PIT_BIOS_DIVISOR;
        _chain_intr(g_old_timer_isr);
    } else {
        outp(0x20, 0x20);
    }
}

void init_timer(void)
{
    g_old_timer_isr = _dos_getvect(0x08);
    _dos_setvect(0x08, timer_handler);

    outp(0x43, 0x36);
    outp(0x40, (u8)(PIT_BASE_DIVISOR & 0xFF));
    outp(0x40, (u8)(PIT_BASE_DIVISOR >> 8));
}

void restore_timer(void)
{
    outp(0x43, 0x36);
    outp(0x40, 0x00);
    outp(0x40, 0x00);
    _dos_setvect(0x08, g_old_timer_isr);
}

u8 consume_base_tick(void)
{
    u8 have_tick;

    _disable();
    have_tick = (g_logic_ticks != 0);
    if (have_tick) {
        --g_logic_ticks;
    }
    _enable();

    return have_tick;
}

u16 pending_base_ticks(void)
{
    u16 ticks;

    _disable();
    ticks = g_logic_ticks;
    _enable();

    return ticks;
}
