#include <string.h>

#include "rakuzitu.h"
#include "assets/bg_strip.h"
#include "assets/bg_sun.h"

static u8 top_backbuffer[TOP_VIEW_HEIGHT][BYTES_PER_SCANLINE];
static u8 clean_background_buffer[TOP_VIEW_HEIGHT][BYTES_PER_SCANLINE];
static u8 floor_backbuffer[FLOOR_VIEW_HEIGHT][BYTES_PER_SCANLINE];
static u8 clean_floor_buffer[FLOOR_VIEW_HEIGHT][BYTES_PER_SCANLINE];
static u16 g_background_step_row = 0;
static u16 g_floor_step_row = FLOOR_VIEW_HEIGHT;
static const u16 BACKGROUND_STEP_ROWS = 25;
static const u16 FLOOR_STEP_ROWS = 25;
static Rect g_previous_player_shadow_rect = { 0, 0, 0, 0, 0 };
static Rect g_previous_opponent_shadow_rect = { 0, 0, 0, 0, 0 };

typedef struct ShadowState {
    int draw_x;
    u16 frame_index;
    u8 repeat_count;
    u8 enabled;
} ShadowState;

static ShadowState g_previous_player_shadow_state = { 0, 0, 0, 0 };
static ShadowState g_previous_opponent_shadow_state = { 0, 0, 0, 0 };

/* 2D LUT: g_recolor_lut[color2_replacement][color3_replacement][packed_byte] */
static u8 g_recolor_lut[4][4][256];
static u8 g_luts_initialized = 0;

static void init_recolor_luts(void)
{
    int c2;
    int c3;
    int i;
    u8 packed;
    u8 out;
    u8 shift;
    u8 c;

    if (g_luts_initialized) {
        return;
    }
    
    for (c2 = 0; c2 < 4; ++c2) {
        for (c3 = 0; c3 < 4; ++c3) {
            for (i = 0; i < 256; ++i) {
                packed = (u8)i;
                out = 0;
                for (shift = 0; shift < 4; ++shift) {
                    c = (u8)((packed >> ((3 - shift) * 2)) & 0x03);
                    if (c == 2) {
                        c = (u8)c2;
                    } else if (c == 3) {
                        c = (u8)c3;
                    }
                    out |= (u8)(c << ((3 - shift) * 2));
                }
                g_recolor_lut[c2][c3][i] = out;
            }
        }
    }
    g_luts_initialized = 1;
}

void set_video_mode(u8 mode)
{
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = mode;
    int86(0x10, &regs, &regs);
}

void init_cga_mode5(void)
{
    union REGS regs;
    
    init_recolor_luts();
    set_video_mode(0x04);

    regs.h.ah = 0x0B;
    regs.h.bh = 0x00;
    regs.h.bl = 0x00;
    int86(0x10, &regs, &regs);

    regs.h.ah = 0x0B;
    regs.h.bh = 0x03;
    regs.h.bl = 0x00;
    int86(0x10, &regs, &regs);
}

static void wait_vblank(void)
{
    while (inp(0x3DA) & 0x08) {
    }
    while ((inp(0x3DA) & 0x08) == 0) {
    }
}

static void fill_scanline(u8 *dst, u8 value)
{
    memset(dst, value, BYTES_PER_SCANLINE);
}

static void fill_horizon_scanline(u8 *dst)
{
    memset(dst, BLACK_BYTE, BYTES_PER_SCANLINE);
}

static u8 shadow_state_equal(ShadowState a, ShadowState b)
{
    return (u8)(
        a.draw_x == b.draw_x &&
        a.frame_index == b.frame_index &&
        a.repeat_count == b.repeat_count &&
        a.enabled == b.enabled
    );
}

static void tile_strip_row(u8 *dst, const u8 far *row_pattern)
{
    u16 w0;
    u16 w1;
    u16 *d;
    int i;

    w0 = (u16)(row_pattern[0] | (((u16)row_pattern[1]) << 8));
    w1 = (u16)(row_pattern[2] | (((u16)row_pattern[3]) << 8));
    d = (u16 *)dst;

    if (w0 == w1) {
        for (i = 0; i < 40; ++i) {
            *d++ = w0;
        }
    } else {
        for (i = 0; i < 20; ++i) {
            *d++ = w0;
            *d++ = w1;
        }
    }
}

/* 16-BIT BRANCHLESS BLIT */
static void composite_row_near(u8 *dst, const u8 *mask, const u8 *pixels, u16 count)
{
    u16 *d16 = (u16 *)dst;
    const u16 *m16 = (const u16 *)mask;
    const u16 *p16 = (const u16 *)pixels;
    u16 words = count >> 1;

    /* Crunch 8 pixels (2 bytes) at a time, unconditionally */
    while (words--) {
        *d16 = (*d16 & *m16++) | *p16++;
        d16++;
    }
    
    /* Handle odd trailing byte if width isn't perfectly divisible by 2 */
    if (count & 1) {
        dst[count - 1] = (u8)((dst[count - 1] & mask[count - 1]) | pixels[count - 1]);
    }
}

/* 8-BIT BRANCHLESS LUT BLIT */
static void composite_row_near_lut(u8 *dst, const u8 *mask, const u8 *pixels, u16 count, const u8 *lut)
{
    while (count--) {
        *dst = (u8)((*dst & *mask++) | lut[*pixels++]);
        dst++;
    }
}

