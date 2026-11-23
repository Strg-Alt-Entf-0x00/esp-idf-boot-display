/**
 * @file lp_main.c
 * @brief LP-Core GC9A01 Boot Animation firmware for ESP32-P4
 *
 * Runs autonomously on LP-Core while HP-Core is busy with heavy boot initialization.
 * Drives GC9A01 240x240 round LCD via configurable LP-SPI on LP GPIO 2-5.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ulp_lp_core_gpio.h"
#include "ulp_lp_core_lp_timer_shared.h"
#include "ulp_lp_core_spi.h"
#include "ulp_lp_core_utils.h"
#include "lp_boot_art.h"
#include "lp_wordmark.h"
#include "lp_gc9a01_init.h"

#define LCD_CS_GPIO   LP_IO_NUM_4
#define LCD_DC_GPIO   LP_IO_NUM_5
#define LP_FRAME_DELAY_US 40000  // 25 FPS (40ms per frame) - CPU-efficient
#define LP_EVENT_PROGRESS_STEP_US 25000U
#define LP_ERROR_DELAY_US 1000000
#define LCD_WIDTH 240U
#define LCD_HEIGHT 240U
#define PIXELS_PER_TRANSFER 30U
#define TRANSFER_BYTES (PIXELS_PER_TRANSFER * 2U)
#if defined(CONFIG_BOOT_DISPLAY_RING_SEGMENTS)
#define RING_SEGMENTS ((uint32_t)CONFIG_BOOT_DISPLAY_RING_SEGMENTS)
#else
#define RING_SEGMENTS 48U
#endif
#define RING_RADIUS 112
#define RING_HALF_WIDTH 1
#define RING_DASH_CROSS_LIMIT 500
#define RING_TILE_SIZE 14U
#define MASCOT_WIDTH 43
#define MASCOT_HEIGHT 57

#define SPLASH_MOOD_SLEEPY  0U
#define SPLASH_MOOD_LOADING 1U
#define SPLASH_MOOD_WORRIED 2U
#define SPLASH_MOOD_HAPPY   3U

#define LCD_CMD_SWRESET 0x01
#define LCD_CMD_SLPOUT  0x11
#define LCD_CMD_DISPOFF 0x28
#define LCD_CMD_DISPON  0x29
#define LCD_CMD_CASET   0x2A
#define LCD_CMD_RASET   0x2B
#define LCD_CMD_RAMWR   0x2C

#define LP_BOOT_MODE_LOOP        0U
#define LP_BOOT_MODE_STATIC      1U
#define LP_BOOT_MODE_SINGLE_RUN  2U
#define LP_BOOT_MODE_EVENT_DRIVEN 3U

#define LP_BOOT_STATUS_IDLE      0U
#define LP_BOOT_STATUS_RUNNING   1U
#define LP_BOOT_STATUS_ERROR     2U
#define LP_BOOT_STATUS_STATIC    3U
#define LP_BOOT_STATUS_DONE      4U

static uint32_t tx_words[15];
static uint32_t pixel_words[PIXELS_PER_TRANSFER / 2];
static int16_t ring_x[RING_SEGMENTS];
static int16_t ring_y[RING_SEGMENTS];
typedef struct {
    uint8_t left;
    uint8_t top;
    uint8_t right;
    uint8_t bottom;
    bool valid;
} ring_bounds_t;
static ring_bounds_t ring_bounds[RING_SEGMENTS];

typedef struct {
    uint8_t source_column[MASCOT_WIDTH];
    uint8_t source_row[MASCOT_HEIGHT];
    int8_t eye_local_x[2][MASCOT_WIDTH];
    int8_t eye_local_y[MASCOT_HEIGHT];
    int8_t mouth_dx[MASCOT_WIDTH];
    int8_t mouth_dy[MASCOT_HEIGHT];
    int16_t eye_left[2];
    int16_t eye_top;
    uint8_t eye_width;
    uint8_t eye_height;
    int8_t gaze_x;
    int8_t gaze_y;
} mascot_render_cache_t;
static mascot_render_cache_t mascot_render_cache;

/* Shared variables exported to HP-Core via generated ulp_main.h */
volatile uint32_t lp_display_progress;
volatile uint32_t lp_display_progress_percent;
volatile uint32_t lp_display_target_progress_percent;
volatile uint32_t lp_display_cycles;
volatile uint32_t lp_display_status;
volatile uint32_t lp_display_error_code;
volatile uint32_t lp_display_mode;
volatile uint32_t lp_display_idle_ticks;
volatile uint32_t lp_display_display_init_time_us;
volatile uint32_t lp_display_frame_time_us;
static uint64_t lp_timer_ticks_per_second;

static uint32_t timer_ticks_to_us(uint64_t ticks)
{
    return (uint32_t)(ticks * 1000000ULL / lp_timer_ticks_per_second);
}

static void transfer(const void *buffer, uint32_t length)
{
    lp_spi_transaction_t transaction = {
        .tx_length = length,
        .tx_buffer = buffer,
    };
    if (lp_core_lp_spi_master_transfer(&transaction, -1) != ESP_OK) {
        lp_display_error_code = 1;
        lp_display_status = LP_BOOT_STATUS_ERROR;
        ulp_lp_core_wakeup_main_processor();
        while (1) {
            ulp_lp_core_delay_us(LP_ERROR_DELAY_US);
        }
    }
}

static void chip_select(bool active)
{
    ulp_lp_core_gpio_set_level(LCD_CS_GPIO, active ? 0 : 1);
}

static void write_command(uint8_t command)
{
    uint32_t word = command;
    chip_select(true);
    ulp_lp_core_gpio_set_level(LCD_DC_GPIO, 0);
    transfer(&word, 1);
    ulp_lp_core_delay_us(1);  // Critical: 1us delay after command
    chip_select(false);
}

static void write_data_bytes(const uint8_t *data, uint32_t length)
{
    while (length > 0) {
        uint32_t chunk_size = length > sizeof(tx_words) ? sizeof(tx_words) : length;
        memset(tx_words, 0, sizeof(tx_words));
        memcpy(tx_words, data, chunk_size);
        ulp_lp_core_gpio_set_level(LCD_DC_GPIO, 1);
        transfer(tx_words, chunk_size);
        data += chunk_size;
        length -= chunk_size;
    }
}

