# LP-Core Boot Display - Console Developer Example

Interactive console for testing and debugging the LP-Core autonomous boot animation on ESP32-P4 with GC9A01 circular display.

## Features

- **Autonomous LP-Core Animation**: Autonomous boot splash runs on LP-Core while HP-Cores initialize
- **Event-synchronized finish**: The completed wordmark remains visible while a moving highlight pulses around the RGB ring
- **Clean panel startup**: The display RAM is filled with black before the panel is enabled
- **Partial-frame rendering**: Only the wordmark/mascot area and changed ring segments are refreshed during animation
- **Real boot progress**: Event-driven mode advances only when the HP-Core reports completed initialization work
- **Interactive Console**: Test different boot modes and configurations
- **Real-time Monitoring**: Watch boot progress, cycles, and state transitions
- **Render timing**: The `info` command reports display initialization and longest animated-frame render times
- **Mode Testing**: Test CONTINUOUS, STATIC_FRAME, SINGLE_CYCLE, and EVENT_DRIVEN modes
- **Clean Handoff**: Seamless transition from LP-Core to HP-Core display control

## Hardware Requirements

- ESP32-P4 development board (Waveshare ESP32-P4-WIFI6 Rev 1.3 recommended)
- GC9A01 240x240 circular display (SPI)

## Wiring

### Display (GC9A01) - LP-Core Pins

| Display Pin | ESP32-P4 GPIO | Function | Notes |
|-------------|---------------|----------|-------|
| MOSI        | GPIO 2        | LP-SPI MOSI | Must be LP-capable |
| SCLK        | GPIO 3        | LP-SPI SCLK | Must be LP-capable |
| CS          | GPIO 4        | LP-GPIO CS  | Must be LP-capable |
| DC          | GPIO 5        | LP-GPIO DC  | Must be LP-capable |
| RST         | GPIO 49       | HP-GPIO RST | HP-Core controlled |
| VCC         | 3.3V          | Power | - |
| GND         | GND           | Ground | - |

**Important**: GPIO 2-5 must be LP-capable (RTC domain) for LP-Core control.

## Building and Flashing

```bash
cd examples/ws-esp32-p4-wifi6-gc9a01-console-boot-display-dev

# Configure display pins and SPI clock
idf.py menuconfig
# Navigate to: LP-Core Boot Display Configuration

# Build
idf.py build

# Build and flash to COM4 with the Windows helper
esp-build.bat all

# Or run the steps separately
esp-build.bat build
esp-build.bat flash
esp-build.bat monitor
```

The helper activates the installed ESP-IDF environment, writes output to
`.logs/esp-build.log`, and stops if a build or flash step fails.

## Console Commands

### System Information
```
info                          Show system status and boot animation state
```

### Boot Animation Control
```
mode <0-3>                    Show/select a boot animation mode
                              0 = CONTINUOUS (loop until HP handoff)
                              1 = STATIC_FRAME (show 50% frame and idle)
                              2 = SINGLE_CYCLE (play once 0->100%, then done)
                              3 = EVENT_DRIVEN (HP-Core reports completed milestones)

handoff                       Manually trigger LP->HP display handoff
restart                       Restart LP-Core boot animation
```

### Event-driven progress

For product firmware, select `LP_BOOT_MODE_EVENT_DRIVEN` and report progress only
when real initialization milestones complete. The example uses NVS completion
and application initialization completion as milestones; replace these calls
with the actual stages in the product:

```c
config.mode = LP_BOOT_MODE_EVENT_DRIVEN;
ESP_ERROR_CHECK(lp_boot_display_start(&config));

// Call after a real milestone, using progress weights for the actual boot tasks.
ESP_ERROR_CHECK(lp_boot_display_set_progress_percent(20));
// ...initialize the remaining services...
ESP_ERROR_CHECK(lp_boot_display_set_progress_percent(100));

while (lp_boot_display_get_state() == LP_BOOT_STATE_ANIMATING) {
    vTaskDelay(pdMS_TO_TICKS(20));
}
```

