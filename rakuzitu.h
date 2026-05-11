#ifndef RAKUZITU_H
#define RAKUZITU_H

#include <dos.h>
#include <conio.h>

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned long  u32;

#define SCREEN_WIDTH              320
#define SCREEN_HEIGHT             200
#define BYTES_PER_SCANLINE        (SCREEN_WIDTH / 4)
#define TOP_VIEW_HEIGHT           100
#define CGA_SEGMENT               0xB800
#define CGA_ODD_OFFSET            0x2000
#define BLACK_BYTE                0x00
#define WHITE_BYTE                0xFF
#define BASE_TIMER_HZ             120
#define LOGIC_HZ                  30
#define RENDER_SLOT_HZ            15
#define BACKGROUND_BAND_HZ        6
#define PIT_INPUT_HZ              1193180UL
#define PIT_BIOS_DIVISOR          65536UL
#define PIT_BASE_DIVISOR          ((u16)(PIT_INPUT_HZ / BASE_TIMER_HZ))

#define FP_SHIFT                  8
#define FP_ONE                    256L
/* Fixed-point conversions replaced with instantaneous inline macros */
#define fp_from_int(value)        (((long)(value)) << FP_SHIFT)
#define fp_to_int(value)          ((int)((value) >> FP_SHIFT))

#define SPEED_0X_FP               0L
#define SPEED_1X_FP               ((80L << FP_SHIFT) / LOGIC_HZ)
#define SPEED_15X_FP              ((120L << FP_SHIFT) / LOGIC_HZ)
#define SPEED_2X_FP               ((160L << FP_SHIFT) / LOGIC_HZ)
#define PLAYER_HOME_X             64
#define PLAYER_BASELINE_Y         100
#define PLAYER_ENTRY_START_X      (-96)
#define PLAYER_ENTRY_SPEED_FP     SPEED_1X_FP
#define PLAYER_FRAME_DIVISOR      2
#define PLAYER_ATTACK_TOTAL_TICKS 18
#define PLAYER_ATTACK_ACTIVE_TICKS 9
#define PLAYER_DEATH_ANIM_TICKS   39
#define PLAYER_GAMEOVER_DELAY_TICKS 45
#define PLAYER_DEATH_TOTAL_TICKS  (PLAYER_DEATH_ANIM_TICKS + PLAYER_GAMEOVER_DELAY_TICKS)
#define OPPONENT_ATTACK_TICKS     18
#define OPPONENT_DEATH_TICKS      30
#define BACKGROUND_SCROLL_TICKS   30

#define SCAN_ESCAPE               0x01
#define ACTION_Z                  0x01
#define ACTION_X                  0x02
#define ACTION_ANY                (ACTION_Z | ACTION_X)

typedef enum GameState {
    GAME_STATE_PLAYER_ENTRY = 0,
    GAME_STATE_PICK_ENCOUNTER,
    GAME_STATE_ACTIVE_ENCOUNTER,
    GAME_STATE_PLAYER_DYING,
    GAME_STATE_GAMEOVER
} GameState;

typedef enum PlayerAnimMode {
    PLAYER_ANIM_RUN = 0,
    PLAYER_ANIM_ATTACK1,
    PLAYER_ANIM_ATTACK2,
    PLAYER_ANIM_DEATH
} PlayerAnimMode;

typedef enum OpponentFigure {
    OPPONENT_FIGURE_NONE = 0,
    OPPONENT_FIGURE_WOLF,
    OPPONENT_FIGURE_RONIN
} OpponentFigure;

typedef enum OpponentAnimMode {
    OPPONENT_ANIM_RUN = 0,
    OPPONENT_ANIM_ATTACK,
    OPPONENT_ANIM_DEATH
} OpponentAnimMode;

typedef enum EyeColor {
    EYE_COLOR_BLACK = 0,
    EYE_COLOR_CYAN = 1,
    EYE_COLOR_RED = 2
} EyeColor;

typedef struct PackedSpriteFrame {
    u16 pixel_even_offset;
    u16 pixel_odd_offset;
    u16 mask_even_offset;
    u16 mask_odd_offset;
    u16 width;
    u16 height;
    u16 bytes_per_row;
    u16 anchor_x;
    u16 anchor_y;
    u16 even_row_count;
    u16 odd_row_count;
} PackedSpriteFrame;