static void write_command_data(uint8_t command, const uint8_t *data, uint32_t length)
{
    uint32_t word = command;
    chip_select(true);
    ulp_lp_core_gpio_set_level(LCD_DC_GPIO, 0);
    transfer(&word, 1);
    ulp_lp_core_delay_us(1);  // Critical: 1us delay after command
    if (length > 0) {
        ulp_lp_core_gpio_set_level(LCD_DC_GPIO, 1);
        write_data_bytes(data, length);
    }
    chip_select(false);
}

static void begin_pixel_stream(void)
{
    uint32_t command = LCD_CMD_RAMWR;
    chip_select(true);
    ulp_lp_core_gpio_set_level(LCD_DC_GPIO, 0);
    transfer(&command, 1);
    ulp_lp_core_gpio_set_level(LCD_DC_GPIO, 1);
}

static void begin_pixel_window(uint16_t x_start, uint16_t y_start,
                               uint16_t x_end, uint16_t y_end)
{
    uint8_t columns[4] = {
        (uint8_t)(x_start >> 8), (uint8_t)x_start,
        (uint8_t)(x_end >> 8), (uint8_t)x_end,
    };
    uint8_t rows[4] = {
        (uint8_t)(y_start >> 8), (uint8_t)y_start,
        (uint8_t)(y_end >> 8), (uint8_t)y_end,
    };

    write_command_data(LCD_CMD_CASET, columns, sizeof(columns));
    write_command_data(LCD_CMD_RASET, rows, sizeof(rows));
    begin_pixel_stream();
}

static void clear_display(void)
{
    begin_pixel_window(0, 0, LCD_WIDTH - 1U, LCD_HEIGHT - 1U);

    memset(pixel_words, 0, sizeof(pixel_words));
    for (uint32_t remaining = LCD_WIDTH * LCD_HEIGHT;
         remaining > 0;
         remaining -= PIXELS_PER_TRANSFER) {
        transfer(pixel_words, TRANSFER_BYTES);
    }

    chip_select(false);
}

static void initialize_display(void)
{
    write_command(LCD_CMD_SWRESET);
    ulp_lp_core_delay_us(150000);
    write_command(LCD_CMD_DISPOFF);

    size_t offset = 0;
    while (offset < sizeof(lcd_init_sequence)) {
        uint8_t command = lcd_init_sequence[offset++];
        uint8_t data_length = lcd_init_sequence[offset++];
        write_command_data(command, &lcd_init_sequence[offset], data_length);
        offset += data_length;
    }

    write_command(LCD_CMD_SLPOUT);
    ulp_lp_core_delay_us(120000);
    clear_display();
    write_command(LCD_CMD_DISPON);
    ulp_lp_core_delay_us(20000);
}

static void initialize_ring_points(void)
{
    int32_t x = 7 * 256;
    int32_t y = -RING_RADIUS * 256;
    const int32_t cosine = 32487;
    const int32_t sine = 4277;

    for (uint32_t i = 0; i < RING_SEGMENTS; ++i) {
        ring_x[i] = (int16_t)x;
        ring_y[i] = (int16_t)y;

        int32_t next_x = (x * cosine - y * sine) / 32768;
        int32_t next_y = (x * sine + y * cosine) / 32768;
        x = next_x;
        y = next_y;
    }
}

static int32_t ring_coordinate_pixel(int16_t coordinate)
{
    return coordinate >= 0
               ? ((int32_t)coordinate + 128) / 256
               : -((-(int32_t)coordinate + 128) / 256);
}

static void prepare_mascot_render_cache(uint32_t width, uint32_t height,
                                         uint32_t frame_number,
                                         uint32_t active_segments,
                                         uint32_t blink_frame)
{
    mascot_render_cache_t *cache = &mascot_render_cache;
    int32_t mouth_left = (16 * (int32_t)width) / 32;
    int32_t mouth_top = (29 * (int32_t)height) / 42;

    cache->eye_width = (uint8_t)((6U * width + 16U) / 32U);
    if (cache->eye_width == 0U) {
        cache->eye_width = 1U;
    }
    cache->eye_height = (uint8_t)((8U * height + 21U) / 42U);
    if (cache->eye_height == 0U) {
        cache->eye_height = 1U;
    }
    cache->eye_top = (int16_t)((16 * (int32_t)height) / 42);
    for (uint32_t eye = 0; eye < 2U; ++eye) {
        cache->eye_left[eye] =
            (int16_t)(((eye == 0U ? 8 : 18) * (int32_t)width) / 32);
    }

    for (uint32_t x = 0; x < width; ++x) {
        uint32_t source_x = x * 32U / width;
        cache->source_column[x] = (uint8_t)(source_x / 2U);
        cache->mouth_dx[x] =
            (int8_t)(((int32_t)x - mouth_left) * 32 / (int32_t)width);
        for (uint32_t eye = 0; eye < 2U; ++eye) {
            cache->eye_local_x[eye][x] = (int8_t)(
                ((int32_t)x - cache->eye_left[eye]) * 32 / (int32_t)width);
        }
    }
    for (uint32_t y = 0; y < height; ++y) {
        uint32_t source_y = y * 42U / height;
        cache->source_row[y] = (uint8_t)source_y;
        cache->mouth_dy[y] =
            (int8_t)(((int32_t)y - mouth_top) * 42 / (int32_t)height);
        cache->eye_local_y[y] = (int8_t)(
            ((int32_t)y - cache->eye_top) * 42 / (int32_t)height);
    }

    cache->gaze_x = 0;
    cache->gaze_y = 0;
    if (active_segments > 0U) {
        int32_t target_x = ring_coordinate_pixel(ring_x[active_segments - 1U]);
        int32_t target_y = ring_coordinate_pixel(ring_y[active_segments - 1U]);
        cache->gaze_x = target_x > 12 ? 2 : (target_x < -12 ? -2 : 0);
        cache->gaze_y = target_y > 12 ? 1 : (target_y < -12 ? -1 : 0);
    }
    if (frame_number == blink_frame) {
        cache->gaze_x = 0;
        cache->gaze_y = 0;
    }
}

