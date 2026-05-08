#include <string.h>

#include "rakuzitu.h"
#include "assets/issen_bg_strip.h"

static u8 __near top_backbuffer[TOP_VIEW_HEIGHT][BYTES_PER_SCANLINE];
static u8 __near clean_background_buffer[TOP_VIEW_HEIGHT][BYTES_PER_SCANLINE];
static u16 g_background_step_row = 0;
static const u16 BACKGROUND_STEP_ROWS = 25;

static u8 g_recolor_lut[3][256];
static u8 g_luts_initialized = 0;

static void init_recolor_luts(void)
{
    int i;
    int color;
    u8 packed, out, shift, c;

    if (g_luts_initialized) return;
    
    for (color = 0; color < 3; ++color) {
        for (i = 0; i < 256; ++i) {
            packed = (u8)i;
            out = 0;
            for (shift = 0; shift < 4; ++shift) {
                c = (u8)((packed >> ((3 - shift) * 2)) & 0x03);
                if (c == 2) {
                    c = (u8)color;
                }
                out |= (u8)(c << ((3 - shift) * 2));
            }
            g_recolor_lut[color][i] = out;
        }
    }
    g_luts_initialized = 1;
}

static void copy_words_near(void __near *dst, const void __near *src, u16 word_count);
#pragma aux copy_words_near = \
    "cld" \
    "rep movsw" \
    parm [di] [si] [cx] \
    modify [di si cx];

static void copy_bytes_near(void __near *dst, const void __near *src, u16 byte_count);
#pragma aux copy_bytes_near = \
    "cld" \
    "rep movsb" \
    parm [di] [si] [cx] \
    modify [di si cx];

static void copy_near_block(void __near *dst, const void __near *src, u16 byte_count)
{
    u16 word_count;

    word_count = byte_count >> 1;
    if (word_count != 0) {
        copy_words_near(dst, src, word_count);
    }
    if (byte_count & 1) {
        copy_bytes_near((u8 __near *)dst + (word_count << 1), (const u8 __near *)src + (word_count << 1), 1);
    }
}

static void far *near_ptr_to_far(void *ptr)
{
    return MK_FP(FP_SEG(&g_game), FP_OFF(ptr));
}

static const void far *near_const_ptr_to_far(const void *ptr)
{
    return (const void far *)MK_FP(FP_SEG(&g_game), FP_OFF(ptr));
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
    u8 tile;
    // Cache the 4 remote bytes into CPU registers immediately
    u8 p0 = row_pattern[0];
    u8 p1 = row_pattern[1];
    u8 p2 = row_pattern[2];
    u8 p3 = row_pattern[3];

    for (tile = 0; tile < 20; ++tile) {
        dst[0] = p0;
        dst[1] = p1;
        dst[2] = p2;
        dst[3] = p3;
        dst += ISSEN_BG_STRIP_BYTES_PER_ROW;
    }
}

static void compose_background(u16 scroll_pixels)
{
    int screen_y;
    int source_y;
    u8 __near *dst_ptr = (u8 __near *)top_backbuffer;

    for (screen_y = 0; screen_y < TOP_VIEW_HEIGHT; ++screen_y) {
        source_y = (ISSEN_BG_STRIP_HEIGHT - TOP_VIEW_HEIGHT) + screen_y - scroll_pixels;

        if (source_y < 0 || source_y >= ISSEN_BG_STRIP_HEIGHT) {
            fill_scanline(dst_ptr, BLACK_BYTE);
        } else {
            tile_strip_row(dst_ptr, issen_bg_strip_pixels[source_y]);
        }
        dst_ptr += BYTES_PER_SCANLINE;
    }
}

