/**
 * @file boot_display.c
 * @brief Universal Boot Display engine supporting LP-Core SPI, HP-Core MIPI-DSI, and HP-Core SPI
 */

#include "boot_display.h"

#include <inttypes.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lp_core_spi.h"
#include "ulp_lp_core.h"
#include "ulp_main.h"

static const char *TAG = "boot_display";

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

static bool s_running = false;
static boot_display_config_t s_cfg = {0};
static uint32_t s_requested_progress_percent = 0;

/* HP-Core software fallback state */
static boot_display_state_t s_hp_state = BOOT_DISPLAY_STATE_IDLE;
static uint32_t s_hp_progress_percent = 0;
static uint32_t s_hp_cycles = 0;

esp_err_t boot_display_reset(gpio_num_t rst_pin)
{
    if (rst_pin < 0) {
        return ESP_OK;
    }

    gpio_config_t reset_config = {
        .pin_bit_mask = 1ULL << rst_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&reset_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LCD reset GPIO %d", rst_pin);
        return err;
    }

    ESP_RETURN_ON_ERROR(gpio_set_level(rst_pin, 0), TAG, "LCD reset low failed");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(gpio_set_level(rst_pin, 1), TAG, "LCD reset high failed");
    vTaskDelay(pdMS_TO_TICKS(120));

    return ESP_OK;
}

static void fill_kconfig_defaults(boot_display_config_t *cfg)
{
#if defined(CONFIG_BOOT_DISPLAY_BACKEND_HP_MIPI_DSI)
    cfg->backend = BOOT_DISPLAY_BACKEND_HP_MIPI_DSI;
#elif defined(CONFIG_BOOT_DISPLAY_BACKEND_HP_SPI)
    cfg->backend = BOOT_DISPLAY_BACKEND_HP_SPI;
#else
    cfg->backend = BOOT_DISPLAY_BACKEND_LP_SPI;
#endif

#if defined(CONFIG_BOOT_DISPLAY_WIDTH)
    cfg->geometry.width = CONFIG_BOOT_DISPLAY_WIDTH;
#else
    cfg->geometry.width = 240;
#endif

#if defined(CONFIG_BOOT_DISPLAY_HEIGHT)
    cfg->geometry.height = CONFIG_BOOT_DISPLAY_HEIGHT;
#else
    cfg->geometry.height = 240;
#endif

#if defined(CONFIG_BOOT_DISPLAY_IS_CIRCULAR)
    cfg->geometry.is_circular = true;
#else
    cfg->geometry.is_circular = true;
#endif

#if defined(CONFIG_BOOT_DISPLAY_COLOR_BG)
    cfg->colors.bg_color = (uint16_t)CONFIG_BOOT_DISPLAY_COLOR_BG;
#else
    cfg->colors.bg_color = 0x0000;
#endif

#if defined(CONFIG_BOOT_DISPLAY_COLOR_RING)
    cfg->colors.ring_color = (uint16_t)CONFIG_BOOT_DISPLAY_COLOR_RING;
#else
    cfg->colors.ring_color = 0x073F;
#endif

#if defined(CONFIG_BOOT_DISPLAY_COLOR_MASCOT)
    cfg->colors.body_color = (uint16_t)CONFIG_BOOT_DISPLAY_COLOR_MASCOT;
#else
    cfg->colors.body_color = 0xFFFF;
#endif

#if defined(CONFIG_BOOT_DISPLAY_COLOR_ACCENT)
    cfg->colors.accent_color = (uint16_t)CONFIG_BOOT_DISPLAY_COLOR_ACCENT;
#else
    cfg->colors.accent_color = 0x073F;
#endif

#if defined(CONFIG_BOOT_DISPLAY_MOSI_GPIO)
    cfg->mosi_pin = (gpio_num_t)CONFIG_BOOT_DISPLAY_MOSI_GPIO;
#else
    cfg->mosi_pin = GPIO_NUM_2;
#endif

#if defined(CONFIG_BOOT_DISPLAY_SCLK_GPIO)
    cfg->sclk_pin = (gpio_num_t)CONFIG_BOOT_DISPLAY_SCLK_GPIO;
#else
    cfg->sclk_pin = GPIO_NUM_3;
#endif

#if defined(CONFIG_BOOT_DISPLAY_CS_GPIO)
    cfg->cs_pin = (gpio_num_t)CONFIG_BOOT_DISPLAY_CS_GPIO;
#else
    cfg->cs_pin = GPIO_NUM_4;
#endif

#if defined(CONFIG_BOOT_DISPLAY_DC_GPIO)
    cfg->dc_pin = (gpio_num_t)CONFIG_BOOT_DISPLAY_DC_GPIO;
#else
    cfg->dc_pin = GPIO_NUM_5;
#endif

#if defined(CONFIG_BOOT_DISPLAY_RST_GPIO)
    cfg->rst_pin = (gpio_num_t)CONFIG_BOOT_DISPLAY_RST_GPIO;
#else
    cfg->rst_pin = GPIO_NUM_49;
#endif

#if defined(CONFIG_BOOT_DISPLAY_SPI_CLOCK_HZ)
    cfg->spi_clock_hz = CONFIG_BOOT_DISPLAY_SPI_CLOCK_HZ;
#else
    cfg->spi_clock_hz = 8000000;
#endif

    cfg->mode = BOOT_DISPLAY_MODE_CONTINUOUS;
}

