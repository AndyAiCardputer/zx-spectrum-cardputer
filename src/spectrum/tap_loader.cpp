#include "tap_loader.h"
#include "tape_listener.h"
#include "tape_cas.h"
#include <SD.h>

TAPLoader::TAPLoader() {
  blocks = nullptr;
  blockCount = 0;
  maxBlocks = 0;
  lastError[0] = '\0';
}

TAPLoader::~TAPLoader() {
  if (blocks) {
    // Освобождаем данные блоков
    for (int i = 0; i < blockCount; i++) {
      if (blocks[i].data) {
        free(blocks[i].data);
      }
    }
    free(blocks);
  }
}

// ═══════════════════════════════════════════════════════════
// LOAD TAP - TAPE EMULATION (правильный способ!)
// ═══════════════════════════════════════════════════════════

bool TAPLoader::loadTAP(const char* filename, ZXSpectrum* spectrum, RenderCallback renderCallback) {
  Serial.println("\n═══════════════════════════════════════════");
  Serial.printf("📼 LOADING TAP (TAPE EMULATION): %s\n", filename);
  Serial.println("═══════════════════════════════════════════");
  
  // ═══ 1. ОТКРЫВАЕМ ФАЙЛ ═══
  File file = SD.open(filename);
  if (!file) {
    snprintf(lastError, sizeof(lastError), "Failed to open file");
    Serial.println("❌ Failed to open TAP file");
    return false;
  }
  
  size_t fileSize = file.size();
  Serial.printf("📊 File size: %d bytes\n", fileSize);
  
  // ═══ 2. ЧИТАЕМ ФАЙЛ В ПАМЯТЬ ═══
  uint8_t* tapData = (uint8_t*)malloc(fileSize);
  if (!tapData) {
    snprintf(lastError, sizeof(lastError), "Out of memory");
    file.close();
    Serial.println("❌ Out of memory");
    return false;
  }
  
  file.read(tapData, fileSize);
  file.close();
  
  // ═══ 3. RESET SPECTRUM ═══
  Serial.println("\n🔄 Resetting Spectrum...");
  spectrum->reset();
  
  // ═══ 4. ЖДЁМ ИНИЦИАЛИЗАЦИЮ ROM (~200 кадров = 4 секунды) ═══
  Serial.println("⏳ Waiting for ROM initialization...");
  for (int i = 0; i < 200; i++) {
    spectrum->runForFrame(nullptr);  // Без звука во время загрузки
    if (i % 50 == 0) {
      Serial.printf("  Frame %d/200\n", i);
    }
  }
  
  // ═══ 5. СИМУЛИРУЕМ НАЖАТИЕ "LOAD ""  ═══
  Serial.println("\n⌨️  Typing: LOAD \"\"");
  
  // J = LOAD
  spectrum->updateKey(SPECKEY_J, 1);
  for (int i = 0; i < 10; i++) spectrum->runForFrame(nullptr);
  spectrum->updateKey(SPECKEY_J, 0);
  for (int i = 0; i < 10; i++) spectrum->runForFrame(nullptr);
  
  // SYMBOL SHIFT + P = "
  spectrum->updateKey(SPECKEY_SYMB, 1);
  for (int i = 0; i < 10; i++) spectrum->runForFrame(nullptr);
  spectrum->updateKey(SPECKEY_P, 1);
  for (int i = 0; i < 10; i++) spectrum->runForFrame(nullptr);
  spectrum->updateKey(SPECKEY_P, 0);
  for (int i = 0; i < 10; i++) spectrum->runForFrame(nullptr);
  
  // SYMBOL SHIFT + P = " (снова)
  spectrum->updateKey(SPECKEY_P, 1);
  for (int i = 0; i < 10; i++) spectrum->runForFrame(nullptr);
  spectrum->updateKey(SPECKEY_P, 0);
  for (int i = 0; i < 10; i++) spectrum->runForFrame(nullptr);
  spectrum->updateKey(SPECKEY_SYMB, 0);
  
  // ENTER
  spectrum->updateKey(SPECKEY_ENTER, 1);
  for (int i = 0; i < 10; i++) spectrum->runForFrame(nullptr);
  spectrum->updateKey(SPECKEY_ENTER, 0);
  for (int i = 0; i < 10; i++) spectrum->runForFrame(nullptr);
  
  Serial.println("✅ LOAD \"\" entered!");
  
  // ═══ 6. ЗАПУСКАЕМ TAPE EMULATION ═══
  Serial.println("\n📼 Starting tape emulation...");
  Serial.println("🎨 Loading screen будет построчно!");
  Serial.println("═══════════════════════════════════════════");
  
  TapeListener* listener = new TapeListener(spectrum, renderCallback);
  TapeCas tapCas;
  
  bool success = tapCas.loadTap(listener, tapData, fileSize);
  
  delete listener;
  free(tapData);
  
  if (!success) {
    snprintf(lastError, sizeof(lastError), "Tape emulation failed");
    Serial.println("\n❌ TAPE EMULATION FAILED!");
    return false;
  }
  
  Serial.println("\n═══════════════════════════════════════════");
  Serial.println("✅ TAP FILE LOADED SUCCESSFULLY!");
  Serial.println("═══════════════════════════════════════════");
  
  return true;
}

// ═══════════════════════════════════════════════════════════
// СТАРЫЕ ФУНКЦИИ (не используются при tape emulation)
// ═══════════════════════════════════════════════════════════

bool TAPLoader::parseTAP(uint8_t* data, size_t fileSize) {
  // Не используется при tape emulation
  return false;
}

bool TAPLoader::loadBlock(TAPBlock& block, ZXSpectrum* spectrum) {
  // Не используется при tape emulation
  return false;
}