static void composite_sun_row_near(u8 *dst, const u8 *mask, u16 count)
{
    u16 *d16 = (u16 *)dst;
    const u16 *m16 = (const u16 *)mask;
    u16 words = count >> 1;
    u16 m;

    while (words--) {
        m = *m16++;
        /* (~m & 0xAAAA) generates perfectly anti-aliased red pixels on the fly */
        *d16 = (*d16 & m) | (~m & 0xAAAA);
        d16++;
    }
    
    if (count & 1) {
        m = mask[count - 1];
        dst[count - 1] = (u8)((dst[count - 1] & m) | (~m & 0xAA));
    }
}

static void draw_sun_slice(int sun_y, u16 start_row, u16 row_count)
{
    int sun_x_byte;
    int visible_byte_count;
    int row;
    int dst_y;
    const u8 far *mask_ptr;
    const u8 far *row_mask_even;
    const u8 far *row_mask_odd;
    u8 *dst;

    /* Only the mask buffer is needed now */
    static u8 g_near_mask_buf[80];

    /* Fixed X position at 104px (byte 26) */
    sun_x_byte = 26;
    visible_byte_count = BG_SUN_frames[0].bytes_per_row;

    row_mask_even = BG_SUN_mask_even + BG_SUN_frames[0].mask_even_offset;
    row_mask_odd  = BG_SUN_mask_odd  + BG_SUN_frames[0].mask_odd_offset;

    for (row = 0; row < (int)BG_SUN_frames[0].height; ++row) {
        dst_y = sun_y + row;
        
        if (row & 1) {
            mask_ptr = row_mask_odd;
            row_mask_odd += visible_byte_count;
        } else {
            mask_ptr = row_mask_even;
            row_mask_even += visible_byte_count;
        }

        if (dst_y < start_row || dst_y >= start_row + row_count) {
            continue;
        }
        if (dst_y < 0 || dst_y >= TOP_VIEW_HEIGHT) {
            continue;
        }

        /* Fetch only the mask */
        _fmemcpy((void far *)g_near_mask_buf, mask_ptr, (size_t)visible_byte_count);

        /* Draw directly into the clean background buffer */
        dst = &clean_background_buffer[dst_y][sun_x_byte];
        composite_sun_row_near(dst, g_near_mask_buf, (u16)visible_byte_count);
    }
}

static void compose_background_rows(GameContext *game, u16 start_row, u16 row_count)
{
    u16 end_row;
    u16 screen_y;
    int source_y;
    u8 *clean_ptr;

    end_row = start_row + row_count;
    if (end_row > TOP_VIEW_HEIGHT) {
        end_row = TOP_VIEW_HEIGHT;
    }

    clean_ptr = clean_background_buffer[start_row];

    for (screen_y = start_row; screen_y < end_row; ++screen_y) {
        source_y = (BG_STRIP_HEIGHT - TOP_VIEW_HEIGHT) + screen_y - game->background_scroll_pixels;

        if (source_y < 0 || source_y >= BG_STRIP_HEIGHT) {
            fill_scanline(clean_ptr, BLACK_BYTE);
        } else {
            tile_strip_row(clean_ptr, bg_strip_pixels[source_y]);
        }
        clean_ptr += BYTES_PER_SCANLINE;
    }

    /* Update Parallax Sun dynamically */
    draw_sun_slice(game->sun_y, start_row, row_count);

    /* Bulk standard block copy to top buffer instead of double-rendering */
    memcpy(top_backbuffer[start_row], clean_background_buffer[start_row], row_count * BYTES_PER_SCANLINE);
}

static void invalidate_rect(Rect *rect)
{
    rect->x = 0;
    rect->y = 0;
    rect->width = 0;
    rect->height = 0;
    rect->valid = 0;
}

static void clip_rect_to_top_area(Rect *rect)
{
    int x1;
    int y1;
    int x2;
    int y2;

    if (rect->valid == 0) {
        return;
    }

    x1 = rect->x;
    y1 = rect->y;
    x2 = rect->x + rect->width;
    y2 = rect->y + rect->height;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > SCREEN_WIDTH) x2 = SCREEN_WIDTH;
    if (y2 > TOP_VIEW_HEIGHT) y2 = TOP_VIEW_HEIGHT;

    if (x2 <= x1 || y2 <= y1) {
        invalidate_rect(rect);
        return;
    }

    rect->x = x1;
    rect->y = y1;
    rect->width = x2 - x1;
    rect->height = y2 - y1;
}

static void clip_rect_to_floor_area(Rect *rect)
{
    int x1;
    int y1;
    int x2;
    int y2;

    if (rect->valid == 0) {
        return;
    }

    x1 = rect->x;
    y1 = rect->y;
    x2 = rect->x + rect->width;
    y2 = rect->y + rect->height;

    if (x1 < 0) x1 = 0;
    if (y1 < FLOOR_VIEW_Y) y1 = FLOOR_VIEW_Y;
    if (x2 > SCREEN_WIDTH) x2 = SCREEN_WIDTH;
    if (y2 > SCREEN_HEIGHT) y2 = SCREEN_HEIGHT;

    if (x2 <= x1 || y2 <= y1) {
        invalidate_rect(rect);
        return;
    }

    rect->x = x1;
    rect->y = y1;
    rect->width = x2 - x1;
    rect->height = y2 - y1;
}

