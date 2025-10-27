# ZX Spectrum Emulator - Technical Details

**Version:** 1.0  
**Date:** October 24, 2025  
**Purpose:** Technical implementation documentation

---

## Architecture Overview

```
┌─────────────────────────────────────────┐
│           main.cpp (ESP32)              │
│  ┌──────────────────────────────────┐   │
│  │   loop() - 50 FPS throttling     │   │
│  └────────────┬─────────────────────┘   │
│               │                          │
│               ▼                          │
│  ┌──────────────────────────────────┐   │
│  │  ZXSpectrum::runForFrame()       │   │
│  │  - Small quanta (128 t-states)   │   │
│  │  - Accumulator control           │   │
│  │  - INT generation (50 Hz)        │   │
│  └────────────┬─────────────────────┘   │
│               │                          │
│               ▼                          │
│  ┌──────────────────────────────────┐   │
│  │     Z80Run(regs, cycles)         │   │
│  │  - Fetch/Decode/Execute          │   │
│  │  - EI-delay handling             │   │
│  │  - Return actual cycles used     │   │
│  └────────────┬─────────────────────┘   │
│               │                          │
│               ▼                          │
│  ┌──────────────────────────────────┐   │
│  │  Memory callbacks (ROM/RAM)      │   │
│  │  - Z80MemRead/Write              │   │
│  │  - Z80InPort/OutPort             │   │
│  └──────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

---

## Key Components

### 1. Z80 CPU Core (`z80.cpp`)

#### Register Structure:
```cpp
typedef struct {
  void *userInfo;              // Pointer to ZXSpectrum instance
  eword AF, BC, DE, HL;        // Main register pairs
  eword IX, IY, PC, SP, R;     // Index and special registers
  eword AFs, BCs, DEs, HLs;    // Shadow register pairs
  byte IFF1, IFF2, I, halted;  // Interrupt flags and state
  char IM;                     // Interrupt mode (0/1/2)
  byte ei_pending;             // EI-delay flag (critical!)
  int cycles;                  // Cycle counter
} Z80Regs;
```

#### Z80Run() Contract:
```cpp
uint16_t Z80Run(Z80Regs *regs, int numcycles) {
  regs->cycles = numcycles;
  
  while (regs->cycles > 0) {
    // 1. Fetch opcode
    opcode = Z80ReadMem(PC++);
    
    // 2. Decode & Execute
    switch (opcode) { ... }
    
    // 3. Apply EI-delay (after instruction completes!)
    if (regs->ei_pending) {
      regs->IFF1 = regs->IFF2 = 1;
      regs->ei_pending = 0;
    }
    
    // 4. Subtract instruction cycles
    regs->cycles -= instruction_cycles;
  }
  
  return micValue;  // Total cycles actually executed
}
```

---

### 2. EI-Delay Mechanism (Critical!)

**Problem:** The Z80 CPU has a 1-instruction delay for the EI instruction.

**Example:**
```asm
EI      ; IFF1 is NOT set immediately!
RET     ; After RET executes: IFF1=1
HALT    ; Now interrupts can occur
```

**Implementation:**

#### In `opcodes.h`:
```cpp
case EI:
  r_ei_pending = 1;  // Don't set IFF1 immediately!
  AddCycles(4);
break;
```

#### In `z80.cpp` (end of instruction cycle):
```cpp
// After EVERY instruction completes
if (regs->ei_pending) {
  regs->IFF1 = regs->IFF2 = 1;  // Now it's safe!
  regs->ei_pending = 0;
}
```

**Why This is Critical:**  
Without EI-delay, the ROM would execute:
1. `IM 1` → sets interrupt mode
2. `EI` → enables interrupts (IFF1=1)
3. INT arrives immediately → interrupts before `HALT`
4. Infinite loop!

With EI-delay:
1. `IM 1` → OK
2. `EI` → ei_pending=1
3. `RET` executes → IFF1=1
4. `HALT` executes → waits for INT
5. INT arrives → OK!

---

### 3. Small Execution Quanta

**Problem:** 
- Request 224 t-states
- Get 226-230 t-states (instructions are atomic)
- 312 lines × 2-6 "extra" = 624-1872 t-states/frame
- Per second: 31,200-93,600 "extra" t-states!
- That's 0.5-1.3 additional frames!

**Solution:**
```cpp
const int SLICE_TST = 128;  // Instead of 224!