static uint16_t mascot_pixel_color(uint32_t x, uint32_t y, int32_t left,
                                   int32_t top, uint32_t width, uint32_t height,
                                   uint16_t foreground, uint16_t background,
                                   uint32_t frame_number, uint32_t blink_frame,
                                   uint32_t mood, bool *opaque)
{
    int32_t relative_x = (int32_t)x - left;
    int32_t relative_y = (int32_t)y - top;
    const mascot_render_cache_t *cache = &mascot_render_cache;
    *opaque = false;

    if (relative_x < 0 || (uint32_t)relative_x >= width ||
        relative_y < 0 || (uint32_t)relative_y >= height) {
        return background;
    }

    uint32_t column = cache->source_column[relative_x];
    uint32_t row = cache->source_row[relative_y];
    uint16_t pattern = row < 39U
                           ? boot_splash_body_rows[row / 2U]
                           : boot_splash_feet_rows[(frame_number / 2U) % 2U];
    *opaque = (pattern & (1U << (15U - column))) != 0U;
    uint16_t color = *opaque ? foreground : background;

    for (uint32_t eye = 0; eye < 2U; ++eye) {
        if ((uint32_t)relative_y >= (uint32_t)cache->eye_top &&
            (uint32_t)relative_y <
                (uint32_t)cache->eye_top + cache->eye_height &&
            relative_x >= cache->eye_left[eye] &&
            relative_x < cache->eye_left[eye] + cache->eye_width) {
            uint32_t local_x = (uint32_t)cache->eye_local_x[eye][relative_x];
            uint32_t local_y = (uint32_t)cache->eye_local_y[relative_y];
            *opaque = true;
            color = background;
            if (mood == SPLASH_MOOD_HAPPY) {
                if (local_y >= 3U && (local_x == 0U || local_x >= 5U)) {
                    color = foreground;
                }
            } else if (frame_number == blink_frame ||
                       mood == SPLASH_MOOD_SLEEPY) {
                if (local_y >= 2U && local_y <= 3U) {
                    color = foreground;
                }
            } else {
                int32_t pupil_x = 2 + cache->gaze_x;
                int32_t pupil_y = 2 + cache->gaze_y;
                if ((int32_t)local_x == pupil_x &&
                    (int32_t)local_y == pupil_y) {
                    color = foreground;
                }
            }
        }
    }

    int32_t mouth_dx = cache->mouth_dx[relative_x];
    int32_t mouth_dy = cache->mouth_dy[relative_y];
    bool mouth_pixel = false;
    if (mood == SPLASH_MOOD_WORRIED) {
        mouth_pixel = mouth_dx >= -1 && mouth_dx <= 1 &&
                      mouth_dy >= -1 && mouth_dy <= 1;
    } else if (mood == SPLASH_MOOD_HAPPY) {
        mouth_pixel = (mouth_dy == 1 && mouth_dx >= -3 && mouth_dx <= 3) ||
                      (mouth_dy == 0 && (mouth_dx == -4 || mouth_dx == 4));
    } else if (mood == SPLASH_MOOD_SLEEPY) {
        mouth_pixel = mouth_dy == 0 && mouth_dx >= -2 && mouth_dx <= 2;
    } else {
        mouth_pixel = (mouth_dy == -1 && mouth_dx >= -4 && mouth_dx <= -2) ||
                      (mouth_dy == 0 && (mouth_dx == -1 || mouth_dx == 0)) ||
                      (mouth_dy == 1 && mouth_dx >= 1 && mouth_dx <= 4);
    }
    if (mouth_pixel) {
        *opaque = true;
        color = background;
    }

    return color;
}

typedef struct {
    int16_t left;
    int16_t top;
    uint16_t color;
    bool visible;
} wordmark_glyph_t;

typedef struct {
    int16_t left;
    uint16_t color;
    bool visible;
} wordmark_dot_t;

static uint16_t scale_rgb565(uint16_t color, float amount)
{
    uint32_t red = (color >> 11) & 0x1FU;
    uint32_t green = (color >> 5) & 0x3FU;
    uint32_t blue = color & 0x1FU;

    red = (uint32_t)((float)red * amount);
    green = (uint32_t)((float)green * amount);
    blue = (uint32_t)((float)blue * amount);
    return (uint16_t)((red << 11) | (green << 5) | blue);
}

static void prepare_wordmark(uint32_t progress_percent,
                             wordmark_glyph_t *glyphs,
                             wordmark_dot_t *dots)
{
    static const uint8_t char_delays[WORDMARK_CHAR_COUNT] = {2, 1, 0, 0, 1, 2};
    float progress = (float)progress_percent / 100.0f;
    float spread = (float)WORDMARK_SPREAD_MAX *
                   (1.0f - ease_out_back((progress - 0.66f) / 0.12f));

    for (uint32_t i = 0; i < WORDMARK_CHAR_COUNT; ++i) {
        float slide = ease_out_cubic(
            (progress - (0.28f + 0.06f * (float)char_delays[i])) / 0.08f);
        float center_x = 120.0f +
            (60.5f + 24.0f * (float)i +
             (i < 3U ? -spread : spread) - 120.0f) * slide;
        float alpha = slide < 0.5f ? slide * 2.0f : 1.0f;

        glyphs[i].left = (int16_t)(center_x - 7.0f);
        glyphs[i].top = 110;
        glyphs[i].visible = slide > 0.0f;
        glyphs[i].color = 0xFFFFU;
        glyphs[i].color = scale_rgb565(glyphs[i].color, alpha);
    }

    for (uint32_t i = 0; i < WORDMARK_CHAR_COUNT - 1U; ++i) {
        uint32_t next = i + 1U;
        float slide = ease_out_cubic(
            (progress - (0.28f + 0.06f * (float)char_delays[i])) / 0.08f);
        float next_slide = ease_out_cubic(
            (progress - (0.28f + 0.06f * (float)char_delays[next])) / 0.08f);
        float dot_slide = slide < next_slide ? slide : next_slide;
        float dot_x = 120.0f +
            (72.5f + 24.0f * (float)i +
             (i < 2U ? -spread : spread) - 120.0f) * dot_slide;

        dots[i].visible = i != 2U && dot_slide > 0.0f;
        dots[i].left = (int16_t)(dot_x - 1.0f);
        dots[i].color = 0xFFFFU;
        dots[i].color = scale_rgb565(dots[i].color,
                                      dot_slide < 0.5f ? dot_slide * 2.0f : 1.0f);
    }
}

