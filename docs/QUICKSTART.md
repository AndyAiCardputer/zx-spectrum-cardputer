# 🚀 ZX SPECTRUM EMULATOR - QUICK START

**Платформа:** M5Stack Cardputer  
**Версия:** 1.0 (Emulator Core - Working!)  
**Дата:** 24 октября 2025

---

## ✅ ЧТО УЖЕ РАБОТАЕТ:

- ✅ Z80 CPU эмуляция (3.5 MHz)
- ✅ 16KB ROM (Sinclair 48K)
- ✅ 48KB RAM
- ✅ Interrupt Mode 1 (50 Hz)
- ✅ EI/DI/HALT инструкции
- ✅ 50 FPS stable timing

**Статус:** Эмулятор работает, ROM выполняется, INT обрабатываются!

---

## 📦 ТРЕБОВАНИЯ:

- **Hardware:** M5Stack Cardputer
- **Software:** PlatformIO
- **USB:** Кабель USB-C

---

## 🛠️ УСТАНОВКА:

### 1. Подключите Cardputer к компьютеру

### 2. Соберите проект:
```bash
cd /Users/a15/Desktop/cardputer/zx_rainbow_cardputer
pio run --target upload
```

### 3. Откройте монитор:
```bash
pio device monitor --baud 115200
```

Или используйте `screen`:
```bash
screen /dev/cu.usbmodem14401 115200
```

---

## 📊 ОЖИДАЕМЫЙ ВЫВОД:

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
✨ IM=1 обнаружен на frame #81!

FPS: 50.00 | INT: 50.00/s | PC: 0x10B4 | IM: 1 | IFF1: 1 ✅
```

**Если видите такой вывод — эмулятор РАБОТАЕТ!** 🎉

---

## 🔍 ПРОВЕРКА РАБОТОСПОСОБНОСТИ:

### ✅ Критерии успеха:

1. **ROM verified:** `✅ ROM verified: correct ZX Spectrum 48K ROM`
2. **FPS:** 49.95-50.00
3. **INT rate:** 49.95-50.00/s
4. **IM:** 1 (после ~81 frames)
5. **IFF1:** 1 (в HUD, между прерываниями)
6. **PC:** меняется (не застревает на 0x0038 или 0x0000)

### ❌ Признаки проблем:

- `🔴 FATAL: ROM signature mismatch` → неправильный ROM
- `PC: 0x0000` постоянно → CPU не выполняется
- `IM: 0` постоянно → ROM не инициализировался
- `IFF1: 0` постоянно → EI-delay не работает

---

## 🐛 TROUBLESHOOTING:

### Проблема: Device not configured
```
Disconnected (read failed: [Errno 6] Device not configured)
```

**Решение:**
1. Нажмите **RESET** на Cardputer
2. Быстро переподключите монитор
3. Или используйте `screen` вместо `pio device monitor`

---

### Проблема: Upload failed
```
A fatal error occurred: Failed to connect to ESP32
```

**Решение:**
1. Удерживайте **G0** кнопку при подключении USB
2. Или нажмите **RESET** во время upload
3. Проверьте USB кабель (должен поддерживать данные, не только питание)

---

### Проблема: Compilation failed
```
fatal error: 48k_rom.h: No such file or directory
```

**Решение:**
Убедитесь что файл `src/spectrum/48k_rom.h` существует и содержит:
```cpp
const unsigned char gb_rom_0_sinclair_48k[]={
0xF3,0xAF,0x11,0xFF,0xFF,0xC3,0xCB,0x11,...
```

---

## 📁 СТРУКТУРА ФАЙЛОВ:

```
zx_rainbow_cardputer/
├── platformio.ini              # Конфигурация проекта
├── src/
│   ├── main.cpp                # Главный файл
│   ├── spectrum/
│   │   ├── spectrum_mini.h     # ZX Spectrum класс
│   │   ├── spectrum_mini.cpp   # Реализация эмулятора
│   │   ├── 48k_rom.h           # ROM (16KB)
│   │   └── memory.h            # Память (ROM+RAM)
│   └── z80/
│       ├── z80.h               # Z80 регистры
│       ├── z80.cpp             # Z80 эмуляция
│       ├── macros.h            # Макросы
│       └── opcodes.h           # Инструкции Z80
└── SUCCESS_REPORT_V1.0.md      # Этот документ!
```

---

## 🎯 СЛЕДУЮЩИЕ ШАГИ:

После успешного запуска эмулятора:

1. **Убрать debug логи** (INT #0-100) → чистый вывод
2. **Добавить экран** → увидеть VRAM
3. **Добавить клавиатуру** → вводить команды
4. **Загрузить игру** → SABOTEUR! 🎮

---

## 📚 ДОКУМЕНТАЦИЯ:

- `SUCCESS_REPORT_V1.0.md` - Полный отчёт о достижениях
- `TECHNICAL_DETAILS.md` - Технические детали реализации
- `SUMMARY_RU.md` - Резюме проекта (старое)

---

## 💡 ПОЛЕЗНЫЕ КОМАНДЫ:

### Сборка без загрузки:
```bash
pio run
```

### Только загрузка:
```bash
pio run --target upload
```

### Очистка проекта:
```bash
pio run --target clean
```

### Логи в файл:
```bash
pio device monitor --baud 115200 > logs.txt
```

---

## 🎉 УСПЕХ!

Если вы видите:
```
FPS: 50.00 | INT: 50.00/s | PC: 0x10B4 | IM: 1 | IFF1: 1 ✅
```

**ПОЗДРАВЛЯЕМ!** У вас работает ZX Spectrum эмулятор на Cardputer! 🚀

---

**Вопросы?** Проверьте `SUCCESS_REPORT_V1.0.md` для деталей.
