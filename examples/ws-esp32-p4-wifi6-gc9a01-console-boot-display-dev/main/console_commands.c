/**
 * @file console_commands.c
 * @brief Console command implementations for LP-Core Boot Display testing
 */

#include "console_commands.h"

#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"
#include "sdkconfig.h"
#include "lp_boot_display.h"

static const char *TAG = "boot_console";
static lp_boot_mode_t s_mode = LP_BOOT_MODE_EVENT_DRIVEN;

// ============================================================================
// State String Helpers
// ============================================================================

static const char *state_to_string(lp_boot_state_t state)
{
    switch (state) {
        case LP_BOOT_STATE_IDLE:      return "IDLE";
        case LP_BOOT_STATE_ANIMATING: return "ANIMATING";
        case LP_BOOT_STATE_ERROR:     return "ERROR";
        case LP_BOOT_STATE_STATIC:    return "STATIC";
        case LP_BOOT_STATE_DONE:      return "DONE";
        default:                       return "UNKNOWN";
    }
}

static const char *mode_to_string(lp_boot_mode_t mode)
{
    switch (mode) {
        case LP_BOOT_MODE_CONTINUOUS:   return "CONTINUOUS";
        case LP_BOOT_MODE_STATIC_FRAME: return "STATIC_FRAME";
        case LP_BOOT_MODE_SINGLE_CYCLE: return "SINGLE_CYCLE";
        case LP_BOOT_MODE_EVENT_DRIVEN: return "EVENT_DRIVEN";
        default:                         return "UNKNOWN";
    }
}

// ============================================================================
// Command: info - Show system status
// ============================================================================

static int cmd_info(int argc, char **argv)
{
    lp_boot_state_t state = lp_boot_display_get_state();
    uint32_t progress = lp_boot_display_get_progress();
    uint32_t progress_percent = lp_boot_display_get_progress_percent();
    uint32_t cycles = lp_boot_display_get_cycles();

    printf("\nLP-Core Boot Display - System Status\n");
    printf("State:    %s\n", state_to_string(state));
    printf("Mode:     %s\n", mode_to_string(s_mode));
    printf("Progress: %lu/12 frames (%lu%%)\n", progress, progress_percent);
    printf("Cycles:   %lu\n", cycles);
    printf("Display init: %lu us\n",
           lp_boot_display_get_display_init_time_us());
    printf("Longest animated frame: %lu us (0 = not measured)\n",
           lp_boot_display_get_frame_time_us());
    printf("\n");

    return 0;
}