static Rect union_rects(Rect a, Rect b)
{
    Rect out;
    int x1, y1, x2, y2;

    if (a.valid == 0) return b;
    if (b.valid == 0) return a;

    x1 = (a.x < b.x) ? a.x : b.x;
    y1 = (a.y < b.y) ? a.y : b.y;
    x2 = ((a.x + a.width) > (b.x + b.width)) ? (a.x + a.width) : (b.x + b.width);
    y2 = ((a.y + a.height) > (b.y + b.height)) ? (a.y + a.height) : (b.y + b.height);

    out.x = x1;
    out.y = y1;
    out.width = x2 - x1;
    out.height = y2 - y1;
    out.valid = 1;
    clip_rect_to_top_area(&out);

    return out;
}

static Rect union_floor_rects(Rect a, Rect b)
{
    Rect out;
    int x1, y1, x2, y2;

    if (a.valid == 0) return b;
    if (b.valid == 0) return a;

    x1 = (a.x < b.x) ? a.x : b.x;
    y1 = (a.y < b.y) ? a.y : b.y;
    x2 = ((a.x + a.width) > (b.x + b.width)) ? (a.x + a.width) : (b.x + b.width);
    y2 = ((a.y + a.height) > (b.y + b.height)) ? (a.y + a.height) : (b.y + b.height);

    out.x = x1;
    out.y = y1;
    out.width = x2 - x1;
    out.height = y2 - y1;
    out.valid = 1;
    clip_rect_to_floor_area(&out);

    return out;
}

static void restore_rect_from_background(const Rect *rect)
{
    int row, start_byte, end_byte, byte_count;
    u8 *dst;
    const u8 *src;

    if (rect == NULL || rect->valid == 0) return;

    start_byte = rect->x >> 2;
    end_byte = (rect->x + rect->width + 3) >> 2;
    byte_count = end_byte - start_byte;

    dst = &top_backbuffer[rect->y][start_byte];
    src = &clean_background_buffer[rect->y][start_byte];

    for (row = rect->y; row < rect->y + rect->height; ++row) {
        memcpy(dst, src, (size_t)byte_count);
        dst += BYTES_PER_SCANLINE;
        src += BYTES_PER_SCANLINE;
    }
}

static void restore_rect_from_floor(const Rect *rect)
{
    int row, local_y, start_byte, end_byte, byte_count;
    u8 *dst;
    const u8 *src;

    if (rect == NULL || rect->valid == 0) return;

    start_byte = rect->x >> 2;
    end_byte = (rect->x + rect->width + 3) >> 2;
    byte_count = end_byte - start_byte;
    local_y = rect->y - FLOOR_VIEW_Y;

    dst = &floor_backbuffer[local_y][start_byte];
    src = &clean_floor_buffer[local_y][start_byte];

    for (row = 0; row < rect->height; ++row) {
        memcpy(dst, src, (size_t)byte_count);
        dst += BYTES_PER_SCANLINE;
        src += BYTES_PER_SCANLINE;
    }
}

static void refresh_clean_background(GameContext *game)
{
    if (game->rendered_background_scroll_pixels == game->background_scroll_pixels) {
        return;
    }

    compose_background_rows(game, 0, TOP_VIEW_HEIGHT);
    game->rendered_background_scroll_pixels = game->background_scroll_pixels;
    g_background_step_row = 0;
}

static void draw_animation_frame(const AnimationAsset *animation, u16 frame_index, int draw_x, int draw_y, u8 c2_rep, u8 c3_rep, Rect *out_rect)
{
    const PackedSpriteFrame far *frame_meta;
    const u8 far *frame_pixels_even;
    const u8 far *frame_pixels_odd;
    const u8 far *frame_mask_even;
    const u8 far *frame_mask_odd;
    const u8 far *row_pixels;
    const u8 far *row_mask;
    u8 *dst_ptr;
    int row;
    int dst_x_byte;
    int dst_y;
    int src_start_byte;
    int visible_byte_count;
    int clip_right_byte;
    const u8 *lut;
    
    /* Statically allocated buffers */
    static u8 g_near_mask_buf[80];
    static u8 g_near_pix_buf[80];

    if (animation == NULL) {
        invalidate_rect(out_rect);
        return;
    }

    frame_meta = animation->frames + (frame_index % animation->frame_count);
    if ((draw_x & 3) != 0) {
        draw_x &= ~3;
    }

    if (out_rect != NULL) {
        out_rect->x = draw_x;
        out_rect->y = draw_y;
        out_rect->width = frame_meta->width;
        out_rect->height = frame_meta->height;
        out_rect->valid = 1;
        clip_rect_to_top_area(out_rect);
    }

    src_start_byte = 0;
    dst_x_byte = draw_x >> 2;

    if (draw_x < 0) {
        src_start_byte = (-draw_x) >> 2;
        dst_x_byte = 0;
    }

    visible_byte_count = (int)frame_meta->bytes_per_row - src_start_byte;
    if (visible_byte_count <= 0) return;

    clip_right_byte = BYTES_PER_SCANLINE - dst_x_byte;
    if (visible_byte_count > clip_right_byte) visible_byte_count = clip_right_byte;
    if (visible_byte_count <= 0) return;

    frame_pixels_even = animation->pixels_even + frame_meta->pixel_even_offset + src_start_byte;
    frame_pixels_odd = animation->pixels_odd + frame_meta->pixel_odd_offset + src_start_byte;
    frame_mask_even = animation->mask_even + frame_meta->mask_even_offset + src_start_byte;
    frame_mask_odd = animation->mask_odd + frame_meta->mask_odd_offset + src_start_byte;

    if (c2_rep == 2 && c3_rep == 3) {
        lut = NULL;
    } else {
        lut = g_recolor_lut[c2_rep][c3_rep];
    }

    for (row = 0; row < (int)frame_meta->height; ++row) {
        dst_y = draw_y + row;
        
        if (row & 1) {
            row_pixels = frame_pixels_odd;
            row_mask = frame_mask_odd;
            frame_pixels_odd += frame_meta->bytes_per_row;
            frame_mask_odd += frame_meta->bytes_per_row;
        } else {
            row_pixels = frame_pixels_even;
            row_mask = frame_mask_even;
            frame_pixels_even += frame_meta->bytes_per_row;
            frame_mask_even += frame_meta->bytes_per_row;
        }

        if (dst_y < 0 || dst_y >= TOP_VIEW_HEIGHT) continue;

        _fmemcpy((void far *)g_near_mask_buf, row_mask, (size_t)visible_byte_count);
        _fmemcpy((void far *)g_near_pix_buf, row_pixels, (size_t)visible_byte_count);

        dst_ptr = &top_backbuffer[dst_y][dst_x_byte];

        if (lut != NULL) {
            composite_row_near_lut(dst_ptr, g_near_mask_buf, g_near_pix_buf, (u16)visible_byte_count, lut);
        } else {
            composite_row_near(dst_ptr, g_near_mask_buf, g_near_pix_buf, (u16)visible_byte_count);
        }
    }
}

