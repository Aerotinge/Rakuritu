#include "assets/player_run_meta.h"
#include "assets/player_attack1_meta.h"
#include "assets/player_attack2_meta.h"
#include "assets/player_death_meta.h"

typedef unsigned char u8;
typedef unsigned short u16;

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

const AnimationAsset g_player_run_animation = {
    PLAYER_RUN_META_FRAME_COUNT,
    player_run_meta_pixels_even,
    player_run_meta_pixels_odd,
    player_run_meta_mask_even,
    player_run_meta_mask_odd,
    (const PackedSpriteFrame far *)player_run_meta_frames
};

const AnimationAsset g_player_attack1_animation = {
    PLAYER_ATTACK1_META_FRAME_COUNT,
    player_attack1_meta_pixels_even,
    player_attack1_meta_pixels_odd,
    player_attack1_meta_mask_even,
    player_attack1_meta_mask_odd,
    (const PackedSpriteFrame far *)player_attack1_meta_frames
};

const AnimationAsset g_player_attack2_animation = {
    PLAYER_ATTACK2_META_FRAME_COUNT,
    player_attack2_meta_pixels_even,
    player_attack2_meta_pixels_odd,
    player_attack2_meta_mask_even,
    player_attack2_meta_mask_odd,
    (const PackedSpriteFrame far *)player_attack2_meta_frames
};

const AnimationAsset g_player_death_animation = {
    PLAYER_DEATH_META_FRAME_COUNT,
    player_death_meta_pixels_even,
    player_death_meta_pixels_odd,
    player_death_meta_mask_even,
    player_death_meta_mask_odd,
    (const PackedSpriteFrame far *)player_death_meta_frames
};