Progress reports must be monotonic and between 0 and 100. The LP-Core does not
advance the ring while the HP-Core is working; it keeps the current progress
visible and renders the finished frame after the 100% report.
The console's `progress <0-100>` form is a developer test control; it is not a
substitute for reporting completed initialization work in product firmware.

### Monitoring
```
progress [<0-100>]            Show progress or report a manual milestone
state                         Show LP-Core state (IDLE, ANIMATING, STATIC, DONE, ERROR)
cycles                        Show number of completed animation cycles
```

### HP-Core Display Testing
```
hp_init                       Initialize HP-Core display driver (after handoff)
hp_test                       Test HP-Core display control (after handoff)
```

## Boot Animation Explained

The boot animation features an animated pixel mascot that:

1. **Sleepy Start** (0-16%): Mascot appears with sleepy eyes
2. **Loading Phase** (17-99%): Mascot eyes track the active ring segment; the ring holds while real initialization work is in progress
3. **Happy Finish** (100%): Mascot smiles after the HP-Core reports completion

### Emotions & Randomization

- **Randomized per boot**: Blink timing and bob offset pattern
- **Eye tracking**: Mascot pupils follow the active ring segment
- **Blinking**: Periodic blink while waiting for the next progress event
- **Bob animation**: Smooth vertical floating motion (±2 pixels)

### Ring Visualization

- **48 segments**: Full circle progress indicator
- **Dot-based**: 6x6 pixel dots at ring radius 112px
- **Color coding**: 
  - White during boot
  - Green at 100% completion

## Usage Examples

### Basic Boot Test

```bash
boot-dev> info
LP-Core State: ANIMATING
Progress: Frame 8/12 (66%)
Cycles: 0
Mode: CONTINUOUS

boot-dev> progress
Frame: 8/12 (66%)

boot-dev> state
State: ANIMATING
```

### Mode Testing

```bash
# Test static frame mode
boot-dev> mode 1
Mode set to: STATIC_FRAME
LP-Core will show single 50% frame

boot-dev> restart
LP-Core restarting with new mode...
LP-Core boot animation successfully launched

# Wait and check
boot-dev> state
State: STATIC

# Test single-cycle mode
boot-dev> mode 2
Mode set to: SINGLE_CYCLE

boot-dev> restart
LP-Core restarting with new mode...

# Wait for completion
boot-dev> state
State: DONE

boot-dev> cycles
Cycles: 1
```

### HP-Core Handoff Test

```bash
# Let boot animation run
boot-dev> info
LP-Core State: ANIMATING
Progress: Frame 11/12 (91%)

# Trigger handoff
boot-dev> handoff
Handing off display from LP-Core to HP-Core...
Display handoff complete. Pins released for HP-Core.

boot-dev> state
State: IDLE

# Initialize HP-Core display
boot-dev> hp_init
Initializing HP-Core display driver...
HP-Core display initialized successfully

boot-dev> hp_test
Testing HP-Core display...
Drawing test pattern...
HP-Core display test complete
```

## Development Workflow

### 1. Test Default Boot Behavior
```bash
# Flash with default CONTINUOUS mode
idf.py -p COM4 flash monitor

# Observe: Mascot animates continuously
# Check: Boot logs show LP-Core launch
# Verify: Animation loops indefinitely
```

### 2. Test Static Frame Mode
```bash
boot-dev> mode 1
boot-dev> restart

# Verify: Mascot shows at 50% position
# Verify: No animation, static display
# Check: State reports STATIC
```

### 3. Test Single-Cycle Mode
```bash
boot-dev> mode 2
boot-dev> restart

# Observe: Animation plays once (0->100%)
# Verify: Mascot ends with happy face
# Check: State changes to DONE
# Verify: No further animation
```

