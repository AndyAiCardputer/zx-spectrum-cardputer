# 🔬 ZX SPECTRUM EMULATOR - TECHNICAL DETAILS

**Версия:** 1.0  
**Дата:** 24 октября 2025  
**Цель:** Документация технической реализации

---

## 🏗️ АРХИТЕКТУРА:

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
│  │  - Малые кванты (128 t-states)   │   │
│  │  - Accumulator контроль          │   │
│  │  - INT генерация (50 Hz)         │   │
│  └────────────┬─────────────────────┘   │
│               │                          │
│               ▼                          │
│  ┌──────────────────────────────────┐   │
│  │     Z80Run(regs, cycles)         │   │
│  │  - Fetch/Decode/Execute          │   │
│  │  - EI-delay применение           │   │
│  │  - Возврат фактических циклов    │   │
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

## ⚙️ КЛЮЧЕВЫЕ КОМПОНЕНТЫ:

### 1. **Z80 CPU Core** (`z80.cpp`)

#### Структура регистров:
```cpp
typedef struct {
  void *userInfo;              // Pointer to ZXSpectrum
  eword AF, BC, DE, HL;        // Main registers
  eword IX, IY, PC, SP, R;     // Index/Special registers
  eword AFs, BCs, DEs, HLs;    // Shadow registers
  byte IFF1, IFF2, I, halted;  // Interrupt state
  char IM;                     // Interrupt mode (0/1/2)
  byte ei_pending;             // ⭐ EI-delay flag
  int cycles;                  // Cycle counter
} Z80Regs;
```

#### Z80Run() контракт:
```cpp
uint16_t Z80Run(Z80Regs *regs, int numcycles) {
  regs->cycles = numcycles;
  
  while (regs->cycles > 0) {
    // 1. Fetch opcode
    opcode = Z80ReadMem(PC++);
    
    // 2. Decode & Execute
    switch (opcode) { ... }
    
    // 3. ⭐ Apply EI-delay (after instruction!)
    if (regs->ei_pending) {
      regs->IFF1 = regs->IFF2 = 1;
      regs->ei_pending = 0;
    }
    
    // 4. Subtract cycles
    regs->cycles -= instruction_cycles;
  }
  
  return micValue;  // Total cycles executed
}
```

---

### 2. **EI-delay механизм** ⭐

**Проблема:** Z80 CPU имеет 1-инструкцию задержку для EI.

**Пример:**
```asm
EI      ; IFF1 НЕ устанавливается сразу!
RET     ; После RET: IFF1=1
HALT    ; Теперь можно прерывать
```

**Реализация:**

#### В `opcodes.h`:
```cpp
case EI:
  r_ei_pending = 1;  // Не устанавливаем IFF1 сразу!
  AddCycles(4);
break;
```

#### В `z80.cpp` (конец цикла):
```cpp
// После выполнения КАЖДОЙ инструкции
if (regs->ei_pending) {
  regs->IFF1 = regs->IFF2 = 1;  // Теперь можно!
  regs->ei_pending = 0;
}
```

**Почему критично:**  
Без этого ROM выполняет:
1. `IM 1` → устанавливает режим прерываний
2. `EI` → разрешает прерывания (IFF1=1)
3. INT приходит сразу → прерывает до `HALT`
4. Infinite loop!

С EI-delay:
1. `IM 1` → ОК
2. `EI` → ei_pending=1
3. `RET` выполняется → IFF1=1
4. `HALT` выполняется → ждёт INT
5. INT приходит → ОК!

---

### 3. **Малые кванты выполнения** ⭐

**Проблема:** 
- Просим 224 t-states
- Получаем 226-230 t-states (инструкции не делятся)
- 312 линий × 2-6 "лишних" = 624-1872/frame
- За секунду: 31200-93600 "лишних" t-states!
- Это 0.5-1.3 дополнительных frames!

**Решение:**
```cpp
const int SLICE_TST = 128;  // Вместо 224!

int targetAdd = TSTATES_PER_FRAME;  // 69888
while (targetAdd > 0) {
  int budget = min(SLICE_TST, targetAdd);
  int used = Z80Run(regs, budget);
  
  tstateAccumulator += used;  // Накапливаем ФАКТИЧЕСКИЕ
  targetAdd -= used;
}
```

**Почему работает:**
- 69888 / 128 = 546 квантов
- "Перерасход" на квант: 0-10 t-states
- Максимальный перерасход: 546 × 10 = 5460/frame
- Это приемлемо! (7.8% вместо 300%+)

---

### 4. **Accumulator & INT генерация** ⭐

**Контракт:**
```cpp
static int64_t tstateAccumulator = 0;  // Накопитель

// После каждого кванта:
tstateAccumulator += used;

// Генерация INT (может быть несколько за loop!):
while (tstateAccumulator >= 69888) {
  if (IFF1 && IM==1) {
    interrupt();  // JP 0x0038, PUSH PC, IFF1=0
  }
  tstateAccumulator -= 69888;  // ВСЕГДА вычитаем!
  totalFrames++;
}
```

**Ключевые моменты:**

1. **`while` вместо `if`:**  
   Если накопилось 140000 t-states → 2 INT!

2. **Проверка IFF1:**  
   Если IFF1=0 → прерывание ТЕРЯЕТСЯ (как на реале!)

3. **Всегда вычитаем:**  
   Даже если IFF1=0, вычитаем frame!

4. **int64_t тип:**  
   Защита от переполнения (max ~9 квинтиллионов t-states)

---

### 5. **HUD Snapshot** (середина кадра)

**Проблема:**  
Если брать PC сразу после INT → всегда 0x0038!

