#include "assets/gameover_1_meta.h"

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

const PackedSpriteFrame far *g_gameover_frame = (const PackedSpriteFrame far *)gameover_1_meta_frames;
const u8 far *g_gameover_pixels_even = gameover_1_meta_pixels_even;
const u8 far *g_gameover_pixels_odd = gameover_1_meta_pixels_odd;
