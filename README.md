# ZX Spectrum Emulator for M5Stack Cardputer

**Version:** 1.0 (Core Working!)  
**Status:** ✅ Emulator Running Stable  
**Platform:** ESP32-S3 (M5Stack Cardputer)

🎥 **[Video Demo on YouTube](https://www.youtube.com/watch?v=AAvEyH9OQ34)** - Watch it in action!

---

## Quick Start

```bash
# 1. Connect Cardputer
# 2. Upload firmware:
cd /path/to/zx_rainbow_cardputer
pio run --target upload

# 3. Open serial monitor:
pio device monitor --baud 115200
```

**Expected output:**
```
✅ ROM verified: correct ZX Spectrum 48K ROM
FPS: 50.00 | INT: 50.00/s | PC: 0x10B4 | IM: 1 | IFF1: 1 ✅
```

**If you see this — the emulator is working!** 🎉

---

## Documentation

| File | Description |
|------|-------------|
| **[QUICKSTART.md](QUICKSTART.md)** | 🚀 Quick start, installation, troubleshooting |
| **[SUCCESS_REPORT_V1.0.md](SUCCESS_REPORT_V1.0.md)** | 🏆 Full achievement report |
| **[TECHNICAL_DETAILS.md](TECHNICAL_DETAILS.md)** | 🔬 Technical documentation |
| **[SESSION_OCT24_2025_SUMMARY.md](SESSION_OCT24_2025_SUMMARY.md)** | 📅 Latest session summary |

**Start with [QUICKSTART.md](QUICKSTART.md)!**

---

## What's Working

- ✅ **Z80 CPU** emulation (3.5 MHz)
- ✅ **16KB ROM** (Sinclair 48K)
- ✅ **48KB RAM**
- ✅ **Interrupt Mode 1** (IM1, 50 Hz)
- ✅ **EI/DI/HALT** instructions
- ✅ **50 FPS** stable timing
- ✅ **EI-delay** mechanism (IFF1 after instruction)
- ✅ **Small execution quanta** (128 t-states)
- ✅ **Accumulator** control

---

## Roadmap

### ✅ Phase 1: CPU Core (DONE!)
- [x] Z80 emulation
- [x] ROM loading & verification
- [x] Interrupt handling (IM1)
- [x] EI-delay
- [x] Stable 50 FPS
- [x] Documentation

### 🚧 Phase 2: Display (TODO)
- [ ] M5GFX initialization
- [ ] ULA emulation
- [ ] VRAM rendering (0x4000-0x5AFF)
- [ ] Attributes (colors)
- [ ] Border

### 🔜 Phase 3: Input (TODO)
- [ ] Keyboard emulation (port 0xFE)
- [ ] Key mapping (Cardputer → ZX)
- [ ] Joystick support

### 📦 Phase 4: Games (TODO)
- [ ] TAP file loading
- [ ] SD card support
- [ ] Test SABOTEUR
- [ ] Test other games

### 🎵 Phase 5: Sound (TODO)
- [ ] Beeper (port 0xFE bit 4)
- [ ] AY-3-8912 (for 128K)

### ⚡ Phase 6: Advanced (TODO)
- [ ] Turbo mode (7 MHz)
- [ ] 128K model
- [ ] SNA/Z80 snapshot support

---

## Technical Specifications

```
CPU:              Z80 @ 3.5 MHz (emulated)
ROM:              16KB Sinclair 48K
RAM:              48KB
Display:          256×192 (not implemented yet)
Colors:           8 (not implemented yet)
Sound:            1-bit beeper (not implemented yet)
Frame rate:       50 FPS (PAL)
Interrupt rate:   50 Hz (IM1)
T-states/frame:   69888
```

---

## Project Structure

```
zx_rainbow_cardputer/
├── README.md                   # This file
├── QUICKSTART.md               # Quick start guide
├── SUCCESS_REPORT_V1.0.md      # Achievement report
├── TECHNICAL_DETAILS.md        # Technical documentation
├── SESSION_OCT24_2025_SUMMARY.md # Session summary
├── platformio.ini              # PlatformIO configuration
├── src/
│   ├── main.cpp                # Main entry point (setup/loop)
│   ├── spectrum/
│   │   ├── spectrum_mini.h     # ZXSpectrum class
│   │   ├── spectrum_mini.cpp   # runForFrame(), INT handling
│   │   ├── 48k_rom.h           # ROM Sinclair 48K (16KB)
│   │   └── memory.h            # Memory (ROM+RAM, 64KB)
│   └── z80/
│       ├── z80.h               # Z80Regs struct
│       ├── z80.cpp             # Z80Run(), Z80Interrupt()
│       ├── macros.h            # Register access macros
│       └── opcodes.h           # Z80 instruction set
└── data/
    └── 48.rom                  # ROM file (not used, embedded)
```

---

## Key Achievements

### 1. EI-delay Mechanism ⭐⭐⭐
Correctly implemented 1-instruction delay for EI:
```cpp
case EI:
  r_ei_pending = 1;  // Don't set IFF1 immediately!

// After each instruction:
if (ei_pending) {
  IFF1 = IFF2 = 1;
  ei_pending = 0;
}
```

### 2. Small Execution Quanta ⭐⭐
```cpp
const int SLICE_TST = 128;  // Instead of 224
```
Reduced accumulator overshoot from 300%+ to ~7%!

### 3. While Loop for INT ⭐
```cpp
while (acc >= 69888) {  // May fire multiple times!
  if (IFF1 && IM==1) interrupt();
  acc -= 69888;
  totalFrames++;
}
```

### 4. ROM Verification
```cpp
if (ROM[0] != 0xF3 || ROM[1] != 0xAF) {
  Serial.println("🔴 FATAL: ROM signature mismatch!");
  return false;
}
```

---

## Known Issues

- [ ] Display not implemented (no screen output)
- [ ] Keyboard not implemented (no input)
- [ ] Sound not implemented (no beeper)
- [ ] Debug logs still enabled (INT #0-100)

---

## Debugging

### Telemetry Output:
```
FPS: 50.00 | INT: 50.00/s | PC: 0x10B4 | SP: 0xFF48 | IM: 1 | IFF1: 1
```

### Good Values:
- **FPS:** 49.95-50.00 ✅
- **INT/s:** 49.95-50.00 ✅
- **PC:** changing (not stuck at 0x0000 or 0x0038) ✅
- **IM:** 1 (after ~2 seconds) ✅
- **IFF1:** 1 (between interrupts) ✅

### Bad Values:
- **PC:** 0x0000 constantly → CPU not executing ❌
- **PC:** 0x0038 constantly → INT loop ❌
- **IM:** 0 constantly → ROM not initialized ❌
- **IFF1:** 0 constantly → EI-delay not working ❌

---

## References

- **Original ESP32 Rainbow:** https://github.com/atomic14/esp32-zxspectrum
- **Z80 CPU Manual:** http://www.zilog.com/docs/z80/um0080.pdf
- **ZX Spectrum Manual:** https://worldofspectrum.org/
- **M5Stack Cardputer:** https://docs.m5stack.com/en/core/Cardputer

---

## License

GPL v2 (same as original ESP32 Rainbow project)

---

## Authors

- **Port to Cardputer:** AI Assistant + User (a15)
- **ESP32 Rainbow:** Alvaro Alea Fernandez
- **Z80 Core:** Santiago Romero Iglesias
- **ZX Spectrum 48K ROM:** Sinclair Research Ltd.

---

## Status

**EMULATOR CORE: WORKING! ✅**

```
   ████████╗███████╗███████╗████████╗███████╗██████╗ 
   ╚══██╔══╝██╔════╝██╔════╝╚══██╔══╝██╔════╝██╔══██╗
      ██║   █████╗  ███████╗   ██║   █████╗  ██║  ██║
      ██║   ██╔══╝  ╚════██║   ██║   ██╔══╝  ██║  ██║
      ██║   ███████╗███████║   ██║   ███████╗██████╔╝
      ╚═╝   ╚══════╝╚══════╝   ╚═╝   ╚══════╝╚═════╝ 
                                                       
   ██╗    ██╗ ██████╗ ██████╗ ██╗  ██╗██╗███╗   ██╗ ██████╗ 
   ██║    ██║██╔═══██╗██╔══██╗██║ ██╔╝██║████╗  ██║██╔════╝ 
   ██║ █╗ ██║██║   ██║██████╔╝█████╔╝ ██║██╔██╗ ██║██║  ███╗
   ██║███╗██║██║   ██║██╔══██╗██╔═██╗ ██║██║╚██╗██║██║   ██║
   ╚███╔███╔╝╚██████╔╝██║  ██║██║  ██╗██║██║ ╚████║╚██████╔╝
    ╚══╝╚══╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝ ╚═════╝ 
```

**Next: Display + Keyboard + SABOTEUR! 🎮**

---

📧 **Questions?** See [QUICKSTART.md](QUICKSTART.md) and [TECHNICAL_DETAILS.md](TECHNICAL_DETAILS.md)
