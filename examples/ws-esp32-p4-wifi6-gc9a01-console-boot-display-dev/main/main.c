/**
 * @file main.c
 * @brief LP-Core Boot Display - Console Developer Example
 * 
 * Interactive console for testing LP-Core autonomous boot animation:
 * - Monitor boot progress and state
 * - Test different boot modes (CONTINUOUS, STATIC, SINGLE_CYCLE)
 * - Trigger LP->HP handoff manually
 * - Verify display control transitions
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"

#include "lp_boot_display.h"
#include "console_commands.h"

static const char *TAG = "main";

// ============================================================================
// Console Initialization
// ============================================================================

static void initialize_console(void)
{
    // Drain stdout before reconfiguring
    fflush(stdout);
    fsync(fileno(stdout));

    // Disable buffering on stdin
    setvbuf(stdin, NULL, _IONBF, 0);

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, 
                                        256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, 
                                      &uart_config));

    // Initialize console
    esp_console_config_t console_config = {
        .max_cmdline_length = 256,
        .max_cmdline_args = 8,
        .hint_color = 36,  // Cyan
        .hint_bold = false,
    };

    ESP_ERROR_CHECK(esp_console_init(&console_config));

    // Configure linenoise
    linenoiseSetMultiLine(1);
    linenoiseSetCompletionCallback(NULL);
    linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);
    linenoiseHistorySetMaxLen(100);
    linenoiseSetDumbMode(0);
}

// ============================================================================
// Main Application
// ============================================================================

void app_main(void)
{
    const lp_boot_display_config_t display_config = {
        .mosi_pin = CONFIG_LP_BOOT_DISPLAY_MOSI_GPIO,
        .sclk_pin = CONFIG_LP_BOOT_DISPLAY_SCLK_GPIO,
        .cs_pin = CONFIG_LP_BOOT_DISPLAY_CS_GPIO,
        .dc_pin = CONFIG_LP_BOOT_DISPLAY_DC_GPIO,
        .rst_pin = CONFIG_LP_BOOT_DISPLAY_RST_GPIO,
        .spi_clock_hz = CONFIG_LP_BOOT_DISPLAY_SPI_CLOCK_HZ,
        .mode = LP_BOOT_MODE_EVENT_DRIVEN,
    };

    // ========================================================================
    // Phase 0: Clear the display and start the LP-Core animation immediately
    // ========================================================================
    esp_err_t ret = lp_boot_display_start(&display_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LP-Core boot animation: %s",
                 esp_err_to_name(ret));
        ESP_LOGE(TAG, "Check: CONFIG_ULP_COPROC_ENABLED=y in sdkconfig");
        ESP_LOGE(TAG, "Check: CONFIG_ULP_COPROC_TYPE_LP_CORE=y in sdkconfig");
        ESP_LOGE(TAG, "Check: Wiring on GPIO 2-5 (LP-capable pins)");
        return;
    }

    ESP_LOGI(TAG, "LP-Core boot animation launched successfully!");
    ESP_LOGI(TAG, "LP-Core will clear display RAM before enabling the panel");

    printf("\n");
    printf("+----------------------------------------------------+\n");
    printf("| LP-Core Boot Display - Console Developer Example  |\n");
    printf("| ESP32-P4 + GC9A01 Circular Display                |\n");
    printf("+----------------------------------------------------+\n");
    printf("\n");

    // ========================================================================
    // Phase 1: Initialize NVS after the display has started
    // ========================================================================
    ESP_LOGI(TAG, "Initializing NVS...");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(lp_boot_display_set_progress_percent(20U));

    // ========================================================================
    // Phase 2: Initialize the console before reporting boot completion
    // ========================================================================
    ESP_LOGI(TAG, "Initializing console interface...");
    initialize_console();

    esp_console_register_help_command();
    register_boot_display_console_commands();

    ESP_LOGI(TAG, "HP-Core initialization complete");
    ESP_ERROR_CHECK(lp_boot_display_set_progress_percent(100U));
    while (lp_boot_display_get_state() == LP_BOOT_STATE_ANIMATING) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    lp_boot_state_t state = lp_boot_display_get_state();
    uint32_t progress = lp_boot_display_get_progress();
    uint32_t progress_percent = lp_boot_display_get_progress_percent();
    uint32_t cycles = lp_boot_display_get_cycles();
    uint32_t display_init_time_us = lp_boot_display_get_display_init_time_us();
    uint32_t frame_time_us = lp_boot_display_get_frame_time_us();

    ESP_LOGI(TAG, "LP-Core Status:");
    ESP_LOGI(TAG, "  State:    %d (1=ANIMATING, 2=ERROR, 3=STATIC, 4=DONE)", state);
    ESP_LOGI(TAG, "  Progress: %lu/12 frames (%lu%%)", progress, progress_percent);
    ESP_LOGI(TAG, "  Cycles:   %lu", cycles);
    ESP_LOGI(TAG, "LP-Core Timing Analysis:");
    ESP_LOGI(TAG, "  Display init: %lu us (GC9A01 init + black clear)",
             display_init_time_us);
    if (frame_time_us > 0U) {
        uint32_t nominal_fps_milli =
            1000000000ULL / (frame_time_us + 40000U);
        ESP_LOGI(TAG, "  Longest animated frame: %lu us", frame_time_us);
        ESP_LOGI(TAG, "  Worst-case nominal rate: %lu.%02lu FPS",
                 nominal_fps_milli / 1000U, (nominal_fps_milli % 1000U) / 10U);
    } else {
        ESP_LOGI(TAG, "  Animated frame: not measured");
    }

    // ========================================================================
    // Phase 5: Console REPL Loop
    // ========================================================================
    const char *prompt = LOG_COLOR_I "boot-dev> " LOG_RESET_COLOR;

    printf("\n");
    printf("+----------------------------------------------------+\n");
    printf("| Interactive console ready                         |\n");
    printf("| LP-Core runs the boot animation autonomously      |\n");
    printf("| Type 'help' for commands or 'info' for status      |\n");
    printf("| Type 'handoff' to release display pins             |\n");
    printf("+----------------------------------------------------+\n");
    printf("\n");

    // Main console loop
    while (true) {
        // Get a line using linenoise
        char *line = linenoise(prompt);

        if (line == NULL) {  // EOF or error
            continue;
        }

        // Add to history if non-empty
        if (strlen(line) > 0) {
            linenoiseHistoryAdd(line);
        }

        // Try to run the command
        int err;
        esp_err_t esp_err = esp_console_run(line, &err);

        if (esp_err == ESP_ERR_NOT_FOUND) {
            printf("Unknown command. Type 'help' for available commands.\n");
        } else if (esp_err == ESP_ERR_INVALID_ARG) {
            // Command was empty
        } else if (esp_err == ESP_OK && err != ESP_OK) {
            printf("Command returned non-zero error code: 0x%x (%s)\n", 
                   err, esp_err_to_name(err));
        } else if (esp_err != ESP_OK) {
            printf("Internal error: %s\n", esp_err_to_name(esp_err));
        }

        // Free line buffer
        linenoiseFree(line);
    }
}
