/**
 * @file lp_boot_display.c
 * @brief Backward compatibility forwarding to universal boot_display engine
 */

#include "lp_boot_display.h"
#include <string.h>

esp_err_t lp_boot_display_start(const lp_boot_display_config_t *legacy_cfg)
{
    boot_display_config_t univ_cfg;
    memset(&univ_cfg, 0, sizeof(univ_cfg));

    univ_cfg.backend = BOOT_DISPLAY_BACKEND_LP_SPI;
    univ_cfg.geometry.width = 240;
    univ_cfg.geometry.height = 240;
    univ_cfg.geometry.is_circular = true;
    univ_cfg.colors.bg_color = 0x0000;
    univ_cfg.colors.ring_color = 0x073F;
    univ_cfg.colors.body_color = 0xFFFF;
    univ_cfg.colors.accent_color = 0x073F;

    if (legacy_cfg) {
        univ_cfg.mosi_pin = legacy_cfg->mosi_pin;
        univ_cfg.sclk_pin = legacy_cfg->sclk_pin;
        univ_cfg.cs_pin   = legacy_cfg->cs_pin;
        univ_cfg.dc_pin   = legacy_cfg->dc_pin;
        univ_cfg.rst_pin  = legacy_cfg->rst_pin;
        univ_cfg.spi_clock_hz = legacy_cfg->spi_clock_hz;
        univ_cfg.mode     = (boot_display_mode_t)legacy_cfg->mode;
    }

    return boot_display_start(&univ_cfg);
}
