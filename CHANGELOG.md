# Changelog

All notable changes to the `esp-idf-boot-display` component are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.1.0] - 2026-11-22

### Added
- Dynamic frame-time compensation in LP-Core engine, eliminating frame jitter across varying dirty rectangle sizes.
- Celebratory rainbow progress ring activation when boot progress reaches completion threshold (configurable, default 85%).
- Configurable completion hold grace period before setting status DONE, ensuring celebratory animations are visible.
- Modular zero-install browser Web Studio (`tools-web/index.html`, with local
  CSS/JS assets) for circular canvas design, dynamic 5x7 ASCII wordmark
  generation, RGB565 palette conversion, C-header export, and Web Serial
  hardware control; disconnected command responses use local emulation.
- Neutral animated pixel mascot theme and dynamic ASCII wordmark (default "ESPIDF").

### Changed
- Relicensed under PolyForm Noncommercial License 1.0.0.
- Cleaned and neutralized all character and brand references.

## [2.0.0] - 2026-11-21

### Added
- Universal multi-backend architecture supporting autonomous LP-Core SPI (`BOOT_DISPLAY_BACKEND_LP_SPI`), high-speed DMA HP-Core SPI (`BOOT_DISPLAY_BACKEND_HP_SPI`), and MIPI-DSI (`BOOT_DISPLAY_BACKEND_HP_MIPI_DSI`).
- Backward-compatible wrapper API preserving compatibility with earlier single-target implementations.
- Unified configuration structures for color palettes, display geometry, and SPI bus routing.

## [1.0.0] - 2026-11-05

### Added
- Initial release of autonomous LP-Core boot animation engine for ESP32-P4 with GC9A01 240x240 circular LCD.
- Animated pixel mascot with dynamic expressions (sleepy, loading, worried, happy) and gaze tracking following progress ring.
- 48-segment circular ring progress indicator.
- Three boot modes: `CONTINUOUS`, `STATIC_FRAME`, and `SINGLE_CYCLE`.
- LP-Core to HP-Core display bus handoff mechanism.
- Kconfig integration for pin and timing configuration.
- Interactive console developer example on Waveshare ESP32-P4-WiFi6.
