# 🎉 ZX Spectrum Emulator v3.137 - First Public Release!

**Release Date:** October 26, 2025

---

## 🚀 What's New

This is the **first public release** of the ZX Spectrum 48K emulator for M5Stack Cardputer!

After months of development and testing, we're excited to share this fully functional emulator with the retro gaming community.

---

## ✨ Key Features

### Core Emulation
- ✅ **Full Z80 CPU** @ 3.5 MHz with accurate timing
- ✅ **48KB RAM** + 16KB ROM (Sinclair 48K)
- ✅ **50 FPS** stable frame rate
- ✅ **Interrupt Mode 1** (IM1) at 50 Hz

### Display & Graphics
- ✅ **256×192 resolution** (authentic ZX Spectrum)
- ✅ **Zoom modes:** x1.0, x1.5, x2.0, x2.5 (`Opt+Z`)
- ✅ **Pan support:** Navigate zoomed display (`Opt + ; . , /`)
- ✅ **Pixel-Perfect mode:** 1:1 pixel scaling (`Opt+P`)
- ✅ **16 colors:** Full ZX Spectrum palette

### Audio
- ✅ **Beeper sound** via I2S @ 16 kHz
- ✅ **Volume control:** 11 levels (0-10)
- ✅ **Hotkeys:** `Opt+M` mute, `Opt++` / `Opt+-` volume

### File Support
- ✅ **.SNA format:** Instant snapshots (fastest loading)
- ✅ **.TAP format:** Tape files with authentic loading screens
- ✅ **.Z80 format:** Compressed snapshots (smaller files)
- ✅ **SD Card:** Load from `/ZXgames/` folder
- ✅ **File browser:** Easy navigation with filters

### Input
- ✅ **Full keyboard:** All ZX Spectrum keys mapped
- ✅ **Joystick2 Unit:** Optional external joystick (Grove port)
- ✅ **Joystick-to-Keys:** Map stick to QAOP+Space (`Opt+J`)

### User Interface
- ✅ **Splash screen:** Version info at boot
- ✅ **Main menu:** `Opt+ESC` to access
- ✅ **Load Game submenu:** Easy file type selection
- ✅ **Information screen:** Hotkeys guide and credits (`Opt+I`)
- ✅ **Notifications:** Visual feedback for actions
- ✅ **Pause/Resume:** Press `TAB` during gameplay

---

## 🎮 Tested & Verified Games

These games have been thoroughly tested and work perfectly:

| Game | Formats | Status |
|------|---------|--------|
| **Saboteur** | .SNA, .TAP, .Z80 | ✅ Perfect |
| **Exolon** | .SNA, .TAP, .Z80 | ✅ Perfect |
| **Tetris** | .SNA, .TAP, .Z80 | ✅ Perfect |
| **Pac-Man** | .SNA, .TAP, .Z80 | ✅ Perfect |

**Compatibility:** Most 48K games work, some have minor issues.

---

## 📦 Downloads

### Pre-built Firmware (Recommended)

**File:** `ZX_Spectrum_v137.bin` (653 KB)

**Flash with esptool:**
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x0 ZX_Spectrum_v137.bin
```

### Source Code

**Repository:** [github.com/AndyAiCardputer/zx-spectrum-cardputer](https://github.com/AndyAiCardputer/zx-spectrum-cardputer)

**Build with PlatformIO:**
```bash
git clone https://github.com/AndyAiCardputer/zx-spectrum-cardputer.git
cd zx-spectrum-cardputer
pio run --target upload
```

---

## 📋 Requirements

### Hardware
- M5Stack Cardputer
- Micro SD card (FAT32, 8GB+ recommended)
- USB-C cable (for flashing)
- Joystick2 Unit (optional)

### Software
- Python 3.7+ with esptool
- OR PlatformIO (for building from source)

### Games
- ZX Spectrum game files (.SNA, .TAP, .Z80)
- Available from [World of Spectrum](https://worldofspectrum.org/)

---

## 🚀 Quick Start

1. **Flash firmware:** `ZX_Spectrum_v137.bin`
2. **Format SD card:** FAT32
3. **Create folder:** `/ZXgames/`
4. **Copy games:** .SNA, .TAP, or .Z80 files
5. **Insert SD card** into Cardputer
6. **Press `Opt+ESC`** to open menu
7. **Select "Load Game"** and enjoy!

**Full guide:** See [INSTALL.md](INSTALL.md)

---

## ⚠️ Known Issues

### Current Limitations
- ❌ **Some games crash on load** (format-specific issues)
- ❌ **Some games load but don't run** (under investigation)
- ❌ **TAP turbo loaders unsupported** (standard speed only)
- ❌ **128K games not supported** (48K only for now)

### Workarounds
- Try different file formats (.SNA usually most reliable)
- Check [World of Spectrum](https://worldofspectrum.org/) for alternative versions
- Report issues on [GitHub](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/issues)

---

## 🛣️ Roadmap (Future Releases)

### Planned Features
- 🔄 **128K ZX Spectrum** support (memory banking + AY sound chip)
- 🔄 **TAP turbo loaders** (faster loading)
- 🔄 **Save states** (.SNA file writing)
- 🔄 **Screenshot capture** (save to SD card)
- 🔄 **Settings persistence** (remember preferences)
- 🔄 **More game compatibility** (ongoing testing and fixes)

---

## 🙏 Credits & Acknowledgments

### Project Contributors
- **Original ESP32 Rainbow:** [atomic14/esp32-zxspectrum](https://github.com/atomic14/esp32-zxspectrum) by Alvaro Alea Fernandez
- **Z80 CPU Core:** Santiago Romero Iglesias
- **Sound System Improvements:** ChatGPT (GPT-4)
- **Port to M5Stack Cardputer:** [AndyAiCardputer](https://github.com/AndyAiCardputer)
- **Development Assistance:** AI Assistant (Claude)

### Special Thanks
- **ZX Spectrum Community** - For preserving this amazing piece of computing history
- **World of Spectrum** - For maintaining the largest game archive
- **M5Stack** - For creating the Cardputer platform
- **Early Testers** - For bug reports and feedback

### Legal
- **ZX Spectrum 48K ROM:** © 1982 Sinclair Research Ltd (included for educational purposes)

---

## 📝 License

This project is licensed under the **MIT License** with GPL components.

See [LICENSE](LICENSE) for full details.

---

## 🤝 Contributing

Contributions welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Areas where help is needed:**
- Game compatibility testing
- Bug fixes
- 128K implementation
- Documentation improvements
- Turbo loader support

---

## 🐛 Reporting Bugs

Found an issue? [Open a bug report](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/issues)

Please include:
- Emulator version (v3.137)
- Game name and format
- Steps to reproduce
- Expected vs actual behavior
- Serial logs (if available)

---

## 📧 Contact

- **GitHub:** [@AndyAiCardputer](https://github.com/AndyAiCardputer)
- **Issues:** [Report bugs or request features](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/issues)

---

## 🎉 Thank You!

Thank you for trying the ZX Spectrum Emulator for M5Stack Cardputer!

We hope you enjoy playing classic games on this amazing portable device.

**Happy gaming!** 🕹️✨

---

```
╔═══════════════════════════════════════════════════════════╗
║  🎮 ZX SPECTRUM EMULATOR v3.137 FOR M5STACK CARDPUTER 🎮 ║
║                                                           ║
║  Bringing 1980s computing to 2020s hardware!             ║
║                                                           ║
║  Made with ❤️ for the retro gaming community             ║
╚═══════════════════════════════════════════════════════════╝
```

---

**Download now:** [Release v3.137](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/releases/tag/v3.137)