static uint16_t wordmark_pixel_color(uint32_t x, uint32_t y, uint32_t progress_percent,
                                    const wordmark_glyph_t *glyphs,
                                    const wordmark_dot_t *dots)
{
    if (y < 110U || y > 132U) {
        return 0;
    }

    for (uint32_t i = 0; i < WORDMARK_CHAR_COUNT; ++i) {
        const wordmark_glyph_t *glyph = &glyphs[i];
        int32_t relative_x = (int32_t)x - glyph->left;
        int32_t relative_y = (int32_t)y - glyph->top;
        if (glyph->visible && relative_x >= 0 && relative_x < 15 &&
            relative_y >= 0 && relative_y < 21) {
            uint32_t font_x = (uint32_t)relative_x / 3U;
            uint32_t font_y = (uint32_t)relative_y / 3U;
            if ((FONT_5X7[i][font_y] & (1U << (4U - font_x))) != 0U) {
                return glyph->color;
            }
        }

        if (i < WORDMARK_CHAR_COUNT - 1U && dots[i].visible &&
            (int32_t)x >= dots[i].left && (int32_t)x < dots[i].left + 3 &&
            y >= 128U && y < 131U) {
            return dots[i].color;
        }
    }

    if (progress_percent >= 66U) {
        uint32_t dot_size = progress_percent < 75U ? 5U : 3U;
        int32_t growth = (int32_t)(dot_size - 3U) / 2;
        if ((int32_t)x >= 119 - growth &&
            (int32_t)x < 119 - growth + (int32_t)dot_size &&
            (int32_t)y >= 128 - growth &&
            (int32_t)y < 128 - growth + (int32_t)dot_size) {
            return 0xFFFFU;
        }
    }

    return 0;
}

typedef struct {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
    bool valid;
} dirty_rect_t;

static void include_dirty_rect(dirty_rect_t *rect, int32_t left, int32_t top,
                               int32_t right, int32_t bottom)
{
    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right > (int32_t)LCD_WIDTH) {
        right = LCD_WIDTH;
    }
    if (bottom > (int32_t)LCD_HEIGHT) {
        bottom = LCD_HEIGHT;
    }
    if (left >= right || top >= bottom) {
        return;
    }

    if (!rect->valid) {
        rect->left = left;
        rect->top = top;
        rect->right = right;
        rect->bottom = bottom;
        rect->valid = true;
        return;
    }
    if (left < rect->left) {
        rect->left = left;
    }
    if (top < rect->top) {
        rect->top = top;
    }
    if (right > rect->right) {
        rect->right = right;
    }
    if (bottom > rect->bottom) {
        rect->bottom = bottom;
    }
}

static void draw_content_rectangle(const dirty_rect_t *dirty,
                                  uint32_t progress_percent,
                                  uint32_t frame_number,
                                  uint32_t blink_frame, uint32_t mood,
                                  uint32_t active_segments,
                                  const wordmark_glyph_t *glyphs,
                                  const wordmark_dot_t *dots,
                                  int32_t mascot_left, int32_t mascot_top,
                                  uint32_t mascot_width, uint32_t mascot_height)
{
    int32_t x_start = dirty->left & ~1;
    int32_t x_end = dirty->right;
    if (((x_end - x_start) & 1) != 0) {
        if (x_end < (int32_t)LCD_WIDTH) {
            ++x_end;
        } else {
            --x_start;
        }
    }

    begin_pixel_window((uint16_t)x_start, (uint16_t)dirty->top,
                       (uint16_t)(x_end - 1), (uint16_t)(dirty->bottom - 1));
    uint8_t *pixels = (uint8_t *)pixel_words;
    uint32_t buffered_pixels = 0;
    for (int32_t y = dirty->top; y < dirty->bottom; ++y) {
        for (int32_t x = x_start; x < x_end; ++x) {
            uint16_t color = wordmark_pixel_color(
                (uint32_t)x, (uint32_t)y, progress_percent,
                glyphs, dots);
            if (mascot_width > 1U && mascot_height > 1U &&
                x >= mascot_left && x < mascot_left + (int32_t)mascot_width &&
                y >= mascot_top && y < mascot_top + (int32_t)mascot_height) {
                bool mascot_opaque;
                uint16_t mascot_color = mascot_pixel_color(
                    (uint32_t)x, (uint32_t)y, mascot_left, mascot_top,
                    mascot_width, mascot_height, 0xFFFFU, 0x0000U,
                    frame_number, blink_frame, mood,
                    &mascot_opaque);
                if (mascot_opaque) {
                    color = mascot_color;
                }
            }

            pixels[buffered_pixels * 2U] = (uint8_t)(color >> 8);
            pixels[buffered_pixels * 2U + 1U] = (uint8_t)color;
            ++buffered_pixels;
            if (buffered_pixels == PIXELS_PER_TRANSFER) {
                transfer(pixel_words, TRANSFER_BYTES);
                buffered_pixels = 0;
            }
        }
    }
    if (buffered_pixels != 0U) {
        transfer(pixel_words, buffered_pixels * 2U);
    }
    chip_select(false);
}

