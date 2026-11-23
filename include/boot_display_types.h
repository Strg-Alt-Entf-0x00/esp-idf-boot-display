#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Boot display hardware backend.
 */
typedef enum {
    BOOT_DISPLAY_BACKEND_LP_SPI = 0,     /**< Autonomous LP-Core (ULP RISC-V @ 40MHz via LP-SPI, <15ms) */
    BOOT_DISPLAY_BACKEND_HP_MIPI_DSI = 1,/**< High-Performance MIPI-DSI Host (Waveshare 4.3\"/7\" LCD) */
    BOOT_DISPLAY_BACKEND_HP_SPI = 2,     /**< High-Performance SPI2_HOST with DMA */
} boot_display_backend_t;

/**
 * @brief Boot animation operational state.
 */
typedef enum {
    BOOT_DISPLAY_STATE_IDLE = 0,         /**< Display engine not yet started */
    BOOT_DISPLAY_STATE_ANIMATING = 1,    /**< Actively rendering boot animation */
    BOOT_DISPLAY_STATE_ERROR = 2,        /**< Hardware or transfer error occurred */
    BOOT_DISPLAY_STATE_STATIC = 3,       /**< Showing static boot frame */
    BOOT_DISPLAY_STATE_DONE = 4,         /**< Boot animation cycle completed, ready for handoff */
} boot_display_state_t;

/**
 * @brief Boot animation playback mode.
 */
typedef enum {
    BOOT_DISPLAY_MODE_CONTINUOUS = 0,    /**< Loop animation continuously until handoff */
    BOOT_DISPLAY_MODE_STATIC_FRAME = 1,  /**< Render single static frame and idle */
    BOOT_DISPLAY_MODE_SINGLE_CYCLE = 2,  /**< Play once (0->100%), set DONE, hold happy frame */
    BOOT_DISPLAY_MODE_EVENT_DRIVEN = 3,  /**< Advance only when HP-Core reports real milestones */
} boot_display_mode_t;

/**
 * @brief Color palette for boot display in RGB565 format.
 */
typedef struct {
    uint16_t bg_color;                   /**< Background color (default: 0x0000 = Black) */
    uint16_t ring_color;                 /**< Progress ring color (default: 0x073F = Cyan) */
    union {
        uint16_t body_color;             /**< Mascot / main subject body color (default: 0xFFFF = White) */
        uint16_t mascot_color;           /**< Mascot body color alias */
    };
    uint16_t accent_color;               /**< Eye and accent color (default: 0x073F = Cyan) */
} boot_display_colors_t;

/**
 * @brief Display geometry and resolution parameters.
 */
typedef struct {
    uint16_t width;                      /**< Panel horizontal pixel count (e.g. 240, 800) */
    uint16_t height;                     /**< Panel vertical pixel count (e.g. 240, 480) */
    bool is_circular;                    /**< True if circular mask/layout applies */
} boot_display_geometry_t;

/**
 * @brief Universal Boot Display Configuration struct.
 */
typedef struct {
    /* SPI specific hardware pin routing (matches legacy order) */
    gpio_num_t mosi_pin;                 /**< SPI MOSI / SDA pin (Default: GPIO 2) */
    gpio_num_t sclk_pin;                 /**< SPI SCLK pin (Default: GPIO 3) */
    gpio_num_t cs_pin;                   /**< SPI CS pin (Default: GPIO 4) */
    gpio_num_t dc_pin;                   /**< SPI DC pin (Default: GPIO 5) */
    gpio_num_t rst_pin;                  /**< Reset pin (Default: GPIO 49) */
    uint32_t   spi_clock_hz;             /**< SPI frequency in Hz (Default: 8 MHz LP, 40 MHz HP) */
    boot_display_mode_t     mode;        /**< Playback mode */
    
    boot_display_backend_t  backend;     /**< Hardware rendering backend */
    boot_display_geometry_t geometry;    /**< Target resolution & form factor */
    boot_display_colors_t   colors;      /**< Configurable color theme */
} boot_display_config_t;

/* =========================================================================
 * Backward Compatibility Aliases for legacy LP-Boot callers
 * ========================================================================= */
typedef boot_display_state_t  lp_boot_state_t;
typedef boot_display_mode_t   lp_boot_mode_t;

/**
 * @brief Legacy LP boot configuration struct for existing firmware calls.
 */
typedef struct {
    gpio_num_t mosi_pin;                 /**< SPI MOSI / SDA pin */
    gpio_num_t sclk_pin;                 /**< SPI SCLK pin */
    gpio_num_t cs_pin;                   /**< SPI CS pin */
    gpio_num_t dc_pin;                   /**< SPI DC pin */
    gpio_num_t rst_pin;                  /**< Reset pin */
    uint32_t   spi_clock_hz;             /**< SPI frequency in Hz */
    lp_boot_mode_t mode;                 /**< Playback mode */
} lp_boot_display_config_t;

#define LP_BOOT_STATE_IDLE       BOOT_DISPLAY_STATE_IDLE
#define LP_BOOT_STATE_ANIMATING  BOOT_DISPLAY_STATE_ANIMATING
#define LP_BOOT_STATE_ERROR      BOOT_DISPLAY_STATE_ERROR
#define LP_BOOT_STATE_STATIC     BOOT_DISPLAY_STATE_STATIC
#define LP_BOOT_STATE_DONE       BOOT_DISPLAY_STATE_DONE

#define LP_BOOT_MODE_CONTINUOUS   BOOT_DISPLAY_MODE_CONTINUOUS
#define LP_BOOT_MODE_STATIC_FRAME BOOT_DISPLAY_MODE_STATIC_FRAME
#define LP_BOOT_MODE_SINGLE_CYCLE BOOT_DISPLAY_MODE_SINGLE_CYCLE
#define LP_BOOT_MODE_EVENT_DRIVEN BOOT_DISPLAY_MODE_EVENT_DRIVEN

#ifdef __cplusplus
}
#endif

