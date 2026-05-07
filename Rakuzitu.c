#include "rakuzitu.h"

GameContext g_game;

static void run_game_loop(void)
{
    u8 logic_divider_count;
    u8 render_divider_count;
    u8 background_divider_count;
    u8 base_ticks_to_process;
    u8 base_ticks_processed;
    u8 foreground_due;
    u8 background_pending;
    u16 backlog;

    logic_divider_count = 0;
    render_divider_count = 0;
    background_divider_count = 0;
    background_pending = 0;

    draw_ui_frame_once();

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
        while (base_ticks_processed < base_ticks_to_process && consume_base_tick()) {
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
            if (render_divider_count >= (BASE_TIMER_HZ / RENDER_SLOT_HZ)) {
                render_divider_count = 0;
                foreground_due = 1;
            }

            ++background_divider_count;
            if (background_divider_count >= (BASE_TIMER_HZ / BACKGROUND_BAND_HZ)) {
                background_divider_count = 0;
                if (g_game.state != GAME_STATE_PLAYER_DYING && g_game.state != GAME_STATE_GAMEOVER) {
                    background_pending = 1;
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
        }
    }
}

int main(void)
{
    init_game(&g_game);
    init_cga_mode5();
    init_keyboard();
    init_timer();
    run_game_loop();
    restore_timer();
    restore_keyboard();
    set_video_mode(0x03);
    return 0;
}