static void build_player_sprite(const GameContext *game, Rect *rect_out, u16 *frame_index_out)
{
    const AnimationAsset *animation;
    u16 frame_index;
    const PackedSpriteFrame far *frame_meta;
    int draw_x;
    int draw_y;

    animation = get_player_animation(game->player.anim_mode);
    frame_index = get_player_frame_index(game, animation);
    frame_meta = animation->frames + frame_index;
    
    draw_x = fp_to_int(game->player.x_fp) & ~3;
    draw_y = game->player.y_baseline - frame_meta->height;
    
    rect_out->x = draw_x;
    rect_out->y = draw_y;
    rect_out->width = frame_meta->width;
    rect_out->height = frame_meta->height;
    rect_out->valid = 1;
    clip_rect_to_top_area(rect_out);
    
    if (frame_index_out != NULL) {
        *frame_index_out = frame_index;
    }
}

static void build_opponent_sprite(const GameContext *game, Rect *rect_out, u16 *frame_index_out)
{
    const AnimationAsset *animation;
    u16 frame_index;
    const PackedSpriteFrame far *frame_meta;
    int draw_x;
    int draw_y;

    if (game->opponent.active == 0 || game->opponent.def == NULL) {
        invalidate_rect(rect_out);
        return;
    }

    animation = get_opponent_animation(&game->opponent);
    frame_index = get_opponent_frame_index(game, animation);
    frame_meta = animation->frames + frame_index;
    
    draw_x = fp_to_int(game->opponent.x_fp) & ~3;
    draw_y = game->opponent.y_baseline - frame_meta->height;
    
    rect_out->x = draw_x;
    rect_out->y = draw_y;
    rect_out->width = frame_meta->width;
    rect_out->height = frame_meta->height;
    rect_out->valid = 1;
    clip_rect_to_top_area(rect_out);
    
    if (frame_index_out != NULL) {
        *frame_index_out = frame_index;
    }
}

static void blit_top_half(const GameContext *game)
{
    u16 row;
    u16 even_offset = 0;
    u16 odd_offset = CGA_ODD_OFFSET;
    u8 *backbuffer_ptr;

    if (game->video_wait_vblank) wait_vblank();

    backbuffer_ptr = top_backbuffer[0];
    for (row = 0; row < TOP_VIEW_HEIGHT; row += 2) {
        _fmemcpy(MK_FP(CGA_SEGMENT, even_offset), (const void far *)backbuffer_ptr, BYTES_PER_SCANLINE);
        even_offset += BYTES_PER_SCANLINE;
        backbuffer_ptr += (BYTES_PER_SCANLINE * 2);
    }

    backbuffer_ptr = top_backbuffer[1];
    for (row = 1; row < TOP_VIEW_HEIGHT; row += 2) {
        _fmemcpy(MK_FP(CGA_SEGMENT, odd_offset), (const void far *)backbuffer_ptr, BYTES_PER_SCANLINE);
        odd_offset += BYTES_PER_SCANLINE;
        backbuffer_ptr += (BYTES_PER_SCANLINE * 2);
    }
}

static void blit_rect_to_vram(const Rect *rect, const GameContext *game)
{
    int row, start_byte, end_byte, byte_count;
    u16 current_offset, offset_even, offset_odd;
    const u8 *backbuffer_ptr;

    if (rect == NULL || rect->valid == 0) return;
    if (game->video_wait_vblank) wait_vblank();

    start_byte = rect->x >> 2;
    end_byte = (rect->x + rect->width + 3) >> 2;
    byte_count = end_byte - start_byte;

    offset_even = (u16)((rect->y >> 1) * BYTES_PER_SCANLINE + start_byte);
    offset_odd  = offset_even + CGA_ODD_OFFSET;
    if (rect->y & 1) offset_even += BYTES_PER_SCANLINE;

    backbuffer_ptr = &top_backbuffer[rect->y][start_byte];

    for (row = rect->y; row < rect->y + rect->height; ++row) {
        if (row & 1) {
            current_offset = offset_odd;
            offset_odd += BYTES_PER_SCANLINE;
        } else {
            current_offset = offset_even;
            offset_even += BYTES_PER_SCANLINE;
        }
        _fmemcpy(MK_FP(CGA_SEGMENT, current_offset), (const void far *)backbuffer_ptr, (size_t)byte_count);
        backbuffer_ptr += BYTES_PER_SCANLINE;
    }
}

