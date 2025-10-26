#ifndef SPECTRUM_MINI_H
#define SPECTRUM_MINI_H

#include <Arduino.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../z80/z80.h"
#include "keyboard_defs.h"

// Global keyboard state (8 rows, bit 0 = pressed)
extern uint8_t speckey[8];

// ZX Spectrum 16-color palette in RGB565 format
extern const uint16_t specpal565[16];

enum models_enum
{
  SPECMDL_48K = 2,
};

// Hardware options for ULA timing
typedef struct
{
  int ts_lebo;   // left border t states
  int ts_grap;   // graphic zone t states
  int ts_ribo;   // right border t states
  int ts_hore;   // horizontal retrace t states
  int ts_line;   // total tstates per line
  int line_poin; // lines in retrace post interrupt
  int line_upbo; // lines of upper border
  int line_grap; // lines of graphic zone = 192
  int line_bobo; // lines of bottom border
  int line_retr; // lines of the retrace
  models_enum hw_model;
  int emulate_FF;
  uint8_t BorderColor;
  uint8_t portFF;
  uint8_t SoundBits;
} tipo_hwopt;

// Simple 48K memory layout: ROM + 48K RAM
class Memory {
public:
  uint8_t *rom = nullptr;      // 16K ROM @ 0x0000-0x3FFF
  uint8_t *ram = nullptr;      // 48K RAM @ 0x4000-0xFFFF
  uint8_t *screen = nullptr;   // Pointer to screen @ 0x4000

  Memory() {
    // Allocate ROM (16K)
    rom = (uint8_t *)malloc(0x4000);
    if (!rom) {
      Serial.println("Failed to allocate ROM");
    }
    memset(rom, 0, 0x4000);

    // Allocate RAM (48K)
    ram = (uint8_t *)malloc(0xC000);
    if (!ram) {
      Serial.println("Failed to allocate RAM");
    }
    memset(ram, 0, 0xC000);

    // Screen points to 0x4000 in RAM
    screen = ram;
  }

  ~Memory() {
    if (rom) free(rom);
    if (ram) free(ram);
  }

  inline uint8_t peek(uint16_t address) {
    if (address < 0x4000) {
      return rom[address];
    } else {
      return ram[address - 0x4000];
    }
  }

  inline void poke(uint16_t address, uint8_t value) {
    if (address >= 0x4000) {
      ram[address - 0x4000] = value;
    }
    // Ignore writes to ROM
  }

  void loadRom(const uint8_t *rom_data, int rom_len) {
    if (rom_len > 0x4000) rom_len = 0x4000;
    memcpy(rom, rom_data, rom_len);
    Serial.printf("ROM loaded: %d bytes\n", rom_len);
  }
  
  // Direct access to VRAM (для быстрого рендеринга)
  inline uint8_t* getScreenData() {
    return screen;  // Указатель на начало VRAM (0x4000)
  }
  
  // Direct access to RAM (для загрузки .SNA файлов)
  inline uint8_t* getRamData() {
    return ram;  // Указатель на начало RAM (0x4000-0xFFFF = 48KB)
  }
};

class ZXSpectrum
{
public:
  Z80Regs *z80Regs;
  Memory mem;
  tipo_hwopt hwopt = {};
  bool micLevel = false;
  uint8_t borderColors[312] = {0};
  uint8_t borderColor = 7;  // Текущий цвет border (0-7)
  
  // ═══ ЗВУКОВАЯ СИСТЕМА (BEEPER) ═══
  uint8_t soundBits = 0;           // Бит 4 из порта 0xFE (beeper state)
  uint16_t soundAccumulator = 0;   // Накопленные t-states когда beeper активен

  ZXSpectrum();
  void reset();
  int runForFrame(uint16_t *accumOut);  // ✅ V3.111: uint16_t[312] для ChatGPT beeper
  inline int runForCycles(int cycles) {
    static int callCount = 0;
    uint16_t pcBefore = z80Regs->PC.W;
    int cyclesBefore = z80Regs->cycles;
    
    int used = Z80Run(z80Regs, cycles);
    
    // ═══ НАКОПЛЕНИЕ ЗВУКА ═══
    // Если beeper активен (soundBits != 0), накапливаем t-states
    if (soundBits) {
      soundAccumulator += used;
    }
    
    uint16_t pcAfter = z80Regs->PC.W;
    int cyclesAfter = z80Regs->cycles;
    
    // КРИТИЧНО: used должен быть > 0 (минимум 4 цикла на NOP)
    if (used <= 0) {
      Serial.printf("⚠️ Z80Run FAILED: used=%d req=%d PC=0x%04X\n", used, cycles, z80Regs->PC.W);
    }
    
    return used;
  }

  void interrupt();
  void updateKey(SpecKeys key, uint8_t state);

  inline uint8_t z80_peek(uint16_t address) {
    return mem.peek(address);
  }

  inline void z80_poke(uint16_t address, uint8_t value) {
    mem.poke(address, value);
  }

  // Port 0xFE read (keyboard + border + mic)
  inline uint8_t z80_in(uint16_t port) {
    if ((port & 0x01) == 0) {
      uint8_t data = 0xFF;
      if (!(port & 0x0100)) data &= speckey[0]; // SHIFT, Z-V
      if (!(port & 0x0200)) data &= speckey[1]; // A-G
      if (!(port & 0x0400)) data &= speckey[2]; // Q-T
      if (!(port & 0x0800)) data &= speckey[3]; // 1-5
      if (!(port & 0x1000)) data &= speckey[4]; // 0-9
      if (!(port & 0x2000)) data &= speckey[5]; // P-Y
      if (!(port & 0x4000)) data &= speckey[6]; // ENTER-H
      if (!(port & 0x8000)) data &= speckey[7]; // SPACE-B

      // Bit 6 = MIC/EAR
      if (micLevel) {
        data |= 0x40;
      } else {
        data &= 0xBF;
      }
      return data;
    }
    // Port 0xFF emulation
    if ((port & 0xFF) == 0xFF && hwopt.emulate_FF) {
      return hwopt.portFF;
    }
    return 0xFF;
  }

  // Port 0xFE write (border + beeper)
  inline void z80_out(uint16_t port, uint8_t data) {
    if (!(port & 0x01)) {  // Порт 0xFE (ULA)
      borderColor = (data & 0x07);        // Биты 0-2: Border Color
      soundBits = (data & 0b00010000);    // Бит 4: BEEPER (звук!)
    }
  }

  bool init_48k();
  void reset_spectrum();
  
  // ═══ TAPE EMULATION: MIC/EAR CONTROL ═══
  inline void setMicHigh() {
    micLevel = true;
  }
  
  inline void setMicLow() {
    micLevel = false;
  }
  
  inline void toggleMic() {
    micLevel = !micLevel;
  }
  
  // HUD getters (snapshot taken at mid-frame, not during INT!)
  uint16_t getHudPC() const;
  uint8_t getHudIM() const;
  uint8_t getHudIFF1() const;
  uint16_t getHudSP() const;
};

#endif // SPECTRUM_MINI_H

