#include <string.h>

#include "rakuzitu.h"

GameContext g_game;

/* Global Engine Configuration Definitions */
u8 g_sys_low_detail = 0;
u8 g_sys_render_hz = 15;
u8 g_sys_bg_hz = 4;
u8 g_sys_floor_hz = 2;
u8 g_sys_frame_divisor = 2;

static void run_game_loop(void)
{
    u8 logic_divider_count;
    u8 render_divider_count;
    u8 background_divider_count;
    u8 floor_divider_count;
    u8 base_ticks_to_process;
    u8 base_ticks_processed;
    u8 foreground_due;
    u8 background_pending;
    u8 floor_pending;
    u8 grabbed_ticks;
    u16 backlog;

    logic_divider_count = 0;
    render_divider_count = 0;
    background_divider_count = 0;
    floor_divider_count = 0;
    background_pending = 0;
    floor_pending = 1;

    while (!g_game.exit_requested) {
        foreground_due = 0;

        backlog = pending_base_ticks();
        if (g_game.state == GAME_STATE_PLAYER_DYING || g_game.state == GAME_STATE_GAMEOVER) {
            if (backlog >= 4) {
                base_ticks_to_process = 4;
            } else {
                base_ticks_to_process = 3;
            }
        } else if (backlog >= 4) {
            base_ticks_to_process = 3;
        } else {
            base_ticks_to_process = 2;
        }

        base_ticks_processed = 0;
        
        grabbed_ticks = consume_available_ticks(base_ticks_to_process);
        
        while (base_ticks_processed < grabbed_ticks) {
            ++base_ticks_processed;

            ++logic_divider_count;
            if (logic_divider_count >= (BASE_TIMER_HZ / LOGIC_HZ)) {
                logic_divider_count = 0;
                tick_game(&g_game);
                if (g_game.exit_requested) {
                    break;
                }
            }

            ++render_divider_count;
            if (render_divider_count >= (BASE_TIMER_HZ / g_sys_render_hz)) {
                render_divider_count = 0;
                foreground_due = 1;
            }

            ++background_divider_count;
            if (background_divider_count >= (BASE_TIMER_HZ / g_sys_bg_hz)) {
                background_divider_count = 0;
                if (g_game.state != GAME_STATE_PLAYER_DYING && g_game.state != GAME_STATE_GAMEOVER) {
                    background_pending = 1;
                }
            }

            ++floor_divider_count;
            if (floor_divider_count >= (BASE_TIMER_HZ / g_sys_floor_hz)) {
                floor_divider_count = 0;
                if (g_game.state != GAME_STATE_PLAYER_DYING && g_game.state != GAME_STATE_GAMEOVER) {
                    floor_pending = 1;
                }
            }
        }

        if (foreground_due && !g_game.exit_requested) {
            render_foreground(&g_game);
        } else if (g_game.state == GAME_STATE_GAMEOVER && !g_game.exit_requested) {
            render_foreground(&g_game);
        } else if (background_pending && !g_game.exit_requested) {
            render_background_step(&g_game);
            background_pending = 0;
        } else if (floor_pending && !g_game.exit_requested) {
            render_floor_step(&g_game);
            floor_pending = 0;
        }
    }
}

int main(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-lo") == 0) {
            g_sys_low_detail = 1;
            g_sys_render_hz = 10;
            g_sys_frame_divisor = 3;
            g_sys_bg_hz = 3;
            g_sys_floor_hz = 1;
        }
    }
    
    init_game(&g_game);
	set_video_mode(0x04);
    set_cga_palette(0x00, 0x01);
    init_keyboard();
    init_timer();
    
    draw_ui_frame_once();
    run_game_loop();
    
    restore_timer();
    restore_keyboard();
    set_video_mode(0x03);
    
    return 0;
}