static void blit_floor_rows_to_vram(u16 start_row, u16 row_count, const GameContext *game)
{
    u16 screen_y, offset_even, offset_odd, current_offset;
    const u8 *backbuffer_ptr;

    (void)game;
    if (row_count == 0) return;

    screen_y = (u16)(FLOOR_VIEW_Y + start_row);
    offset_even = (u16)((screen_y >> 1) * BYTES_PER_SCANLINE);
    offset_odd = offset_even + CGA_ODD_OFFSET;
    if (screen_y & 1) offset_even += BYTES_PER_SCANLINE;

    backbuffer_ptr = &floor_backbuffer[start_row][0];
    while (row_count--) {
        if (screen_y & 1) {
            current_offset = offset_odd;
            offset_odd += BYTES_PER_SCANLINE;
        } else {
            current_offset = offset_even;
            offset_even += BYTES_PER_SCANLINE;
        }
        _fmemcpy(MK_FP(CGA_SEGMENT, current_offset), (const void far *)backbuffer_ptr, BYTES_PER_SCANLINE);
        backbuffer_ptr += BYTES_PER_SCANLINE;
        ++screen_y;
    }
}

static void blit_floor_rect_to_vram(const Rect *rect)
{
    int row, local_y, start_byte, end_byte, byte_count;
    u16 current_offset, offset_even, offset_odd;
    const u8 *backbuffer_ptr;

    if (rect == NULL || rect->valid == 0) return;

    start_byte = rect->x >> 2;
    end_byte = (rect->x + rect->width + 3) >> 2;
    byte_count = end_byte - start_byte;
    local_y = rect->y - FLOOR_VIEW_Y;

    offset_even = (u16)(((rect->y) >> 1) * BYTES_PER_SCANLINE + start_byte);
    offset_odd  = offset_even + CGA_ODD_OFFSET;
    if (rect->y & 1) offset_even += BYTES_PER_SCANLINE;

    backbuffer_ptr = &floor_backbuffer[local_y][start_byte];
    for (row = rect->y; row < rect->y + rect->height; ++row) {
        if (row & 1) {
            current_offset = offset_odd;
            offset_odd += BYTES_PER_SCANLINE;
        } else {
            current_offset = offset_even;
            offset_even += BYTES_PER_SCANLINE;
        }
        _fmemcpy(MK_FP(CGA_SEGMENT, current_offset), (const void far *)backbuffer_ptr, (size_t)byte_count);
        backbuffer_ptr += BYTES_PER_SCANLINE;
    }
}

static int get_floor_phase_rows(const GameContext *game)
{
    return (int)((game->background_scroll_pixels / 32U) * 32U);
}

static void compose_floor_rows(const GameContext *game, u16 start_row, u16 row_count)
{
    int base_top, phase_rows, window_top, floor_row, source_y;
    u16 local_row;

    base_top = BG_STRIP_HEIGHT - 208;
    phase_rows = get_floor_phase_rows(game);
    window_top = base_top - phase_rows;

    for (local_row = 0; local_row < row_count; ++local_row) {
        floor_row = (int)start_row + (int)local_row;
        source_y = window_top + (FLOOR_VIEW_HEIGHT - 1) - floor_row;

        if (source_y < 0 || source_y >= BG_STRIP_HEIGHT) {
            fill_scanline(clean_floor_buffer[floor_row], BLACK_BYTE);
        } else {
            tile_strip_row(clean_floor_buffer[floor_row], bg_strip_pixels[source_y]);
        }
    }

    memcpy(floor_backbuffer[start_row], clean_floor_buffer[start_row], row_count * BYTES_PER_SCANLINE);
}

static void refresh_floor_buffer(GameContext *game, u8 blit_now)
{
    compose_floor_rows(game, 0, FLOOR_VIEW_HEIGHT);
    game->rendered_floor_phase = (u16)get_floor_phase_rows(game);
    g_floor_step_row = FLOOR_VIEW_HEIGHT;
    if (blit_now) {
        blit_floor_rows_to_vram(0, FLOOR_VIEW_HEIGHT, game);
    }
}

static u8 get_shadow_repeat_count(int sun_y)
{
    if (sun_y < 26) return 3;
    if (sun_y < 53) return 4;
    if (sun_y < 80) return 5;
    return 0;
}

static void build_shadow_rect_from_even_rows(const AnimationAsset *animation, u16 frame_index, int draw_x, u8 repeat_count, Rect *out_rect)
{
    const PackedSpriteFrame far *frame_meta;
    int shadow_height;

    if (animation == NULL || repeat_count == 0) {
        invalidate_rect(out_rect);
        return;
    }

    frame_meta = animation->frames + (frame_index % animation->frame_count);
    shadow_height = (int)frame_meta->even_row_count * repeat_count;

    out_rect->x = draw_x & ~3;
    out_rect->y = FLOOR_VIEW_Y;
    out_rect->width = frame_meta->width;
    out_rect->height = shadow_height;
    out_rect->valid = 1;
    clip_rect_to_floor_area(out_rect);
}

