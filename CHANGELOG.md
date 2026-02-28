# Changelog

All notable changes to SAQ Air Quality will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.6.1] - 2026-06-22

### Added
- SAQ-4: clap detection enabled with tuned parameters
- CI/CD pipeline (Particle cloud compile, cppcheck)

### Changed
- Skip PM2.5 screen repaint when display value is unchanged
- Documentation files moved to `docs/` directory

## [0.6.0] - 2026-02-28

### Added
- `FONT_OPTIMIZATION_GUIDE.md` with subsetting procedure
- Font subset `SANSSERIF_24_NUM` (digits-only, saves ~5.8 KB flash)
- `GLCDFONT` made optional via `ENABLE_GLCDFONT` guard (saves ~3.1 KB)
- `README.md` with complete MQTT API reference

### Changed
- Buzzer code wrapped with `#ifdef ENABLE_BUZZER` (buzzer.h/.cpp, NonBlockingRtttl, handler)
- Goertzel whistle code wrapped with `#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL`
- MQTT topic defines guarded per feature (BUZZER, NEOPIXEL, WHISTLE)
- HA `clear_discovery_all()` refactored to table-driven loop
- Log strings shortened in comms.cpp (~120 bytes saved)
- Total flash savings: ~13 KB (resolved SAQ-2 APP_FLASH overflow of 68 bytes)

### Fixed
- HA discovery truncation (MQTT_OUT_BUFFER 512 → 640)
- dBFS gain compensation (`_max_signal_amplitude` formula)
- PMS state feedback: LCD parentheses + MQTT retained JSON
- SAQ-2 clap detection tuning (REL_GATE_DB=15, TRIGGER_MIN_DB=-15, DEBOUNCE_MS=500)

## [0.5.0] - 2026-02-23

### Added
- CCS811 baseline save/restore in EEPROM (persistence v5)
- PMS sensor duty-cycle state machine (sleep/awake via MQTT)
- `ENABLE_AQI` compile directive
- Firmware version tracking (`DEVICE_SOFTWARE_VERSION` + `BUILD_TIMESTAMP`)

### Changed
- Persistence module refactored with FNV-1a checksum and versioned header
- All dynamic allocations replaced with static buffers and `EmaF` (zero heap)

### Fixed
- HA cleanup: removed conditions on cleanup command
- Multiple bug fixes in persistence and sensor modules

## [0.4.0] - 2026-02-21

### Added
- Noise floor calibration for audio pipeline
- `PERSISTENCE_GUIDE.md`

### Changed
- SoundMeter and ClapDetector optimized (IIR filters, clap FSM)
- HA discovery improved
- Global optimization pass on audio detection pipeline

## [0.3.0] - 2025-11-24

### Added
- Home Assistant MQTT auto-discovery (sensors, commands)
- MQTT command router with handler table (`comms_router.cpp`)
- Command to refresh/clear retained MQTT topics
- JSON builder (zero-allocation, `COMMS::JSON` namespace)
- New cooperative task scheduler with profiler
- RSSI display on screen

### Changed
- MQTT loop redesigned
- Screen update split for better task performance
- SAQ-3 support without Neopixel
- JSON parser switched to Particle API
- Neopixel and TimerInterval libraries bumped to latest

### Fixed
- MQTT reconnection stability (resolved connect/disconnect loop)
- Gas sensor reading reimplemented
- Persistence integrated with test endpoints for offsets
- Multiple compilation warnings removed

## [0.2.0] - 2025-11-04

### Added
- AQI Calculator updated to EPA 2025 model
- Logging and debug macros (`LOG_DBG`, `LOG_INFO`, etc.)
- MQTT input command handling

### Changed
- Device notification improved on compilation
- MQTT command input restructured
- Global refactor: logging, JSON output, debug

## [0.1.0] - 2018-06-18

### Added
- PMS particle sensor library (compatible with PMS5003/5003ST/7003)
- CO2/TVOC sensor (CCS811) with OLED display
- Clap detection (16 kHz sampling, 512-sample buffers, smoothed Z-score)
- Whistle detection (Goertzel algorithm)
- Neopixel LED strip with NeoPatterns effects
- MQTT-TLS communication
- Multi-device config system (`config_device_1..4.h`)
- OLED display with sensor readings
- WiFi signal strength display

### Fixed
- Audio library pointer bug (object pointer incremented instead of counter)
- FastLED incompatibility — switched to Adafruit Neopixel

[0.6.1]: https://github.com/mdps/saq-air-quality/compare/v0.6.0...v0.6.1
[0.6.0]: https://github.com/mdps/saq-air-quality/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/mdps/saq-air-quality/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/mdps/saq-air-quality/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/mdps/saq-air-quality/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/mdps/saq-air-quality/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/mdps/saq-air-quality/releases/v0.1.0