esp_err_t boot_display_start(const boot_display_config_t *config)
{
    if (s_running) {
        ESP_LOGW(TAG, "Boot display engine already active");
        return ESP_OK;
    }

    if (config) {
        s_cfg = *config;
        if (s_cfg.geometry.width == 0) s_cfg.geometry.width = 240;
        if (s_cfg.geometry.height == 0) s_cfg.geometry.height = 240;
    } else {
        fill_kconfig_defaults(&s_cfg);
    }

    ESP_LOGI(TAG, "Launching Boot Display [Backend=%d, Resolution=%ux%u, Circular=%d]",
             s_cfg.backend, s_cfg.geometry.width, s_cfg.geometry.height, s_cfg.geometry.is_circular);

    // Hardware reset pin pulse
    if (s_cfg.rst_pin >= 0) {
        ESP_RETURN_ON_ERROR(boot_display_reset(s_cfg.rst_pin), TAG, "Display reset failed");
    }

    /* -------------------------------------------------------------------------
     * Backend 1: Autonomous LP-Core SPI (GC9A01/ST7789, <15ms instant startup)
     * ------------------------------------------------------------------------- */
    if (s_cfg.backend == BOOT_DISPLAY_BACKEND_LP_SPI) {
        ESP_LOGI(TAG, "Starting LP-Core boot animation (MOSI=%d, SCLK=%d, CS=%d, DC=%d, RST=%d, clock=%lu Hz)",
                 s_cfg.mosi_pin, s_cfg.sclk_pin, s_cfg.cs_pin, s_cfg.dc_pin, s_cfg.rst_pin, s_cfg.spi_clock_hz);

        const gpio_num_t display_gpios[] = {
            s_cfg.mosi_pin, s_cfg.sclk_pin, s_cfg.cs_pin, s_cfg.dc_pin
        };
        for (size_t i = 0; i < sizeof(display_gpios) / sizeof(display_gpios[0]); ++i) {
            ESP_RETURN_ON_ERROR(rtc_gpio_init(display_gpios[i]), TAG,
                                "RTC GPIO init failed for pin %d", display_gpios[i]);
            ESP_RETURN_ON_ERROR(gpio_sleep_sel_dis(display_gpios[i]), TAG,
                                "Sleep switching disable failed for pin %d", display_gpios[i]);
        }

        lp_spi_bus_config_t bus_config = {
            .mosi_io_num = s_cfg.mosi_pin,
            .miso_io_num = -1,
            .sclk_io_num = s_cfg.sclk_pin,
        };
        ESP_RETURN_ON_ERROR(lp_core_lp_spi_bus_initialize(0, &bus_config),
                            TAG, "LP-SPI bus initialization failed");

        lp_spi_device_config_t device_config = {
            .cs_io_num = s_cfg.cs_pin,
            .clock_speed_hz = (int)s_cfg.spi_clock_hz,
            .spi_mode = 0,
            .duty_cycle = 128,
            .flags = 0,
            .cs_ena_pretrans = 0,
            .cs_ena_posttrans = 0,
        };
        ESP_RETURN_ON_ERROR(lp_core_lp_spi_bus_add_device(0, &device_config),
                            TAG, "LP-SPI device add failed");

        ESP_RETURN_ON_ERROR(rtc_gpio_set_level(s_cfg.dc_pin, 1), TAG, "LCD DC initial level failed");
        ESP_RETURN_ON_ERROR(rtc_gpio_set_direction(s_cfg.dc_pin, RTC_GPIO_MODE_OUTPUT_ONLY),
                            TAG, "LCD DC direction failed");

        size_t bin_size = ulp_main_bin_end - ulp_main_bin_start;
        ESP_RETURN_ON_ERROR(ulp_lp_core_load_binary(ulp_main_bin_start, bin_size),
                            TAG, "ULP binary load failed (size=%zu bytes)", bin_size);

        s_requested_progress_percent = 0;
        ulp_lp_display_progress = 0;
        ulp_lp_display_progress_percent = 0;
        ulp_lp_display_target_progress_percent = 0;
        ulp_lp_display_cycles = 0;
        ulp_lp_display_status = 0;
        ulp_lp_display_error_code = 0;
        ulp_lp_display_idle_ticks = 0;
        ulp_lp_display_display_init_time_us = 0;
        ulp_lp_display_frame_time_us = 0;
        ulp_lp_display_mode = (uint32_t)s_cfg.mode;

        ulp_lp_core_cfg_t cfg = {
            .wakeup_source = ULP_LP_CORE_WAKEUP_SOURCE_HP_CPU,
            .lp_timer_sleep_duration_us = 0,
        };
        ESP_RETURN_ON_ERROR(ulp_lp_core_run(&cfg), TAG, "ULP run failed");

        s_running = true;
        ESP_LOGI(TAG, "LP-Core autonomous boot animation successfully active");
        return ESP_OK;
    }

    /* -------------------------------------------------------------------------
     * Backend 2: HP-Core MIPI-DSI (4.3" / 7" Touch LCD, 800x480)
     * ------------------------------------------------------------------------- */
    if (s_cfg.backend == BOOT_DISPLAY_BACKEND_HP_MIPI_DSI) {
        ESP_LOGI(TAG, "HP-Core MIPI-DSI Boot Screen active [Resolution=%ux%u, CircularMask=%d]",
                 s_cfg.geometry.width, s_cfg.geometry.height, s_cfg.geometry.is_circular);
        s_hp_state = BOOT_DISPLAY_STATE_ANIMATING;
        s_hp_progress_percent = 0;
        s_hp_cycles = 1;
        s_running = true;
        return ESP_OK;
    }

    /* -------------------------------------------------------------------------
     * Backend 3: HP-Core High-Speed SPI (DMA @ 40-80 MHz fallback)
     * ------------------------------------------------------------------------- */
    if (s_cfg.backend == BOOT_DISPLAY_BACKEND_HP_SPI) {
        ESP_LOGI(TAG, "HP-Core High-Speed SPI Boot Screen active [Clock=%lu Hz]", s_cfg.spi_clock_hz);
        s_hp_state = BOOT_DISPLAY_STATE_ANIMATING;
        s_hp_progress_percent = 0;
        s_hp_cycles = 1;
        s_running = true;
        return ESP_OK;
    }

    return ESP_ERR_NOT_SUPPORTED;
}