/* 16-BIT BRANCHLESS SHADOW BLIT */
static void draw_shadow_from_even_rows(const AnimationAsset *animation, u16 frame_index, int draw_x, u8 repeat_count, Rect *out_rect)
{
    const PackedSpriteFrame far *frame_meta;
    const u8 far *frame_mask_even;
    const u8 far *mask_row_ptr;
    u8 *dst_ptr;
    int src_start_byte, dst_x_byte, visible_byte_count, clip_right_byte;
    int source_row, repeat_index, dst_y;
    u16 *d16;
    const u16 *m16;
    u16 words;

    static u8 g_near_mask_buf[80];

    build_shadow_rect_from_even_rows(animation, frame_index, draw_x, repeat_count, out_rect);
    if (out_rect->valid == 0) return;

    frame_meta = animation->frames + (frame_index % animation->frame_count);
    if ((draw_x & 3) != 0) draw_x &= ~3;

    src_start_byte = 0;
    dst_x_byte = draw_x >> 2;

    if (draw_x < 0) {
        src_start_byte = (-draw_x) >> 2;
        dst_x_byte = 0;
    }

    visible_byte_count = (int)frame_meta->bytes_per_row - src_start_byte;
    if (visible_byte_count <= 0) return;

    clip_right_byte = BYTES_PER_SCANLINE - dst_x_byte;
    if (visible_byte_count > clip_right_byte) visible_byte_count = clip_right_byte;
    if (visible_byte_count <= 0) return;

    frame_mask_even = animation->mask_even + frame_meta->mask_even_offset + src_start_byte;

    for (source_row = (int)frame_meta->even_row_count - 1; source_row >= 0; --source_row) {
        mask_row_ptr = frame_mask_even + (source_row * frame_meta->bytes_per_row);
        
        _fmemcpy((void far *)g_near_mask_buf, mask_row_ptr, (size_t)visible_byte_count);

        for (repeat_index = 0; repeat_index < repeat_count; ++repeat_index) {
            dst_y = FLOOR_VIEW_Y + (((int)frame_meta->even_row_count - 1 - source_row) * repeat_count) + repeat_index;
            if (dst_y < FLOOR_VIEW_Y || dst_y >= SCREEN_HEIGHT) continue;

            dst_ptr = &floor_backbuffer[dst_y - FLOOR_VIEW_Y][dst_x_byte];
            words = (u16)(visible_byte_count >> 1);
            
            d16 = (u16 *)dst_ptr;
            m16 = (const u16 *)g_near_mask_buf;

            /* Pure branchless 16-bit AND */
            while (words--) {
                *d16++ &= *m16++;
            }
            if (visible_byte_count & 1) {
                dst_ptr[visible_byte_count - 1] &= g_near_mask_buf[visible_byte_count - 1];
            }
        }
    }
}

void draw_ui_frame_once(void)
{
    static u8 horizon_scanline[BYTES_PER_SCANLINE];
    u16 offset;
    fill_horizon_scanline(horizon_scanline);
    offset = (u16)((TOP_VIEW_HEIGHT >> 1) * BYTES_PER_SCANLINE);

    wait_vblank();
    _fmemcpy(MK_FP(CGA_SEGMENT, offset), (const void far *)horizon_scanline, BYTES_PER_SCANLINE);
}

static void draw_gameover_fullscreen(const GameContext *game)
{
    const PackedSpriteFrame far *frame_meta;
    const u8 far *pixels_even;
    const u8 far *pixels_odd;
    u16 block_size;

    (void)game;
    
    frame_meta = g_gameover_frame;
    pixels_even = g_gameover_pixels_even + frame_meta->pixel_even_offset;
    pixels_odd = g_gameover_pixels_odd + frame_meta->pixel_odd_offset;
    block_size = (SCREEN_HEIGHT / 2) * BYTES_PER_SCANLINE;

    wait_vblank();

    _fmemcpy(MK_FP(CGA_SEGMENT, 0), (const void far *)pixels_even, block_size);
    _fmemcpy(MK_FP(CGA_SEGMENT, CGA_ODD_OFFSET), (const void far *)pixels_odd, block_size);
}

