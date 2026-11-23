#pragma once

/**
 * @file lp_boot_art.h
 * @brief ESP-IDF Boot Splash - Graphics Data and Constants
 * 
 * Optimized for LP-Core with limited SRAM
 */

#include <stdint.h>

/* ========================================================================
 * Wordmark - 5x7 Pixel Font (E S P I D F)
 * ======================================================================== */

/* 5x7 bitmap font, each row is 5 bits packed into uint8_t */
static const uint8_t FONT_5X7[6][7] = {
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},  // E
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},  // S
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},  // P
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},  // I
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},  // D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},  // F
};

/* Wordmark animation constants (from HTML reference) */
#define WORDMARK_CHAR_COUNT 6
#define WORDMARK_CHAR_WIDTH 5
#define WORDMARK_CHAR_HEIGHT 7
#define WORDMARK_CHAR_SPACING 24    // Pixels between character centers
#define WORDMARK_CENTER_X 120       // Display center X
#define WORDMARK_CENTER_Y 120       // Display center Y (adjusted for mascot)
#define WORDMARK_SPREAD_MAX 20      // Max spread distance during slide-in

/* Animation timing (progress 0.0-1.0) */
#define WORDMARK_START_PROGRESS 0.28f   // First character starts sliding
#define WORDMARK_CHAR_DELAY 0.06f       // Delay between characters
#define WORDMARK_SLIDE_DURATION 0.08f   // Slide-in duration per character
#define WORDMARK_CENTER_DOT_PROGRESS 0.66f  // Center dot appears
#define WORDMARK_DONE_PROGRESS 0.78f    // All characters positioned

/* ========================================================================
 * Mascot Pixel Art - 16x42 pixels at scale 1
 * ======================================================================== */

static const uint16_t boot_splash_body_rows[] = {
    0x07E0, 0x1FF8, 0x3FFC, 0x7FFE, 0x7FFE,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
};

static const uint16_t boot_splash_feet_rows[] = {
    0xF3CF, 0xE79F,
};
