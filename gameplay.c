#include <string.h>

#include "rakuzitu.h"
#include "assets/bg_strip.h"

static const OpponentFigureDef g_wolf_def = {
    OPPONENT_FIGURE_WOLF,
    &g_wolf_run_animation,
    &g_wolf_attack_animation,
    &g_wolf_death_animation,
    SPEED_2X_FP,
    40,
    100,
    100
};

static const OpponentFigureDef g_ronin_def = {
    OPPONENT_FIGURE_RONIN,
    &g_ronin_run_animation,
    &g_ronin_attack_animation,
    &g_ronin_death_animation,
    SPEED_15X_FP,
    60,
    135,
    135
};

static const OpponentFigureDef g_ashigaru_def = {
    OPPONENT_FIGURE_ASHIGARU,
    &g_ashigaru_run_animation,
    &g_ashigaru_attack_animation,
    &g_ashigaru_death_animation,
    SPEED_2X_FP,
    60,
    135,
    135
};

static const OpponentFigureDef g_peasant_def = {
    OPPONENT_FIGURE_PEASANT,
    &g_peasant_run_animation,
    &g_peasant_attack_animation,
    &g_peasant_death_animation,
    SPEED_15X_FP,
    60,
    135,
    135
};

static const OpponentFigureDef g_tsujigiri_def = {
    OPPONENT_FIGURE_TSUJIGIRI,
    &g_tsujigiri_run_animation,
    &g_tsujigiri_attack_animation,
    &g_tsujigiri_death_animation,
    SPEED_1X_FP,
    60,
    135,
    135
};

static const OpponentFigure g_encounter_figures[] = {
    OPPONENT_FIGURE_WOLF,
    OPPONENT_FIGURE_RONIN,
    OPPONENT_FIGURE_ASHIGARU,
    OPPONENT_FIGURE_PEASANT,
    OPPONENT_FIGURE_TSUJIGIRI
};

static const OpponentFigureDef *get_opponent_def(OpponentFigure figure)
{
    switch (figure) {
    case OPPONENT_FIGURE_WOLF:
        return &g_wolf_def;
    case OPPONENT_FIGURE_RONIN:
        return &g_ronin_def;
	case OPPONENT_FIGURE_ASHIGARU:
        return &g_ashigaru_def;
	case OPPONENT_FIGURE_PEASANT:
        return &g_peasant_def;
	case OPPONENT_FIGURE_TSUJIGIRI:
        return &g_tsujigiri_def;
    default:
        return NULL;
    }
}

static u16 seed_rng(void)
{
    union REGS regs;
    u16 pit_lsb;
    u16 pit_msb;
    u16 seed;

    regs.h.ah = 0x00;
    int86(0x1A, &regs, &regs);

    outp(0x43, 0x00);
    pit_lsb = inp(0x40);
    pit_msb = inp(0x40);

    seed = regs.x.dx ^ (((u16)(pit_msb << 8)) | pit_lsb) ^ 0x5AA5;
    if (seed == 0) {
        seed = 1; /* LFSR will get stuck if seeded with 0 */
    }
    return seed;
}

static u16 rng_next_u16(GameContext *game)
{
    u16 lsb;
    
    lsb = game->rng & 1;
    game->rng >>= 1;
    if (lsb) {
        game->rng ^= 0xB400;
    }
    return game->rng;
}

static int get_spawn_blink_x(const GameContext *game, OpponentFigure figure)
{
    int blink_x;

    if (figure == OPPONENT_FIGURE_TSUJIGIRI) {
        return 96;
    }

    blink_x = game->sun_y + game->sun_y;
    if (blink_x > 80) {
        blink_x = 80;
    }
    return blink_x;
}

static u8 get_spawn_hostile(GameContext *game)
{
    return (rng_next_u16(game) & 3) != 0;
}