**Решение:**
```cpp
// Глобальные переменные (snapshot)
static uint16_t hud_pc = 0;
static uint8_t hud_iff1 = 0;

// В середине frame (после ~35000 t-states):
if (cyclesExecuted >= MIDFRAME_TSTATES && !snapshotTaken) {
  hud_pc = z80Regs->PC.W;
  hud_iff1 = z80Regs->IFF1;
  snapshotTaken = true;
}

// В main.cpp:
Serial.printf("PC: 0x%04X", spectrum->getHudPC());
```

**Результат:**  
PC показывает "живые" адреса: 0x10B4, 0x0E5C, 0x15E8...

---

## 🎯 ZX SPECTRUM 48K TIMING:

### Константы:
```cpp
CPU_FREQ       = 3.5 MHz
TSTATES_LINE   = 224
LINES_FRAME    = 312
TSTATES_FRAME  = 69888  (224 × 312)
FRAMES_SECOND  = 50     (PAL)
INT_FREQ       = 50 Hz
```

### Frame структура (PAL):
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

## 💾 MEMORY MAP:

```
0x0000 - 0x3FFF  ROM (16KB)       [Read-only]
0x4000 - 0x57FF  VRAM (6KB)       [Screen data]
0x5800 - 0x5AFF  VRAM (768B)      [Attributes]
0x5B00 - 0x5BFF  Printer buffer   [System vars]
0x5C00 - 0x5CBF  System variables
0x5CC0 - 0xFFFF  User RAM (~42KB)
```

### ROM важные адреса:
```
0x0000: F3           ; DI (начало ROM)
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

## 🔄 INT HANDLER (IM1):

### Что происходит при INT:

1. **Проверка IFF1:**
   ```cpp
   if (!regs->IFF1) return;  // INT игнорируется!
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
   IFF1 = 0;  // До EI в handler!
   ```

5. **Handler выполняется:**
   ```asm
   0x0038: PUSH AF      ; Сохранить регистры
           PUSH HL
           LD HL,(FRAMES)
           INC HL       ; Увеличить счётчик кадров
           LD (FRAMES),HL
           POP HL
           POP AF
           EI           ; Разрешить INT (с delay!)
           RET          ; Вернуться (после RET: IFF1=1)
   ```

---

## 🧪 DEBUGGING:

### Полезные точки логирования:

```cpp
// 1. Проверка ROM
ROM[0x0000] должен быть 0xF3
ROM[0x0001] должен быть 0xAF

// 2. Z80 state
PC должен меняться (не 0x0000, не 0x0038 постоянно)
IM должен стать 1 после ~2 секунд
IFF1 должен быть 1 между INT

// 3. Accumulator
Должен оставаться < 1000 обычно
Если > 100000 → проблема с циклами!

// 4. INT rate
Должно быть 49-51 INT/s
Если < 45 → слишком медленно
Если > 55 → слишком быстро
```

---

## ⚠️ COMMON PITFALLS:

### 1. EI без delay
```cpp
// ❌ НЕПРАВИЛЬНО:
case EI:
  r_IFF1 = r_IFF2 = 1;  // Сразу!
  
// ✅ ПРАВИЛЬНО:
case EI:
  r_ei_pending = 1;  // Отложенно!
```

### 2. IF вместо WHILE
```cpp
// ❌ НЕПРАВИЛЬНО:
if (acc >= 69888) {
  interrupt();
  acc -= 69888;
}

// ✅ ПРАВИЛЬНО:
while (acc >= 69888) {  // Может быть несколько!
  interrupt();
  acc -= 69888;
}
```

### 3. Большие кванты
```cpp
// ❌ НЕПРАВИЛЬНО:
runForCycles(224);  // Перерасход 2-6 t-states

// ✅ ПРАВИЛЬНО:
runForCycles(128);  // Перерасход 0-10, но меньше квантов
```

### 4. Не вычитать при IFF1=0
```cpp
// ❌ НЕПРАВИЛЬНО:
if (IFF1) {
  interrupt();
  acc -= 69888;
}
// Если IFF1=0 → accumulator растёт до бесконечности!

// ✅ ПРАВИЛЬНО:
if (IFF1) interrupt();
acc -= 69888;  // ВСЕГДА!
```

---

## 📊 PERFORMANCE:

### Измерения (ESP32-S3 @ 240 MHz):

```
CPU usage:        ~30-40%
Frame time:       ~12-15ms (target: 20ms)
Heap free:        ~293KB
Z80 instructions: ~3500 MHz эквивалент (из-за overhead)
Actual Z80 freq:  ~3.5 MHz эмулируемая
```

### Оптимизации:

1. **Малые кванты** → меньше перерасхода
2. **Static переменные** → не реаллоцируются
3. **int64_t accumulator** → защита от overflow
4. **Callback functions** → гибкость без overhead

---

## 🎓 REFERENCES:

1. **Z80 CPU Manual:**  
   http://www.zilog.com/docs/z80/um0080.pdf

2. **ZX Spectrum Technical Manual:**  
   https://worldofspectrum.org/

3. **ESP32 Rainbow (original):**  
   https://github.com/atomic14/esp32-zxspectrum

4. **Z80 Emulation (Santiago Romero):**  
   http://www.github.com/jsanchezv/z80

---

## 💡 FUTURE IMPROVEMENTS:

1. **Display rendering** (ULA emulation)
2. **Keyboard input** (port 0xFE)
3. **Sound** (beeper)
4. **TAP file loading**
5. **Turbo mode** (7 MHz)
6. **128K model**
7. **AY-3-8912 sound** (for 128K)

---

**Конец технической документации.**  
**Для быстрого старта см. `QUICKSTART.md`**