static void render_active_scene(GameContext *game)
{
    Rect current_player_rect;
    Rect current_opponent_rect;
    Rect dirty_rect;
    Rect current_player_shadow_rect;
    Rect current_opponent_shadow_rect;
    Rect floor_dirty_rect;
    u8 background_changed;
    u16 player_frame;
    u16 opp_frame;
    const AnimationAsset *player_anim;
    const AnimationAsset *opp_anim;
    ShadowState current_player_shadow_state;
    ShadowState current_opponent_shadow_state;
    u8 body_color;
    u8 shadow_repeat_count;
    int player_draw_x;
    int player_draw_y;
    int opponent_draw_x;
    int opponent_draw_y;

    background_changed = (game->rendered_background_scroll_pixels == 0xFFFF);
    if (background_changed) {
        refresh_clean_background(game);
    }
    if (game->rendered_floor_phase == 0xFFFF) {
        refresh_floor_buffer(game, 1);
    }

    body_color = (game->sun_y < 56 || game->sun_y >= 120) ? 0 : 3;

    invalidate_rect(&current_player_rect);
    invalidate_rect(&current_opponent_rect);
    invalidate_rect(&current_player_shadow_rect);
    invalidate_rect(&current_opponent_shadow_rect);
    current_player_shadow_state.draw_x = 0;
    current_player_shadow_state.frame_index = 0;
    current_player_shadow_state.repeat_count = 0;
    current_player_shadow_state.enabled = 0;
    current_opponent_shadow_state.draw_x = 0;
    current_opponent_shadow_state.frame_index = 0;
    current_opponent_shadow_state.repeat_count = 0;
    current_opponent_shadow_state.enabled = 0;
    
    player_anim = get_player_animation(game->player.anim_mode);
    build_player_sprite(game, &current_player_rect, &player_frame);
    player_draw_x = fp_to_int(game->player.x_fp) & ~3;
    player_draw_y = game->player.y_baseline - (player_anim->frames + player_frame)->height;
    shadow_repeat_count = get_shadow_repeat_count(game->sun_y);
    
    if (game->opponent.active) {
        build_opponent_sprite(game, &current_opponent_rect, &opp_frame);
        opp_anim = get_opponent_animation(&game->opponent);
        opponent_draw_x = fp_to_int(game->opponent.x_fp) & ~3;
        opponent_draw_y = game->opponent.y_baseline - (opp_anim->frames + opp_frame)->height;
    } else {
        opp_anim = NULL;
        opp_frame = 0;
        opponent_draw_x = 0;
        opponent_draw_y = 0;
    }

    if (background_changed) {
        draw_animation_frame(player_anim, player_frame, player_draw_x,
            player_draw_y, 2, body_color, &game->player.rect);
            
        if (game->opponent.active) {
            draw_animation_frame(opp_anim, opp_frame, opponent_draw_x,
                opponent_draw_y, (u8)game->opponent.eye_color, body_color, &game->opponent.rect);
        } else {
            invalidate_rect(&game->opponent.rect);
        }
        blit_top_half(game);
    } else {
        dirty_rect = union_rects(union_rects(game->previous_player_rect, game->previous_opponent_rect), union_rects(current_player_rect, current_opponent_rect));
        restore_rect_from_background(&dirty_rect);

        draw_animation_frame(player_anim, player_frame, player_draw_x,
            player_draw_y, 2, body_color, &game->player.rect);
            
        if (game->opponent.active) {
            draw_animation_frame(opp_anim, opp_frame, opponent_draw_x,
                opponent_draw_y, (u8)game->opponent.eye_color, body_color, &game->opponent.rect);
        } else {
            invalidate_rect(&game->opponent.rect);
        }
        blit_rect_to_vram(&dirty_rect, game);
    }

    floor_dirty_rect = union_floor_rects(g_previous_player_shadow_rect, g_previous_opponent_shadow_rect);
    if (!game->low_detail && shadow_repeat_count != 0) {
        current_player_shadow_state.draw_x = player_draw_x;
        current_player_shadow_state.frame_index = player_frame;
        current_player_shadow_state.repeat_count = shadow_repeat_count;
        current_player_shadow_state.enabled = 1;
        build_shadow_rect_from_even_rows(player_anim, player_frame, player_draw_x, shadow_repeat_count, &current_player_shadow_rect);
        if (game->opponent.active) {
            current_opponent_shadow_state.draw_x = opponent_draw_x;
            current_opponent_shadow_state.frame_index = opp_frame;
            current_opponent_shadow_state.repeat_count = shadow_repeat_count;
            current_opponent_shadow_state.enabled = 1;
            build_shadow_rect_from_even_rows(opp_anim, opp_frame, opponent_draw_x, shadow_repeat_count, &current_opponent_shadow_rect);
        }
    }

    if (!shadow_state_equal(g_previous_player_shadow_state, current_player_shadow_state) ||
        !shadow_state_equal(g_previous_opponent_shadow_state, current_opponent_shadow_state)) {
        floor_dirty_rect = union_floor_rects(floor_dirty_rect, union_floor_rects(current_player_shadow_rect, current_opponent_shadow_rect));
        restore_rect_from_floor(&floor_dirty_rect);

        if (!game->low_detail && shadow_repeat_count != 0) {
            draw_shadow_from_even_rows(player_anim, player_frame, player_draw_x, shadow_repeat_count, &current_player_shadow_rect);
            if (game->opponent.active) {
                draw_shadow_from_even_rows(opp_anim, opp_frame, opponent_draw_x, shadow_repeat_count, &current_opponent_shadow_rect);
            }
        }
        if (floor_dirty_rect.valid) {
            blit_floor_rect_to_vram(&floor_dirty_rect);
        }
    }

    game->previous_player_rect = game->player.rect;
    game->previous_opponent_rect = game->opponent.rect;
    g_previous_player_shadow_rect = current_player_shadow_rect;
    g_previous_opponent_shadow_rect = current_opponent_shadow_rect;
    g_previous_player_shadow_state = current_player_shadow_state;
    g_previous_opponent_shadow_state = current_opponent_shadow_state;
}

void render_foreground(GameContext *game)
{
    if (game->state == GAME_STATE_GAMEOVER) {
        if (game->gameover_drawn == 0) {
            draw_gameover_fullscreen(game);
            game->gameover_drawn = 1;
        }
        return;
    }
    render_active_scene(game);
}