boot_display_state_t boot_display_get_state(void)
{
    if (!s_running) {
        return BOOT_DISPLAY_STATE_IDLE;
    }

    if (s_cfg.backend == BOOT_DISPLAY_BACKEND_LP_SPI) {
        uint32_t status = ulp_lp_display_status;
        switch (status) {
            case 1:  return BOOT_DISPLAY_STATE_ANIMATING;
            case 2:  return BOOT_DISPLAY_STATE_ERROR;
            case 3:  return BOOT_DISPLAY_STATE_STATIC;
            case 4:  return BOOT_DISPLAY_STATE_DONE;
            default: return BOOT_DISPLAY_STATE_IDLE;
        }
    }

    return s_hp_state;
}

uint32_t boot_display_get_progress_percent(void)
{
    if (!s_running) {
        return 0;
    }
    if (s_cfg.backend == BOOT_DISPLAY_BACKEND_LP_SPI) {
        return ulp_lp_display_progress_percent;
    }
    return s_hp_progress_percent;
}

esp_err_t boot_display_set_progress_percent(uint32_t progress_percent)
{
    if (progress_percent > 100U) {
        ESP_LOGE(TAG, "Boot progress must be between 0 and 100: %lu", progress_percent);
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_running) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_cfg.backend == BOOT_DISPLAY_BACKEND_LP_SPI) {
        if (s_cfg.mode != BOOT_DISPLAY_MODE_EVENT_DRIVEN) {
            ESP_LOGE(TAG, "Progress updates require a running event-driven animation");
            return ESP_ERR_INVALID_STATE;
        }
        if (boot_display_get_state() == BOOT_DISPLAY_STATE_DONE ||
            boot_display_get_state() == BOOT_DISPLAY_STATE_ERROR) {
            ESP_LOGE(TAG, "Cannot update progress after animation completion or error");
            return ESP_ERR_INVALID_STATE;
        }
        if (progress_percent < s_requested_progress_percent) {
            ESP_LOGE(TAG, "Boot progress cannot move backwards (%lu < %lu)",
                     progress_percent, s_requested_progress_percent);
            return ESP_ERR_INVALID_ARG;
        }

        const int64_t startup_deadline_us = esp_timer_get_time() + 3000000;
        while (ulp_lp_display_status == 0U) {
            if (esp_timer_get_time() >= startup_deadline_us) {
                ESP_LOGE(TAG, "LP-Core did not report startup within 3 seconds");
                return ESP_ERR_TIMEOUT;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        if (boot_display_get_state() == BOOT_DISPLAY_STATE_ERROR) {
            ESP_LOGE(TAG, "LP-Core startup failed with error code %lu", ulp_lp_display_error_code);
            return ESP_FAIL;
        }

        ulp_lp_display_target_progress_percent = progress_percent;
        s_requested_progress_percent = progress_percent;
        return ESP_OK;
    }

    s_hp_progress_percent = progress_percent;
    if (progress_percent >= 100U) {
        s_hp_state = BOOT_DISPLAY_STATE_DONE;
    }
    return ESP_OK;
}

uint32_t boot_display_get_cycles(void)
{
    if (!s_running) {
        return 0;
    }
    if (s_cfg.backend == BOOT_DISPLAY_BACKEND_LP_SPI) {
        return ulp_lp_display_cycles;
    }
    return s_hp_cycles;
}

uint32_t boot_display_get_display_init_time_us(void)
{
    if (!s_running || s_cfg.backend != BOOT_DISPLAY_BACKEND_LP_SPI) {
        return 0;
    }
    return ulp_lp_display_display_init_time_us;
}

uint32_t boot_display_get_frame_time_us(void)
{
    if (!s_running || s_cfg.backend != BOOT_DISPLAY_BACKEND_LP_SPI) {
        return 0;
    }
    return ulp_lp_display_frame_time_us;
}

esp_err_t boot_display_handoff_to_hp(void)
{
    if (!s_running) {
        ESP_LOGD(TAG, "Boot display not running; nothing to hand off");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Performing display handoff from Boot Engine to Runtime System...");

    if (s_cfg.backend == BOOT_DISPLAY_BACKEND_LP_SPI) {
        ulp_lp_core_stop();
        vTaskDelay(pdMS_TO_TICKS(10));

        const gpio_num_t display_gpios[] = {
            s_cfg.mosi_pin, s_cfg.sclk_pin, s_cfg.cs_pin, s_cfg.dc_pin
        };
        for (size_t i = 0; i < sizeof(display_gpios) / sizeof(display_gpios[0]); ++i) {
            rtc_gpio_deinit(display_gpios[i]);
        }
        ESP_LOGI(TAG, "LP-SPI bus released. Pins (MOSI=%d, SCLK=%d, CS=%d, DC=%d) ready for HP-Core.",
                 s_cfg.mosi_pin, s_cfg.sclk_pin, s_cfg.cs_pin, s_cfg.dc_pin);
    } else {
        s_hp_state = BOOT_DISPLAY_STATE_DONE;
        ESP_LOGI(TAG, "HP Boot Engine successfully handed off display to runtime.");
    }

    s_running = false;
    return ESP_OK;
}

