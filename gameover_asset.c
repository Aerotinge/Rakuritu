#include "rakuzitu.h"
#include "assets/GAMEOVER_JINXED.h"
#include "assets/GAMEOVER_KIA.h"
#include "assets/GAMEOVER_LOST.h"
#include "assets/GAMEOVER_MIDNIGHT_SUN.h"
#include "assets/GAMEOVER_PROLONGED.h"

/* These are mutable pointers that point to const far data */
const PackedSpriteFrame far *g_gameover_frame = (const PackedSpriteFrame far *)GAMEOVER_KIA_frames;
const u8 far *g_gameover_pixels_even = GAMEOVER_KIA_pixels_even;
const u8 far *g_gameover_pixels_odd = GAMEOVER_KIA_pixels_odd;

void set_gameover_asset(GameOverReason reason)
{
    switch (reason) {
    case GAMEOVER_LOST:
        g_gameover_frame = (const PackedSpriteFrame far *)GAMEOVER_LOST_frames;
        g_gameover_pixels_even = GAMEOVER_LOST_pixels_even;
        g_gameover_pixels_odd = GAMEOVER_LOST_pixels_odd;
        break;
    case GAMEOVER_PROLONGED:
        g_gameover_frame = (const PackedSpriteFrame far *)GAMEOVER_PROLONGED_frames;
        g_gameover_pixels_even = GAMEOVER_PROLONGED_pixels_even;
        g_gameover_pixels_odd = GAMEOVER_PROLONGED_pixels_odd;
        break;
    case GAMEOVER_JINXED:
        g_gameover_frame = (const PackedSpriteFrame far *)GAMEOVER_JINXED_frames;
        g_gameover_pixels_even = GAMEOVER_JINXED_pixels_even;
        g_gameover_pixels_odd = GAMEOVER_JINXED_pixels_odd;
        break;
    case GAMEOVER_MIDNIGHT_SUN:
        g_gameover_frame = (const PackedSpriteFrame far *)GAMEOVER_MIDNIGHT_SUN_frames;
        g_gameover_pixels_even = GAMEOVER_MIDNIGHT_SUN_pixels_even;
        g_gameover_pixels_odd = GAMEOVER_MIDNIGHT_SUN_pixels_odd;
        break;
    case GAMEOVER_KIA:
    default:
        g_gameover_frame = (const PackedSpriteFrame far *)GAMEOVER_KIA_frames;
        g_gameover_pixels_even = GAMEOVER_KIA_pixels_even;
        g_gameover_pixels_odd = GAMEOVER_KIA_pixels_odd;
        break;
    }
}
