# 📋 Changelog

All notable changes to the ZX Spectrum Emulator for M5Stack Cardputer will be documented in this file.

---

## [v3.137] - 2025-10-26

### 🎉 First Public Release!

This is the initial public release of the ZX Spectrum emulator for M5Stack Cardputer, ported from the excellent atomic14/esp32-zxspectrum project.

### ✨ Features

- **Full Z80 CPU emulation** @ 3.5 MHz with accurate timing
- **Display rendering** - 256×192 with zoom (x1.0, x1.5, x2.0, x2.5) and pan
- **Beeper sound system** - I2S output with volume control (0-10 levels)
- **File loading** - Supports .SNA, .TAP, and .Z80 formats
- **SD card support** - Load games from `/ZXgames/` folder
- **File browser** - Easy navigation with filters (.SNA/.TAP/.Z80)
- **Full keyboard emulation** - All ZX Spectrum keys mapped
- **Joystick2 Unit support** - Optional external joystick via Grove port
- **Menu system** - Press Opt+ESC to access
- **Pause/Resume** - Press TAB during gameplay
- **Information screen** - Built-in hotkey guide and credits
- **Notifications** - Visual feedback for actions (volume, zoom, etc)

### 🎮 Tested Games

The following games have been thoroughly tested and confirmed working:
- **Saboteur** (.SNA, .TAP, .Z80) ✅
- **Exolon** (.SNA, .TAP, .Z80) ✅
- **Tetris** (.SNA, .TAP, .Z80) ✅
- **Pac-Man** (.SNA, .TAP, .Z80) ✅

### ⚠️ Known Issues

- Some games may fail to load (format-specific issues)
- Some games load but don't run properly
- TAP files with turbo loaders are not supported (standard speed only)

### 🙏 Credits

- **Original ESP32 Rainbow:** atomic14/esp32-zxspectrum
- **Z80 Core:** Santiago Romero Iglesias
- **Sound improvements:** ChatGPT (GPT-4)
- **Port to Cardputer:** AndyAiCardputer
- **Development:** AI Assistant (Claude)

---

## [Future Releases]

### Planned Features

- 128K ZX Spectrum support
- TAP turbo loader support
- Save state functionality
- Screenshot capture
- More game compatibility improvements
- Settings persistence
- Kempston joystick emulation

---

**Format:** Based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)

