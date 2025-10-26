# 🎮 ZX Spectrum Emulator for M5Stack Cardputer

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)](https://github.com/m5stack/M5Cardputer)
[![Version](https://img.shields.io/badge/version-v3.137-green)](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/releases)

Full-featured **ZX Spectrum 48K emulator** for M5Stack Cardputer, ported from the excellent [atomic14/esp32-zxspectrum](https://github.com/atomic14/esp32-zxspectrum) project.

---

## ✨ Features

- ✅ **Full Z80 CPU emulation** @ 3.5 MHz (accurate timing)
- ✅ **256×192 display** with zoom (x1.0, x1.5, x2.0, x2.5) and pan support
- ✅ **Beeper sound** with volume control (0-10 levels)
- ✅ **Load games from SD card** - supports .SNA, .TAP, and .Z80 formats
- ✅ **Full keyboard emulation** - all ZX Spectrum keys mapped to Cardputer
- ✅ **Joystick2 Unit support** - optional external joystick via Grove port
- ✅ **File browser** - easy navigation through your game collection
- ✅ **Pause/Resume** - press TAB to pause during gameplay
- ✅ **Information screen** - built-in hotkey guide and credits

---

## 🎮 Tested Games

The following games have been thoroughly tested and work perfectly:

| Game | Format | Status |
|------|--------|--------|
| **Saboteur** | ---, .TAP, .Z80 | ✅ Works perfectly |
| **Exolon** |  --- , .TAP, .Z80 | ✅ Works perfectly |
| **Tetris** | .SNA, .TAP, .Z80 | ✅ Works perfectly |
| **Pac-Man** | .SNA, .TAP, .Z80 | ✅ Works perfectly |

**Note:** Most 48K games work, some have minor issues. Compatibility is constantly improving!

---

## 📸 Screenshots

<table>
  <tr>
    <td><img src="screenshots/01_exolon.jpg" alt="Exolon" width="300"/></td>
    <td><img src="screenshots/02_andy_ai_logo.jpg" alt="Andy+AI Logo" width="300"/></td>
  </tr>
  <tr>
    <td align="center"><b>Exolon - Run and gun platformer</b></td>
    <td align="center"><b>Custom splash screen</b></td>
  </tr>
  <tr>
    <td><img src="screenshots/08_saboteur_game.jpg" alt="Saboteur" width="300"/></td>
    <td><img src="screenshots/05_exolon_large.jpg" alt="Exolon Large" width="300"/></td>
  </tr>
  <tr>
    <td align="center"><b>Saboteur - Ninja stealth action</b></td>
    <td align="center"><b>Exolon on large display</b></td>
  </tr>
</table>

*More screenshots in the [screenshots/](screenshots/) folder*

---

## 📦 Quick Start

### Option 1: Flash Pre-built Binary (Easiest!)

1. Download `ZX_Spectrum_v137.bin` from [Releases](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/releases)
2. Connect your M5Stack Cardputer via USB-C
3. Flash using [esptool](https://github.com/espressif/esptool):
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x0 ZX_Spectrum_v137.bin
```

### Option 2: Build from Source

1. Install [PlatformIO](https://platformio.org/)
2. Clone this repository:
```bash
git clone https://github.com/AndyAiCardputer/zx-spectrum-cardputer.git
cd zx-spectrum-cardputer
```
3. Build and upload:
```bash
pio run --target upload
```

### Prepare Your SD Card

1. Format a Micro SD card as **FAT32**
2. Create a folder named `ZXgames` in the root
3. Copy your ZX Spectrum game files (.SNA, .TAP, .Z80) into `/ZXgames/`
4. Insert the SD card into your Cardputer

**Done!** Press `Opt+ESC` to open the menu and load games!

---

## ⌨️ Keyboard Controls

### Basic Controls

| Key | Function |
|-----|----------|
| `A-Z`, `0-9` | Standard keys |
| `Space` | SPACE |
| `Enter` | ENTER |
| `Backspace` | DELETE (CAPS SHIFT + 0) |
| `Shift` | CAPS SHIFT |
| `Fn` | SYMBOL SHIFT |
| `ESC` | Open Menu |
| `TAB` | Pause/Resume game |

### Hotkeys (Opt + Key)

| Hotkey | Function |
|--------|----------|
| `Opt + Z` | Cycle zoom (x1.0 → x1.5 → x2.0 → x2.5) |
| `Opt + P` | Toggle Pixel-Perfect mode |
| `Opt + ; . , /` | Pan display (up, down, left, right) |
| `Opt + J` | Toggle Joystick-to-Keys mapping |
| `Opt + M` | Mute/Unmute sound |
| `Opt + +` | Increase volume |
| `Opt + -` | Decrease volume |

### Joystick2 Unit (Optional)

If you have a [Joystick2 Unit](https://docs.m5stack.com/en/unit/joystick2) connected to the Grove port:

- **Stick** = Navigation in menus
- **Button** = Select/Confirm

During gameplay (when `Opt+J` is enabled):
- **UP** → A key
- **DOWN** → Q key  
- **LEFT** → P key
- **RIGHT** → O key
- **FIRE** → SPACE

---

## 📂 File Browser

1. Press `Opt+ESC` to open the Main Menu
2. Select `1. Load Game`
3. Choose file type:
   - `1. Load .SNA` - Instant snapshots
   - `2. Load .TAP` - Tape files (with loading screen!)
   - `3. Load .Z80` - Compressed snapshots
4. Navigate with arrow keys or Joystick2
5. Press `Enter` or Button to load
6. Press `ESC` to go back

---

## 🛠️ Requirements

### Hardware

- **M5Stack Cardputer** ([Buy here](https://shop.m5stack.com/products/m5stack-cardputer-kit-w-m5stamps3))
- **Micro SD card** (formatted as FAT32, recommended 8GB+)
- **USB-C cable** for flashing firmware
- **Joystick2 Unit** (optional, for enhanced gameplay)

### Software

- **PlatformIO** (if building from source)
- **ZX Spectrum game files** - download from [World of Spectrum](https://worldofspectrum.org/)

---

## ⚠️ Known Issues

- Some games may fail to load (format-specific issues)
- Some games load but don't run properly (requires further investigation)
- TAP files with turbo loaders may not work (standard speed only)

**Workaround:** Try different file formats (.SNA, .TAP, .Z80) for problematic games. Most games have multiple versions available.

---

## 🏗️ Project Structure

```
zx-spectrum-cardputer/
├── src/
│   ├── main.cpp                 # Main emulator loop
│   ├── audio/
│   │   └── beeper.h             # Beeper sound system
│   ├── input/
│   │   └── keyboard.h           # Keyboard handling
│   ├── renderer/
│   │   └── renderer.h           # Display rendering
│   ├── spectrum/
│   │   ├── spectrum_mini.cpp    # Core emulator
│   │   ├── 48k_rom.h            # Sinclair 48K ROM
│   │   ├── tap_loader.cpp       # TAP file loader
│   │   └── z80_loader.cpp       # Z80 snapshot loader
│   └── z80/
│       ├── z80.cpp              # Z80 CPU emulator
│       └── opcodes.h            # Z80 instruction set
├── platformio.ini               # PlatformIO configuration
├── firmware/
│   └── ZX_Spectrum_v137.bin     # Pre-built binary
└── docs/
    └── ...                      # Additional documentation
```

---

## 📖 Documentation

- **[Quick Start Guide](docs/QUICKSTART.md)** - Get up and running in 5 minutes
- **[Technical Details](docs/TECHNICAL_DETAILS.md)** - How the emulator works
- **[Troubleshooting](docs/TROUBLESHOOTING.md)** - Common issues and solutions

---

## 🙏 Credits

- **Original ESP32 Rainbow project:** [atomic14/esp32-zxspectrum](https://github.com/atomic14/esp32-zxspectrum) by Alvaro Alea Fernandez
- **Z80 CPU core:** Santiago Romero Iglesias
- **M5Stack Cardputer library & sound examples:** [SeanKwok](https://github.com/SeanKwok) @ M5Stack
- **Sound system improvements:** ChatGPT (GPT-4)
- **Port to M5Stack Cardputer:** [AndyAiCardputer](https://github.com/AndyAiCardputer)
- **Development assistance:** AI Assistant (Claude)
- **ZX Spectrum 48K ROM:** © 1982 Sinclair Research Ltd

Special thanks to the ZX Spectrum community and [World of Spectrum](https://worldofspectrum.org/) for preserving this amazing piece of computing history!

---

## 📝 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

**Note:** The ZX Spectrum 48K ROM is © 1982 Sinclair Research Ltd. It is included for educational and preservation purposes only.

---

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

**Areas where help is needed:**
- Improving game compatibility
- Adding 128K support
- Implementing tape turbo loaders
- Testing on different hardware variations
- Documentation improvements

---

## 🐛 Bug Reports

Found a bug? Please [open an issue](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/issues) with:
- Emulator version
- Game name and file format
- Steps to reproduce
- Expected vs actual behavior

---

## 🌟 Show Your Support

If you find this project useful, please give it a ⭐ on GitHub!

---

## 📧 Contact

- **GitHub:** [@AndyAiCardputer](https://github.com/AndyAiCardputer)
- **Issues:** [Report bugs or request features](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/issues)

---

## 🎮 Happy Gaming!

Enjoy playing classic ZX Spectrum games on your M5Stack Cardputer! 🕹️

```
   ███████╗██╗  ██╗    ███████╗██████╗ ███████╗ ██████╗████████╗██████╗ ██╗   ██╗███╗   ███╗
   ╚══███╔╝╚██╗██╔╝    ██╔════╝██╔══██╗██╔════╝██╔════╝╚══██╔══╝██╔══██╗██║   ██║████╗ ████║
     ███╔╝  ╚███╔╝     ███████╗██████╔╝█████╗  ██║        ██║   ██████╔╝██║   ██║██╔████╔██║
    ███╔╝   ██╔██╗     ╚════██║██╔═══╝ ██╔══╝  ██║        ██║   ██╔══██╗██║   ██║██║╚██╔╝██║
   ███████╗██╔╝ ██╗    ███████║██║     ███████╗╚██████╗   ██║   ██║  ██║╚██████╔╝██║ ╚═╝ ██║
   ╚══════╝╚═╝  ╚═╝    ╚══════╝╚═╝     ╚══════╝ ╚═════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝ ╚═╝     ╚═╝
```

---

**Made with ❤️ for the retro gaming community**

