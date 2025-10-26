#ifndef TAP_LOADER_H
#define TAP_LOADER_H

#include <Arduino.h>
#include <functional>
#include "spectrum_mini.h"

// Callback для рендеринга loading screen
using RenderCallback = std::function<void()>;

// ═══════════════════════════════════════════════════════════
// 📼 INSTANT TAP LOADER (простая загрузка без эмуляции ленты)
// ═══════════════════════════════════════════════════════════
// 
// Формат .TAP:
// [2 bytes: length] [data...] [2 bytes: length] [data...] ...
//
// Блоки:
// - Header (0x00): [тип] [имя 10 байт] [length] [addr] [param2] [checksum]
// - Data (0xFF): [данные...] [checksum]
//
// Типы:
// 0x00 = Program (BASIC)
// 0x01 = Number array
// 0x02 = Character array
// 0x03 = Code (machine code) ← нам нужен этот!
// ═══════════════════════════════════════════════════════════

struct TAPBlock {
  uint8_t flag;        // 0x00 = header, 0xFF = data
  uint8_t type;        // Тип блока (для header)
  char name[11];       // Имя файла (для header)
  uint16_t length;     // Длина данных
  uint16_t addr;       // Адрес загрузки (для CODE)
  uint16_t param2;     // Дополнительный параметр
  uint8_t* data;       // Указатель на данные (для data блока)
  bool isHeader;       // true = header, false = data
};

class TAPLoader {
public:
  TAPLoader();
  ~TAPLoader();
  
  // Загрузить .TAP файл (tape emulation с loading screen!)
  bool loadTAP(const char* filename, ZXSpectrum* spectrum, RenderCallback renderCallback = nullptr);
  
  // Информация о последней загрузке
  const char* getLastError() { return lastError; }
  int getBlockCount() { return blockCount; }
  
private:
  // Парсинг TAP файла
  bool parseTAP(uint8_t* data, size_t fileSize);
  
  // Загрузка блока в память
  bool loadBlock(TAPBlock& block, ZXSpectrum* spectrum);
  
  // Утилиты
  uint16_t readU16LE(uint8_t* p) {
    return p[0] | (p[1] << 8);
  }
  
  TAPBlock* blocks;
  int blockCount;
  int maxBlocks;
  char lastError[128];
};

#endif // TAP_LOADER_H

