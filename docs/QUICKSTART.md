# ZX Spectrum Emulator - Quick Start Guide

**Platform:** M5Stack Cardputer  
**Version:** 1.0 (Emulator Core - Working!)  
**Date:** October 24, 2025

---

## What's Working

- ✅ Z80 CPU emulation (3.5 MHz)
- ✅ 16KB ROM (Sinclair 48K)
- ✅ 48KB RAM
- ✅ Interrupt Mode 1 (50 Hz)
- ✅ EI/DI/HALT instructions
- ✅ 50 FPS stable timing

**Status:** Emulator is running, ROM executes, INTs are handled correctly!

---

## Requirements

- **Hardware:** M5Stack Cardputer
- **Software:** PlatformIO
- **USB:** USB-C cable

---

## Installation

### 1. Connect Cardputer to Computer

### 2. Build and Upload:
```bash
cd /path/to/zx_rainbow_cardputer
pio run --target upload
```

### 3. Open Serial Monitor:
```bash
pio device monitor --baud 115200
```

Or use `screen`:
```bash
screen /dev/cu.usbmodem14401 115200
```

---

## Expected Output

```
========================================
   ZX SPECTRUM EMULATOR STARTING
========================================

Free heap: 359248 bytes
ROM loaded: 16384 bytes
=== ROM VERIFICATION ===
ROM[0x0000] = 0xF3 (expect 0xF3 = DI)
ROM[0x0001] = 0xAF (expect 0xAF = XOR A)
✅ ROM verified: correct ZX Spectrum 48K ROM

=== EMULATOR READY ===
Z80 PC: 0x0000 (should be 0x0000)
Z80 SP: 0xFFFF
Z80 IM: 0 (should be 0)
Z80 IFF1: 0 (should be 0)

Starting emulation...

🎯 INT #81: PC=0x0E5C→0x0038 IM=1 IFF1=1→0 ✅
✨ IM=1 detected at frame #81!

FPS: 50.00 | INT: 50.00/s | PC: 0x10B4 | IM: 1 | IFF1: 1 ✅
```

**If you see this output — the emulator is WORKING!** 🎉

---

## Verification Checklist

### ✅ Success Criteria:

1. **ROM verified:** `✅ ROM verified: correct ZX Spectrum 48K ROM`
2. **FPS:** 49.95-50.00
3. **INT rate:** 49.95-50.00/s
4. **IM:** 1 (after ~81 frames)
5. **IFF1:** 1 (in HUD, between interrupts)
6. **PC:** changing (not stuck at 0x0038 or 0x0000)

### ❌ Problem Indicators:

- `🔴 FATAL: ROM signature mismatch` → wrong ROM file
- `PC: 0x0000` constantly → CPU not executing
- `IM: 0` constantly → ROM not initialized
- `IFF1: 0` constantly → EI-delay not working

---

## Troubleshooting

### Issue: Device not configured
```
Disconnected (read failed: [Errno 6] Device not configured)
```

**Solution:**
1. Press **RESET** on Cardputer
2. Quickly reconnect monitor
3. Or use `screen` instead of `pio device monitor`

---

### Issue: Upload failed
```
A fatal error occurred: Failed to connect to ESP32
```

**Solution:**
1. Hold **G0** button while connecting USB
2. Or press **RESET** during upload
3. Check USB cable (must support data, not just power)

---

### Issue: Compilation failed
```
fatal error: 48k_rom.h: No such file or directory
```

**Solution:**
Ensure `src/spectrum/48k_rom.h` exists and contains:
```cpp
const unsigned char gb_rom_0_sinclair_48k[]={
0xF3,0xAF,0x11,0xFF,0xFF,0xC3,0xCB,0x11,...
```

---

## Project Structure

```
zx_rainbow_cardputer/
├── platformio.ini              # Project configuration
├── src/
│   ├── main.cpp                # Main entry point
│   ├── spectrum/
│   │   ├── spectrum_mini.h     # ZX Spectrum class
│   │   ├── spectrum_mini.cpp   # Emulator implementation
│   │   ├── 48k_rom.h           # ROM data (16KB)
│   │   └── memory.h            # Memory management (ROM+RAM)
│   └── z80/
│       ├── z80.h               # Z80 register definitions
│       ├── z80.cpp             # Z80 emulation core
│       ├── macros.h            # Helper macros
│       └── opcodes.h           # Z80 instruction set
└── SUCCESS_REPORT_V1.0.md      # Full achievement report
```

---

## Next Steps

After successful emulator startup:

1. **Remove debug logs** (INT #0-100) → clean output
2. **Add display** → see VRAM contents
3. **Add keyboard** → input commands
4. **Load a game** → Play SABOTEUR! 🎮

---

## Documentation

- `SUCCESS_REPORT_V1.0.md` - Full achievement report
- `TECHNICAL_DETAILS.md` - Technical implementation details
- `SUMMARY_RU.md` - Legacy project summary (Russian)

---

## Useful Commands

### Build without upload:
```bash
pio run
```

### Upload only:
```bash
pio run --target upload
```

### Clean project:
```bash
pio run --target clean
```

### Log to file:
```bash
pio device monitor --baud 115200 > logs.txt
```

---

## Success!

If you see:
```
FPS: 50.00 | INT: 50.00/s | PC: 0x10B4 | IM: 1 | IFF1: 1 ✅
```

**CONGRATULATIONS!** You have a working ZX Spectrum emulator on Cardputer! 🚀

---

**Questions?** Check `SUCCESS_REPORT_V1.0.md` for details.