static uint16_t ring_pixel_color(uint32_t x, uint32_t y, uint32_t base_segment,
                                 uint32_t active_segments,
                                 const uint16_t *ring_colors)
{
    int32_t dx = (int32_t)x - (int32_t)(LCD_WIDTH / 2U);
    int32_t dy = (int32_t)y - (int32_t)(LCD_HEIGHT / 2U);
    int32_t distance_squared = dx * dx + dy * dy;
    int32_t inner_radius = RING_RADIUS - RING_HALF_WIDTH;
    int32_t outer_radius = RING_RADIUS + RING_HALF_WIDTH;
    if (distance_squared < inner_radius * inner_radius ||
        distance_squared > outer_radius * outer_radius || active_segments == 0U) {
        return 0;
    }

    int32_t best_dot = INT32_MIN;
    uint32_t nearest_segment = 0;
    uint32_t previous_segment = base_segment == 0U
                                    ? RING_SEGMENTS - 1U
                                    : base_segment - 1U;
    uint32_t next_segment = base_segment + 1U == RING_SEGMENTS
                                ? 0U
                                : base_segment + 1U;
    uint32_t candidates[3] = {previous_segment, base_segment, next_segment};
    for (uint32_t i = 0; i < 3U; ++i) {
        uint32_t candidate = candidates[i];
        int32_t dot = dx * ring_x[candidate] + dy * ring_y[candidate];
        if (dot > best_dot || (dot == best_dot && candidate < nearest_segment)) {
            best_dot = dot;
            nearest_segment = candidate;
        }
    }
    if (nearest_segment >= active_segments) {
        return 0;
    }

    int32_t cross = dx * ring_y[nearest_segment] - dy * ring_x[nearest_segment];
    if (cross < 0) {
        cross = -cross;
    }
    return cross <= RING_DASH_CROSS_LIMIT * 256
               ? ring_colors[nearest_segment]
               : 0;
}

static void initialize_ring_bounds(void)
{
    uint16_t owner_colors[RING_SEGMENTS];
    memset(ring_bounds, 0, sizeof(ring_bounds));
    for (uint32_t i = 0; i < RING_SEGMENTS; ++i) {
        owner_colors[i] = (uint16_t)(i + 1U);
    }

    for (uint32_t base_segment = 0; base_segment < RING_SEGMENTS;
         ++base_segment) {
        int32_t x_start = (int32_t)(LCD_WIDTH / 2U) +
                          ring_coordinate_pixel(ring_x[base_segment]) -
                          (int32_t)(RING_TILE_SIZE / 2U);
        int32_t y_start = (int32_t)(LCD_HEIGHT / 2U) +
                          ring_coordinate_pixel(ring_y[base_segment]) -
                          (int32_t)(RING_TILE_SIZE / 2U);
        if (x_start < 0) {
            x_start = 0;
        } else if (x_start > (int32_t)(LCD_WIDTH - RING_TILE_SIZE)) {
            x_start = (int32_t)(LCD_WIDTH - RING_TILE_SIZE);
        }
        if (y_start < 0) {
            y_start = 0;
        } else if (y_start > (int32_t)(LCD_HEIGHT - RING_TILE_SIZE)) {
            y_start = (int32_t)(LCD_HEIGHT - RING_TILE_SIZE);
        }

        for (uint32_t tile_y = 0; tile_y < RING_TILE_SIZE; ++tile_y) {
            for (uint32_t tile_x = 0; tile_x < RING_TILE_SIZE; ++tile_x) {
                uint32_t x = (uint32_t)x_start + tile_x;
                uint32_t y = (uint32_t)y_start + tile_y;
                uint16_t owner_color = ring_pixel_color(
                    x, y, base_segment, RING_SEGMENTS, owner_colors);
                if (owner_color == 0U) {
                    continue;
                }

                ring_bounds_t *bounds = &ring_bounds[owner_color - 1U];
                if (!bounds->valid) {
                    bounds->left = (uint8_t)x;
                    bounds->top = (uint8_t)y;
                    bounds->right = (uint8_t)(x + 1U);
                    bounds->bottom = (uint8_t)(y + 1U);
                    bounds->valid = true;
                } else {
                    if (x < bounds->left) {
                        bounds->left = (uint8_t)x;
                    }
                    if (y < bounds->top) {
                        bounds->top = (uint8_t)y;
                    }
                    if (x + 1U > bounds->right) {
                        bounds->right = (uint8_t)(x + 1U);
                    }
                    if (y + 1U > bounds->bottom) {
                        bounds->bottom = (uint8_t)(y + 1U);
                    }
                }
            }
        }
    }
}

static void draw_ring_segment(uint32_t segment, uint32_t active_segments,
                              const uint16_t *ring_colors)
{
    const ring_bounds_t *bounds = &ring_bounds[segment];
    if (!bounds->valid) {
        return;
    }

    uint32_t x_start = bounds->left;
    uint32_t y_start = bounds->top;
    uint32_t tile_width = bounds->right - x_start;
    uint32_t tile_height = bounds->bottom - y_start;
    uint32_t tile_pixels = tile_width * tile_height;
    begin_pixel_window((uint16_t)x_start, (uint16_t)y_start,
                       (uint16_t)(bounds->right - 1U),
                       (uint16_t)(bounds->bottom - 1U));
    uint8_t *pixels = (uint8_t *)pixel_words;
    for (uint32_t pixel = 0; pixel < tile_pixels; ++pixel) {
        uint32_t x = x_start + pixel % tile_width;
        uint32_t y = y_start + pixel / tile_width;
        uint16_t color = ring_pixel_color(
            x, y, segment, active_segments, ring_colors);
        pixels[(pixel % PIXELS_PER_TRANSFER) * 2U] = (uint8_t)(color >> 8);
        pixels[(pixel % PIXELS_PER_TRANSFER) * 2U + 1U] = (uint8_t)color;

        if ((pixel + 1U) % PIXELS_PER_TRANSFER == 0U) {
            transfer(pixel_words, TRANSFER_BYTES);
        }
    }
    uint32_t remaining = tile_pixels % PIXELS_PER_TRANSFER;
    if (remaining != 0U) {
        transfer(pixel_words, remaining * 2U);
    }
    chip_select(false);
}

