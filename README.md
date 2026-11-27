# esp-idf-boot-display

[![License: PolyForm Noncommercial 1.0.0](https://img.shields.io/badge/License-PolyForm%20Noncommercial-blue.svg)](LICENSE)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.0%2B%20%7C%20v6.0%2B-red.svg)](https://github.com/espressif/esp-idf)
[![Target](https://img.shields.io/badge/Targets-ESP32--P4%20%7C%20ESP32--S3%20%7C%20ESP32--C6-green.svg)](https://www.espressif.com/)

High-performance, ultra-low latency boot splash and animation engine for **ESP32-P4**, **ESP32-S3**, and **ESP32-C6**.  
Features autonomous **LP-Core Boot Animations (<15 ms cold start)** on circular SPI displays (GC9A01 / ST7789), high-speed **HP-Core SPI with DMA**, and **HP-Core MIPI-DSI** (800×480 / 1024×600) with seamless runtime handoff to LVGL or custom applications.

---

## 1. System Architecture & Timeline

Traditional MCU splash screens wait until the RTOS kernel, Wi-Fi stack, NVS flash, and large PSRAM heaps finish initializing before rendering their first frame (often taking 800–2500 ms).  
`esp-idf-boot-display` splits the startup pipeline into two execution domains:

```
Power-On / Reset (t = 0 ms)
   |
   +---> [LP-Core Domain] (ULP RISC-V @ 40 MHz)
   |        |
   |        +-- t < 15 ms: Initialize LP-SPI, configure GC9A01 LCD, clear display RAM
   |        +-- Render autonomous 25-30 FPS splash animation (progress ring, mascot, wordmark)
   |        +-- Interactive gaze tracking and celebratory rainbow ring
   |
   +---> [HP-Core Domain] (Dual RISC-V / Xtensa @ 400 MHz)
            |
            +-- Bootloader -> ROM -> FreeRTOS Kernel Init
            +-- Initialize NVS Flash, PSRAM heaps, Wi-Fi / Bluetooth
            +-- Dispatch milestone updates via shared LP/HP mailbox
            +-- Initialization Complete -> Request Display Handoff
                    |
                    v
   [Seamless Handoff] -> LP-Core completes animation frame & releases SPI bus -> Runtime UI (LVGL/App) takes control
```

---

## 2. Key Features

- **Autonomous LP-Core Engine (<15 ms):** Executes on the ESP32-P4 / C6 Low-Power RISC-V core immediately at power-on. Eliminates initial display noise/glitches and gives instant feedback.
- **Multi-Backend Architecture:**
  - `BOOT_DISPLAY_BACKEND_LP_SPI`: Ultra-fast autonomous LP-Core SPI (GC9A01/ST7789 on LP GPIOs).
  - `BOOT_DISPLAY_BACKEND_HP_SPI`: Standard high-speed SPI2_HOST with DMA transfers up to 80 MHz (universal for ESP32-S3, P4, C6).
  - `BOOT_DISPLAY_BACKEND_HP_MIPI_DSI`: High-resolution centered splash rendering on MIPI-DSI LCD panels (4.3" / 7").
- **Jitter-Free Framerate:** Dynamic frame-time compensation maintains constant 25–30 FPS regardless of dirty-rectangle size or SPI transfer variance.
- **Celebratory Rainbow Ring Finish:** At the completion threshold (configurable, default 85%), the outer ring transitions from static cyan to a vibrant rotating rainbow pulse with celebratory mascot mood expressions.
- **Modular Artwork & Themes:**
  - `BOOT_DISPLAY_THEME_PIXEL_MASCOT`: Animated pixel mascot with eye gaze tracking, blinking, and letter-by-letter spring sliding wordmark.
  - `BOOT_DISPLAY_THEME_ESP32_CORE`: Modern high-tech geometric microchip with pulsing core icon.
  - `BOOT_DISPLAY_THEME_CUSTOM`: Custom user bitmap art exported directly from the Web Studio.
- **Interactive Zero-Install Web Studio:** Local HTML, CSS, and JavaScript app (`tools-web/index.html`) for live browser previews, color palette adjustments, and one-click C-header generation.
- **Clean Hardware Handoff:** Releases SPI bus lines and display control gracefully to LVGL, runtime drivers, or secondary tasks.

---

## 3. Add to an ESP-IDF Project

### 3.1 Via Component Manager (`idf_component.yml`)

Add `esp-idf-boot-display` to your project's `main/idf_component.yml`:

```yaml
dependencies:
  esp-idf-boot-display:
    git: https://github.com/Strg-Alt-Entf-0x00/esp-idf-boot-display.git
```

### 3.2 Direct Component Registration

Declare the component dependency in your application's `CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp-idf-boot-display
)
```

---

## 4. Basic Usage

```c
#include "esp_log.h"
#include "nvs_flash.h"
#include "boot_display.h"

static const char *TAG = "app_main";

void app_main(void)
{
    // 1. Launch boot display engine immediately (uses Kconfig defaults)
    esp_err_t err = boot_display_start(NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Boot display start failed: %s", esp_err_to_name(err));
    }

    // 2. Heavy HP-Core initializations...
    ESP_LOGI(TAG, "Initializing storage and networking...");
    nvs_flash_init();
    boot_display_set_progress_percent(40);

    // Initialize audio pipeline, Wi-Fi, sensors...
    boot_display_set_progress_percent(85); // Triggers celebratory rainbow ring

    // Finalize initialization
    boot_display_set_progress_percent(100);

    // 3. Graceful handoff to runtime UI
    // Releases pins, stops animation, yields display bus to LVGL / display driver
    boot_display_handoff_to_hp();
    ESP_LOGI(TAG, "Boot complete. Runtime UI active.");
}
```

---

## 5. Boot Display Studio Designer & Web Serial (`tools-web/index.html`)

The modular, zero-install browser Studio is included in `tools-web/index.html`,
with local CSS and JavaScript assets under `tools-web/css/` and `tools-web/js/`:

- **Interactive Visual Baukasten:** Drag & drop layout elements (Progress Ring, Pixel Mascot, Tech Core, 5×7 Wordmark, and Custom Logo) directly on the circular 240×240 display with magnetic snapping to center ($X = 120, Y = 120$).
- **Live Web Serial Hardware Sync:** Connect directly to your ESP32's USB-C COM port (115200 baud) in Google Chrome or Microsoft Edge. Scrubbing progress in the browser updates the physical GC9A01 LCD in real time, and you can test reboots, handoffs, and query measured LP-Core boot times.
- **Custom Image & Logo Converter:** Drag & drop any PNG, JPG, or SVG image to convert it via thresholding into a compact 1-bit C bitmap array (`custom_logo_rows[]`) for zero-code integration.
- **Color & Timing Customizer:** Tweak RGB565 color hexes, rainbow celebration thresholds, and hold durations in real time.
- **C Header Exporter:** Click *Copy to Clipboard* or *Download `boot_logo_custom.h`* to generate production-ready firmware headers.

The canvas preview and disconnected command responses use local emulation;
simulated state and timing output are not measurements from a device. Physical
display updates and device diagnostics require a connected ESP32 over Web
Serial, supported in Chrome or Edge. The local designer can be opened by
double-clicking `tools-web/index.html`; no server or Node.js is required.

---

## 6. Kconfig Options Reference

Run `idf.py menuconfig` -> **Boot Display Configuration**:

| Kconfig Symbol | Default | Description |
|:---|:---|:---|
| `CONFIG_BOOT_DISPLAY_BACKEND` | `LP_SPI` | Selects engine: `LP_SPI` (autonomous LP-Core), `HP_SPI` (DMA), or `HP_MIPI_DSI` |
| `CONFIG_BOOT_DISPLAY_THEME` | `PIXEL_MASCOT` | Visual theme: `PIXEL_MASCOT`, `ESP32_CORE`, or `CUSTOM` |
| `CONFIG_BOOT_DISPLAY_HAPPY_THRESHOLD_PERCENT` | `85` | Progress percentage at which celebratory rainbow ring activates |
| `CONFIG_BOOT_DISPLAY_COMPLETION_HOLD_FRAMES` | `15` | Frames (~600 ms) to celebrate at 100% before signaling DONE |
| `CONFIG_BOOT_DISPLAY_WIDTH` | `240` | Horizontal panel resolution in pixels |
| `CONFIG_BOOT_DISPLAY_HEIGHT` | `240` | Vertical panel resolution in pixels |
| `CONFIG_BOOT_DISPLAY_COLOR_BG` | `0x0000` | Background color in RGB565 |
| `CONFIG_BOOT_DISPLAY_COLOR_RING` | `0x03EF` | Progress ring indicator color in RGB565 |
| `CONFIG_BOOT_DISPLAY_COLOR_MASCOT` | `0xFFFF` | Mascot body fill color in RGB565 |
| `CONFIG_BOOT_DISPLAY_COLOR_ACCENT` | `0x03EF` | Eye and accent details color in RGB565 |
| `CONFIG_BOOT_DISPLAY_MOSI_GPIO` | `2` | SPI MOSI / SDA GPIO |
| `CONFIG_BOOT_DISPLAY_SCLK_GPIO` | `3` | SPI SCLK GPIO |
| `CONFIG_BOOT_DISPLAY_CS_GPIO` | `4` | SPI CS GPIO |
| `CONFIG_BOOT_DISPLAY_DC_GPIO` | `5` | SPI DC / RS GPIO |
| `CONFIG_BOOT_DISPLAY_RST_GPIO` | `49` | Hardware Reset GPIO |

---

## 7. Board Examples

The [Waveshare ESP32-P4-WiFi6 + GC9A01 Example](examples/ws-esp32-p4-wifi6-gc9a01-console-boot-display-dev/README.md) documents board wiring, interactive console commands, and board-specific build and flash steps.

---

## 8. License & Commercial Inquiries

This component is distributed under the **PolyForm Noncommercial License 1.0.0**.

- **Free Use:** You are free to run, modify, and distribute this software for personal, hobbyist, educational, and noncommercial research purposes.
- **Commercial Use:** Commercial use is not permitted under this license. For commercial licensing terms, please contact the author directly via GitHub:  
  **GitHub:** [https://github.com/Strg-Alt-Entf-0x00](https://github.com/Strg-Alt-Entf-0x00)

---

## License

This project is licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE).
- **Free for noncommercial use**: Personal, educational, research, and hobbyist projects are completely free.
- **Commercial use**: Not permitted without prior authorization. For commercial inquiries, please contact the author via GitHub.