int targetAdd = TSTATES_PER_FRAME;  // 69888
while (targetAdd > 0) {
  int budget = min(SLICE_TST, targetAdd);
  int used = Z80Run(regs, budget);
  
  tstateAccumulator += used;  // Accumulate ACTUAL cycles
  targetAdd -= used;
}
```

**Why This Works:**
- 69888 / 128 = 546 quanta
- "Overshoot" per quantum: 0-10 t-states
- Maximum overshoot: 546 × 10 = 5460 t-states/frame
- This is acceptable! (7.8% instead of 300%+)

---

### 4. Accumulator & INT Generation

**Contract:**
```cpp
static int64_t tstateAccumulator = 0;  // Accumulator

// After each quantum:
tstateAccumulator += used;

// INT generation (may fire multiple times per loop!):
while (tstateAccumulator >= 69888) {
  if (IFF1 && IM==1) {
    interrupt();  // JP 0x0038, PUSH PC, IFF1=0
  }
  tstateAccumulator -= 69888;  // ALWAYS subtract!
  totalFrames++;
}
```

**Key Points:**

1. **`while` instead of `if`:**  
   If 140,000 t-states accumulated → 2 INTs!

2. **Check IFF1:**  
   If IFF1=0 → interrupt is LOST (like real hardware!)

3. **Always subtract:**  
   Even if IFF1=0, we subtract the frame!

4. **int64_t type:**  
   Protection against overflow (~9 quintillion t-states max)

---

### 5. HUD Snapshot (Mid-Frame)

**Problem:**  
If we read PC right after INT → always 0x0038!

**Solution:**
```cpp
// Global variables (snapshot)
static uint16_t hud_pc = 0;
static uint8_t hud_iff1 = 0;

// Mid-frame (after ~35000 t-states):
if (cyclesExecuted >= MIDFRAME_TSTATES && !snapshotTaken) {
  hud_pc = z80Regs->PC.W;
  hud_iff1 = z80Regs->IFF1;
  snapshotTaken = true;
}

// In main.cpp:
Serial.printf("PC: 0x%04X", spectrum->getHudPC());
```

**Result:**  
PC displays "live" addresses: 0x10B4, 0x0E5C, 0x15E8...

---

## ZX Spectrum 48K Timing

### Constants:
```cpp
CPU_FREQ       = 3.5 MHz
TSTATES_LINE   = 224
LINES_FRAME    = 312
TSTATES_FRAME  = 69888  (224 × 312)
FRAMES_SECOND  = 50     (PAL)
INT_FREQ       = 50 Hz
```

### Frame Structure (PAL):
```
┌────────────────────────────────────┐
│  Top Border      (64 lines)        │  14336 t-states
├────────────────────────────────────┤
│  Screen          (192 lines)       │  43008 t-states
├────────────────────────────────────┤
│  Bottom Border   (56 lines)        │  12544 t-states
├────────────────────────────────────┤
│  VBlank          (?)               │
└────────────────────────────────────┘
Total: 312 lines = 69888 t-states
INT triggered at start of frame
```

---

## Memory Map

```
0x0000 - 0x3FFF  ROM (16KB)       [Read-only]
0x4000 - 0x57FF  VRAM (6KB)       [Screen data]
0x5800 - 0x5AFF  VRAM (768B)      [Attributes]
0x5B00 - 0x5BFF  Printer buffer   [System vars]
0x5C00 - 0x5CBF  System variables
0x5CC0 - 0xFFFF  User RAM (~42KB)
```

### ROM Important Addresses:
```
0x0000: F3           ; DI (ROM start)
0x0001: AF           ; XOR A
0x0038: F5           ; PUSH AF (IM1 vector)
      : E5           ; PUSH HL
      : 2A 78 5C     ; LD HL,(FRAMES)
      : 23           ; INC HL
      : 22 78 5C     ; LD (FRAMES),HL
      : ...          ; (increment frame counter)
      : F1           ; POP AF
      : E1           ; POP HL
      : FB           ; EI
      : C9           ; RET