static void draw_splash_frame(uint32_t progress_percent,
                              uint32_t ring_progress_percent,
                              uint32_t frame_number,
                              uint32_t blink_frame, uint32_t mood,
                              int32_t bob_offset)
{
    static uint32_t drawn_ring_segments;
    static bool previous_content_valid;
    static bool previous_center_dot_visible;
    static bool previous_happy;
    static uint32_t previous_pulse_segment;
    static uint32_t previous_mascot_width;
    static uint32_t previous_mascot_height;
    static int32_t previous_mascot_left;
    static int32_t previous_mascot_top;
    static wordmark_glyph_t previous_glyphs[WORDMARK_CHAR_COUNT];
    static wordmark_dot_t previous_dots[WORDMARK_CHAR_COUNT - 1U];
    uint8_t *pixels = (uint8_t *)pixel_words;
    uint16_t ring_colors[RING_SEGMENTS];
    wordmark_glyph_t glyphs[WORDMARK_CHAR_COUNT];
    wordmark_dot_t dots[WORDMARK_CHAR_COUNT - 1U];
    dirty_rect_t dirty_rects[WORDMARK_CHAR_COUNT * 2U + 1U] = {0};
    uint32_t dirty_count = 0;
    uint32_t active_segments = ring_progress_percent * RING_SEGMENTS / 100U;
    float progress = (float)progress_percent / 100.0f;
    float grow = progress / 0.1f;
    if (grow > 1.0f) {
        grow = 1.0f;
    }
    float inverse_grow = 1.0f - grow;
    float mascot_scale = (1.0f - inverse_grow * inverse_grow * inverse_grow) *
                         1.35f;
    float collapse = 0.0f;
    if (progress > 0.52f) {
        collapse = (progress - 0.52f) / 0.14f;
        if (collapse > 1.0f) {
            collapse = 1.0f;
        }
        collapse *= collapse;
    }
    mascot_scale *= 1.0f - collapse;
    uint32_t mascot_width = (uint32_t)((float)MASCOT_WIDTH * mascot_scale / 1.35f + 0.5f);
    uint32_t mascot_height = (uint32_t)((float)MASCOT_HEIGHT * mascot_scale / 1.35f + 0.5f);
    int32_t mascot_center_x = (int32_t)(LCD_WIDTH / 2U) + (int32_t)(0.5f * collapse + 0.5f);
    int32_t mascot_center_y = (int32_t)(LCD_HEIGHT / 2U) + (int32_t)(9.5f * collapse + 0.5f);
    int32_t mascot_left = mascot_center_x - (int32_t)mascot_width / 2;
    int32_t mascot_top = mascot_center_y - (int32_t)mascot_height / 2 +
                         (int32_t)((float)bob_offset * mascot_scale / 1.35f);

    if (active_segments > RING_SEGMENTS) {
        active_segments = RING_SEGMENTS;
    }
    uint32_t pulse_segment = 0;
    if (mood == SPLASH_MOOD_HAPPY && active_segments > 0U) {
        pulse_segment = previous_happy
                            ? (previous_pulse_segment + 1U) % active_segments
                            : 0U;
    } else {
        previous_happy = false;
    }
    for (uint32_t i = 0; i < RING_SEGMENTS; ++i) {
        ring_colors[i] = mood == SPLASH_MOOD_HAPPY
                             ? get_ok_ring_color(i, 0U)
                             : 0xFFFFU;
    }
    if (mood == SPLASH_MOOD_HAPPY && active_segments > 0U) {
        ring_colors[pulse_segment] = 0xFFFFU;
    }
    if (mascot_width > 1U && mascot_height > 1U) {
        prepare_mascot_render_cache(mascot_width, mascot_height, frame_number,
                                    active_segments, blink_frame);
    }
    prepare_wordmark(progress_percent, glyphs, dots);

    for (uint32_t i = 0; i < WORDMARK_CHAR_COUNT; ++i) {
        bool changed = !previous_content_valid ||
                       previous_glyphs[i].left != glyphs[i].left ||
                       previous_glyphs[i].top != glyphs[i].top ||
                       previous_glyphs[i].color != glyphs[i].color ||
                       previous_glyphs[i].visible != glyphs[i].visible;
        if (changed) {
            dirty_rect_t component_dirty = {0};
            if (previous_content_valid && previous_glyphs[i].visible) {
                include_dirty_rect(&component_dirty, previous_glyphs[i].left,
                                   previous_glyphs[i].top,
                                   previous_glyphs[i].left + 15,
                                   previous_glyphs[i].top + 21);
            }
            if (glyphs[i].visible) {
                include_dirty_rect(&component_dirty, glyphs[i].left, glyphs[i].top,
                                   glyphs[i].left + 15, glyphs[i].top + 21);
            }
            if (component_dirty.valid) {
                dirty_rects[dirty_count++] = component_dirty;
            }
        }
    }
    for (uint32_t i = 0; i < WORDMARK_CHAR_COUNT - 1U; ++i) {
        bool changed = !previous_content_valid ||
                       previous_dots[i].left != dots[i].left ||
                       previous_dots[i].color != dots[i].color ||
                       previous_dots[i].visible != dots[i].visible;
        if (changed) {
            dirty_rect_t component_dirty = {0};
            if (previous_content_valid && previous_dots[i].visible) {
                include_dirty_rect(&component_dirty, previous_dots[i].left, 128,
                                   previous_dots[i].left + 3, 131);
            }
            if (dots[i].visible) {
                include_dirty_rect(&component_dirty, dots[i].left, 128,
                                   dots[i].left + 3, 131);
            }
            if (component_dirty.valid) {
                dirty_rects[dirty_count++] = component_dirty;
            }
        }
    }

    bool previous_mascot_visible = previous_mascot_width > 1U &&
                                    previous_mascot_height > 1U;
    bool mascot_visible = mascot_width > 1U && mascot_height > 1U;
    dirty_rect_t mascot_dirty = {0};
    if (previous_mascot_visible) {
        include_dirty_rect(&mascot_dirty, previous_mascot_left, previous_mascot_top,
                           previous_mascot_left + (int32_t)previous_mascot_width,
                           previous_mascot_top + (int32_t)previous_mascot_height);
    }
    if (mascot_visible) {
        include_dirty_rect(&mascot_dirty, mascot_left, mascot_top,
                           mascot_left + (int32_t)mascot_width,
                           mascot_top + (int32_t)mascot_height);
    }
    if (mascot_dirty.valid) {
        dirty_rects[dirty_count++] = mascot_dirty;
    }

    bool center_dot_visible = progress_percent >= 66U;
    if (previous_center_dot_visible || center_dot_visible) {
        dirty_rect_t center_dot_dirty = {0};
        include_dirty_rect(&center_dot_dirty, 117, 126, 124, 133);
        dirty_rects[dirty_count++] = center_dot_dirty;
    }

    uint32_t first_ring_segment = drawn_ring_segments;
    uint32_t ring_update_limit = active_segments;
    if (active_segments < drawn_ring_segments) {
        first_ring_segment = 0;
        ring_update_limit = drawn_ring_segments;
    }
    if (mood == SPLASH_MOOD_HAPPY) {
        if (previous_happy && active_segments > 0U) {
            first_ring_segment = 0;
            ring_update_limit = 0;
        } else {
            first_ring_segment = 0;
            ring_update_limit = active_segments;
        }
    }

    for (uint32_t segment = first_ring_segment;
         segment < ring_update_limit;
         ++segment) {
        draw_ring_segment(segment, active_segments, ring_colors);
    }
    if (mood == SPLASH_MOOD_HAPPY && previous_happy &&
        active_segments > 0U) {
        draw_ring_segment(previous_pulse_segment, active_segments, ring_colors);
        if (pulse_segment != previous_pulse_segment) {
            draw_ring_segment(pulse_segment, active_segments, ring_colors);
        }
    }

    drawn_ring_segments = active_segments;
    if (mood == SPLASH_MOOD_HAPPY && active_segments > 0U) {
        previous_pulse_segment = pulse_segment;
        previous_happy = true;
    }

    for (uint32_t i = 0; i < dirty_count; ++i) {
        draw_content_rectangle(&dirty_rects[i], progress_percent, frame_number,
                               blink_frame, mood, active_segments,
                               glyphs, dots, mascot_left, mascot_top,
                               mascot_width, mascot_height);
    }

    memcpy(previous_glyphs, glyphs, sizeof(glyphs));
    memcpy(previous_dots, dots, sizeof(dots));
    previous_mascot_left = mascot_left;
    previous_mascot_top = mascot_top;
    previous_mascot_width = mascot_width;
    previous_mascot_height = mascot_height;
    previous_center_dot_visible = center_dot_visible;
    previous_content_valid = true;
}