static void compose_background_rows(u16 scroll_pixels, u16 start_row, u16 row_count, u8 write_top_buffer)
{
    u16 end_row;
    u16 screen_y;
    int source_y;
    u8 __near *clean_ptr;
    u8 __near *top_ptr;

    end_row = (u16)(start_row + row_count);
    if (end_row > TOP_VIEW_HEIGHT) {
        end_row = TOP_VIEW_HEIGHT;
    }

    clean_ptr = clean_background_buffer[start_row];
    top_ptr = top_backbuffer[start_row];

    for (screen_y = start_row; screen_y < end_row; ++screen_y) {
        source_y = (ISSEN_BG_STRIP_HEIGHT - TOP_VIEW_HEIGHT) + screen_y - scroll_pixels;

        if (source_y < 0 || source_y >= ISSEN_BG_STRIP_HEIGHT) {
            fill_scanline(clean_ptr, BLACK_BYTE);
            if (write_top_buffer) {
                fill_scanline(top_ptr, BLACK_BYTE);
            }
        } else {
            tile_strip_row(clean_ptr, issen_bg_strip_pixels[source_y]);
            if (write_top_buffer) {
                tile_strip_row(top_ptr, issen_bg_strip_pixels[source_y]);
            }
        }
        clean_ptr += BYTES_PER_SCANLINE;
        top_ptr += BYTES_PER_SCANLINE;
    }
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
    u8 __near *dst;
    const u8 __near *src;

    if (rect == NULL || rect->valid == 0) {
        return;
    }

    start_byte = rect->x >> 2;
    end_byte = (rect->x + rect->width + 3) >> 2;
    byte_count = end_byte - start_byte;

    dst = &top_backbuffer[rect->y][start_byte];
    src = &clean_background_buffer[rect->y][start_byte];

    for (row = rect->y; row < rect->y + rect->height; ++row) {
        copy_near_block(dst, src, (u16)byte_count);
        dst += BYTES_PER_SCANLINE;
        src += BYTES_PER_SCANLINE;
    }
}

static void refresh_clean_background(GameContext *game)
{
    if (game->rendered_background_scroll_pixels == game->background_scroll_pixels) {
        return;
    }

    compose_background(game->background_scroll_pixels);
    copy_near_block(clean_background_buffer, top_backbuffer, (u16)sizeof(top_backbuffer));
    game->rendered_background_scroll_pixels = game->background_scroll_pixels;
    g_background_step_row = TOP_VIEW_HEIGHT;
}

static void draw_animation_frame(const AnimationAsset *animation, u16 frame_index, int draw_x, int draw_y, u8 recolor_red, u8 red_replacement, Rect *out_rect)
{
    const PackedSpriteFrame far *frame_meta;
    const u8 far *frame_pixels_even;
    const u8 far *frame_pixels_odd;
    const u8 far *frame_mask_even;
    const u8 far *frame_mask_odd;
    const u8 far *mask_ptr;
    const u8 far *pix_ptr;
    u8 __near *dst_ptr;
    int row;
    int col;
    int dst_x_byte;
    int dst_y;
    int src_start_byte;
    int visible_byte_count;
    int clip_right_byte;
    u8 mask_byte;
    u8 pixel_byte;
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

    lut = recolor_red ? g_recolor_lut[red_replacement] : NULL;

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

        if (lut) {
            for (col = 0; col < visible_byte_count; ++col) {
                mask_byte = *mask_ptr++;
                if (mask_byte == 0xFF) {  // Highly probable transparent byte
                    pix_ptr++;
                    dst_ptr++;
                    continue;
                }
                pixel_byte = lut[*pix_ptr++];
                if (mask_byte == 0x00) {  // Complete replacement block
                    *dst_ptr++ = pixel_byte;
                } else {
                    *dst_ptr = (*dst_ptr & mask_byte) | pixel_byte;
                    dst_ptr++;
                }
            }
        } else {
            for (col = 0; col < visible_byte_count; ++col) {
                mask_byte = *mask_ptr++;
                if (mask_byte == 0xFF) {
                    pix_ptr++;
                    dst_ptr++;
                    continue;
                }
                pixel_byte = *pix_ptr++;
                if (mask_byte == 0x00) {
                    *dst_ptr++ = pixel_byte;
                } else {
                    *dst_ptr = (*dst_ptr & mask_byte) | pixel_byte;
                    dst_ptr++;
                }
            }
        }
    }
}