static int get_dynamic_weight(const GameContext *game, OpponentFigure figure)
{
    int weight;

    switch (figure) {
    case OPPONENT_FIGURE_WOLF:
        if (game->sun_y < 80) {
            return 0;
        }
        return (int)game->kill_counter;

    case OPPONENT_FIGURE_ASHIGARU:
        weight = 20 + (int)game->kill_counter;
        if (game->sun_y < 40) {
            weight >>= 1;
        }
        return weight;

    case OPPONENT_FIGURE_RONIN:
        weight = 20 + (int)game->kill_counter;
        if (game->sun_y >= 40) {
            weight >>= 1;
        }
        return weight;

    case OPPONENT_FIGURE_PEASANT:
        weight = game->sun_y;
        if (game->sun_y < 40) {
            weight >>= 1;
        }
        return weight;

    case OPPONENT_FIGURE_TSUJIGIRI:
        if (game->tsujigiri_spawned || game->sun_y < 60) {
            return 0;
        }
        return 1;

    default:
        return 0;
    }
}

static OpponentFigure pick_weighted_figure(GameContext *game)
{
    int total_weight;
    int pick;
    int weight;
    int i;

    total_weight = 0;
    for (i = 0; i < (int)(sizeof(g_encounter_figures) / sizeof(g_encounter_figures[0])); ++i) {
        weight = get_dynamic_weight(game, g_encounter_figures[i]);
        total_weight += weight;
    }

    if (total_weight <= 0) {
        return OPPONENT_FIGURE_RONIN;
    }

    pick = rng_next_u16(game) % total_weight;
    for (i = 0; i < (int)(sizeof(g_encounter_figures) / sizeof(g_encounter_figures[0])); ++i) {
        weight = get_dynamic_weight(game, g_encounter_figures[i]);
        if (weight <= 0) {
            continue;
        }
        if (pick < weight) {
            return g_encounter_figures[i];
        }
        pick -= weight;
    }

    return OPPONENT_FIGURE_RONIN;
}

static int rects_overlap(Rect a, Rect b)
{
    if (a.valid == 0 || b.valid == 0) {
        return 0;
    }

    return (a.x < b.x + b.width) &&
           (a.x + a.width > b.x) &&
           (a.y < b.y + b.height) &&
           (a.y + a.height > b.y);
}

static void invalidate_rect(Rect *rect)
{
    rect->x = 0;
    rect->y = 0;
    rect->width = 0;
    rect->height = 0;
    rect->valid = 0;
}

const AnimationAsset *get_player_animation(PlayerAnimMode mode)
{
    switch (mode) {
    case PLAYER_ANIM_ATTACK1:
        return &g_player_attack1_animation;
    case PLAYER_ANIM_ATTACK2:
        return &g_player_attack2_animation;
    case PLAYER_ANIM_DEATH:
        return &g_player_death_animation;
    case PLAYER_ANIM_RUN:
    default:
        return &g_player_run_animation;
    }
}

const AnimationAsset *get_opponent_animation(const OpponentRuntime *opponent)
{
    if (opponent->def == NULL) {
        return NULL;
    }

    switch (opponent->anim_mode) {
    case OPPONENT_ANIM_ATTACK:
        return opponent->def->attack_anim;
    case OPPONENT_ANIM_DEATH:
        return opponent->def->death_anim;
    case OPPONENT_ANIM_RUN:
    default:
        return opponent->def->run_anim;
    }
}

static u16 get_looping_frame(u16 anim_tick, const AnimationAsset *animation)
{
    u16 frame_tick;

    if (animation == NULL || animation->frame_count == 0) {
        return 0;
    }

    frame_tick = anim_tick / g_sys_frame_divisor;
    return (u16)(frame_tick % animation->frame_count);
}

static u16 get_one_shot_frame(u16 anim_tick, u16 total_ticks, const AnimationAsset *animation)
{
    u16 tick_per_frame;
    u16 frame_index;

    if (animation == NULL || animation->frame_count == 0) {
        return 0;
    }

    tick_per_frame = (u16)(total_ticks / animation->frame_count);
    if (tick_per_frame == 0) {
        tick_per_frame = 1;
    }

    frame_index = anim_tick / tick_per_frame;
    if (frame_index >= animation->frame_count) {
        frame_index = (u16)(animation->frame_count - 1);
    }
    return frame_index;
}