static void draw_splash_animation_frame(uint32_t progress_percent,
                                        uint32_t ring_progress_percent,
                                        uint32_t frame_number,
                                        uint32_t boot_seed,
                                        uint32_t stall_progress_percent)
{
    static const int8_t bob_offsets[] = {-2, 0, 2, 1, -1};
    uint32_t blink_frame = boot_seed % 13U;
    uint32_t mood = SPLASH_MOOD_LOADING;
#if defined(CONFIG_BOOT_DISPLAY_HAPPY_THRESHOLD_PERCENT)
    const uint32_t happy_threshold = (uint32_t)CONFIG_BOOT_DISPLAY_HAPPY_THRESHOLD_PERCENT;
#else
    const uint32_t happy_threshold = 85U;
#endif
    if (progress_percent < 17U) {
        mood = SPLASH_MOOD_SLEEPY;
    } else if (progress_percent == stall_progress_percent) {
        mood = SPLASH_MOOD_WORRIED;
    } else if (progress_percent >= happy_threshold) {
        mood = SPLASH_MOOD_HAPPY;
    }

    int32_t bob_offset = bob_offsets[(frame_number + boot_seed) %
                                     (sizeof(bob_offsets) / sizeof(bob_offsets[0]))];
    draw_splash_frame(progress_percent, ring_progress_percent, frame_number,
                      blink_frame, mood, bob_offset);

    if (progress_percent == stall_progress_percent) {
        ulp_lp_core_delay_us(250000U + (boot_seed % 400000U));
    }
}

static void update_display_progress(uint32_t progress_percent)
{
    lp_display_progress_percent = progress_percent;
    lp_display_progress = (progress_percent * 12U + 50U) / 100U;
}

