#include <string.h>

#include "rakuzitu.h"
#include "assets/bg_strip.h"
#include "assets/bg_sun.h"

static u8 top_backbuffer[TOP_VIEW_HEIGHT][BYTES_PER_SCANLINE];
static u8 clean_background_buffer[TOP_VIEW_HEIGHT][BYTES_PER_SCANLINE];
static u16 g_background_step_row = 0;
static const u16 BACKGROUND_STEP_ROWS = 25;

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
    set_video_mode(0x05);

    regs.h.ah = 0x0B;
    regs.h.bh = 0x00;
    regs.h.bl = 0x00;
    int86(0x10, &regs, &regs);

    regs.h.ah = 0x0B;
    regs.h.bh = 0x01;
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

static void draw_sun_slice(int sun_y, u16 start_row, u16 row_count, u8 write_clean, u8 write_top)
{
    int sun_x_byte;
    int visible_byte_count;
    int row;
    int dst_y;
    const u8 far *pix_ptr;
    const u8 far *mask_ptr;
    const u8 far *row_pixels_even;
    const u8 far *row_pixels_odd;
    const u8 far *row_mask_even;
    const u8 far *row_mask_odd;
    const u8 far *temp_mask;
    const u8 far *temp_pix;
    u8 *dst;
    int count;
    u8 m;
    u8 p;

    sun_x_byte = 26; /* Fixed X position at 104px (byte 26) */
    visible_byte_count = BG_SUN_frames[0].bytes_per_row;

    row_pixels_even = BG_SUN_pixels_even + BG_SUN_frames[0].pixel_even_offset;
    row_pixels_odd  = BG_SUN_pixels_odd  + BG_SUN_frames[0].pixel_odd_offset;
    row_mask_even   = BG_SUN_mask_even   + BG_SUN_frames[0].mask_even_offset;
    row_mask_odd    = BG_SUN_mask_odd    + BG_SUN_frames[0].mask_odd_offset;

    for (row = 0; row < (int)BG_SUN_frames[0].height; ++row) {
        dst_y = sun_y + row;
        
        if (row & 1) {
            pix_ptr = row_pixels_odd;
            mask_ptr = row_mask_odd;
            row_pixels_odd += visible_byte_count;
            row_mask_odd += visible_byte_count;
        } else {
            pix_ptr = row_pixels_even;
            mask_ptr = row_mask_even;
            row_pixels_even += visible_byte_count;
            row_mask_even += visible_byte_count;
        }

        if (dst_y < start_row || dst_y >= start_row + row_count) {
            continue;
        }
        if (dst_y < 0 || dst_y >= TOP_VIEW_HEIGHT) {
            continue;
        }

        if (write_clean) {
            dst = &clean_background_buffer[dst_y][sun_x_byte];
            count = visible_byte_count;
            temp_mask = mask_ptr;
            temp_pix = pix_ptr;
            while (count--) {
                m = *temp_mask++;
                if (m == 0xFF) {
                    temp_pix++;
                    dst++;
                } else {
                    p = *temp_pix++;
                    if (m == 0x00) {
                        *dst++ = p;
                    } else {
                        *dst = (*dst & m) | p;
                        dst++;
                    }
                }
            }
        }
        
        if (write_top) {
            dst = &top_backbuffer[dst_y][sun_x_byte];
            count = visible_byte_count;
            temp_mask = mask_ptr;
            temp_pix = pix_ptr;
            while (count--) {
                m = *temp_mask++;
                if (m == 0xFF) {
                    temp_pix++;
                    dst++;
                } else {
                    p = *temp_pix++;
                    if (m == 0x00) {
                        *dst++ = p;
                    } else {
                        *dst = (*dst & m) | p;
                        dst++;
                    }
                }
            }
        }
    }
}

static void compose_background(GameContext *game)
{
    int screen_y;
    int source_y;
    u8 *dst_ptr;

    dst_ptr = (u8 *)top_backbuffer;

    for (screen_y = 0; screen_y < TOP_VIEW_HEIGHT; ++screen_y) {
        source_y = (BG_STRIP_HEIGHT - TOP_VIEW_HEIGHT) + screen_y - game->background_scroll_pixels;

        if (source_y < 0 || source_y >= BG_STRIP_HEIGHT) {
            fill_scanline(dst_ptr, BLACK_BYTE);
        } else {
            tile_strip_row(dst_ptr, bg_strip_pixels[source_y]);
        }
        dst_ptr += BYTES_PER_SCANLINE;
    }

    /* Draw Parallax Sun purely to Top Buffer; it will be cloned to Clean */
    draw_sun_slice(game->sun_y, 0, TOP_VIEW_HEIGHT, 0, 1);
}