void render_background_step(GameContext *game)
{
    Rect player_rect;
    Rect opponent_rect;
    Rect step_rect;
    u16 player_frame;
    u16 opp_frame;
    u16 step_start_row;
    u16 step_row_count;
    const AnimationAsset *player_anim;
    const AnimationAsset *opp_anim;
    int player_draw_x;
    int player_draw_y;
    int opponent_draw_x;
    int opponent_draw_y;
    u8 body_color;

    if (game->state == GAME_STATE_GAMEOVER || game->state == GAME_STATE_PLAYER_DYING) {
        return;
    }
    if (game->rendered_background_scroll_pixels == game->background_scroll_pixels && g_background_step_row == 0) {
        return;
    }
    if (g_background_step_row >= TOP_VIEW_HEIGHT) {
        g_background_step_row = 0;
    }

    step_start_row = g_background_step_row;
    step_row_count = BACKGROUND_STEP_ROWS;
    if (step_start_row + step_row_count > TOP_VIEW_HEIGHT) {
        step_row_count = TOP_VIEW_HEIGHT - step_start_row;
    }

    compose_background_rows(game, step_start_row, step_row_count);

    body_color = (game->sun_y < 56 || game->sun_y >= 120) ? 0 : 3;

    player_anim = get_player_animation(game->player.anim_mode);
    build_player_sprite(game, &player_rect, &player_frame);
    player_draw_x = fp_to_int(game->player.x_fp) & ~3;
    player_draw_y = game->player.y_baseline - (player_anim->frames + player_frame)->height;
    
    if (player_rect.valid && player_rect.y < (int)(step_start_row + step_row_count) && (player_rect.y + player_rect.height) > (int)step_start_row) {
        draw_animation_frame(player_anim, player_frame, player_draw_x, player_draw_y, 2, body_color, &game->player.rect);
    }

    if (game->opponent.active) {
        opp_anim = get_opponent_animation(&game->opponent);
        build_opponent_sprite(game, &opponent_rect, &opp_frame);
        opponent_draw_x = fp_to_int(game->opponent.x_fp) & ~3;
        opponent_draw_y = game->opponent.y_baseline - (opp_anim->frames + opp_frame)->height;
        
        if (opponent_rect.valid && opponent_rect.y < (int)(step_start_row + step_row_count) && (opponent_rect.y + opponent_rect.height) > (int)step_start_row) {
            draw_animation_frame(opp_anim, opp_frame, opponent_draw_x, opponent_draw_y, (u8)game->opponent.eye_color, body_color, &game->opponent.rect);
        }
    }

    step_rect.x = 0;
    step_rect.y = step_start_row;
    step_rect.width = SCREEN_WIDTH;
    step_rect.height = step_row_count;
    step_rect.valid = 1;
    blit_rect_to_vram(&step_rect, game);

    g_background_step_row += step_row_count;
    if (g_background_step_row >= TOP_VIEW_HEIGHT) {
        game->rendered_background_scroll_pixels = game->background_scroll_pixels;
        g_background_step_row = 0;
    }
}

void render_floor_step(GameContext *game)
{
    Rect player_shadow_rect;
    Rect opponent_shadow_rect;
    const AnimationAsset *player_anim;
    const AnimationAsset *opp_anim;
    u16 player_frame;
    u16 opp_frame;
    u16 step_start_row;
    u16 step_row_count;
    u16 current_phase;
    u8 shadow_repeat_count;

    if (game->state == GAME_STATE_GAMEOVER || game->state == GAME_STATE_PLAYER_DYING) {
        return;
    }
    invalidate_rect(&player_shadow_rect);
    invalidate_rect(&opponent_shadow_rect);

    current_phase = (u16)get_floor_phase_rows(game);
    if (game->rendered_floor_phase == current_phase && g_floor_step_row >= FLOOR_VIEW_HEIGHT) {
        return;
    }
    if (g_floor_step_row == 0 || g_floor_step_row > FLOOR_VIEW_HEIGHT) {
        g_floor_step_row = FLOOR_VIEW_HEIGHT;
    }

    step_row_count = FLOOR_STEP_ROWS;
    if (step_row_count > g_floor_step_row) {
        step_row_count = g_floor_step_row;
    }
    step_start_row = g_floor_step_row - step_row_count;

    compose_floor_rows(game, step_start_row, step_row_count);

    shadow_repeat_count = get_shadow_repeat_count(game->sun_y);
    if (!game->low_detail && shadow_repeat_count != 0) {
        player_anim = get_player_animation(game->player.anim_mode);
        player_frame = get_player_frame_index(game, player_anim);
        build_shadow_rect_from_even_rows(player_anim, player_frame, fp_to_int(game->player.x_fp) & ~3, shadow_repeat_count, &player_shadow_rect);

        if (player_shadow_rect.valid &&
            player_shadow_rect.y < (int)(FLOOR_VIEW_Y + step_start_row + step_row_count) &&
            (player_shadow_rect.y + player_shadow_rect.height) > (int)(FLOOR_VIEW_Y + step_start_row)) {
            draw_shadow_from_even_rows(player_anim, player_frame, fp_to_int(game->player.x_fp) & ~3, shadow_repeat_count, &player_shadow_rect);
        }

        if (game->opponent.active) {
            opp_anim = get_opponent_animation(&game->opponent);
            opp_frame = get_opponent_frame_index(game, opp_anim);
            build_shadow_rect_from_even_rows(opp_anim, opp_frame, fp_to_int(game->opponent.x_fp) & ~3, shadow_repeat_count, &opponent_shadow_rect);
            if (opponent_shadow_rect.valid &&
                opponent_shadow_rect.y < (int)(FLOOR_VIEW_Y + step_start_row + step_row_count) &&
                (opponent_shadow_rect.y + opponent_shadow_rect.height) > (int)(FLOOR_VIEW_Y + step_start_row)) {
                draw_shadow_from_even_rows(opp_anim, opp_frame, fp_to_int(game->opponent.x_fp) & ~3, shadow_repeat_count, &opponent_shadow_rect);
            }
        }
    }
    blit_floor_rows_to_vram(step_start_row, step_row_count, game);

    g_floor_step_row -= step_row_count;
    if (g_floor_step_row == 0) {
        game->rendered_floor_phase = current_phase;
        g_floor_step_row = FLOOR_VIEW_HEIGHT;
    }
}