int main(void)
{
    lp_timer_ticks_per_second =
        ulp_lp_core_lp_timer_calculate_sleep_ticks(1000000ULL);
    if (lp_timer_ticks_per_second == 0U) {
        lp_display_error_code = 2;
        lp_display_status = LP_BOOT_STATUS_ERROR;
        ulp_lp_core_wakeup_main_processor();
        while (1) {
            ulp_lp_core_delay_us(LP_ERROR_DELAY_US);
        }
    }
    
    ulp_lp_core_gpio_init(LCD_CS_GPIO);
    ulp_lp_core_gpio_output_enable(LCD_CS_GPIO);
    ulp_lp_core_gpio_init(LCD_DC_GPIO);
    ulp_lp_core_gpio_output_enable(LCD_DC_GPIO);
    chip_select(false);
    
    uint64_t t_gpio_done = ulp_lp_core_lp_timer_get_cycle_count();
    
    initialize_display();
    
    uint64_t t_display_init = ulp_lp_core_lp_timer_get_cycle_count();
    lp_display_display_init_time_us = timer_ticks_to_us(t_display_init - t_gpio_done);

    initialize_ring_points();
    initialize_ring_bounds();

    update_display_progress(0);
    lp_display_cycles = 0;
    lp_display_status = LP_BOOT_STATUS_RUNNING;
    lp_display_error_code = 0;
    lp_display_idle_ticks = 0;
    lp_display_frame_time_us = 0;

    uint64_t timer_seed = ulp_lp_core_lp_timer_get_cycle_count();
    uint32_t boot_seed = (uint32_t)timer_seed ^ (uint32_t)(timer_seed >> 32);
    boot_seed ^= lp_display_cycles * 2654435761U;
    uint32_t stall_frame = 4U + (boot_seed % 5U);
    uint32_t animation_frame = 0;
    
    if (lp_display_mode == LP_BOOT_MODE_STATIC) {
        uint64_t t_frame_start = ulp_lp_core_lp_timer_get_cycle_count();
        draw_splash_animation_frame(50U, 50U, 6U, boot_seed, UINT32_MAX);
        uint64_t t_frame_end = ulp_lp_core_lp_timer_get_cycle_count();
        lp_display_frame_time_us = timer_ticks_to_us(t_frame_end - t_frame_start);
        
        update_display_progress(50U);
        lp_display_status = LP_BOOT_STATUS_STATIC;
        while (1) {
            ulp_lp_core_delay_us(LP_FRAME_DELAY_US);
            ++lp_display_idle_ticks;
        }
    }

    if (lp_display_mode == LP_BOOT_MODE_EVENT_DRIVEN) {
        uint32_t progress_percent = 0;
        uint32_t frame_number = 0;
        uint32_t completed_hold_frames = 0;
#if defined(CONFIG_BOOT_DISPLAY_COMPLETION_HOLD_FRAMES)
        const uint32_t hold_frames_target = (uint32_t)CONFIG_BOOT_DISPLAY_COMPLETION_HOLD_FRAMES;
#else
        const uint32_t hold_frames_target = 15U;
#endif
        uint64_t last_progress_update_ticks =
            ulp_lp_core_lp_timer_get_cycle_count();
        while (1) {
            uint32_t target_progress = lp_display_target_progress_percent;
            uint64_t now = ulp_lp_core_lp_timer_get_cycle_count();
            uint32_t elapsed_us =
                timer_ticks_to_us(now - last_progress_update_ticks);
            uint32_t max_advance_percent =             elapsed_us / LP_EVENT_PROGRESS_STEP_US;
            last_progress_update_ticks = now;

            if (target_progress > progress_percent) {
                if (max_advance_percent > 0U) {
                    uint32_t remaining = target_progress - progress_percent;
                    progress_percent += max_advance_percent < remaining
                                            ? max_advance_percent
                                            : remaining;
                }
            }

            uint32_t scene_progress_percent =
                progress_percent == 0U ? 10U : progress_percent;
            uint64_t t_frame_start = ulp_lp_core_lp_timer_get_cycle_count();
            draw_splash_animation_frame(scene_progress_percent,
                                        progress_percent, frame_number,
                                        boot_seed, UINT32_MAX);
            uint64_t t_frame_end = ulp_lp_core_lp_timer_get_cycle_count();

            uint32_t frame_time_us = timer_ticks_to_us(t_frame_end - t_frame_start);
            if (progress_percent > 0U) {
                if (frame_time_us > lp_display_frame_time_us) {
                    lp_display_frame_time_us = frame_time_us;
                }
            }
            update_display_progress(progress_percent);

            /* Celebratory hold period once 100% reached before releasing to HP-Core */
            if (progress_percent >= 100U) {
                if (completed_hold_frames < hold_frames_target) {
                    ++completed_hold_frames;
                } else if (lp_display_status != LP_BOOT_STATUS_DONE) {
                    lp_display_cycles = 1;
                    lp_display_status = LP_BOOT_STATUS_DONE;
                }
            }

            frame_number = (frame_number + 1U) % 13U;
            /* Jitter-free dynamic delay: maintain stable target frame period */
            if (frame_time_us < LP_FRAME_DELAY_US) {
                ulp_lp_core_delay_us(LP_FRAME_DELAY_US - frame_time_us);
            }
        }

        while (1) {
            ulp_lp_core_delay_us(LP_FRAME_DELAY_US);
            ++lp_display_idle_ticks;
        }
    }

    if (lp_display_mode == LP_BOOT_MODE_SINGLE_RUN) {
        for (animation_frame = 0; animation_frame < 13U; ++animation_frame) {
            uint32_t progress_percent = animation_frame * 100U / 12U;
            uint32_t stall_progress_percent = stall_frame * 100U / 12U;
            uint64_t t_frame_start = ulp_lp_core_lp_timer_get_cycle_count();
            draw_splash_animation_frame(progress_percent, progress_percent,
                                        animation_frame, boot_seed,
                                        stall_progress_percent);
            uint64_t t_frame_end = ulp_lp_core_lp_timer_get_cycle_count();
            
            uint32_t frame_time_us = timer_ticks_to_us(t_frame_end - t_frame_start);
            if (animation_frame == 1) {
                lp_display_frame_time_us = frame_time_us;
            }
            
            update_display_progress(progress_percent);
            if (frame_time_us < LP_FRAME_DELAY_US) {
                ulp_lp_core_delay_us(LP_FRAME_DELAY_US - frame_time_us);
            }
        }
        
        lp_display_cycles = 1;
        lp_display_status = LP_BOOT_STATUS_DONE;

        /* Boot animation completed; hold last frame (smiling mascot) and idle until HP handoff */
        while (1) {
            ulp_lp_core_delay_us(LP_FRAME_DELAY_US);
            ++lp_display_idle_ticks;
        }
    }

    /* Default (Mode 0): Continuous loop until HP-Core signals handoff */
    while (1) {
        uint32_t progress_percent = animation_frame * 100U / 12U;
        uint32_t stall_progress_percent = stall_frame * 100U / 12U;
        uint64_t t_frame_start = ulp_lp_core_lp_timer_get_cycle_count();
        draw_splash_animation_frame(progress_percent, progress_percent,
                                    animation_frame, boot_seed,
                                    stall_progress_percent);
        uint64_t t_frame_end = ulp_lp_core_lp_timer_get_cycle_count();
        
        uint32_t frame_time_us = timer_ticks_to_us(t_frame_end - t_frame_start);
        if (animation_frame == 1 && lp_display_cycles == 0) {
            lp_display_frame_time_us = frame_time_us;
        }
        
        update_display_progress(progress_percent);
        ++animation_frame;
        if (animation_frame == 13U) {
            animation_frame = 0;
            ++lp_display_cycles;
        }
        if (frame_time_us < LP_FRAME_DELAY_US) {
            ulp_lp_core_delay_us(LP_FRAME_DELAY_US - frame_time_us);
        }
    }

    return 0;
}
