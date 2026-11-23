#pragma once

/**
 * @file lp_wordmark.h
 * @brief Wordmark Animation and RGB Color Functions
 * 
 * High-performance math and color conversions for LP-Core
 */

#include <stdint.h>
#include <stdbool.h>
#include "lp_boot_art.h"

/* ========================================================================
 * Helper Functions - Easing and Math
 * ======================================================================== */

/**
 * @brief Ease-out cubic interpolation (0 to 1)
 */
static inline float ease_out_cubic(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    float inv = 1.0f - t;
    return 1.0f - (inv * inv * inv);
}

/**
 * @brief Ease-out back interpolation (for bounce effect)
 */
static inline float ease_out_back(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    float t1 = t - 1.0f;
    return 1.0f + 2.1f * t1 * t1 * t1 + 1.1f * t1 * t1;
}

/**
 * @brief Calculate spread distance for wordmark slide-in animation
 * Characters E,S,P start left, I,R,A start right, converge at 0.66 progress
 */
static inline int32_t get_wordmark_spread(uint32_t progress_frame)
{
    // Convert frame (0-12) to progress (0.0-1.0)
    float progress = (float)progress_frame / 12.0f;
    
    if (progress < WORDMARK_CENTER_DOT_PROGRESS) {
        // Before convergence: calculate spread
        float t = (progress - WORDMARK_CENTER_DOT_PROGRESS) / 0.12f;
        return (int32_t)(WORDMARK_SPREAD_MAX * (1.0f - ease_out_back(t)));
    }
    
    return 0;  // Fully converged
}

/**
 * @brief Get slide-in progress for a character (0.0 to 1.0)
 */
static inline float get_char_slide_progress(uint32_t progress_frame, uint32_t char_index)
{
    float progress = (float)progress_frame / 12.0f;
    float char_start = WORDMARK_START_PROGRESS + (WORDMARK_CHAR_DELAY * (float)char_index);
    
    if (progress < char_start) {
        return 0.0f;  // Not started yet
    }
    
    float t = (progress - char_start) / WORDMARK_SLIDE_DURATION;
    return ease_out_cubic(t);
}

/**
 * @brief Get X position for a character
 */
static inline int32_t get_char_x_position(uint32_t progress_frame, uint32_t char_index)
{
    // Base positions: centered around 120, 24px spacing
    // Character 0 (E): 60.5 + 24*0 = 60.5
    // Character 1 (S): 60.5 + 24*1 = 84.5
    // Character 2 (P): 60.5 + 24*2 = 108.5
    // Character 3 (I): 60.5 + 24*3 = 132.5
    // Character 4 (R): 60.5 + 24*4 = 156.5
    // Character 5 (A): 60.5 + 24*5 = 180.5
    
    int32_t base_x = 60 + ((int32_t)char_index * WORDMARK_CHAR_SPACING) + WORDMARK_CHAR_SPACING / 2;
    int32_t spread = get_wordmark_spread(progress_frame);
    
    // E,S,P (indices 0,1,2) slide from left
    // I,R,A (indices 3,4,5) slide from right
    int32_t spread_offset = (char_index < 3) ? -spread : spread;
    
    float slide = get_char_slide_progress(progress_frame, char_index);
    
    // Interpolate from (base + spread_offset) to base
    return base_x + (int32_t)((float)spread_offset * (1.0f - slide));
}

/**
 * @brief Check if a pixel belongs to a character
 */
static inline bool is_char_pixel(uint32_t char_index, int32_t char_x, int32_t char_y, 
                                  uint32_t pixel_x, uint32_t pixel_y, uint32_t scale)
{
    // Character is 5x7 pixels, centered at char_x, char_y
    int32_t left = char_x - (WORDMARK_CHAR_WIDTH * (int32_t)scale) / 2;
    int32_t top = char_y - (WORDMARK_CHAR_HEIGHT * (int32_t)scale) / 2;
    
    int32_t rel_x = (int32_t)pixel_x - left;
    int32_t rel_y = (int32_t)pixel_y - top;
    
    // Out of bounds
    if (rel_x < 0 || rel_y < 0 || 
        rel_x >= (int32_t)(WORDMARK_CHAR_WIDTH * scale) || 
        rel_y >= (int32_t)(WORDMARK_CHAR_HEIGHT * scale)) {
        return false;
    }
    
    // Map to font grid
    uint32_t font_x = (uint32_t)rel_x / scale;
    uint32_t font_y = (uint32_t)rel_y / scale;
    
    // Check bit in font data
    uint8_t row = FONT_5X7[char_index][font_y];
    return (row >> (4 - font_x)) & 1;
}