### 4. Test HP-Core Handoff
```bash
# Start with default mode
boot-dev> handoff

# Verify: Console reports handoff complete
# Verify: Pins released message
# Check: LP-Core state is IDLE

boot-dev> hp_init
boot-dev> hp_test

# Verify: HP-Core can control display
# Check: Test pattern renders correctly
```

### 5. Performance Monitoring
```bash
# Monitor during boot
boot-dev> progress
Frame: 3/12 (25%)

boot-dev> cycles
Cycles: 0

# After first loop
boot-dev> cycles
Cycles: 1

# Check memory usage in build logs
# LP-Core binary should be < 10 KB
# LP-SRAM usage should be < 16 KB
```

## Troubleshooting

**LP-Core not starting:**
- Check LP-capable pins (GPIO 2-5 for SPI)
- Verify Kconfig: `CONFIG_ULP_COPROC_ENABLED=y`
- Verify Kconfig: `CONFIG_ULP_COPROC_TYPE_LP_CORE=y`
- Check reserved memory: `CONFIG_ULP_COPROC_RESERVE_MEM=32768`

**Display not showing animation:**
- Verify GC9A01 wiring (especially CS, DC, RST)
- Check SPI clock speed (4 MHz for LP-Core)
- Enable debug logs: `idf.py menuconfig` -> Component config -> Log output

**Handoff fails:**
- Ensure LP-Core stopped before HP init
- Check RTC GPIO deinit completed
- Verify pins are released (check console output)

**Animation looks wrong:**
- Verify display orientation (should be 0x48 in init sequence)
- Check pixel format (RGB565)
- Verify ring radius and segments constants

## Performance Characteristics

- **LP-Core Clock**: Typically 20 MHz
- **SPI Clock**: 4 MHz (LP-SPI Master)
- **Frame Rate**: ~10 FPS (100ms frame delay)
- **Frame Render Time**: ~300-400ms per full frame
- **Binary Size**: ~8-10 KB (ULP firmware)
- **LP-SRAM Usage**: ~4-6 KB (runtime)
- **HP-Core CPU Load**: 0% (runs on LP-Core)

## Technical Details

### Pixel Mascot Character

- **Dimensions**: 16x42 pixels (@ 2x scale = 32x84 px)
- **Position**: Centered, with vertical bob animation
- **Eyes**: 6x5 pixels, with gaze tracking
- **Mouth**: Dynamic based on emotion
- **Feet**: Alternating animation pattern

### Ring Geometry

- **Segments**: 48 dots around circle
- **Radius**: 112 pixels from center
- **Dot Size**: 6x6 pixels
- **Angle Step**: 7.5° per segment
- **Rotation**: Fixed-point math (cosine/sine lookup)

### Animation States

```c
typedef enum {
    SPLASH_MOOD_SLEEPY = 0,   // Eyes half-closed
    SPLASH_MOOD_LOADING = 1,  // Eyes track ring
    SPLASH_MOOD_WORRIED = 2,  // Eyes wide, O-mouth
    SPLASH_MOOD_HAPPY = 3,    // Smile, crescent eyes
} splash_mood_t;
```

### Memory Map

```
LP-SRAM Layout (32 KB reserved):
├─ ULP Binary:     ~8-10 KB
├─ Stack:          ~2 KB
├─ Globals:        ~4 KB
│  ├─ tx_words[15]
│  ├─ pixel_words[PIXELS_PER_TRANSFER/2]
│  ├─ ring_x[48]
│  └─ ring_y[48]
└─ Shared Vars:    ~24 bytes
   ├─ lp_display_progress
   ├─ lp_display_cycles
   ├─ lp_display_status
   ├─ lp_display_error_code
   ├─ lp_display_mode
   └─ lp_display_idle_ticks
```

## Related Documentation

- [Component README](../../README.md) - Main component documentation
- [API Reference](../../include/lp_boot_display.h) - Public API
- [ULP Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-guides/ulp_lp_core.html)

## License

PolyForm Noncommercial License 1.0.0 - Copyright (c) 2026 Strg-Alt-Entf-0x00