```

---

## INT Handler (IM1)

### What Happens During INT:

1. **Check IFF1:**
   ```cpp
   if (!regs->IFF1) return;  // INT is ignored!
   ```

2. **PUSH PC:**
   ```cpp
   SP -= 2;
   memory[SP] = PC_lo;
   memory[SP+1] = PC_hi;
   ```

3. **Jump to vector:**
   ```cpp
   if (IM == 1) PC = 0x0038;
   ```

4. **Disable interrupts:**
   ```cpp
   IFF1 = 0;  // Until EI in handler!
   ```

5. **Handler executes:**
   ```asm
   0x0038: PUSH AF      ; Save registers
           PUSH HL
           LD HL,(FRAMES)
           INC HL       ; Increment frame counter
           LD (FRAMES),HL
           POP HL
           POP AF
           EI           ; Enable INT (with delay!)
           RET          ; Return (after RET: IFF1=1)
   ```

---

## Debugging

### Useful Logging Points:

```cpp
// 1. ROM verification
ROM[0x0000] should be 0xF3
ROM[0x0001] should be 0xAF

// 2. Z80 state
PC should change (not stuck at 0x0000 or 0x0038)
IM should become 1 after ~2 seconds
IFF1 should be 1 between INTs

// 3. Accumulator
Should remain < 1000 normally
If > 100000 → problem with cycle counting!

// 4. INT rate
Should be 49-51 INT/s
If < 45 → too slow
If > 55 → too fast
```

---

## Common Pitfalls

### 1. EI Without Delay
```cpp
// WRONG:
case EI:
  r_IFF1 = r_IFF2 = 1;  // Immediately!
  
// CORRECT:
case EI:
  r_ei_pending = 1;  // Deferred!
```

### 2. IF Instead of WHILE
```cpp
// WRONG:
if (acc >= 69888) {
  interrupt();
  acc -= 69888;
}

// CORRECT:
while (acc >= 69888) {  // May fire multiple times!
  interrupt();
  acc -= 69888;
}
```

### 3. Large Quanta
```cpp
// WRONG:
runForCycles(224);  // Overshoot: 2-6 t-states

// CORRECT:
runForCycles(128);  // Overshoot: 0-10, but fewer quanta
```

### 4. Not Subtracting When IFF1=0
```cpp
// WRONG:
if (IFF1) {
  interrupt();
  acc -= 69888;
}
// If IFF1=0 → accumulator grows to infinity!

// CORRECT:
if (IFF1) interrupt();
acc -= 69888;  // ALWAYS!
```

---

## Performance

### Measurements (ESP32-S3 @ 240 MHz):

```
CPU usage:        ~30-40%
Frame time:       ~12-15ms (target: 20ms)
Heap free:        ~293KB
Z80 instructions: ~3500 MHz equivalent (due to overhead)
Actual Z80 freq:  ~3.5 MHz emulated
```

### Optimizations:

1. **Small quanta** → less overshoot
2. **Static variables** → no reallocation
3. **int64_t accumulator** → overflow protection
4. **Callback functions** → flexibility without overhead

---

## References

1. **Z80 CPU Manual:**  
   http://www.zilog.com/docs/z80/um0080.pdf

2. **ZX Spectrum Technical Manual:**  
   https://worldofspectrum.org/

3. **ESP32 Rainbow (original):**  
   https://github.com/atomic14/esp32-zxspectrum

4. **Z80 Emulation (Santiago Romero):**  
   http://www.github.com/jsanchezv/z80

---

## Future Improvements

1. **Display rendering** (ULA emulation)
2. **Keyboard input** (port 0xFE)
3. **Sound** (beeper)
4. **TAP file loading**
5. **Turbo mode** (7 MHz)
6. **128K model**
7. **AY-3-8912 sound** (for 128K)

---

**End of technical documentation.**  
**For quick start guide, see `QUICKSTART.md`**