u16 get_player_frame_index(const GameContext *game, const AnimationAsset *animation)
{
    if (game->player.anim_mode == PLAYER_ANIM_RUN) {
        return get_looping_frame(game->player.anim_tick, animation);
    } else if (game->player.anim_mode == PLAYER_ANIM_DEATH) {
        return get_one_shot_frame(game->player.anim_tick, PLAYER_DEATH_ANIM_TICKS, animation);
    } else {
        return get_one_shot_frame(game->player.anim_tick, PLAYER_ATTACK_TOTAL_TICKS, animation);
    }
}

u16 get_opponent_frame_index(const GameContext *game, const AnimationAsset *animation)
{
    if (game->opponent.anim_mode == OPPONENT_ANIM_RUN) {
        return get_looping_frame(game->opponent.anim_tick, animation);
    } else if (game->opponent.anim_mode == OPPONENT_ANIM_ATTACK) {
        return get_one_shot_frame(game->opponent.anim_tick, OPPONENT_ATTACK_TICKS, animation);
    } else {
        return get_one_shot_frame(game->opponent.anim_tick, OPPONENT_DEATH_TICKS, animation);
    }
}

static void spawn_opponent(GameContext *game)
{
    OpponentFigure figure;
    const OpponentFigureDef *def;

    figure = pick_weighted_figure(game);
    def = get_opponent_def(figure);
    if (figure == OPPONENT_FIGURE_TSUJIGIRI) {
        game->tsujigiri_spawned = 1;
    }
    memset(&game->opponent, 0, sizeof(game->opponent));
    game->opponent.def = def;
    game->opponent.active = 1;
    game->opponent.hostile = get_spawn_hostile(game);
    game->opponent.x_fp = fp_from_int(SCREEN_WIDTH + 16);
    game->opponent.move_speed_fp = def->speed_fp;
    game->opponent.y_baseline = PLAYER_BASELINE_Y;
    game->opponent.anim_mode = OPPONENT_ANIM_RUN;
    game->opponent.anim_tick = 0;
    game->opponent.eye_color = EYE_COLOR_BLACK;
    game->opponent.blink_x = get_spawn_blink_x(game, figure);
    invalidate_rect(&game->opponent.rect);
}

static void start_player_attack(GameContext *game, PlayerAnimMode mode)
{
    if (game->player.anim_mode == PLAYER_ANIM_DEATH) {
        return;
    }
    if (game->player.anim_mode == PLAYER_ANIM_ATTACK1 || game->player.anim_mode == PLAYER_ANIM_ATTACK2) {
        return;
    }

    game->player.anim_mode = mode;
    game->player.anim_tick = 0;
    game->player.attack_active = 0;
    game->player.attack_kind = (mode == PLAYER_ANIM_ATTACK1) ? 1 : 2;

    if (game->opponent.active &&
        game->opponent.def != NULL &&
        game->opponent.def->figure == OPPONENT_FIGURE_TSUJIGIRI &&
        game->opponent.hostile == 0) {
        game->opponent.hostile = 1;
        if (game->opponent.eye_locked) {
            game->opponent.eye_color = EYE_COLOR_RED;
        }
    }
}

static void kill_opponent(GameContext *game)
{
    if (game->opponent.active == 0 || game->opponent.anim_mode == OPPONENT_ANIM_DEATH) {
        return;
    }

    game->opponent.anim_mode = OPPONENT_ANIM_DEATH;
    game->opponent.anim_tick = 0;
    game->opponent.move_speed_fp = SPEED_1X_FP;
    game->kill_counter++;
    if (game->opponent.hostile == 0) {
        game->karma_counter++;
    }
    game->opponent.hostile = 0;
}

static void start_player_death(GameContext *game)
{
    game->player.anim_mode = PLAYER_ANIM_DEATH;
    game->player.anim_tick = 0;
    game->player.attack_active = 0;
    game->state = GAME_STATE_PLAYER_DYING;
    game->state_tick = 0;
    game->opponent.anim_mode = OPPONENT_ANIM_ATTACK;
    game->opponent.move_speed_fp = SPEED_0X_FP;
    game->opponent.anim_tick = OPPONENT_ATTACK_TICKS;
}