/**
 * @brief Check if pixel is part of center dot
 */
static inline bool is_center_dot_pixel(uint32_t progress_frame, 
                                        uint32_t pixel_x, uint32_t pixel_y, 
                                        uint32_t scale)
{
    float progress = (float)progress_frame / 12.0f;
    
    if (progress < WORDMARK_CENTER_DOT_PROGRESS) {
        return false;  // Dot not visible yet
    }
    
    // Dot is 3x3 pixels at (120, 128) - between P and I
    int32_t dot_left = 119 - (int32_t)scale / 2;
    int32_t dot_top = 128 - (int32_t)scale / 2;
    int32_t dot_size = 3 * (int32_t)scale;
    
    return ((int32_t)pixel_x >= dot_left && (int32_t)pixel_x < dot_left + dot_size &&
            (int32_t)pixel_y >= dot_top && (int32_t)pixel_y < dot_top + dot_size);
}

/**
 * @brief Check if pixel belongs to any wordmark element
 */
static inline bool is_wordmark_pixel(uint32_t progress_frame, 
                                      uint32_t pixel_x, uint32_t pixel_y,
                                      uint32_t scale)
{
    // Check center dot first (smaller, cheaper)
    if (is_center_dot_pixel(progress_frame, pixel_x, pixel_y, scale)) {
        return true;
    }
    
    // Check each character
    for (uint32_t i = 0; i < WORDMARK_CHAR_COUNT; ++i) {
        float slide = get_char_slide_progress(progress_frame, i);
        if (slide <= 0.0f) continue;  // Not visible yet
        
        int32_t char_x = get_char_x_position(progress_frame, i);
        int32_t char_y = 120 + 8;  // Slightly below center
        
        if (is_char_pixel(i, char_x, char_y, pixel_x, pixel_y, scale)) {
            return true;
        }
    }
    
    return false;
}

/* ========================================================================
 * HSL to RGB565 Conversion for Rainbow Effect
 * ======================================================================== */

/**
 * @brief Convert HSL to RGB565
 * @param hue 0-359 degrees
 * @param saturation 0-100 percent
 * @param lightness 0-100 percent
 * @return RGB565 color
 */
static inline uint16_t hsl_to_rgb565(uint32_t hue, uint32_t saturation, uint32_t lightness)
{
    // Normalize inputs
    hue = hue % 360;
    if (saturation > 100) saturation = 100;
    if (lightness > 100) lightness = 100;
    
    float s = (float)saturation / 100.0f;
    float l = (float)lightness / 100.0f;
    
    // Calculate chroma: C = (1 - |2L - 1|) * S
    float c = (1.0f - (l < 0.5f ? l : 1.0f - l) * 2.0f) * s;
    
    // Calculate X: second largest component
    uint32_t h_sector = hue / 60;
    float h_mod = ((float)(hue % 120)) / 60.0f;
    float x = c * (1.0f - (h_mod < 1.0f ? h_mod : 2.0f - h_mod));
    
    // Match value: m = L - C/2
    float m = l - c / 2.0f;
    
    // Determine RGB based on hue sector
    float r, g, b;
    
    switch (h_sector) {
        case 0: r = c; g = x; b = 0; break;
        case 1: r = x; g = c; b = 0; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        default: r = c; g = 0; b = x; break;
    }
    
    // Add match value and convert to 8-bit
    uint8_t r8 = (uint8_t)((r + m) * 255.0f);
    uint8_t g8 = (uint8_t)((g + m) * 255.0f);
    uint8_t b8 = (uint8_t)((b + m) * 255.0f);
    
    // Convert to RGB565
    return ((r8 & 0xF8) << 8) | ((g8 & 0xFC) << 3) | (b8 >> 3);
}

/**
 * @brief Get OK ring segment color with rainbow effect
 * @param segment Segment index (0-47)
 * @param frame_number Animation frame for rotation
 * @return RGB565 color
 */
static inline uint16_t get_ok_ring_color(uint32_t segment, uint32_t frame_number)
{
    // Hue per segment: 360 / 48 = 7.5°, but we use 12° steps for SPI efficiency
    // Rotation: +12° per frame
    uint32_t base_hue = (segment * 360) / 48;
    uint32_t rotation = (frame_number * 12) % 360;
    uint32_t hue = (base_hue + rotation) % 360;
    
    // Ring: saturation 100%, lightness 58%
    return hsl_to_rgb565(hue, 100, 58);
}