static void build_player_sprite(const GameContext *game, Rect *rect_out, u16 *frame_index_out)
{
    const AnimationAsset *animation;
    const PackedSpriteFrame far *frame_meta;
    u16 frame_index;
    int draw_x;
    int draw_y;

    animation = get_player_animation(game->player.anim_mode);
    frame_index = get_player_frame_index(game, animation);
    frame_meta = animation->frames + frame_index;
    draw_x = fp_to_int(game->player.x_fp);
    if (draw_x & 3) {
        draw_x &= ~3;
    }
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
    const PackedSpriteFrame far *frame_meta;
    u16 frame_index;
    int draw_x;
    int draw_y;

    if (game->opponent.active == 0 || game->opponent.def == NULL) {
        invalidate_rect(rect_out);
        return;
    }

    animation = get_opponent_animation(&game->opponent);
    frame_index = get_opponent_frame_index(game, animation);
    frame_meta = animation->frames + frame_index;
    draw_x = fp_to_int(game->opponent.x_fp);
    if (draw_x & 3) {
        draw_x &= ~3;
    }
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
    void far *dst;
    void far *src;
    u8 __near *backbuffer_ptr;

    if (game->video_wait_vblank) {
        wait_vblank();
    }

    even_offset = 0;
    odd_offset = CGA_ODD_OFFSET;

    backbuffer_ptr = top_backbuffer[0];
    for (row = 0; row < TOP_VIEW_HEIGHT; row += 2) {
        dst = MK_FP(CGA_SEGMENT, even_offset);
        src = (void far *)near_const_ptr_to_far(backbuffer_ptr);
        _fmemcpy(dst, src, BYTES_PER_SCANLINE);
        even_offset += BYTES_PER_SCANLINE;
        backbuffer_ptr += (BYTES_PER_SCANLINE * 2);
    }

    backbuffer_ptr = top_backbuffer[1];
    for (row = 1; row < TOP_VIEW_HEIGHT; row += 2) {
        dst = MK_FP(CGA_SEGMENT, odd_offset);
        src = (void far *)near_const_ptr_to_far(backbuffer_ptr);
        _fmemcpy(dst, src, BYTES_PER_SCANLINE);
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
    void far *dst_ptr;
    void far *src_ptr;
    const u8 __near *backbuffer_ptr;

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
    offset_odd  = (u16)((rect->y >> 1) * BYTES_PER_SCANLINE + start_byte) + CGA_ODD_OFFSET;
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

        dst_ptr = MK_FP(CGA_SEGMENT, current_offset);
        src_ptr = (void far *)near_const_ptr_to_far(backbuffer_ptr);
        _fmemcpy(dst_ptr, src_ptr, byte_count);

        backbuffer_ptr += BYTES_PER_SCANLINE;
    }
}

void draw_ui_frame_once(void)
{
    u8 ui_scanline[BYTES_PER_SCANLINE];
    u16 screen_y;
    u16 offset;
    u16 x;
    u16 offset_even;
    u16 offset_odd;
    void far *dst;
    void far *src;

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

        dst = MK_FP(CGA_SEGMENT, offset);
        src = (void far *)near_const_ptr_to_far(ui_scanline);
        _fmemcpy(dst, src, BYTES_PER_SCANLINE);
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

    _fmemcpy(MK_FP(CGA_SEGMENT, 0), (void far *)pixels_even, block_size);
    _fmemcpy(MK_FP(CGA_SEGMENT, CGA_ODD_OFFSET), (void far *)pixels_odd, block_size);
}