static void update_background(GameContext *game)
{
    ++game->background_scroll_ticks;
    if (game->background_scroll_ticks >= BACKGROUND_SCROLL_TICKS) {
        game->background_scroll_ticks = 0;
        if (game->background_scroll_pixels < BG_STRIP_HEIGHT) {
            ++game->background_scroll_pixels;
            /* Calculate derived sun coordinate exactly once per background scroll */
            game->sun_y = 0 + (game->background_scroll_pixels >> 1);
        }
    }
}

static void update_player(GameContext *game)
{
    const AnimationAsset *animation;
    u16 active_frame_start;

    if (game->state == GAME_STATE_PLAYER_ENTRY) {
        if (game->player.x_fp < fp_from_int(PLAYER_HOME_X)) {
            game->player.x_fp += PLAYER_ENTRY_SPEED_FP;
            if (game->player.x_fp > fp_from_int(PLAYER_HOME_X)) {
                game->player.x_fp = fp_from_int(PLAYER_HOME_X);
            }
        }
    }

    if (game->player.anim_mode == PLAYER_ANIM_RUN) {
        ++game->player.anim_tick;
        if (game->input.pressed_mask & ACTION_Z) {
            start_player_attack(game, PLAYER_ANIM_ATTACK1);
        } else if (game->input.pressed_mask & ACTION_X) {
            start_player_attack(game, PLAYER_ANIM_ATTACK2);
        }
        return;
    }

    animation = get_player_animation(game->player.anim_mode);
    if (game->player.anim_mode == PLAYER_ANIM_DEATH) {
        if (game->player.anim_tick < PLAYER_DEATH_ANIM_TICKS) {
            ++game->player.anim_tick;
        }
        return;
    }

    if (game->player.anim_tick < PLAYER_ATTACK_TOTAL_TICKS) {
        ++game->player.anim_tick;
    }

    if (animation != NULL) {
        active_frame_start = (animation->frame_count > 2) ? (u16)(animation->frame_count - 2) : 0;
        game->player.attack_active = (get_one_shot_frame(game->player.anim_tick, PLAYER_ATTACK_TOTAL_TICKS, animation) >= active_frame_start);
    } else {
        game->player.attack_active = 0;
    }

    if (game->player.anim_tick >= PLAYER_ATTACK_TOTAL_TICKS) {
        game->player.anim_mode = PLAYER_ANIM_RUN;
        game->player.anim_tick = 0;
        game->player.attack_active = 0;
        game->player.attack_kind = 0;
    }
}

static void update_opponent(GameContext *game)
{
    if (game->opponent.active == 0 || game->opponent.def == NULL) {
        return;
    }

    if (game->opponent.anim_mode == OPPONENT_ANIM_DEATH) {
        if (game->opponent.anim_tick < OPPONENT_DEATH_TICKS) {
            ++game->opponent.anim_tick;
        }
        game->opponent.x_fp -= game->opponent.move_speed_fp;
    } else {
        game->opponent.x_fp -= game->opponent.move_speed_fp;
        ++game->opponent.anim_tick;
    }

    if (!game->opponent.eye_locked) {
        if (fp_to_int(game->opponent.x_fp) <= (320 - game->opponent.blink_x)) {
            game->opponent.eye_color = game->opponent.hostile ? EYE_COLOR_RED : EYE_COLOR_CYAN;
            game->opponent.eye_locked = 1;
        }
    }

    if (game->opponent.hostile && game->opponent.anim_mode == OPPONENT_ANIM_RUN) {
        if ((fp_to_int(game->opponent.x_fp) - fp_to_int(game->player.x_fp)) < game->opponent.def->attack_trigger_px) {
            game->opponent.anim_mode = OPPONENT_ANIM_ATTACK;
            game->opponent.anim_tick = 0;
        }
    }
}

