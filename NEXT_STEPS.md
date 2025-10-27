# 🚀 NEXT STEPS - ZX Spectrum Emulator

**Current Version:** 1.0 (Phase 1 Complete)  
**Current Status:** ✅ CPU Core Working  
**Next Goal:** Phase 2 - Display

---

## ✅ PHASE 1: CPU CORE (DONE!)

- [x] Z80 CPU emulation (3.5 MHz)
- [x] ROM loading & verification (Sinclair 48K)
- [x] Interrupt handling (IM1, 50 Hz)
- [x] EI-delay mechanism
- [x] Stable 50 FPS timing
- [x] Accumulator control
- [x] Documentation (README, QUICKSTART, SUCCESS_REPORT, TECHNICAL_DETAILS)

**Date Completed:** October 24, 2025

---

## 🚧 PHASE 2: DISPLAY (TODO)

### Goal: Display ZX Spectrum screen on Cardputer

### Prerequisites:
- ✅ Emulator core working
- ✅ VRAM accessible (0x4000-0x5AFF)
- ⏳ M5GFX library (add to platformio.ini)
- ⏳ ULA emulator (convert VRAM to pixels)

### Tasks:

#### 1. Add M5GFX Library
```ini
# platformio.ini
lib_deps = 
    m5stack/M5GFX@^0.1.11
```

**Files to modify:**
- `platformio.ini`

---

#### 2. Initialize M5GFX in main.cpp
```cpp
#include <M5GFX.h>
M5GFX display;

void setup() {
  display.init();
  display.setRotation(1);  // Landscape
  display.fillScreen(TFT_BLACK);
}
```

**Files to modify:**
- `src/main.cpp`

---

#### 3. Create ULA Emulator

**New files to create:**
- `src/spectrum/ula.h`
- `src/spectrum/ula.cpp`

**What ULA does:**
- Reads VRAM bitmap (0x4000-0x57FF, 6KB)
- Reads VRAM attributes (0x5800-0x5AFF, 768 bytes)
- Converts to RGB565 pixels
- Handles border color

**Reference:**
- `esp32-rainbow/VideoOut.cpp` (lines 200-400)
- `esp32-rainbow/spectrum.cpp` (lines 100-200)

---

#### 4. Implement renderScreen()

```cpp
void renderScreen() {
  // For each 8x8 character cell (32x24 grid):
  for (int cy = 0; cy < 24; cy++) {
    for (int cx = 0; cx < 32; cx++) {
      // Get bitmap address (weird ZX layout!)
      int addr = getBitmapAddress(cx, cy);
      
      // Get attribute (INK/PAPER/BRIGHT/FLASH)
      byte attr = mem.peek(0x5800 + cy * 32 + cx);
      
      // Render 8x8 pixels
      renderCell(cx, cy, addr, attr);
    }
  }
  
  // Render border
  renderBorder();
}
```

**Files to modify:**
- `src/spectrum/spectrum_mini.cpp` (add renderScreen method)
- `src/spectrum/spectrum_mini.h` (declare renderScreen)

---

#### 5. ZX Spectrum Screen Layout

```
┌────────────────────────────────────────────┐
│  Border                                    │
│  ┌──────────────────────────────────────┐  │
│  │                                      │  │
│  │  Screen: 256x192 pixels              │  │
│  │  32x24 characters (8x8 each)         │  │
│  │                                      │  │
│  │  Bitmap:  0x4000-0x57FF (6KB)       │  │
│  │  Attrs:   0x5800-0x5AFF (768B)      │  │
│  │                                      │  │
│  └──────────────────────────────────────┘  │
│  Border                                    │
└────────────────────────────────────────────┘
```

---

#### 6. Bitmap Address Calculation

**ZX Spectrum has WEIRD memory layout:**

```
Line Y → Address:
  0-7   → 0x4000-0x40FF  (top of each cell)
  8-15  → 0x4100-0x41FF
  ...
  64-71 → 0x4800-0x48FF  (middle of each cell)
  ...
```

**Formula:**
```cpp
int getBitmapAddress(int x, int y) {
  int line = y * 8;  // Character row → pixel row
  int col = x;
  
  // ZX layout: bits 0-4=col, 5-7=line%8, 8-10=line/64, 11-12=line/8%8
  int addr = 0x4000;
  addr += (line & 0x07) << 8;      // bits 5-7 of line
  addr += ((line & 0x38) >> 3) << 5; // bits 3-5 of line
  addr += (line & 0xC0) << 5;      // bits 6-7 of line
  addr += col;
  
  return addr;
}
```

**Reference:**
- https://worldofspectrum.org/faq/reference/48kreference.htm#MemoryMap

---

#### 7. Attribute Format

