#pragma once

#include "boot_display_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Pulse reset pin to perform a hardware reset of the display panel.
 *
 * @param rst_pin GPIO pin connected to LCD RST line (e.g. GPIO 49).
 * @return ESP_OK on success, or error code.
 */
esp_err_t boot_display_reset(gpio_num_t rst_pin);

/**
 * @brief Initialize and launch the boot display animation.
 *
 * Depending on Kconfig and config->backend, this launches:
 * - LP-Core Autonomous SPI Engine (<15ms instant startup, runs in parallel to HP boot)
 * - HP-Core MIPI-DSI Engine (Runs on dual-core RISC-V with DSI PHY for 4.3"/7" panels)
 * - HP-Core High-Speed SPI Engine (DMA @ 40-80 MHz)
 *
 * @param config Pointer to configuration struct. If NULL, Kconfig hardware defaults are used.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t boot_display_start(const boot_display_config_t *config);

/**
 * @brief Get the current state of the boot display animation.
 *
 * @return Current boot_display_state_t.
 */
boot_display_state_t boot_display_get_state(void);

/**
 * @brief Get the latest progress percentage rendered by the display.
 *
 * @return Progress from 0 to 100.
 */
uint32_t boot_display_get_progress_percent(void);

/**
 * @brief Report a real boot milestone to an event-driven animation.
 *
 * Progress must be monotonic and between 0 and 100. Call this from HP-Core
 * task context during boot initialization.
 *
 * @param progress_percent Completed boot progress from 0 to 100.
 * @return ESP_OK on success, or error code.
 */
esp_err_t boot_display_set_progress_percent(uint32_t progress_percent);

/**
 * @brief Get the number of completed full animation cycles.
 *
 * @return Cycle count (0 = currently on first run).
 */
uint32_t boot_display_get_cycles(void);

/**
 * @brief Get measured display initialization and longest animated-frame render times.
 */
uint32_t boot_display_get_display_init_time_us(void);
uint32_t boot_display_get_frame_time_us(void);

/**
 * @brief Stop boot display engine and release bus/pins for runtime UI takeover.
 *
 * For LP-Core: Safely shuts down LP-Core display controller, frees LP-SPI, and resets
 * RTC pin mappings so HP-Core can take over the display without bus conflicts.
 * For HP-Core: Finishes rendering and yields display handle to the runtime UI engine.
 *
 * @return ESP_OK on successful handoff.
 */
esp_err_t boot_display_handoff_to_hp(void);

/* =========================================================================
 * 100% Backward Compatibility Declarations for legacy lp_boot_display_* calls
 * ========================================================================= */
esp_err_t lp_boot_display_start(const lp_boot_display_config_t *config);

static inline esp_err_t lp_boot_display_reset(gpio_num_t rst_pin) {
    return boot_display_reset(rst_pin);
}

static inline lp_boot_state_t lp_boot_display_get_state(void) {
    return boot_display_get_state();
}

static inline uint32_t lp_boot_display_get_progress(void) {
    return boot_display_get_progress_percent() * 12 / 100;
}

static inline uint32_t lp_boot_display_get_progress_percent(void) {
    return boot_display_get_progress_percent();
}

static inline esp_err_t lp_boot_display_set_progress_percent(uint32_t progress_percent) {
    return boot_display_set_progress_percent(progress_percent);
}

static inline uint32_t lp_boot_display_get_cycles(void) {
    return boot_display_get_cycles();
}

static inline uint32_t lp_boot_display_get_display_init_time_us(void) {
    return boot_display_get_display_init_time_us();
}

static inline uint32_t lp_boot_display_get_frame_time_us(void) {
    return boot_display_get_frame_time_us();
}

static inline esp_err_t lp_boot_display_handoff_to_hp(void) {
    return boot_display_handoff_to_hp();
}

#ifdef __cplusplus
}
#endif