static void render_active_scene(GameContext *game)
{
    Rect current_player_rect;
    Rect current_opponent_rect;
    Rect dirty_rect;
    Rect current_union;
    u8 background_changed;
    u16 player_frame;
    u16 opp_frame;
    const AnimationAsset *player_anim;
    const AnimationAsset *opp_anim;

    background_changed = (game->rendered_background_scroll_pixels == 0xFFFF);
    if (background_changed) {
        refresh_clean_background(game);
    }

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
        copy_near_block(top_backbuffer, clean_background_buffer, (u16)sizeof(top_backbuffer));
        draw_animation_frame(player_anim, player_frame, fp_to_int(game->player.x_fp) & ~3,
            game->player.y_baseline - (player_anim->frames + player_frame)->height, 0, 0, &game->player.rect);
        if (game->opponent.active) {
            draw_animation_frame(opp_anim, opp_frame, fp_to_int(game->opponent.x_fp) & ~3,
                game->opponent.y_baseline - (opp_anim->frames + opp_frame)->height, 1, (u8)game->opponent.eye_color, &game->opponent.rect);
        } else {
            invalidate_rect(&game->opponent.rect);
        }
        blit_top_half(game);
    } else {
        current_union = union_rects(current_player_rect, current_opponent_rect);
        dirty_rect = union_rects(union_rects(game->previous_player_rect, game->previous_opponent_rect), current_union);
        restore_rect_from_background(&dirty_rect);

        draw_animation_frame(player_anim, player_frame, fp_to_int(game->player.x_fp) & ~3,
            game->player.y_baseline - (player_anim->frames + player_frame)->height, 0, 0, &game->player.rect);
        if (game->opponent.active) {
            draw_animation_frame(opp_anim, opp_frame, fp_to_int(game->opponent.x_fp) & ~3,
                game->opponent.y_baseline - (opp_anim->frames + opp_frame)->height, 1, (u8)game->opponent.eye_color, &game->opponent.rect);
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
    u16 player_frame;
    u16 opp_frame;
    const AnimationAsset *player_anim;
    const AnimationAsset *opp_anim;
    u16 step_start_row;
    u16 step_row_count;
    Rect step_rect;
    int player_draw_x;
    int player_draw_y;
    int opponent_draw_x;
    int opponent_draw_y;

    if (game->state == GAME_STATE_GAMEOVER || game->state == GAME_STATE_PLAYER_DYING) {
        return;
    }

    if (game->rendered_background_scroll_pixels == game->background_scroll_pixels &&
        g_background_step_row == 0) {
        return;
    }

    if (g_background_step_row >= TOP_VIEW_HEIGHT) {
        g_background_step_row = 0;
    }

    step_start_row = g_background_step_row;
    step_row_count = BACKGROUND_STEP_ROWS;
    if (step_start_row + step_row_count > TOP_VIEW_HEIGHT) {
        step_row_count = (u16)(TOP_VIEW_HEIGHT - step_start_row);
    }

    compose_background_rows(game->background_scroll_pixels, step_start_row, step_row_count, 1);

    player_anim = get_player_animation(game->player.anim_mode);
    build_player_sprite(game, &player_rect, &player_frame);
    player_draw_x = fp_to_int(game->player.x_fp) & ~3;
    player_draw_y = game->player.y_baseline - (player_anim->frames + player_frame)->height;
    if (player_rect.valid &&
        player_rect.y < (int)(step_start_row + step_row_count) &&
        (player_rect.y + player_rect.height) > (int)step_start_row) {
        draw_animation_frame(player_anim, player_frame, player_draw_x, player_draw_y, 0, 0, &game->player.rect);
    }

    if (game->opponent.active) {
        opp_anim = get_opponent_animation(&game->opponent);
        build_opponent_sprite(game, &opponent_rect, &opp_frame);
        opponent_draw_x = fp_to_int(game->opponent.x_fp) & ~3;
        opponent_draw_y = game->opponent.y_baseline - (opp_anim->frames + opp_frame)->height;
        if (opponent_rect.valid &&
            opponent_rect.y < (int)(step_start_row + step_row_count) &&
            (opponent_rect.y + opponent_rect.height) > (int)step_start_row) {
            draw_animation_frame(opp_anim, opp_frame, opponent_draw_x, opponent_draw_y, 1, (u8)game->opponent.eye_color, &game->opponent.rect);
        }
    }

    step_rect.x = 0;
    step_rect.y = step_start_row;
    step_rect.width = SCREEN_WIDTH;
    step_rect.height = step_row_count;
    step_rect.valid = 1;
    blit_rect_to_vram(&step_rect, game);

    g_background_step_row = (u16)(g_background_step_row + step_row_count);
    if (g_background_step_row >= TOP_VIEW_HEIGHT) {
        game->rendered_background_scroll_pixels = game->background_scroll_pixels;
        g_background_step_row = 0;
    }
}