static void register_cmd_info(void)
{
    const esp_console_cmd_t cmd = {
        .command = "info",
        .help = "Show LP-Core boot display system status",
        .hint = NULL,
        .func = &cmd_info,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// ============================================================================
// Command: state - Show LP-Core state
// ============================================================================

static int cmd_state(int argc, char **argv)
{
    lp_boot_state_t state = lp_boot_display_get_state();
    printf("State: %s\n", state_to_string(state));
    return 0;
}

static void register_cmd_state(void)
{
    const esp_console_cmd_t cmd = {
        .command = "state",
        .help = "Show current LP-Core state",
        .hint = NULL,
        .func = &cmd_state,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// ============================================================================
// Command: progress - Show boot progress
// ============================================================================

static struct {
    struct arg_int *value;
    struct arg_end *end;
} progress_args;

static int cmd_progress(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&progress_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, progress_args.end, argv[0]);
        return 1;
    }

    if (progress_args.value->count > 0) {
        int value = progress_args.value->ival[0];
        if (value < 0 || value > 100) {
            printf("Error: Progress must be between 0 and 100\n");
            return 1;
        }
        esp_err_t err = lp_boot_display_set_progress_percent((uint32_t)value);
        if (err != ESP_OK) {
            printf("Error: Could not report progress: %s\n", esp_err_to_name(err));
            return 1;
        }
        printf("Reported event-driven progress: %d%%\n", value);
        return 0;
    }

    uint32_t progress = lp_boot_display_get_progress();
    uint32_t progress_percent = lp_boot_display_get_progress_percent();
    printf("Frame: %lu/12 (%lu%%)\n", progress, progress_percent);
    return 0;
}

static void register_cmd_progress(void)
{
    progress_args.value = arg_int0(NULL, NULL, "<0-100>",
                                   "Report progress in event-driven mode");
    progress_args.end = arg_end(2);
    const esp_console_cmd_t cmd = {
        .command = "progress",
        .help = "Show or report boot progress",
        .hint = NULL,
        .func = &cmd_progress,
        .argtable = &progress_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// ============================================================================
// Command: cycles - Show completed cycles
// ============================================================================

static int cmd_cycles(int argc, char **argv)
{
    uint32_t cycles = lp_boot_display_get_cycles();
    printf("Cycles: %lu\n", cycles);
    return 0;
}

static void register_cmd_cycles(void)
{
    const esp_console_cmd_t cmd = {
        .command = "cycles",
        .help = "Show number of completed animation cycles",
        .hint = NULL,
        .func = &cmd_cycles,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// ============================================================================
// Command: mode - Set boot mode
// ============================================================================

static struct {
    struct arg_int *mode;
    struct arg_end *end;
} mode_args;

static int cmd_mode(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&mode_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mode_args.end, argv[0]);
        return 1;
    }

    int requested_mode = mode_args.mode->ival[0];
    if (requested_mode < LP_BOOT_MODE_CONTINUOUS ||
        requested_mode > LP_BOOT_MODE_EVENT_DRIVEN) {
        printf("Error: Invalid mode %d (valid: 0-3)\n", requested_mode);
        return 1;
    }

    lp_boot_mode_t mode = (lp_boot_mode_t)requested_mode;
    s_mode = mode;
    printf("Mode set to: %s\n", mode_to_string(mode));
    printf("Note: Restart LP-Core for mode change to take effect\n");
    return 0;
}

static void register_cmd_mode(void)
{
    mode_args.mode = arg_int1(NULL, NULL, "<0-3>",
        "0=CONTINUOUS, 1=STATIC_FRAME, 2=SINGLE_CYCLE, 3=EVENT_DRIVEN");
    mode_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "mode",
        .help = "Set boot animation mode",
        .hint = NULL,
        .func = &cmd_mode,
        .argtable = &mode_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// ============================================================================
// Command: handoff - Trigger LP->HP handoff
// ============================================================================

static int cmd_handoff(int argc, char **argv)
{
    printf("Handing off display from LP-Core to HP-Core...\n");
    
    esp_err_t ret = lp_boot_display_handoff_to_hp();
    if (ret != ESP_OK) {
        printf("Error: Handoff failed: %s\n", esp_err_to_name(ret));
        return 1;
    }

    printf("Handoff complete. LP-Core stopped, pins released.\n");
    return 0;
}

static void register_cmd_handoff(void)
{
    const esp_console_cmd_t cmd = {
        .command = "handoff",
        .help = "Stop LP-Core and hand off display to HP-Core",
        .hint = NULL,
        .func = &cmd_handoff,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// ============================================================================
// Command: restart - Restart LP-Core
// ============================================================================

static int cmd_restart(int argc, char **argv)
{
    printf("Restarting LP-Core boot animation...\n");

    // First handoff to stop LP-Core
    esp_err_t ret = lp_boot_display_handoff_to_hp();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        printf("Error: Failed to stop LP-Core: %s\n", esp_err_to_name(ret));
        return 1;
    }

    // Small delay
    vTaskDelay(pdMS_TO_TICKS(100));

    const lp_boot_display_config_t config = {
        .mosi_pin = CONFIG_LP_BOOT_DISPLAY_MOSI_GPIO,
        .sclk_pin = CONFIG_LP_BOOT_DISPLAY_SCLK_GPIO,
        .cs_pin = CONFIG_LP_BOOT_DISPLAY_CS_GPIO,
        .dc_pin = CONFIG_LP_BOOT_DISPLAY_DC_GPIO,
        .rst_pin = CONFIG_LP_BOOT_DISPLAY_RST_GPIO,
        .spi_clock_hz = CONFIG_LP_BOOT_DISPLAY_SPI_CLOCK_HZ,
        .mode = s_mode,
    };
    ret = lp_boot_display_start(&config);
    if (ret != ESP_OK) {
        printf("Error: Failed to restart LP-Core: %s\n", esp_err_to_name(ret));
        return 1;
    }

    printf("LP-Core boot animation successfully restarted\n");
    return 0;
}

static void register_cmd_restart(void)
{
    const esp_console_cmd_t cmd = {
        .command = "restart",
        .help = "Restart LP-Core boot animation",
        .hint = NULL,
        .func = &cmd_restart,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// ============================================================================
// Command: hp_init - Initialize HP-Core display (placeholder)
// ============================================================================

static int cmd_hp_init(int argc, char **argv)
{
    printf("HP-Core display initialization placeholder\n");
    printf("TODO: Implement HP-Core SPI2_HOST @ 40 MHz init\n");
    printf("Note: This would use LovyanGFX or esp_lcd in production\n");
    return 0;
}

static void register_cmd_hp_init(void)
{
    const esp_console_cmd_t cmd = {
        .command = "hp_init",
        .help = "Initialize HP-Core display driver (after handoff)",
        .hint = NULL,
        .func = &cmd_hp_init,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// ============================================================================
// Command: hp_test - Test HP-Core display (placeholder)
// ============================================================================

static int cmd_hp_test(int argc, char **argv)
{
    printf("HP-Core display test placeholder\n");
    printf("TODO: Draw test pattern using HP-Core SPI\n");
    return 0;
}

static void register_cmd_hp_test(void)
{
    const esp_console_cmd_t cmd = {
        .command = "hp_test",
        .help = "Test HP-Core display control (after handoff)",
        .hint = NULL,
        .func = &cmd_hp_test,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// ============================================================================
// Public API: Register all commands
// ============================================================================

esp_err_t register_boot_display_console_commands(void)
{
    register_cmd_info();
    register_cmd_state();
    register_cmd_progress();
    register_cmd_cycles();
    register_cmd_mode();
    register_cmd_handoff();
    register_cmd_restart();
    register_cmd_hp_init();
    register_cmd_hp_test();

    ESP_LOGI(TAG, "Boot display console commands registered");
    return ESP_OK;
}
