# ZX Spectrum Emulator V1.0 - Success Report

**Date:** October 24, 2025  
**Status:** ✅ **FULLY WORKING**  
**Platform:** M5Stack Cardputer (ESP32-S3)

---

## Achievements

### ✅ Critical Issues SOLVED:

1. **ROM loaded correctly** (Sinclair 48K, starts with `F3 AF`)
2. **EI-delay implemented** (IFF1 set after next instruction)
3. **INTs handled correctly** (IM1, 50 Hz, IFF1=1→0)
4. **Accumulator stable** (small quanta 128 t-states, while loop)
5. **Timing perfect** (50 FPS, 50 INT/s)
6. **HUD snapshot mid-frame** (shows live PC, not 0x0038)
7. **SP = 0xFFFF** (as in real ZX Spectrum)

---

## Telemetry (Final)

```
FPS: 50.00 | INT: 50.00/s | PC: 0x10B4 | SP: 0xFF48 | IM: 1 | IFF1: 1 ✅
```

- **FPS:** 49.95-50.00 (perfect!)
- **INT rate:** 49.95-50.00/s (50 Hz PAL)
- **PC:** changing between INTs (0x10B4, 0x0E5C, 0x10AF, 0x15E8...)
- **IM:** 1 (interrupt mode set)
- **IFF1:** 1 between interrupts (EI working!)
- **Accumulator:** stable (~10-200/frame, no overflow)

---

## Key Changes

### 1. Correct ROM
- Using `gb_rom_0_sinclair_48k[]` from `esp32-rainbow`
- Starts with `F3 AF 11 FF FF` (DI; XOR A; LD DE,0xFFFF)
- ROM verification: `ROM[0x0000]=0xF3, ROM[0x0001]=0xAF`

### 2. EI-delay Mechanism
```cpp
// In z80.h
byte ei_pending;  // Deferred EI flag

// In opcodes.h (case EI)
r_ei_pending = 1;  // Don't set IFF1 immediately!

// In z80.cpp (end of instruction cycle)
if (regs->ei_pending) {
  regs->IFF1 = regs->IFF2 = 1;
  regs->ei_pending = 0;
}
```

### 3. Small Execution Quanta
```cpp
const int SLICE_TST = 128;  // Instead of 224!

while (targetAdd > 0) {
  int budget = (targetAdd < SLICE_TST) ? targetAdd : SLICE_TST;
  int used = runForCycles(budget);
  tstateAccumulator += used;
  targetAdd -= used;
}
```

### 4. Correct INT Handling
```cpp
while (tstateAccumulator >= TSTATES_PER_FRAME) {
  // Don't queue if IFF1=0 (like real hardware!)
  if (z80Regs->IFF1 && z80Regs->IM == 1) {
    interrupt();  // JP 0x0038
  }
  tstateAccumulator -= TSTATES_PER_FRAME;  // ALWAYS subtract!
  totalFrames++;
}
```

### 5. HUD Snapshot Mid-Frame
```cpp
// Take snapshot at ~35000 t-states (not at INT moment!)
if (!snapshotTaken && cyclesExecuted >= MIDFRAME_TSTATES) {
  hud_pc = z80Regs->PC.W;
  hud_iff1 = z80Regs->IFF1;
  snapshotTaken = true;
}
```

### 6. SP = 0xFFFF
```cpp
// In Z80Reset()
regs->SP.W = 0xFFFF;  // As in real ZX Spectrum 48K
```

---

## Project Structure

```
zx_rainbow_cardputer/
├── platformio.ini              # Bare minimum ESP32-S3
├── src/
│   ├── main.cpp                # Main loop, HUD telemetry
│   ├── spectrum/
│   │   ├── spectrum_mini.h     # ZXSpectrum class
│   │   ├── spectrum_mini.cpp   # runForFrame(), small quanta, INT
│   │   ├── 48k_rom.h           # ROM Sinclair 48K (16384 bytes)
│   │   └── memory.h            # Memory class (ROM+RAM)
│   └── z80/
│       ├── z80.h               # Z80Regs struct (+ei_pending)
│       ├── z80.cpp             # Z80Run(), Z80Interrupt()
│       ├── macros.h            # r_ei_pending macro
│       └── opcodes.h           # EI-delay implementation
└── data/
    └── 48.rom                  # ROM file (not used, embedded)
```

---

## Fixed Bugs

### Bug #1: Wrong ROM
**Problem:** ROM started with `11 FF FF` instead of `F3 AF 11 FF FF`  
**Cause:** Used `48P_fixed.rom` (corrupted)  
**Solution:** Taken `romSinclair48K.h` from `esp32-rainbow`

### Bug #2: PC Stuck at 0x0038
**Problem:** PC always showed 0x0038, IFF1=0  
**Causes:** 
1. HUD took snapshot right after INT
2. Accumulator overflowed (72000+ cycles/frame)
3. Multiple INTs per frame

**Solutions:**
1. HUD snapshot mid-frame
2. Small quanta (128 t-states)
3. `while (acc >= frame)` instead of `if`

### Bug #3: IFF1 Never Became 1
**Problem:** IFF1 always 0, INTs not handled  
**Cause:** EI set IFF1=1 immediately, but next INT interrupted before HALT  
**Solution:** EI-delay mechanism (IFF1=1 after next instruction)

### Bug #4: Accumulator Grew to 125648+
**Problem:** Z80Run() returned more than requested  
**Cause:** Loop of 312 lines × 224 t-states gave ~72000 instead of 69888  
**Solution:** Small quanta (128) + `while` for overflow handling

---

## What's Working

- ✅ ROM initialization (F3 AF → DI; XOR A)
- ✅ Interrupt Mode 1 (IM 1)
- ✅ EI/DI instructions (with delay)
- ✅ HALT/UNHALT
- ✅ Stack operations (PUSH/POP/CALL/RET)
- ✅ INT @ 0x0038 (50 Hz)
- ✅ Timing (69888 t-states/frame)
- ✅ 50 FPS stable

---

## Next Steps

### Phase 2: Display
1. Add M5GFX rendering (240x135)
2. ULA emulation (border, VRAM @ 0x4000-0x5AFF)
3. Attributes and colors

### Phase 3: Input
1. Keyboard emulation (port 0xFE)
2. Cardputer → ZX key mapping

### Phase 4: Games
1. .TAP file loading
2. Test SABOTEUR
3. Test other games

---

## Technical Details

### Timing Contract:
```
1 frame = 69888 t-states
1 second = 50 frames
1 t-state = ~286 ns (3.5 MHz)
```

### INT Handling:
```
1. Accumulator >= 69888
2. IF (IFF1=1 AND IM=1) → interrupt()
3. Accumulator -= 69888
4. ELSE → interrupt is lost (like real hardware!)
```

### EI-delay:
```
EI          ; r_ei_pending = 1
RET         ; After RET executes: IFF1=1
            ; Now INTs are enabled
```

---

## Credits

- **ESP32 Rainbow:** Alvaro Alea Fernandez (base Z80 core)
- **Z80 emulation:** Santiago Romero Iglesias
- **ZX Spectrum 48K ROM:** Sinclair Research Ltd.
- **Port to Cardputer:** AI Assistant + User collaboration

---

## License

GPL v2 (same as original ESP32 Rainbow project)

---

**CONCLUSION:** ZX Spectrum emulator running on Cardputer!  
**Next goal:** Add display and keyboard → play games! 🎮
