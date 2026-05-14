#include "assets/oppo_wolf_run_meta.h"
#include "assets/oppo_wolf_atk_meta.h"
#include "assets/oppo_wolf_death_meta.h"
#include "assets/oppo_ronin_run_meta.h"
#include "assets/oppo_ronin_attack_meta.h"
#include "assets/oppo_ronin_death_meta.h"
#include "assets/oppo_ashigaru_run_meta.h"
#include "assets/oppo_ashigaru_attack_meta.h"
#include "assets/oppo_ashigaru_death_meta.h"
#include "assets/oppo_peasant_run_meta.h"
#include "assets/oppo_peasant_attack_meta.h"
#include "assets/oppo_peasant_death_meta.h"
#include "assets/oppo_tsujigiri_run_meta.h"
#include "assets/oppo_tsujigiri_attack_meta.h"
#include "assets/oppo_tsujigiri_death_meta.h"

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

const AnimationAsset g_wolf_run_animation = {
    OPPO_WOLF_RUN_META_FRAME_COUNT,
    oppo_wolf_run_meta_pixels_even,
    oppo_wolf_run_meta_pixels_odd,
    oppo_wolf_run_meta_mask_even,
    oppo_wolf_run_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_wolf_run_meta_frames
};

const AnimationAsset g_wolf_attack_animation = {
    OPPO_WOLF_ATK_META_FRAME_COUNT,
    oppo_wolf_atk_meta_pixels_even,
    oppo_wolf_atk_meta_pixels_odd,
    oppo_wolf_atk_meta_mask_even,
    oppo_wolf_atk_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_wolf_atk_meta_frames
};

const AnimationAsset g_wolf_death_animation = {
    OPPO_WOLF_DEATH_META_FRAME_COUNT,
    oppo_wolf_death_meta_pixels_even,
    oppo_wolf_death_meta_pixels_odd,
    oppo_wolf_death_meta_mask_even,
    oppo_wolf_death_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_wolf_death_meta_frames
};

const AnimationAsset g_ronin_run_animation = {
    OPPO_RONIN_RUN_META_FRAME_COUNT,
    oppo_ronin_run_meta_pixels_even,
    oppo_ronin_run_meta_pixels_odd,
    oppo_ronin_run_meta_mask_even,
    oppo_ronin_run_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_ronin_run_meta_frames
};

const AnimationAsset g_ronin_attack_animation = {
    OPPO_RONIN_ATTACK_META_FRAME_COUNT,
    oppo_ronin_attack_meta_pixels_even,
    oppo_ronin_attack_meta_pixels_odd,
    oppo_ronin_attack_meta_mask_even,
    oppo_ronin_attack_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_ronin_attack_meta_frames
};

const AnimationAsset g_ronin_death_animation = {
    OPPO_RONIN_DEATH_META_FRAME_COUNT,
    oppo_ronin_death_meta_pixels_even,
    oppo_ronin_death_meta_pixels_odd,
    oppo_ronin_death_meta_mask_even,
    oppo_ronin_death_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_ronin_death_meta_frames
};

const AnimationAsset g_ashigaru_run_animation = {
    OPPO_ASHIGARU_RUN_META_FRAME_COUNT,
    oppo_ashigaru_run_meta_pixels_even,
    oppo_ashigaru_run_meta_pixels_odd,
    oppo_ashigaru_run_meta_mask_even,
    oppo_ashigaru_run_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_ashigaru_run_meta_frames
};

const AnimationAsset g_ashigaru_attack_animation = {
    OPPO_ASHIGARU_ATTACK_META_FRAME_COUNT,
    oppo_ashigaru_attack_meta_pixels_even,
    oppo_ashigaru_attack_meta_pixels_odd,
    oppo_ashigaru_attack_meta_mask_even,
    oppo_ashigaru_attack_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_ashigaru_attack_meta_frames
};

const AnimationAsset g_ashigaru_death_animation = {
    OPPO_ASHIGARU_DEATH_META_FRAME_COUNT,
    oppo_ashigaru_death_meta_pixels_even,
    oppo_ashigaru_death_meta_pixels_odd,
    oppo_ashigaru_death_meta_mask_even,
    oppo_ashigaru_death_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_ashigaru_death_meta_frames
};

const AnimationAsset g_peasant_run_animation = {
    OPPO_PEASANT_RUN_META_FRAME_COUNT,
    oppo_peasant_run_meta_pixels_even,
    oppo_peasant_run_meta_pixels_odd,
    oppo_peasant_run_meta_mask_even,
    oppo_peasant_run_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_peasant_run_meta_frames
};

const AnimationAsset g_peasant_attack_animation = {
    OPPO_PEASANT_ATTACK_META_FRAME_COUNT,
    oppo_peasant_attack_meta_pixels_even,
    oppo_peasant_attack_meta_pixels_odd,
    oppo_peasant_attack_meta_mask_even,
    oppo_peasant_attack_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_peasant_attack_meta_frames
};

const AnimationAsset g_peasant_death_animation = {
    OPPO_PEASANT_DEATH_META_FRAME_COUNT,
    oppo_peasant_death_meta_pixels_even,
    oppo_peasant_death_meta_pixels_odd,
    oppo_peasant_death_meta_mask_even,
    oppo_peasant_death_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_peasant_death_meta_frames
};

const AnimationAsset g_tsujigiri_run_animation = {
    OPPO_TSUJIGIRI_RUN_META_FRAME_COUNT,
    oppo_tsujigiri_run_meta_pixels_even,
    oppo_tsujigiri_run_meta_pixels_odd,
    oppo_tsujigiri_run_meta_mask_even,
    oppo_tsujigiri_run_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_tsujigiri_run_meta_frames
};

const AnimationAsset g_tsujigiri_attack_animation = {
    OPPO_TSUJIGIRI_ATTACK_META_FRAME_COUNT,
    oppo_tsujigiri_attack_meta_pixels_even,
    oppo_tsujigiri_attack_meta_pixels_odd,
    oppo_tsujigiri_attack_meta_mask_even,
    oppo_tsujigiri_attack_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_tsujigiri_attack_meta_frames
};

const AnimationAsset g_tsujigiri_death_animation = {
    OPPO_TSUJIGIRI_DEATH_META_FRAME_COUNT,
    oppo_tsujigiri_death_meta_pixels_even,
    oppo_tsujigiri_death_meta_pixels_odd,
    oppo_tsujigiri_death_meta_mask_even,
    oppo_tsujigiri_death_meta_mask_odd,
    (const PackedSpriteFrame far *)oppo_tsujigiri_death_meta_frames
};
