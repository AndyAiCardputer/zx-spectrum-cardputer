# 📥 Installation Guide

Complete guide to installing and running the ZX Spectrum Emulator on your M5Stack Cardputer.

---

## 📋 What You Need

### Hardware
- ✅ **M5Stack Cardputer** ([Buy here](https://shop.m5stack.com/products/m5stack-cardputer-kit-w-m5stamps3))
- ✅ **Micro SD card** (FAT32, 8GB or larger recommended)
- ✅ **USB-C cable** (for flashing firmware)
- ⚠️ **Joystick2 Unit** (optional, enhances gameplay)

### Software
- ✅ **Python 3.7+** (for esptool)
- ✅ **esptool.py** (for flashing firmware)
- OR **PlatformIO** (if building from source)

---

## 🚀 Method 1: Flash Pre-built Binary (Recommended!)

### Step 1: Download Firmware

1. Go to [Releases](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/releases)
2. Download `ZX_Spectrum_v137.bin`

### Step 2: Install esptool

```bash
# Install Python if you don't have it (macOS example)
brew install python3

# Install esptool
pip3 install esptool
```

### Step 3: Connect Cardputer

1. Connect your M5Stack Cardputer to your computer via USB-C
2. Find the serial port:

**macOS:**
```bash
ls /dev/cu.*
# Look for something like: /dev/cu.usbmodem14401
```

**Linux:**
```bash
ls /dev/ttyUSB*
# Look for something like: /dev/ttyUSB0
```

**Windows:**
```bash
# Check Device Manager for COM port (e.g., COM3)
```

### Step 4: Flash Firmware

**macOS/Linux:**
```bash
esptool.py --chip esp32s3 \
  --port /dev/cu.usbmodem14401 \
  --baud 921600 \
  write_flash 0x0 ZX_Spectrum_v137.bin
```

**Windows:**
```bash
esptool.py --chip esp32s3 ^
  --port COM3 ^
  --baud 921600 ^
  write_flash 0x0 ZX_Spectrum_v137.bin
```

### Step 5: Wait for Completion

```
Connecting........____.
Chip is ESP32-S3 (revision v0.1)
...
Writing at 0x00100000... (100 %)
Wrote 1234567 bytes at 0x00000000 in 45.6 seconds
...
Hard resetting via RTS pin...
```

**Done!** Your Cardputer will reboot automatically.

---

## 🔧 Method 2: Build from Source

### Step 1: Install PlatformIO

**Via VSCode:**
1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Install PlatformIO extension from marketplace

**Via CLI:**
```bash
# macOS/Linux
python3 -c "$(curl -fsSL https://raw.githubusercontent.com/platformio/platformio/master/scripts/get-platformio.py)"

# Add to PATH
export PATH=$PATH:~/.platformio/penv/bin
```

### Step 2: Clone Repository

```bash
git clone https://github.com/AndyAiCardputer/zx-spectrum-cardputer.git
cd zx-spectrum-cardputer
```

### Step 3: Build and Upload

```bash
# Build only
pio run

# Build and upload
pio run --target upload

# Monitor serial output (optional)
pio device monitor --baud 115200
```

---

## 💾 Prepare SD Card

### Step 1: Format SD Card

1. Insert Micro SD card into your computer
2. Format as **FAT32**:

**macOS:**
```bash
diskutil list  # Find your SD card (e.g., /dev/disk2)
diskutil eraseDisk FAT32 ZXGAMES /dev/disk2
```

**Linux:**
```bash
sudo mkfs.vfat -F 32 /dev/sdb1  # Replace with your SD card
```

**Windows:**
- Right-click SD card in File Explorer
- Select "Format"
- Choose FAT32
- Click "Start"

### Step 2: Create Games Folder

Create a folder named `ZXgames` in the root of the SD card:

```
SD Card:
└── ZXgames/
    (your game files go here)
```

### Step 3: Add Games

Copy your ZX Spectrum game files into `/ZXgames/`:

Supported formats:
- **.SNA** - Snapshot files (instant load, best for quick testing)
- **.TAP** - Tape files (shows loading screen, authentic experience)
- **.Z80** - Compressed snapshots (smaller file size)

**Where to get games?**
- [World of Spectrum](https://worldofspectrum.org/) - Largest collection
- [Spectrum Computing](https://spectrumcomputing.co.uk/) - Well-organized archive
- [Archive.org](https://archive.org/details/world-of-spectrum) - Mirror

**Tested games:** Saboteur, Exolon, Tetris, Pac-Man (all formats)

### Step 4: Insert SD Card

1. Power off your Cardputer
2. Insert the SD card into the slot
3. Power on

---

## ✅ First Run

### What to Expect

1. **Boot screen** appears (ZX Spectrum logo)
2. **Splash screen** shows version info
3. Press any key to continue
4. **Main menu** appears

### Main Menu Options

```
════════════════════════════════════
         ZX SPECTRUM v3.137
════════════════════════════════════

1. Load Game
2. Reset Emulator
3. Information
4. Exit Menu

Press number or navigate with arrows
════════════════════════════════════
```

### Loading Your First Game

1. Press `1` or navigate to "Load Game"
2. Choose file type:
   - `1` - .SNA files (instant load)
   - `2` - .TAP files (with loading screen)
   - `3` - .Z80 files (compressed)
3. Select a game from the list
4. Press `Enter` to load

**TIP:** Start with a **.SNA** file for the fastest experience!

---

## ⌨️ Quick Keyboard Test

Once loaded, try typing:

```basic
PRINT "HELLO ZX!"
```

Press `Enter`. You should see:
```
HELLO ZX!
0 OK, 0:1
```

**If this works, your emulator is running correctly!** 🎉

---

## 🎮 Optional: Joystick2 Unit

### Hardware Setup

1. Connect Joystick2 Unit to Grove port (I2C)
2. Power on Cardputer
3. Joystick should be auto-detected

### Test Joystick

1. Open menu (`Opt+ESC`)
2. Try navigating with joystick stick
3. Press joystick button to select

**Not working?** Check cable connection and restart Cardputer.

---

## 🐛 Troubleshooting

### "Failed to connect to ESP32"

**Solution:**
1. Press and hold BOOT button on Cardputer
2. Run flash command
3. Release BOOT button when "Connecting..." appears

### "SD Card not detected"

**Solutions:**
- Ensure SD card is formatted as FAT32
- Try a different SD card (some cards aren't compatible)
- Check SD card is fully inserted
- Restart Cardputer after inserting card

### "No games in /ZXgames/ folder"

**Solutions:**
- Verify folder is named exactly `ZXgames` (case-sensitive)
- Ensure game files have extensions: .sna, .tap, or .z80
- Check files aren't inside subfolders

### "Game loads but crashes immediately"

**Solutions:**
- Try a different file format (.SNA, .TAP, .Z80)
- Try a different game to verify emulator works
- Check if game requires 128K (not supported yet)

### "No sound"

**Solutions:**
- Press `Opt+M` to unmute
- Press `Opt++` to increase volume
- Check volume isn't set to 0

---

## 📖 Next Steps

- **[README](README.md)** - Full features and controls
- **[CHANGELOG](CHANGELOG.md)** - Version history
- **[CONTRIBUTING](CONTRIBUTING.md)** - Help improve the project

---

## 🎉 Success!

Your ZX Spectrum emulator is now ready! Enjoy playing classic games! 🕹️

**Need help?** [Open an issue](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/issues)

---

**Happy gaming!** 🎮✨