static void compose_background_rows(GameContext *game, u16 start_row, u16 row_count, u8 write_top_buffer)
{
    u16 end_row;
    u16 screen_y;
    int source_y;
    u8 *clean_ptr;
    u8 *top_ptr;

    end_row = start_row + row_count;
    if (end_row > TOP_VIEW_HEIGHT) {
        end_row = TOP_VIEW_HEIGHT;
    }

    clean_ptr = clean_background_buffer[start_row];
    top_ptr = top_backbuffer[start_row];

    for (screen_y = start_row; screen_y < end_row; ++screen_y) {
        source_y = (BG_STRIP_HEIGHT - TOP_VIEW_HEIGHT) + screen_y - game->background_scroll_pixels;

        if (source_y < 0 || source_y >= BG_STRIP_HEIGHT) {
            fill_scanline(clean_ptr, BLACK_BYTE);
            if (write_top_buffer) {
                fill_scanline(top_ptr, BLACK_BYTE);
            }
        } else {
            tile_strip_row(clean_ptr, bg_strip_pixels[source_y]);
            if (write_top_buffer) {
                tile_strip_row(top_ptr, bg_strip_pixels[source_y]);
            }
        }
        clean_ptr += BYTES_PER_SCANLINE;
        top_ptr += BYTES_PER_SCANLINE;
    }

    /* Update Parallax Sun dynamically onto the drawn background slices */
    draw_sun_slice(game->sun_y, start_row, row_count, 1, write_top_buffer);
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

    if (x1 < 0) {
        x1 = 0;
    }
    if (y1 < 0) {
        y1 = 0;
    }
    if (x2 > SCREEN_WIDTH) {
        x2 = SCREEN_WIDTH;
    }
    if (y2 > TOP_VIEW_HEIGHT) {
        y2 = TOP_VIEW_HEIGHT;
    }

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
    int x1;
    int y1;
    int x2;
    int y2;

    if (a.valid == 0) {
        return b;
    }
    if (b.valid == 0) {
        return a;
    }

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

static void restore_rect_from_background(const Rect *rect)
{
    int row;
    int start_byte;
    int end_byte;
    int byte_count;
    u8 *dst;
    const u8 *src;

    if (rect == NULL || rect->valid == 0) {
        return;
    }

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

static void refresh_clean_background(GameContext *game)
{
    if (game->rendered_background_scroll_pixels == game->background_scroll_pixels) {
        return;
    }

    compose_background(game);
    memcpy(clean_background_buffer, top_backbuffer, sizeof(top_backbuffer));
    game->rendered_background_scroll_pixels = game->background_scroll_pixels;
    g_background_step_row = TOP_VIEW_HEIGHT;
}

static void draw_animation_frame(const AnimationAsset *animation, u16 frame_index, int draw_x, int draw_y, u8 c2_rep, u8 c3_rep, Rect *out_rect)
{
    const PackedSpriteFrame far *frame_meta;
    const u8 far *frame_pixels_even;
    const u8 far *frame_pixels_odd;
    const u8 far *frame_mask_even;
    const u8 far *frame_mask_odd;
    const u8 far *mask_ptr;
    const u8 far *pix_ptr;
    u8 *dst_ptr;
    int row;
    int dst_x_byte;
    int dst_y;
    int src_start_byte;
    int visible_byte_count;
    int clip_right_byte;
    int count;
    u8 m;
    u8 p;
    const u8 *lut;

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
    if (visible_byte_count <= 0) {
        return;
    }

    clip_right_byte = BYTES_PER_SCANLINE - dst_x_byte;
    if (visible_byte_count > clip_right_byte) {
        visible_byte_count = clip_right_byte;
    }

    if (visible_byte_count <= 0) {
        return;
    }

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
            pix_ptr = frame_pixels_odd;
            mask_ptr = frame_mask_odd;
            frame_pixels_odd += frame_meta->bytes_per_row;
            frame_mask_odd += frame_meta->bytes_per_row;
        } else {
            pix_ptr = frame_pixels_even;
            mask_ptr = frame_mask_even;
            frame_pixels_even += frame_meta->bytes_per_row;
            frame_mask_even += frame_meta->bytes_per_row;
        }

        if (dst_y < 0 || dst_y >= TOP_VIEW_HEIGHT) {
            continue;
        }

        dst_ptr = &top_backbuffer[dst_y][dst_x_byte];
        count = visible_byte_count;

        if (lut != NULL) {
            while (count--) {
                m = *mask_ptr++;
                if (m == 0xFF) {
                    pix_ptr++;
                    dst_ptr++;
                } else {
                    p = lut[*pix_ptr++];
                    if (m == 0x00) {
                        *dst_ptr++ = p;
                    } else {
                        *dst_ptr = (*dst_ptr & m) | p;
                        dst_ptr++;
                    }
                }
            }
        } else {
            while (count--) {
                m = *mask_ptr++;
                if (m == 0xFF) {
                    pix_ptr++;
                    dst_ptr++;
                } else {
                    p = *pix_ptr++;
                    if (m == 0x00) {
                        *dst_ptr++ = p;
                    } else {
                        *dst_ptr = (*dst_ptr & m) | p;
                        dst_ptr++;
                    }
                }
            }
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
    u16 even_offset;
    u16 odd_offset;
    u8 *backbuffer_ptr;

    even_offset = 0;
    odd_offset = CGA_ODD_OFFSET;

    if (game->video_wait_vblank) {
        wait_vblank();
    }

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
    int row;
    int start_byte;
    int end_byte;
    int byte_count;
    u16 current_offset;
    u16 offset_even;
    u16 offset_odd;
    const u8 *backbuffer_ptr;

    if (rect == NULL || rect->valid == 0) {
        return;
    }
    
    if (game->video_wait_vblank) {
        wait_vblank();
    }

    start_byte = rect->x >> 2;
    end_byte = (rect->x + rect->width + 3) >> 2;
    byte_count = end_byte - start_byte;

    offset_even = (u16)((rect->y >> 1) * BYTES_PER_SCANLINE + start_byte);
    offset_odd  = offset_even + CGA_ODD_OFFSET;
    if (rect->y & 1) {
        offset_even += BYTES_PER_SCANLINE;
    }

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

void draw_ui_frame_once(void)
{
    static u8 ui_scanline[BYTES_PER_SCANLINE];
    u16 screen_y;
    u16 offset;
    u16 x;
    u16 offset_even;
    u16 offset_odd;

    fill_scanline(ui_scanline, WHITE_BYTE);
    for (x = 20; x < 30; ++x) {
        ui_scanline[x] = BLACK_BYTE;
        ui_scanline[x + 12] = BLACK_BYTE;
        ui_scanline[x + 24] = BLACK_BYTE;
    }

    offset_even = (TOP_VIEW_HEIGHT >> 1) * BYTES_PER_SCANLINE;
    offset_odd = offset_even + CGA_ODD_OFFSET;

    wait_vblank();

    for (screen_y = TOP_VIEW_HEIGHT; screen_y < SCREEN_HEIGHT; ++screen_y) {
        if (screen_y & 1) {
            offset = offset_odd;
            offset_odd += BYTES_PER_SCANLINE;
        } else {
            offset = offset_even;
            offset_even += BYTES_PER_SCANLINE;
        }
        _fmemcpy(MK_FP(CGA_SEGMENT, offset), (const void far *)ui_scanline, BYTES_PER_SCANLINE);
    }
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
    u8 background_changed;
    u16 player_frame;
    u16 opp_frame;
    const AnimationAsset *player_anim;
    const AnimationAsset *opp_anim;
    u8 body_color;

    background_changed = (game->rendered_background_scroll_pixels == 0xFFFF);
    if (background_changed) {
        refresh_clean_background(game);
    }

    /* Set body color: 0 (Black) when silhouetted, 3 (White/Grey) when normal */
    body_color = (game->sun_y < 56 || game->sun_y >= 120) ? 0 : 3;

    invalidate_rect(&current_player_rect);
    invalidate_rect(&current_opponent_rect);
    
    player_anim = get_player_animation(game->player.anim_mode);
    build_player_sprite(game, &current_player_rect, &player_frame);
    
    if (game->opponent.active) {
        build_opponent_sprite(game, &current_opponent_rect, &opp_frame);
        opp_anim = get_opponent_animation(&game->opponent);
    } else {
        opp_anim = NULL;
        opp_frame = 0;
    }

    if (background_changed) {
        memcpy(top_backbuffer, clean_background_buffer, sizeof(top_backbuffer));
        
        draw_animation_frame(player_anim, player_frame, fp_to_int(game->player.x_fp) & ~3,
            game->player.y_baseline - (player_anim->frames + player_frame)->height, 2, body_color, &game->player.rect);
            
        if (game->opponent.active) {
            draw_animation_frame(opp_anim, opp_frame, fp_to_int(game->opponent.x_fp) & ~3,
                game->opponent.y_baseline - (opp_anim->frames + opp_frame)->height, (u8)game->opponent.eye_color, body_color, &game->opponent.rect);
        } else {
            invalidate_rect(&game->opponent.rect);
        }
        blit_top_half(game);
    } else {
        dirty_rect = union_rects(union_rects(game->previous_player_rect, game->previous_opponent_rect), union_rects(current_player_rect, current_opponent_rect));
        restore_rect_from_background(&dirty_rect);

        draw_animation_frame(player_anim, player_frame, fp_to_int(game->player.x_fp) & ~3,
            game->player.y_baseline - (player_anim->frames + player_frame)->height, 2, body_color, &game->player.rect);
            
        if (game->opponent.active) {
            draw_animation_frame(opp_anim, opp_frame, fp_to_int(game->opponent.x_fp) & ~3,
                game->opponent.y_baseline - (opp_anim->frames + opp_frame)->height, (u8)game->opponent.eye_color, body_color, &game->opponent.rect);
        } else {
            invalidate_rect(&game->opponent.rect);
        }
        blit_rect_to_vram(&dirty_rect, game);
    }

    game->previous_player_rect = game->player.rect;
    game->previous_opponent_rect = game->opponent.rect;
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

    compose_background_rows(game, step_start_row, step_row_count, 1);

    /* Set body color: 0 (Black) when silhouetted, 3 (White/Grey) when normal */
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