Each byte @ 0x5800-0x5AFF:
```
Bit 7: FLASH (0=normal, 1=flash)
Bit 6: BRIGHT (0=normal, 1=bright)
Bits 5-3: PAPER color (0-7)
Bits 2-0: INK color (0-7)
```

**ZX Spectrum Colors:**
```cpp
const uint16_t ZX_COLORS[16] = {
  // Normal (BRIGHT=0):
  0x0000,  // 0: Black
  0x0015,  // 1: Blue
  0xA800,  // 2: Red
  0xA815,  // 3: Magenta
  0x0540,  // 4: Green
  0x0555,  // 5: Cyan
  0xAD40,  // 6: Yellow
  0xAD55,  // 7: White
  
  // Bright (BRIGHT=1):
  0x0000,  // 8: Black
  0x001F,  // 9: Blue
  0xF800,  // 10: Red
  0xF81F,  // 11: Magenta
  0x07E0,  // 12: Green
  0x07FF,  // 13: Cyan
  0xFFE0,  // 14: Yellow
  0xFFFF,  // 15: White
};
```

---

#### 8. Optimize Rendering

**Don't render every frame!**
- Z80: 50 FPS
- Display: 25 FPS (every 2nd frame)

```cpp
static int renderCounter = 0;

void loop() {
  spectrum->runForFrame();
  
  renderCounter++;
  if (renderCounter >= 2) {
    renderScreen();  // Only 25 FPS
    renderCounter = 0;
  }
}
```

---

#### 9. Testing Checklist

- [ ] Display initializes (white screen = good)
- [ ] Border renders (black rectangle)
- [ ] Screen area renders (should see ZX copyright message!)
- [ ] Colors work (copyright has colors)
- [ ] No flickering (25 FPS should be smooth)
- [ ] FPS still 50 (rendering doesn't slow emulation)

---

#### 10. Expected Result

After Phase 2, you should see:
```
┌────────────────────────────────┐
│                                │
│   © 1982 Sinclair Research Ltd │
│                                │
│   48K                          │
│                                │
│   READY                        │
│   █                            │
│                                │
└────────────────────────────────┘
```

**White on black, blinking cursor!** 🎉

---

## 🎯 PHASE 3: INPUT (AFTER DISPLAY)

### Goal: Keyboard input working

### Tasks:
- [ ] Implement port 0xFE read (keyboard half-rows)
- [ ] Map Cardputer keys → ZX Spectrum keys
- [ ] Test BASIC commands (PRINT "HELLO")

---

## 🎮 PHASE 4: GAMES (FINAL)

### Goal: Play SABOTEUR!

### Tasks:
- [ ] Implement TAP file loader
- [ ] Load SABOTEUR.TAP from SD card
- [ ] Test gameplay
- [ ] Celebrate! 🎉

---

## 📚 REFERENCES FOR PHASE 2:

### Essential Reading:
1. **ZX Spectrum Memory Map:**  
   https://worldofspectrum.org/faq/reference/48kreference.htm#MemoryMap

2. **ULA Reference:**  
   https://worldofspectrum.org/faq/reference/48kreference.htm#ULA

3. **ESP32 Rainbow VideoOut.cpp:**  
   https://github.com/atomic14/esp32-zxspectrum/blob/main/firmware/src/VideoOut.cpp

4. **M5GFX Documentation:**  
   https://github.com/m5stack/M5GFX

---

## 💡 TIPS FOR PHASE 2:

1. **Start simple:** Render all pixels as white, ignore colors
2. **Test with static VRAM:** Write test pattern to 0x4000
3. **Debug one cell at a time:** Print bitmap/attr bytes
4. **Use M5GFX drawPixel():** Start slow, optimize later
5. **Reference esp32-rainbow:** Don't reinvent the wheel!

---

## ⚠️ COMMON PITFALLS:

1. **Weird bitmap layout!** Don't use `addr = 0x4000 + y * 32 + x`
2. **Attributes are separate!** Not interleaved with bitmap
3. **BRIGHT adds 8 to color!** `color = (attr & 0x07) + (bright ? 8 : 0)`
4. **FLASH not implemented yet!** Skip for Phase 2
5. **Border from last OUT 0xFE!** Track in Z80OutPort()

---

## 🎉 GOOD LUCK WITH PHASE 2!

**You have a working emulator!**  
**Now make it visible!** 👀

---

**Estimated time:** 4-8 hours (if following references)  
**Difficulty:** Medium (bitmap layout is tricky!)  
**Reward:** SEE THE ZX SPECTRUM SCREEN! 🖥️

---

**Start Date:** _____________________  
**Completion Date:** _____________________  
**Status:** ⏳ TODO

---

🚀 **GO CODE!**

