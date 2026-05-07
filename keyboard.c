#include <string.h>

#include "rakuzitu.h"

static volatile u8 g_key_held[128];
static volatile u8 g_key_pressed[128];
static volatile u8 g_any_key_pressed = 0;
static void (__interrupt __far *g_old_keyboard_isr)(void);

static void __interrupt __far keyboard_handler(void)
{
    u8 scan_code;
    u8 key_code;
    u8 port61;

    scan_code = inp(0x60);
    key_code = (u8)(scan_code & 0x7F);

    if (key_code < 128) {
        if (scan_code & 0x80) {
            g_key_held[key_code] = 0;
        } else {
            if (g_key_held[key_code] == 0) {
                g_key_pressed[key_code] = 1;
            }
            g_key_held[key_code] = 1;
            g_any_key_pressed = 1;
        }
    }

    port61 = inp(0x61);
    outp(0x61, (u8)(port61 | 0x80));
    outp(0x61, port61);
    outp(0x20, 0x20);
}

void init_keyboard(void)
{
    memset((void *)g_key_held, 0, sizeof(g_key_held));
    memset((void *)g_key_pressed, 0, sizeof(g_key_pressed));
    g_any_key_pressed = 0;
    g_old_keyboard_isr = _dos_getvect(0x09);
    _dos_setvect(0x09, keyboard_handler);
}

void restore_keyboard(void)
{
    _dos_setvect(0x09, g_old_keyboard_isr);
}

void sample_input(GameContext *game)
{
    game->input.held_mask = 0;
    game->input.pressed_mask = 0;
    game->input.exit_pressed = 0;
    game->input.any_pressed = 0;

    _disable();

    if (g_key_held[game->bindings.key_z_scancode]) {
        game->input.held_mask |= ACTION_Z;
    }
    if (g_key_held[game->bindings.key_x_scancode]) {
        game->input.held_mask |= ACTION_X;
    }

    if (g_key_pressed[game->bindings.key_z_scancode]) {
        game->input.pressed_mask |= ACTION_Z;
        game->input.any_pressed = 1;
        g_key_pressed[game->bindings.key_z_scancode] = 0;
    }
    if (g_key_pressed[game->bindings.key_x_scancode]) {
        game->input.pressed_mask |= ACTION_X;
        game->input.any_pressed = 1;
        g_key_pressed[game->bindings.key_x_scancode] = 0;
    }
    if (g_key_pressed[game->bindings.key_exit_scancode]) {
        game->input.exit_pressed = 1;
        game->input.any_pressed = 1;
        g_key_pressed[game->bindings.key_exit_scancode] = 0;
    }
    if (g_any_key_pressed) {
        game->input.any_pressed = 1;
        g_any_key_pressed = 0;
    }

    _enable();
}

void init_default_bindings(InputBindings *bindings)
{
    bindings->key_z_scancode = 0x2C;
    bindings->key_x_scancode = 0x2D;
    bindings->key_exit_scancode = SCAN_ESCAPE;
}
