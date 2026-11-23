/**
 * @file console_commands.h
 * @brief Console command registration for LP-Core Boot Display testing
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register all boot display console commands
 * 
 * Registers commands for:
 * - System information (info, state, progress, cycles)
 * - Mode control (mode, handoff, restart)
 * - HP-Core display testing (hp_init, hp_test)
 * 
 * @return ESP_OK on success
 */
esp_err_t register_boot_display_console_commands(void);

#ifdef __cplusplus
}
#endif