typedef struct AnimationAsset {
    u16 frame_count;
    const u8 far *pixels_even;
    const u8 far *pixels_odd;
    const u8 far *mask_even;
    const u8 far *mask_odd;
    const PackedSpriteFrame far *frames;
} AnimationAsset;

typedef struct Rect {
    int x;
    int y;
    int width;
    int height;
    u8 valid;
} Rect;

typedef struct InputBindings {
    u8 key_z_scancode;
    u8 key_x_scancode;
    u8 key_exit_scancode;
} InputBindings;

typedef struct InputSnapshot {
    u8 held_mask;
    u8 pressed_mask;
    u8 exit_pressed;
    u8 any_pressed;
} InputSnapshot;

typedef struct WeightedEntry {
    int weight;
    OpponentFigure result;
} WeightedEntry;

typedef struct OpponentFigureDef {
    OpponentFigure figure;
    const AnimationAsset *run_anim;
    const AnimationAsset *attack_anim;
    const AnimationAsset *death_anim;
    u8 hostile;
    long speed_fp;
    int attack_trigger_px;
    int source_width;
    int source_height;
} OpponentFigureDef;

typedef struct PlayerRuntime {
    long x_fp;
    int y_baseline;
    PlayerAnimMode anim_mode;
    u16 anim_tick;
    u8 attack_active;
    u8 attack_kind;
    Rect rect;
} PlayerRuntime;

typedef struct OpponentRuntime {
    const OpponentFigureDef *def;
    long x_fp;
    long move_speed_fp;
    int y_baseline;
    OpponentAnimMode anim_mode;
    u16 anim_tick;
    u8 active;
    u8 hostile;
    u8 eye_locked;
    EyeColor eye_color;
    int blink_x;
    Rect rect;
} OpponentRuntime;

typedef struct GameContext {
    GameState state;
    u32 global_loop_counter;
    u16 background_scroll_ticks;
    u16 background_scroll_pixels;
    u16 rendered_background_scroll_pixels;
    u8 video_wait_vblank;
    u8 exit_requested;
    u8 gameover_drawn;
    u16 state_tick;
    u16 kill_counter;
    u16 karma_counter;
    u16 rng; /* Replaced massive 32-bit PRNG with fast 16-bit PRNG */
    InputBindings bindings;
    InputSnapshot input;
    PlayerRuntime player;
    OpponentRuntime opponent;
    Rect previous_player_rect;
    Rect previous_opponent_rect;
} GameContext;

extern GameContext g_game;

extern const AnimationAsset g_player_run_animation;
extern const AnimationAsset g_player_attack1_animation;
extern const AnimationAsset g_player_attack2_animation;
extern const AnimationAsset g_player_death_animation;
extern const AnimationAsset g_wolf_run_animation;
extern const AnimationAsset g_wolf_attack_animation;
extern const AnimationAsset g_wolf_death_animation;
extern const AnimationAsset g_ronin_run_animation;
extern const AnimationAsset g_ronin_attack_animation;
extern const AnimationAsset g_ronin_death_animation;
extern const PackedSpriteFrame far *g_gameover_frame;
extern const u8 far *g_gameover_pixels_even;
extern const u8 far *g_gameover_pixels_odd;

void set_video_mode(u8 mode);
void init_cga_mode5(void);
void render_foreground(GameContext *game);
void render_background_step(GameContext *game);
void draw_ui_frame_once(void);

void init_timer(void);
void restore_timer(void);
u8 consume_available_ticks(u8 max_ticks);
u16 pending_base_ticks(void);

void init_keyboard(void);
void restore_keyboard(void);
void sample_input(GameContext *game);
void init_default_bindings(InputBindings *bindings);

void init_game(GameContext *game);
void tick_game(GameContext *game);
const AnimationAsset *get_player_animation(PlayerAnimMode mode);
const AnimationAsset *get_opponent_animation(const OpponentRuntime *opponent);
u16 get_player_frame_index(const GameContext *game, const AnimationAsset *animation);
u16 get_opponent_frame_index(const GameContext *game, const AnimationAsset *animation);

#endif