static void update_gameplay(GameContext *game)
{
    if (game->state == GAME_STATE_PLAYER_ENTRY) {
        if (game->player.x_fp >= fp_from_int(PLAYER_HOME_X)) {
            game->state = GAME_STATE_PICK_ENCOUNTER;
            game->state_tick = 0;
        }
        return;
    }

    if (game->state == GAME_STATE_PICK_ENCOUNTER) {
        spawn_opponent(game);
        game->state = GAME_STATE_ACTIVE_ENCOUNTER;
        game->state_tick = 0;
        return;
    }

    if (game->state == GAME_STATE_ACTIVE_ENCOUNTER) {
        if (game->player.attack_active && rects_overlap(game->player.rect, game->opponent.rect)) {
            kill_opponent(game);
        } else if (game->opponent.hostile &&
                   game->opponent.anim_mode == OPPONENT_ANIM_ATTACK &&
                   rects_overlap(game->player.rect, game->opponent.rect)) {
            start_player_death(game);
            return;
        }

        if ((fp_to_int(game->opponent.x_fp) + game->opponent.def->source_width) < 0) {
            game->opponent.active = 0;
            invalidate_rect(&game->opponent.rect);
            
            if (game->background_scroll_pixels >= BG_STRIP_HEIGHT) {
                set_gameover_asset(GAMEOVER_LOST);
                game->state = GAME_STATE_GAMEOVER;
                game->state_tick = 0;
                game->gameover_drawn = 0;
                return;
            }
            
            /* kill_counter > 40 + (7 * karma), but avoiding slow MUL instruction */
            if (game->kill_counter > (40U + ((game->karma_counter << 3) - game->karma_counter))) {
                set_gameover_asset(GAMEOVER_PROLONGED);
                game->state = GAME_STATE_GAMEOVER;
                game->state_tick = 0;
                game->gameover_drawn = 0;
                return;
            }

            game->state = GAME_STATE_PICK_ENCOUNTER;
            game->state_tick = 0;
        }
        return;
    }

    if (game->state == GAME_STATE_PLAYER_DYING) {
        ++game->state_tick;
        if (game->state_tick >= PLAYER_DEATH_TOTAL_TICKS) {
            game->state = GAME_STATE_GAMEOVER;
            game->state_tick = 0;
            game->gameover_drawn = 0;
            
            /* Killed by Tsujigiri check */
            if (game->opponent.def != NULL && game->opponent.def->figure == OPPONENT_FIGURE_TSUJIGIRI) {
                set_gameover_asset(GAMEOVER_JINXED);
            } else {
                set_gameover_asset(GAMEOVER_KIA);
            }
        }
    }
}

void tick_game(GameContext *game)
{
    sample_input(game);

    if (game->input.exit_pressed && game->state != GAME_STATE_GAMEOVER) {
        game->exit_requested = 1;
        return;
    }

    if (game->state == GAME_STATE_GAMEOVER) {
        if (game->input.any_pressed) {
            init_game(game);
            draw_ui_frame_once();
        }
        return;
    }

    ++game->global_loop_counter;
    if (game->state != GAME_STATE_PLAYER_DYING) {
        update_background(game);
    }
    update_player(game);
    if (game->state != GAME_STATE_PLAYER_DYING) {
        update_opponent(game);
    }
    update_gameplay(game);
}

void init_game(GameContext *game)
{
    memset(game, 0, sizeof(*game));
    game->state = GAME_STATE_PLAYER_ENTRY;
    game->background_scroll_pixels = 0;
    game->sun_y = 0;
    game->rendered_background_scroll_pixels = 0xFFFF;
    game->rendered_floor_phase = 0xFFFF;
    game->video_wait_vblank = 1;
    init_default_bindings(&game->bindings);
    game->rng = seed_rng();
    game->player.x_fp = fp_from_int(PLAYER_ENTRY_START_X);
    game->player.y_baseline = PLAYER_BASELINE_Y;
    game->player.anim_mode = PLAYER_ANIM_RUN;
    invalidate_rect(&game->player.rect);
    invalidate_rect(&game->opponent.rect);
    invalidate_rect(&game->previous_player_rect);
    invalidate_rect(&game->previous_opponent_rect);
}
