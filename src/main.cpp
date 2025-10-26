#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include "spectrum/spectrum_mini.h"
#include "spectrum/keyboard_defs.h"
#include "spectrum/tap_loader.h"  // ✅ TAP Loader!
#include "spectrum/z80_loader.h"  // ✅ Z80 Loader! (V3.134)

// ============================================
// ШАГ 3: Эмулятор С ДИСПЛЕЕМ + ЦВЕТА!
// ============================================

// ═══════════════════════════════════════════
// FORWARD DECLARATIONS (V3.134)
// ═══════════════════════════════════════════
void Task_Audio(void* pv);
void ZX_BeeperSubmitFrame(const uint16_t* accum312);
void showNotification(const char* text, uint16_t color, unsigned long duration);
void drawNotificationOverlay();

// ═══════════════════════════════════════════
// SD CARD CONFIGURATION
// ═══════════════════════════════════════════
#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12

// ═══════════════════════════════════════════
// JOYSTICK2 CONFIGURATION
// ═══════════════════════════════════════════
#define JOYSTICK2_ADDR  0x63
#define JOYSTICK2_SDA   2
#define JOYSTICK2_SCL   1

// Регистры Joystick2Unit (из официальной документации)
#define REG_ADC_X_8   0x10  // X ADC 8-bit (0-255)
#define REG_ADC_Y_8   0x11  // Y ADC 8-bit (0-255)
#define REG_BUTTON    0x20  // Button (1=no press, 0=press)

struct Joystick2Data {
  uint8_t x;       // 0-255 (центр: ~127)
  uint8_t y;       // 0-255 (центр: ~127)
  uint8_t button;  // 0 = отпущена, 1 = нажата
};

bool joystick2Available = false;

// ГЛОБАЛЬНОЕ состояние кнопки джойстика (чтобы избежать двойного нажатия между меню/браузером)
bool globalJoyButtonState = false;           // Текущее состояние кнопки
unsigned long globalJoyButtonPressTime = 0;  // Время последнего нажатия
const unsigned long JOY_BUTTON_DEBOUNCE = 500; // Строгий debounce 500ms

// ГЛОБАЛЬНОЕ состояние кнопки ENTER (чтобы избежать двойного нажатия между меню/браузером)
bool globalEnterButtonState = false;           // Текущее состояние кнопки Enter
unsigned long globalEnterButtonPressTime = 0;  // Время последнего нажатия Enter
const unsigned long ENTER_BUTTON_DEBOUNCE = 500; // Строгий debounce 500ms

// ═══════════════════════════════════════════
// JOYSTICK-TO-KEYS STATE
// ═══════════════════════════════════════════
bool joystickEnabled = true;  // Включен ли джойстик для игр (toggle Opt+J)
// Джойстик мапится на клавиши QAOP + Space (ИНВЕРТИРОВАНО):
// Физический UP → A, DOWN → Q, LEFT → P, RIGHT → O, FIRE → Space

// ═══════════════════════════════════════════
// SOUND SYSTEM STATE - V3.134 ChatGPT Solution! 🎵
// ═══════════════════════════════════════════
bool soundEnabled = true;    // Включен ли звук (toggle Opt+M)
int soundVolume = 5;         // Громкость 0-10 (было 3, вернули 5)

// ═══ АУДИО ПАРАМЕТРЫ (от ChatGPT) ═══
static constexpr int SAMPLE_RATE   = 16000;   // 16 kHz I2S
static constexpr int SPPF          = 320;     // samples per frame @50fps = 20ms
static constexpr int TSTATES_LINE_CONST = 224; // тактов/строку
static constexpr int AMP           = 9000;    // амплитуда (без клиппинга)
static constexpr int ENV_NS        = 48;      // ~3ms огибающая

// ═══ ДВОЙНОЙ БУФЕР (моно 16-бит) ═══
static int16_t bufA[SPPF];
static int16_t bufB[SPPF];
static volatile bool useA = true;

// ═══ ВХОДНЫЕ ДАННЫЕ от эмулятора ═══
static uint16_t accumBuffer[312];             // RAW accumulator от эмулятора
static volatile uint16_t accumFrame[312];     // Копия для Audio Task
static volatile bool frameReady = false;

// ═══════════════════════════════════════════
// NOTIFICATION SYSTEM (async overlay)
// ═══════════════════════════════════════════
bool notificationActive = false;
unsigned long notificationStartTime = 0;
unsigned long notificationDuration = 500;  // ms
String notificationText = "";
uint16_t notificationColor = TFT_YELLOW;

// ═══════════════════════════════════════════
// FILE BROWSER STATE
// ═══════════════════════════════════════════
String fileList[100];      // Список файлов (макс. 100)
int fileCount = 0;         // Количество найденных файлов
bool sdCardAvailable = false; // SD-карта доступна?
int gameFolderStatus = -1;    // V3.137: Статус папки ZXgames (0=created, 1=exists, -1=error)
bool folderNotificationShown = false; // V3.137: Уведомление о папке показано?

// ═══════════════════════════════════════════
// MENU STATE
// ═══════════════════════════════════════════
bool showMenu = false;        // Флаг отображения меню
bool showLoadGameMenu = false; // Флаг отображения подменю "Load Game" (V3.134)
bool emulatorPaused = false;  // Эмулятор на паузе?
bool gamePaused = false;      // V3.134: Игра на паузе (TAB)
int selectedMenuItem = 0;     // Выбранный пункт меню (0-3) V3.134: 4 пункта, убран "Back"
int selectedLoadGameItem = 0; // Выбранный пункт в подменю "Load Game" (0-3) V3.134: 4 пункта
bool showInformation = false; // Флаг отображения Information
int informationPage = 0;      // Текущая страница Information (0-5: Hotkeys1, Hotkeys2, ZX Buttons, Credits, Thanks1, Thanks2)
int snaCount = 0;             // Количество .SNA файлов
int tapCount = 0;             // Количество .TAP файлов
int z80Count = 0;             // Количество .Z80 файлов

// Бегущая строка для длинных названий в меню
int menuScrollOffset = 0;           // Смещение прокрутки (в символах)
unsigned long menuScrollStartTime = 0;  // Время начала прокрутки
unsigned long lastMenuScrollTime = 0;   // Время последнего обновления прокрутки
const int MENU_SCROLL_DELAY = 2000;     // Задержка перед началом прокрутки (2 сек)
const int MENU_SCROLL_SPEED = 150;      // Скорость прокрутки (150ms между символами)

// ═══════════════════════════════════════════
// FILE BROWSER STATE
// ═══════════════════════════════════════════
bool showBrowser = false;           // Флаг отображения браузера
String browserFilter = "";          // Фильтр: ".SNA", ".TAP", ".Z80"
String filteredFiles[100];          // Отфильтрованный список
int filteredCount = 0;              // Количество файлов после фильтра
int selectedFile = 0;               // Выбранный файл (индекс)

// ZX Spectrum палитра (RGB565)
extern const uint16_t specpal565[16];

ZXSpectrum *spectrum = nullptr;

// Телеметрия
unsigned long lastStatsTime = 0;
uint32_t frameCount = 0;
uint32_t intCount = 0;

// Z80 callback функции
extern "C" {
  byte Z80MemRead(uint16_t address, void *userInfo) {
    ZXSpectrum *spec = (ZXSpectrum *)userInfo;
    return spec->z80_peek(address);
  }

  void Z80MemWrite(uint16_t address, byte data, void *userInfo) {
    ZXSpectrum *spec = (ZXSpectrum *)userInfo;
    spec->z80_poke(address, data);
  }

  byte Z80InPort(uint16_t port, void *userInfo) {
    ZXSpectrum *spec = (ZXSpectrum *)userInfo;
    return spec->z80_in(port);
  }

  void Z80OutPort(uint16_t port, byte data, void *userInfo) {
    ZXSpectrum *spec = (ZXSpectrum *)userInfo;
    spec->z80_out(port, data);
  }
}

// ═══════════════════════════════════════════
// SD CARD FUNCTIONS
// ═══════════════════════════════════════════

void scanSDFiles() {
  Serial.println("\n═══════════════════════════════════════════");
  Serial.println("📂 SCANNING /ZXgames/ FOR GAMES...");
  Serial.println("═══════════════════════════════════════════");
  
  fileCount = 0;
  snaCount = 0;
  tapCount = 0;
  z80Count = 0;
  
  // V3.137: Сканируем /ZXgames/ вместо корня
  File root = SD.open("/ZXgames");
  if (!root) {
    Serial.println("❌ Failed to open /ZXgames/ directory!");
    return;
  }
  
  if (!root.isDirectory()) {
    Serial.println("❌ /ZXgames/ is not a directory!");
    return;
  }
  
  File file = root.openNextFile();
  while (file && fileCount < 100) {
    if (!file.isDirectory()) {
      String fileName = String(file.name());
      
      // Пропускаем системные файлы
      if (!fileName.startsWith(".") && !fileName.startsWith("_")) {
        // Проверяем расширения: .SNA, .TAP, .Z80
        if (fileName.endsWith(".SNA") || fileName.endsWith(".sna")) {
          fileList[fileCount++] = fileName;
          snaCount++;
          Serial.printf("  ✅ [%2d] %s (%d bytes)\n", fileCount, fileName.c_str(), file.size());
        } else if (fileName.endsWith(".TAP") || fileName.endsWith(".tap")) {
          fileList[fileCount++] = fileName;
          tapCount++;
          Serial.printf("  ✅ [%2d] %s (%d bytes)\n", fileCount, fileName.c_str(), file.size());
        } else if (fileName.endsWith(".Z80") || fileName.endsWith(".z80")) {
          fileList[fileCount++] = fileName;
          z80Count++;
          Serial.printf("  ✅ [%2d] %s (%d bytes)\n", fileCount, fileName.c_str(), file.size());
        }
      }
    }
    file = root.openNextFile();
  }
  
  Serial.println("═══════════════════════════════════════════");
  Serial.printf("📊 TOTAL GAMES FOUND: %d\n", fileCount);
  Serial.printf("   .SNA files: %d\n", snaCount);
  Serial.printf("   .TAP files: %d\n", tapCount);
  Serial.printf("   .Z80 files: %d\n", z80Count);
  Serial.println("═══════════════════════════════════════════\n");
}

// ═══════════════════════════════════════════
// SETUP GAME FOLDERS (V3.137)
// ═══════════════════════════════════════════
// Возвращает: 0=created, 1=already exists, -1=error
int setupGameFolders() {
  if (!sdCardAvailable) {
    return -1;
  }
  
  Serial.println("\n═══════════════════════════════════════════");
  Serial.println("📁 CHECKING GAME FOLDERS...");
  Serial.println("═══════════════════════════════════════════");
  
  // Проверяем существование папки /ZXgames
  if (SD.exists("/ZXgames")) {
    Serial.println("✅ /ZXgames/ folder already exists");
    return 1;  // Папка уже существует
  }
  
  // Создаём папку
  Serial.println("📂 Creating /ZXgames/ folder...");
  if (!SD.mkdir("/ZXgames")) {
    Serial.println("❌ Failed to create /ZXgames/ folder!");
    return -1;  // Ошибка
  }
  
  Serial.println("✅ /ZXgames/ folder created successfully!");
  Serial.println("   Please put your .tap, .z80, .sna files there");
  Serial.println("═══════════════════════════════════════════\n");
  
  return 0;  // Папка создана
}

void initSDCard() {
  Serial.println("\n═══════════════════════════════════════════");
  Serial.println("💾 INITIALIZING SD CARD...");
  Serial.println("═══════════════════════════════════════════");
  Serial.printf("  SPI Pins: SCK=%d MISO=%d MOSI=%d CS=%d\n", 
                SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  
  // Инициализация SPI
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  
  // Инициализация SD-карты (25 MHz)
  if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
    Serial.println("❌ SD CARD INITIALIZATION FAILED!");
    Serial.println("   - Check if SD card is inserted");
    Serial.println("   - Check SPI connections");
    sdCardAvailable = false;
    return;
  }
  
  // Определяем тип карты
  uint8_t cardType = SD.cardType();
  
  if (cardType == CARD_NONE) {
    Serial.println("❌ NO SD CARD ATTACHED!");
    sdCardAvailable = false;
    return;
  }
  
  Serial.print("✅ SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  
  // Размер карты
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("✅ SD Card Size: %llu MB\n", cardSize);
  Serial.printf("✅ Total Space: %llu MB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("✅ Used Space: %llu MB\n", SD.usedBytes() / (1024 * 1024));
  
  sdCardAvailable = true;
  
  // Сканируем файлы
  scanSDFiles();
}

// ═══════════════════════════════════════════
// JOYSTICK2 FUNCTIONS
// ═══════════════════════════════════════════

Joystick2Data readJoystick2() {
  Joystick2Data data = {127, 127, 0};  // Центр по умолчанию
  
  if (!joystick2Available) {
    return data;
  }
  
  // Читаем X (регистр 0x10)
  Wire.beginTransmission(JOYSTICK2_ADDR);
  Wire.write(REG_ADC_X_8);
  if (Wire.endTransmission(false) == 0) {
    Wire.requestFrom(JOYSTICK2_ADDR, 1);
    if (Wire.available() >= 1) {
      data.x = Wire.read();
    }
  }
  
  // Читаем Y (регистр 0x11)
  Wire.beginTransmission(JOYSTICK2_ADDR);
  Wire.write(REG_ADC_Y_8);
  if (Wire.endTransmission(false) == 0) {
    Wire.requestFrom(JOYSTICK2_ADDR, 1);
    if (Wire.available() >= 1) {
      data.y = Wire.read();
    }
  }
  
  // Читаем кнопку (регистр 0x20)
  // ВНИМАНИЕ: 0 = нажата, 1 = не нажата (инвертировано!)
  Wire.beginTransmission(JOYSTICK2_ADDR);
  Wire.write(REG_BUTTON);
  if (Wire.endTransmission(false) == 0) {
    Wire.requestFrom(JOYSTICK2_ADDR, 1);
    if (Wire.available() >= 1) {
      uint8_t rawButton = Wire.read();
      data.button = (rawButton == 0) ? 1 : 0;  // Инвертируем
    }
  }
  
  return data;
}

void initJoystick2() {
  Serial.println("\n═══════════════════════════════════════════");
  Serial.println("🕹️  INITIALIZING JOYSTICK2...");
  Serial.println("═══════════════════════════════════════════");
  Serial.printf("  I2C: SDA=G%d, SCL=G%d\n", JOYSTICK2_SDA, JOYSTICK2_SCL);
  Serial.printf("  Address: 0x%02X\n", JOYSTICK2_ADDR);
  
  // Инициализация I2C
  Wire.begin(JOYSTICK2_SDA, JOYSTICK2_SCL, 100000);  // 100 kHz
  delay(100);
  
  // Проверяем наличие Joystick2
  Wire.beginTransmission(JOYSTICK2_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.printf("✅ Joystick2 detected at 0x%02X!\n", JOYSTICK2_ADDR);
    joystick2Available = true;
    
    // Тестовое чтение
    Joystick2Data test = readJoystick2();
    Serial.printf("  Test read: X=%d Y=%d BTN=%d\n", test.x, test.y, test.button);
  } else {
    Serial.println("❌ Joystick2 NOT found!");
    Serial.println("   (Browser will use keyboard only)");
    joystick2Available = false;
  }
  
  Serial.println("═══════════════════════════════════════════\n");
}

// ═══════════════════════════════════════════
// FORWARD DECLARATIONS
// ═══════════════════════════════════════════
void renderScreen();  // Нужно для TAP loader callback

// ═══════════════════════════════════════════
// FILE LOADER FUNCTIONS
// ═══════════════════════════════════════════

bool loadSNAFile(String filename) {
  Serial.println("\n═══════════════════════════════════════════");
  Serial.printf("📂 LOADING .SNA FILE: %s\n", filename.c_str());
  Serial.println("═══════════════════════════════════════════");
  
  // Открываем файл
  // V3.137: Загружаем из /ZXgames/
  File file = SD.open("/ZXgames/" + filename);
  if (!file) {
    Serial.println("❌ Failed to open file!");
    return false;
  }
  
  // Проверяем размер (48K SNA = 49179 байт)
  size_t fileSize = file.size();
  Serial.printf("  File size: %d bytes\n", fileSize);
  
  if (fileSize != 49179) {
    Serial.printf("❌ Invalid SNA file size! Expected 49179, got %d\n", fileSize);
    file.close();
    return false;
  }
  
  // ═══ ЧИТАЕМ РЕГИСТРЫ (27 байт) ═══
  uint8_t header[27];
  if (file.read(header, 27) != 27) {
    Serial.println("❌ Failed to read header!");
    file.close();
    return false;
  }
  
  // Декодируем регистры
  uint8_t I = header[0];
  
  // Alternate registers (HL', DE', BC', AF')
  uint16_t HLp = header[1] | (header[2] << 8);
  uint16_t DEp = header[3] | (header[4] << 8);
  uint16_t BCp = header[5] | (header[6] << 8);
  uint16_t AFp = header[7] | (header[8] << 8);
  
  // Main registers
  uint16_t HL = header[9] | (header[10] << 8);
  uint16_t DE = header[11] | (header[12] << 8);
  uint16_t BC = header[13] | (header[14] << 8);
  uint16_t IY = header[15] | (header[16] << 8);
  uint16_t IX = header[17] | (header[18] << 8);
  
  // Interrupt & R register
  uint8_t IFF = (header[19] & 0x04) ? 1 : 0;  // Bit 2 = IFF2 (копируется в IFF1)
  uint8_t R = header[20];
  
  // AF, SP, IM, Border
  uint16_t AF = header[21] | (header[22] << 8);
  uint16_t SP = header[23] | (header[24] << 8);
  uint8_t IM = header[25];
  uint8_t border = header[26];
  
  Serial.println("\n  📋 Registers:");
  Serial.printf("    PC: Will be popped from stack\n");
  Serial.printf("    SP: 0x%04X\n", SP);
  Serial.printf("    AF: 0x%04X  HL: 0x%04X  BC: 0x%04X  DE: 0x%04X\n", AF, HL, BC, DE);
  Serial.printf("    IX: 0x%04X  IY: 0x%04X\n", IX, IY);
  Serial.printf("    I:  0x%02X    R:  0x%02X\n", I, R);
  Serial.printf("    IM: %d      IFF: %d    Border: %d\n", IM, IFF, border);
  
  // ═══ ЗАГРУЖАЕМ RAM (48KB = 49152 байт, адреса 0x4000-0xFFFF) ═══
  Serial.println("\n  💾 Loading RAM (48KB)...");
  
  uint8_t* ram = spectrum->mem.getRamData();
  size_t ramLoaded = 0;
  
  // Читаем блоками по 512 байт для скорости
  uint8_t buffer[512];
  size_t offset = 0;
  
  while (offset < 49152) {
    size_t toRead = min((size_t)512, (size_t)(49152 - offset));
    size_t bytesRead = file.read(buffer, toRead);
    
    if (bytesRead != toRead) {
      Serial.printf("❌ Failed to read RAM at offset %d!\n", offset);
      file.close();
      return false;
    }
    
    // Копируем в RAM эмулятора (начиная с 0x4000)
    memcpy(ram + offset, buffer, bytesRead);
    
    offset += bytesRead;
    ramLoaded += bytesRead;
    
    // Прогресс каждые 8KB
    if (ramLoaded % 8192 == 0) {
      Serial.printf("    Loaded: %d/%d KB\n", ramLoaded / 1024, 48);
    }
  }
  
  file.close();
  
  Serial.printf("  ✅ RAM loaded: %d bytes\n", ramLoaded);
  
  // ═══ ВОССТАНАВЛИВАЕМ СОСТОЯНИЕ Z80 ═══
  Serial.println("\n  🔧 Restoring Z80 state...");
  
  Z80Regs* regs = spectrum->z80Regs;
  
  // Main registers
  regs->AF.W = AF;
  regs->HL.W = HL;
  regs->BC.W = BC;
  regs->DE.W = DE;
  regs->IX.W = IX;
  regs->IY.W = IY;
  regs->SP.W = SP;
  
  // Alternate registers (shadow registers)
  regs->AFs.W = AFp;
  regs->HLs.W = HLp;
  regs->BCs.W = BCp;
  regs->DEs.W = DEp;
  
  // Special registers
  regs->I = I;
  regs->R.W = R;  // R - 7-bit register
  regs->IFF1 = IFF;
  regs->IFF2 = IFF;
  regs->IM = IM;
  
  // PC извлекаем из стека (как в реальном Spectrum при RETN)
  uint8_t pcl = spectrum->z80_peek(SP);
  uint8_t pch = spectrum->z80_peek(SP + 1);
  regs->PC.W = pcl | (pch << 8);
  regs->SP.W += 2;  // Корректируем SP после "pop PC"
  
  Serial.printf("    PC popped from stack: 0x%04X\n", regs->PC.W);
  Serial.printf("    SP adjusted to: 0x%04X\n", regs->SP.W);
  
  // Border color (храним в портах эмулятора)
  spectrum->borderColor = border & 0x07;
  
  Serial.println("\n═══════════════════════════════════════════");
  Serial.println("✅ .SNA FILE LOADED SUCCESSFULLY!");
  Serial.println("═══════════════════════════════════════════\n");
  
  return true;
}

// ═══════════════════════════════════════════
// LOAD .TAP FILE (INSTANT LOAD)
// ═══════════════════════════════════════════

bool loadTAPFile(String filename) {
  Serial.println("\n═══════════════════════════════════════════");
  Serial.printf("📼 LOADING .TAP FILE: %s\n", filename.c_str());
  Serial.println("═══════════════════════════════════════════");
  
  TAPLoader tapLoader;
  // Передаём renderScreen для построчного loading screen! 🎨
  // V3.137: Загружаем из /ZXgames/
  bool success = tapLoader.loadTAP(("/ZXgames/" + filename).c_str(), spectrum, renderScreen);
  
  if (success) {
    Serial.println("\n═══════════════════════════════════════════");
    Serial.println("✅ .TAP FILE LOADED SUCCESSFULLY!");
    Serial.println("═══════════════════════════════════════════\n");
  } else {
    Serial.println("\n═══════════════════════════════════════════");
    Serial.printf("❌ FAILED TO LOAD .TAP: %s\n", tapLoader.getLastError());
    Serial.println("═══════════════════════════════════════════\n");
  }
  
  return success;
}

// ═══════════════════════════════════════════
// .Z80 FILE LOADER (V3.134)
// ═══════════════════════════════════════════

bool loadZ80File(String filename) {
  Serial.println("\n═══════════════════════════════════════════");
  Serial.printf("💾 LOADING .Z80 FILE: %s\n", filename.c_str());
  Serial.println("═══════════════════════════════════════════");
  
  Z80Loader z80Loader;
  // V3.137: Загружаем из /ZXgames/
  bool success = z80Loader.loadZ80(("/ZXgames/" + filename).c_str(), spectrum);
  
  if (success) {
    Serial.println("\n═══════════════════════════════════════════");
    Serial.println("✅ .Z80 FILE LOADED SUCCESSFULLY!");
    Serial.println("═══════════════════════════════════════════\n");
  } else {
    Serial.println("\n═══════════════════════════════════════════");
    Serial.printf("❌ FAILED TO LOAD .Z80: %s\n", z80Loader.getLastError());
    Serial.println("═══════════════════════════════════════════\n");
  }
  
  return success;
}

// ═══════════════════════════════════════════
// SPLASH SCREEN
// ═══════════════════════════════════════════

void drawSplashScreen() {
  M5Cardputer.Display.fillScreen(BLACK);
  
  // ═══ БЕЛАЯ РАМКА (как у ZX Spectrum) ═══
  M5Cardputer.Display.drawRect(5, 5, 230, 125, WHITE);
  M5Cardputer.Display.drawRect(6, 6, 228, 123, WHITE);
  
  // ═══ ВЕРСИЯ В ПРАВОМ ВЕРХНЕМ УГЛУ ═══
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(185, 10);
  M5Cardputer.Display.print("V3.137");
  
  // ═══ РАДУЖНЫЕ ПОЛОСКИ (диагональные) ═══
  // Красный
  M5Cardputer.Display.fillRect(80, 30, 100, 10, TFT_RED);
  M5Cardputer.Display.fillRect(90, 40, 100, 10, TFT_RED);
  
  // Оранжевый
  M5Cardputer.Display.fillRect(100, 50, 100, 10, TFT_ORANGE);
  M5Cardputer.Display.fillRect(110, 60, 100, 10, TFT_ORANGE);
  
  // Желтый
  M5Cardputer.Display.fillRect(120, 70, 100, 10, TFT_YELLOW);
  M5Cardputer.Display.fillRect(130, 80, 100, 10, TFT_YELLOW);
  
  // Зеленый (обе полоски одинаковой длины!)
  M5Cardputer.Display.fillRect(140, 90, 90, 10, TFT_GREEN);  // ✅ 90px чтобы обе заканчивались на x=230!
  M5Cardputer.Display.fillRect(150, 100, 80, 10, TFT_GREEN);
  
  // ═══ ТЕКСТ "ZX-" ═══
  M5Cardputer.Display.setTextSize(3);
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.setCursor(15, 35);
  M5Cardputer.Display.print("ZX-");
  
  // ═══ ТЕКСТ "Cardputer" ═══
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.setCursor(15, 70);
  M5Cardputer.Display.print("Cardputer");
  
  // ═══ ИКОНКА CARDPUTER (стилизованный прямоугольник) ═══
  // По центру ШИРИНЫ зеленых полосок: x=168 (188-20)
  M5Cardputer.Display.drawRect(168, 75, 40, 30, BLACK);  // ✅ V3.134: ЧЁРНЫЙ
  M5Cardputer.Display.drawRect(169, 76, 38, 28, BLACK);  // ✅ V3.134: ЧЁРНЫЙ
  
  // ═══ ТЕКСТ "48K" ВНУТРИ КВАДРАТИКА ПО ЦЕНТРУ ═══
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(BLACK);  // ✅ V3.134: ЧЁРНЫЙ
  M5Cardputer.Display.setCursor(179, 86);  // ✅ Центр: x=168+40/2-9=179, y=75+30/2-4=86
  M5Cardputer.Display.print("48K");
  
  // ═══ ТЕКСТ "press ok" (немного выше) ═══
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_YELLOW);
  M5Cardputer.Display.setCursor(85, 105);
  M5Cardputer.Display.print("press ok");
  
  // ═══ COPYRIGHT SINCLAIR (внизу по центру) ═══
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(18, 120);  // Центрируем: ~31 символов × 6px = 186px, (240-186)/2 ≈ 27px
  M5Cardputer.Display.print("(C) 1982 Sinclair Research Ltd");
}

// ═══════════════════════════════════════════
// MENU FUNCTIONS
// ═══════════════════════════════════════════

void drawMainMenu() {
  M5Cardputer.Display.fillScreen(BLACK);
  
  // ═══ ЗАГОЛОВОК ═══
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(50, 5);
  M5Cardputer.Display.print("ZX Spectrum V3.137");
  
  // ═══ РАМКА ═══
  M5Cardputer.Display.drawRect(10, 20, 220, 110, TFT_WHITE);
  
  // ═══ ПУНКТЫ МЕНЮ (С РАЗДЕЛЬНЫМ РЕНДЕРИНГОМ НОМЕРА И ТЕКСТА) ═══
  int y = 25;
  const int SPACING_NORMAL = 11;  // Расстояние между обычными пунктами
  const int SPACING_SELECTED = 17; // Расстояние для выбранного (т.к. он больше)
  
  // ═══ V3.134: УБРАЛИ "BACK" (4 пункта) - ESC для возврата! ═══
  // Массив ТОЛЬКО текстов (БЕЗ номеров!)
  String menuTexts[4] = {
    "Basic",
    "Load Game",
    "Reset Emulator",
    "Information"
  };
  
  for (int i = 0; i < 4; i++) {
    String number = String(i + 1) + ". ";  // "1. ", "2. ", etc.
    String text = menuTexts[i];
    
    if (selectedMenuItem == i) {
      // ═══ ВЫБРАННЫЙ ПУНКТ: НОМЕР (size=1) + ТЕКСТ (size=2) ═══
      
      // Рисуем номер (обычный размер)
      M5Cardputer.Display.setTextColor(TFT_YELLOW);
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.setCursor(15, y + 4);  // +4 для выравнивания с крупным текстом
      M5Cardputer.Display.print(number);
      
      // Вычисляем доступную ширину для текста
      // Экран = 240px, отступ = 15px, номер "X. " = ~18px (3 символа * 6px)
      // Для size=2: каждый символ = 12px ширины
      const int MAX_TEXT_WIDTH = 220 - 18;  // 202px для текста
      const int CHAR_WIDTH_SIZE2 = 12;  // Ширина символа при size=2
      const int MAX_CHARS = MAX_TEXT_WIDTH / CHAR_WIDTH_SIZE2;  // ~16 символов
      
      // Проверяем длину текста
      if (text.length() > MAX_CHARS) {
        // ТЕКСТ ДЛИННЫЙ → БЕГУЩАЯ СТРОКА!
        
        // Инициализируем таймер при смене пункта
        static int lastSelectedItem = -1;
        if (lastSelectedItem != selectedMenuItem) {
          menuScrollOffset = 0;
          menuScrollStartTime = millis();
          lastMenuScrollTime = millis();
          lastSelectedItem = selectedMenuItem;
        }
        
        // Прокрутка начинается через 2 секунды
        if (millis() - menuScrollStartTime > MENU_SCROLL_DELAY) {
          // Обновляем прокрутку каждые 150ms
          if (millis() - lastMenuScrollTime > MENU_SCROLL_SPEED) {
            menuScrollOffset++;
            
            // Циклическая прокрутка: дошли до конца → в начало
            if (menuScrollOffset > text.length()) {
              menuScrollOffset = 0;
            }
            
            lastMenuScrollTime = millis();
          }
        }
        
        // Формируем прокручиваемый текст
        String scrolledText = text.substring(menuScrollOffset) + "   " + text.substring(0, menuScrollOffset);
        scrolledText = scrolledText.substring(0, MAX_CHARS);
        
        // Рисуем текст с прокруткой
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.setCursor(15 + 18, y);  // 18px для номера
        M5Cardputer.Display.print(scrolledText);
        
      } else {
        // ТЕКСТ КОРОТКИЙ → ОБЫЧНЫЙ РЕНДЕРИНГ
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.setCursor(15 + 18, y);
        M5Cardputer.Display.print(text);
      }
      
      y += SPACING_SELECTED;
      
    } else {
      // ═══ ОБЫЧНЫЙ ПУНКТ: НОМЕР + ТЕКСТ (size=1) ═══
      
      M5Cardputer.Display.setTextColor(TFT_WHITE);
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.setCursor(15, y);
      M5Cardputer.Display.print(number + text);
      
      y += SPACING_NORMAL;
    }
  }
  
  // ═══ ПОДСКАЗКА ═══
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_DARKGREY);
  M5Cardputer.Display.setCursor(15, 118);
  M5Cardputer.Display.print("up/down Joy/Enter = Select");
  
  Serial.println("\n═══════════════════════════════════════════");
  Serial.println("📱 MENU OPENED");
  Serial.printf("   Selected item: %d (%s)\n", selectedMenuItem, menuTexts[selectedMenuItem].c_str());
  Serial.println("═══════════════════════════════════════════");
}

// ═══════════════════════════════════════════
// LOAD GAME SUBMENU (V3.134)
// ═══════════════════════════════════════════

void drawLoadGameMenu() {
  M5Cardputer.Display.fillScreen(BLACK);
  
  // ═══ ЗАГОЛОВОК ═══
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(65, 5);
  M5Cardputer.Display.print("LOAD GAME");
  
  // ═══ РАМКА ═══
  M5Cardputer.Display.drawRect(10, 20, 220, 110, TFT_WHITE);
  
  // ═══ ПУНКТЫ ПОДМЕНЮ ═══
  int y = 25;
  const int SPACING_NORMAL = 11;
  const int SPACING_SELECTED = 17;
  
  // V3.134: УБРАЛИ "BACK" (4 пункта) - ESC для возврата!
  String loadGameTexts[4] = {
    ".SNA files (" + String(snaCount) + ")",
    ".Z80 files (" + String(z80Count) + ")",
    ".TAP files (" + String(tapCount) + ")",
    "Audio TAP"
  };
  
  for (int i = 0; i < 4; i++) {
    String number = String(i + 1) + ". ";
    String text = loadGameTexts[i];
    
    if (selectedLoadGameItem == i) {
      // ВЫБРАННЫЙ ПУНКТ
      M5Cardputer.Display.setTextColor(TFT_YELLOW);
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.setCursor(15, y + 4);
      M5Cardputer.Display.print(number);
      
      M5Cardputer.Display.setTextColor(TFT_YELLOW);
      M5Cardputer.Display.setTextSize(2);
      M5Cardputer.Display.setCursor(15 + 18, y);
      M5Cardputer.Display.print(text);
      
      y += SPACING_SELECTED;
    } else {
      // ОБЫЧНЫЙ ПУНКТ
      M5Cardputer.Display.setTextColor(TFT_WHITE);
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.setCursor(15, y);
      M5Cardputer.Display.print(number + text);
      
      y += SPACING_NORMAL;
    }
  }
  
  // ═══ ПОДСКАЗКА ═══
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_DARKGREY);
  M5Cardputer.Display.setCursor(15, 118);
  M5Cardputer.Display.print("up/down Joy/Enter = Select");
  
  Serial.println("\n═══════════════════════════════════════════");
  Serial.println("🎮 LOAD GAME MENU OPENED");
  Serial.printf("   Selected item: %d (%s)\n", selectedLoadGameItem, loadGameTexts[selectedLoadGameItem].c_str());
  Serial.println("═══════════════════════════════════════════");
}

// ═══════════════════════════════════════════
// FILE BROWSER FUNCTIONS
// ═══════════════════════════════════════════

void filterFiles(String extension) {
  Serial.printf("\n📂 Filtering files: %s\n", extension.c_str());
  
  filteredCount = 0;
  
  // Создаём lowercase версию расширения
  String lowerExt = extension;
  lowerExt.toLowerCase();
  
  for (int i = 0; i < fileCount && filteredCount < 100; i++) {
    String fileName = fileList[i];
    
    if (fileName.endsWith(extension) || 
        fileName.endsWith(lowerExt)) {
      filteredFiles[filteredCount++] = fileName;
    }
  }
  
  selectedFile = 0;  // Сбрасываем выбор на первый файл
  
  Serial.printf("✅ Found %d files with %s extension\n", filteredCount, extension.c_str());
}

// ═══════════════════════════════════════════
// INFORMATION SCREEN
// ═══════════════════════════════════════════

void drawInformationScreen(int page) {
  M5Cardputer.Display.fillScreen(BLACK);
  
  // ═══ ЗАГОЛОВОК ═══
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(50, 5);
  M5Cardputer.Display.printf("Information (%d/6)", page + 1);
  
  // ═══ РАМКА (как в меню) ═══
  M5Cardputer.Display.drawRect(10, 20, 220, 95, TFT_WHITE);
  
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  
  if (page == 0) {
    // ═══ СТРАНИЦА 1: ГОРЯЧИЕ КЛАВИШИ (V3.134) ═══
    int y = 25;
    int spacing = 9;
    
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Navigation & Display:");
    
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("ESC = Menu");
    
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Opt + P = Pixel-Perfect");
    
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Opt + Z = Zoom cycle");
    
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Opt + up/down = PAN v");
    
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Opt + left/right = PAN h");
    
    y += 3;  // Разделитель
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Sound & System:");
    
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Opt + [+]/[-] = Volume");
    
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Opt + M = Mute");
    
  } else if (page == 1) {
    // ═══ СТРАНИЦА 2: ЕЩЁ HOTKEYS (V3.134) ═══
    int y = 25;
    int spacing = 9;
    
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Game Controls:");
    
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Opt + J = Joystick mode");
    
    M5Cardputer.Display.setTextColor(TFT_CYAN);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(20, y); y += spacing;
    M5Cardputer.Display.print("(Joy -> QAOP+Space)");
    
    y += 3;
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("System:");
    
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Opt + ESC = Reset");
    
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("TAB = Pause/Resume");
    
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Ctrl = Screenshot (BMP)");
    
  } else if (page == 2) {
    // ═══ СТРАНИЦА 3: ZX SPECTRUM BUTTONS (V3.134) ═══
    int y = 25;
    int spacing = 10;
    
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(40, y); y += spacing + 3;
    M5Cardputer.Display.print("ZX Spectrum Buttons:");
    
    M5Cardputer.Display.setTextSize(1);
    
    // Fn = SYMBOL SHIFT (КРАСНЫЙ)
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.print("Fn");
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.print(" = SYMBOL SHIFT");
    
    // Aa = CAPS SHIFT (СИНИЙ)
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.setTextColor(TFT_BLUE);
    M5Cardputer.Display.print("Aa");
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.print(" = CAPS SHIFT");
    
    // [_] = BREAK SPACE (бортики как у Space)
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.print("[_] = BREAK SPACE");
    
    // Ok = ENTER
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.print("Ok = ENTER");
    
    y += 5;
    M5Cardputer.Display.setTextColor(TFT_CYAN);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(15, y); y += spacing;
    M5Cardputer.Display.print("Note: Standard keys work");
    M5Cardputer.Display.setCursor(15, y);
    M5Cardputer.Display.print("as on ZX Spectrum!");
    
  } else if (page == 3) {
    // ═══ СТРАНИЦА 4: CREDITS (CENTERED) ═══
    int y = 35;
    int spacing = 12;
    
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(40, y); y += 20;
    M5Cardputer.Display.print("ZX-Cardputer");
    
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setCursor(80, y); y += spacing;
    M5Cardputer.Display.print("Andy + AI");
    
    M5Cardputer.Display.setCursor(90, y); y += spacing;
    M5Cardputer.Display.print("2025");
    
    M5Cardputer.Display.setCursor(85, y); y += spacing;
    M5Cardputer.Display.print("Portugal");
    
    M5Cardputer.Display.setTextColor(TFT_CYAN);
    M5Cardputer.Display.setCursor(90, y); y += 12;
    M5Cardputer.Display.print("V3.137");
    
    M5Cardputer.Display.setTextColor(TFT_MAGENTA);
    M5Cardputer.Display.setCursor(60, y);
    M5Cardputer.Display.print("@github/andyai");
    
  } else if (page == 4) {
    // ═══ СТРАНИЦА 5: SPECIAL THANKS 1/2 (CENTERED) ═══
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(55, 25);
    M5Cardputer.Display.print("Special Thanks 1/2");
    
    int y = 42;
    int spacing = 13;
    
    // ✅ ESP32 Rainbow (centered)
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setCursor(45, y); y += spacing;
    M5Cardputer.Display.print("ESP32 Rainbow");
    M5Cardputer.Display.setTextColor(TFT_CYAN);
    M5Cardputer.Display.setCursor(40, y); y += spacing;
    M5Cardputer.Display.print("Z80 core & tape");
    
    y += 8;  // Отступ между субъектами
    
    // ✅ M5Stack (centered)
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setCursor(75, y); y += spacing;
    M5Cardputer.Display.print("M5Stack");
    M5Cardputer.Display.setTextColor(TFT_CYAN);
    M5Cardputer.Display.setCursor(50, y); y += spacing;
    M5Cardputer.Display.print("Cardputer HW");
    
  } else if (page == 5) {
    // ═══ СТРАНИЦА 6: SPECIAL THANKS 2/2 (CENTERED) ═══
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(55, 25);
    M5Cardputer.Display.print("Special Thanks 2/2");
    
    int y = 42;
    int spacing = 13;
    
    // ✅ ChatGPT-4 & Claude (AI Team, centered)
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setCursor(45, y); y += spacing;
    M5Cardputer.Display.print("ChatGPT & Claude");
    M5Cardputer.Display.setTextColor(TFT_CYAN);
    M5Cardputer.Display.setCursor(50, y); y += spacing;
    M5Cardputer.Display.print("Perfect code");
    
    y += 8;  // Отступ между субъектами
    
    // ✅ VolosR (centered)
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setCursor(80, y); y += spacing;
    M5Cardputer.Display.print("VolosR");
    M5Cardputer.Display.setTextColor(TFT_CYAN);
    M5Cardputer.Display.setCursor(50, y); y += spacing;
    M5Cardputer.Display.print("M5Mp3 audio ref");
  }
  
  // ═══ ПОДСКАЗКИ УПРАВЛЕНИЯ ═══
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(10, 120);
  M5Cardputer.Display.print("left/right = Page  ESC = Back");
}

void drawFileBrowser() {
  M5Cardputer.Display.fillScreen(BLACK);
  
  // ═══ ЗАГОЛОВОК ═══
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(10, 5);
  M5Cardputer.Display.printf("Load %s", browserFilter.c_str());
  
  // ═══ СЧЁТЧИК (справа) ═══
  M5Cardputer.Display.setCursor(180, 5);
  M5Cardputer.Display.printf("%d/%d", selectedFile + 1, filteredCount);
  
  // ═══ РАМКА (как в меню) ═══
  M5Cardputer.Display.drawRect(10, 20, 220, 95, TFT_WHITE);
  
  // ═══ СПИСОК ФАЙЛОВ (8 штук) ═══
  // Вычисляем первый видимый файл (скроллинг)
  // ВАЖНО: Выбранный файл ВСЕГДА на позиции 3 (в центре)!
  // Это гарантирует что большой текст не вылезет за рамку
  int firstVisible = selectedFile - 3;  // Выбранный будет на позиции 3 (в центре)
  if (firstVisible < 0) firstVisible = 0;  // Если файлов меньше 4, начинаем с первого
  
  int y = 25;  // Начальная Y позиция
  
  for (int i = 0; i < 8; i++) {
    int fileIdx = firstVisible + i;
    
    // Проверяем границы
    if (fileIdx >= filteredCount) break;
    
    bool isSelected = (fileIdx == selectedFile);
    
    // Устанавливаем размер и цвет
    if (isSelected) {
      M5Cardputer.Display.setTextSize(2);
      M5Cardputer.Display.setTextColor(TFT_YELLOW);
    } else {
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.setTextColor(TFT_WHITE);
    }
    
    M5Cardputer.Display.setCursor(15, y);
    
    // Получаем имя файла
    String name = filteredFiles[fileIdx];
    
    // Обрезаем длинные имена
    int maxLen = isSelected ? 18 : 28;  // Для большого шрифта меньше символов влезает
    if (name.length() > maxLen) {
      name = name.substring(0, maxLen - 3) + "...";
    }
    
    M5Cardputer.Display.print(name);
    
    // Следующая строка (большой шрифт занимает больше места)
    y += isSelected ? 16 : 11;
  }
  
  // ═══ ПОДСКАЗКИ УПРАВЛЕНИЯ ═══
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(5, 120);
  if (joystick2Available) {
    M5Cardputer.Display.print("Joy/up/down=Nav Enter=Load ESC=Back");
  } else {
    M5Cardputer.Display.print("up/down=Nav Enter=Load ESC=Back");
  }
}

void setup() {
  // Инициализация Cardputer (Display + Serial + etc)
  M5Cardputer.begin();
  
  // ✅ КРИТИЧНО: Выключаем микрофон (I2S RX конфликт!)
  M5Cardputer.Mic.end();
  
  // ✅ ИНИЦИАЛИЗАЦИЯ SPEAKER (ChatGPT beeper!)
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(192);  // 0-255
  
  Serial.begin(115200);
  delay(1000);
  
  // Очистка экрана (черный фон)
  M5Cardputer.Display.fillScreen(BLACK);
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.print("ZX Spectrum V3.137");
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════╗");
  Serial.println("║  ZX Spectrum V3.137 📁 Folders!    ║");
  Serial.println("║  M5Stack Cardputer                 ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  // ═══ ИНИЦИАЛИЗАЦИЯ SD-КАРТЫ ═══
  initSDCard();
  
  // ═══ V3.137: СОЗДАНИЕ ПАПКИ ДЛЯ ИГР ═══
  gameFolderStatus = setupGameFolders();
  
  // ═══ ИНИЦИАЛИЗАЦИЯ JOYSTICK2 ═══
  initJoystick2();

  // Создаем эмулятор
  spectrum = new ZXSpectrum();
  if (!spectrum) {
    Serial.println("🔴 FATAL: Failed to allocate spectrum!");
    while (1) delay(1000);
  }

  // Инициализируем Z80
  spectrum->reset();
  
  // Инициализируем 48K Spectrum
  if (!spectrum->init_48k()) {
    Serial.println("🔴 FATAL: Failed to init 48K!");
    while (1) delay(1000);
  }

  // Сбрасываем Z80
  spectrum->reset_spectrum();

  Serial.printf("✅ Z80: PC=0x%04X SP=0x%04X IM=%d\n", 
                spectrum->z80Regs->PC.W, spectrum->z80Regs->SP.W, spectrum->z80Regs->IM);
  Serial.printf("✅ Heap: %d bytes free\n", ESP.getFreeHeap());
  
  // ═══ 🎵 ЗАПУСКАЕМ AUDIO TASK НА CORE 1 (ChatGPT!) ═══
  Serial.println("🔊 Starting Audio Task on Core 1...");
  xTaskCreatePinnedToCore(Task_Audio, "Task_Audio", 8192, nullptr, 3, nullptr, 1);
  Serial.println("✅ Audio Task started!");
  
  Serial.println("⚡ Starting emulation...\n");

  lastStatsTime = millis();
  
  // ═══ ПОКАЗЫВАЕМ SPLASH SCREEN ═══
  drawSplashScreen();
  
  // ═══ ЖДЁМ НАЖАТИЯ ENTER ИЛИ КНОПКИ ДЖОЙСТИКА ═══
  Serial.println("⏳ Waiting for OK button...");
  bool splashActive = true;
  while (splashActive) {
    M5Cardputer.update();
    
    // Проверяем Enter на клавиатуре
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    if (status.enter) {
      Serial.println("✅ Enter pressed - starting menu");
      splashActive = false;
      delay(200);  // Debounce
    }
    
    // Проверяем кнопку джойстика
    if (joystick2Available) {
      Joystick2Data joyData = readJoystick2();
      if (joyData.button == 1) {
        Serial.println("✅ Joystick button pressed - starting menu");
        splashActive = false;
        delay(500);  // Debounce
      }
    }
    
    delay(50);  // Экономим CPU
  }
  
  // ═══ ОТКРЫВАЕМ МЕНЮ ПОСЛЕ SPLASH ═══
  showMenu = true;
  emulatorPaused = true;
  drawMainMenu();
  Serial.println("\n🎮 WELCOME! Main menu opened.\n");
}

// Буфер для рендеринга (RGB565, 16-bit color)
uint16_t* frameBuffer = nullptr;

// ===== ZOOM/PAN VARIABLES =====
enum RenderMode {
  MODE_ZOOM,           // Режим масштабирования (текущий)
  MODE_PIXEL_PERFECT   // Режим 1:1 без масштабирования (новый!)
};

RenderMode renderMode = MODE_ZOOM;  // По умолчанию - режим ZOOM

// Для режима ZOOM:
float zoomLevel = 1.0;     // Уровни: 1.0, 1.5, 2.0, 2.5
int panX = 0;              // Смещение по X (-maxPanX .. +maxPanX)
int panY = 0;              // Смещение по Y (-maxPanY .. +maxPanY)
const int PAN_STEP = 8;    // Шаг перемещения (8 пикселей = размер ZX символа)

// Для режима PIXEL-PERFECT (1:1) - V3.134: добавлен horizontal PAN!
int pixelPerfectPanX = 0;  // Горизонтальная прокрутка (0..16)
int pixelPerfectPanY = 57; // Вертикальная прокрутка (0..57), default = левый нижний угол
const int PP_MAX_PAN_X = 16; // Максимальная прокрутка по X (256 - 240)
const int PP_MAX_PAN_Y = 57; // Максимальная прокрутка по Y (192 - 135)

// Функция рендеринга ZX Spectrum экрана (С ЦВЕТАМИ + ZOOM/PAN + PIXEL-PERFECT!)
void renderScreen() {
  const int ZX_WIDTH = 256;
  const int ZX_HEIGHT = 192;
  const int DISPLAY_WIDTH = 240;
  const int DISPLAY_HEIGHT = 135;
  
  // ═══════════════════════════════════════════════════════════
  // ═══ РЕЖИМ PIXEL-PERFECT (1:1 без масштабирования) ═══
  // ═══════════════════════════════════════════════════════════
  if (renderMode == MODE_PIXEL_PERFECT) {
    // Выделяем frameBuffer если ещё не выделен
    if (!frameBuffer) {
      frameBuffer = (uint16_t*)malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
      if (!frameBuffer) {
        Serial.println("🔴 FATAL: Failed to allocate framebuffer!");
        return;
      }
    }
    
    // Прямой доступ к VRAM
    uint8_t* vram = spectrum->mem.getScreenData();
    
    // PIXEL-PERFECT: 1:1 рендеринг БЕЗ масштабирования
    // V3.134: добавлен horizontal PAN!
    // x_offset = pixelPerfectPanX (0..16, горизонтальная прокрутка)
    // y_offset = pixelPerfectPanY (0..57, вертикальная прокрутка)
    
    int bufferIdx = 0;
    for (int dy = 0; dy < DISPLAY_HEIGHT; dy++) {
      for (int dx = 0; dx < DISPLAY_WIDTH; dx++) {
        // Координаты в ZX Spectrum (1:1!)
        int zx = dx + pixelPerfectPanX;  // dx + (0..16)
        int zy = dy + pixelPerfectPanY;  // dy + (0..57)
        
        // Проверка границ ZX экрана
        if (zx >= ZX_WIDTH || zy >= ZX_HEIGHT) {
          frameBuffer[bufferIdx++] = 0x0000;  // Черный за пределами
          continue;
        }
        
        // ZX Spectrum bitmap layout
        int y2 = (zy >> 6) & 0x03;
        int y1 = (zy >> 3) & 0x07;
        int y0 = zy & 0x07;
        int col = zx >> 3;
        
        // Читаем пиксель из VRAM
        int offset = (y2 << 11) + (y0 << 8) + (y1 << 5) + col;
        uint8_t byte = vram[offset];
        
        int bit = zx & 0x07;
        bool pixel = (byte & (0x80 >> bit)) != 0;
        
        // Читаем атрибут для этого знакоместа (8×8 пикселей)
        int attrRow = zy >> 3;
        int attrCol = zx >> 3;
        int attrOffset = 0x1800 + (attrRow * 32) + attrCol;
        uint8_t attr = vram[attrOffset];
        
        // Распаковываем атрибут
        int ink = attr & 0x07;
        int paper = (attr >> 3) & 0x07;
        bool bright = (attr & 0x40) != 0;
        
        if (bright) {
          ink += 8;
          paper += 8;
        }
        
        // Выбираем цвет
        uint16_t color = pixel ? specpal565[ink] : specpal565[paper];
        frameBuffer[bufferIdx++] = color;
      }
    }
    
    // ═══ ГРАНИЦЫ при прокрутке в PP режиме ═══
    const uint16_t RED = TFT_RED;
    
    // V3.134: КРАСНЫЕ ГРАНИЦЫ (вертикальные + горизонтальные)
    
    // Верхняя граница: когда в самом верху (y_offset = 0)
    if (pixelPerfectPanY == 0) {
      for (int x = 0; x < DISPLAY_WIDTH; x++) {
        frameBuffer[0 * DISPLAY_WIDTH + x] = RED;
      }
    }
    
    // Нижняя граница: когда в самом низу (y_offset = 57)
    if (pixelPerfectPanY >= PP_MAX_PAN_Y) {
      for (int x = 0; x < DISPLAY_WIDTH; x++) {
        frameBuffer[134 * DISPLAY_WIDTH + x] = RED;
      }
    }
    
    // Левая граница: когда в самом левом положении (x_offset = 0)
    if (pixelPerfectPanX == 0) {
      for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        frameBuffer[y * DISPLAY_WIDTH + 0] = RED;
      }
    }
    
    // Правая граница: когда в самом правом положении (x_offset = 16)
    if (pixelPerfectPanX >= PP_MAX_PAN_X) {
      for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        frameBuffer[y * DISPLAY_WIDTH + 239] = RED;
      }
    }
    
    // Отрисовываем буфер
    M5Cardputer.Display.pushImage(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, frameBuffer);
    
    // ═══ БЕЙДЖ "PP" (жёлтый, правый верхний угол) ═══
    M5Cardputer.Display.fillRect(185, 2, 53, 14, BLACK);
    M5Cardputer.Display.drawRect(185, 2, 53, 14, WHITE);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(200, 5);
    M5Cardputer.Display.print("PP");
    
    // ═══ V3.135: УВЕДОМЛЕНИЯ И ПЛАШКИ В PP РЕЖИМЕ! ═══
    drawNotificationOverlay();
    
    if (gamePaused && !notificationActive) {
      M5Cardputer.Display.fillRect(60, 55, 120, 30, BLACK);
      M5Cardputer.Display.drawRect(60, 55, 120, 30, TFT_YELLOW);
      M5Cardputer.Display.drawRect(61, 56, 118, 28, TFT_YELLOW);
      M5Cardputer.Display.setTextSize(2);
      M5Cardputer.Display.setTextColor(TFT_YELLOW);
      M5Cardputer.Display.setCursor(80, 62);
      M5Cardputer.Display.print("PAUSE");
    }
    
    return;  // Выходим из функции (не используем ZOOM логику!)
  }
  
  // ═══════════════════════════════════════════════════════════
  // ═══ РЕЖИМ ZOOM (масштабирование) ═══
  // ═══════════════════════════════════════════════════════════
  
  // ЛОГИКА РЕНДЕРА:
  // zoom=1.0: сохраняем 4:3 (180×135 + черные полосы 30px)
  // zoom>1.0: используем ВЕСЬ экран (240×135, БЕЗ полос!)
  int RENDER_WIDTH, RENDER_HEIGHT, OFFSET_X;
  
  if (zoomLevel <= 1.05) {
    // zoom=1.0: сохраняем соотношение 4:3
    RENDER_HEIGHT = 135;
    RENDER_WIDTH = (RENDER_HEIGHT * 4) / 3;  // 180
    OFFSET_X = (DISPLAY_WIDTH - RENDER_WIDTH) / 2;  // 30 (черные полосы)
  } else {
    // zoom>1.0: ВЕСЬ ЭКРАН (максимальный размер!)
    RENDER_WIDTH = DISPLAY_WIDTH;   // 240 (весь экран!)
    RENDER_HEIGHT = DISPLAY_HEIGHT; // 135
    OFFSET_X = 0;  // БЕЗ черных полос!
  }
  
  if (!frameBuffer) {
    frameBuffer = (uint16_t*)malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
    if (!frameBuffer) {
      Serial.println("🔴 FATAL: Failed to allocate framebuffer!");
      return;
    }
  }
  
  // Прямой доступ к VRAM (быстрее чем peek()!)
  uint8_t* vram = spectrum->mem.getScreenData();
  
  // ═══ ZOOM/PAN РЕНДЕРИНГ (с сохранением 4:3!) ═══
  
  int ZX_OFFSET_X, ZX_OFFSET_Y;
  int ZX_VIEW_W, ZX_VIEW_H;
  
  if (zoomLevel <= 1.05) {
    // НЕТ ZOOM: показываем весь ZX экран (256×192)
    ZX_OFFSET_X = 0;
    ZX_OFFSET_Y = 0;
    ZX_VIEW_W = ZX_WIDTH;   // 256
    ZX_VIEW_H = ZX_HEIGHT;  // 192
  } else {
    // ZOOM АКТИВЕН: вычисляем видимую область
    // zoom=1.5 → видим 180/1.5=120 × 135/1.5=90 пикселей ZX
    // zoom=2.0 → видим 180/2=90 × 135/2=67.5 пикселей ZX
    ZX_VIEW_W = (int)(RENDER_WIDTH / zoomLevel);
    ZX_VIEW_H = (int)(RENDER_HEIGHT / zoomLevel);
    
    // Вычисляем offset с учётом PAN
    int maxPanX = (ZX_WIDTH - ZX_VIEW_W) / 2;
    int maxPanY = (ZX_HEIGHT - ZX_VIEW_H) / 2;
    
    ZX_OFFSET_X = ((ZX_WIDTH - ZX_VIEW_W) / 2) + panX;
    ZX_OFFSET_Y = ((ZX_HEIGHT - ZX_VIEW_H) / 2) + panY;
    
    // Ограничиваем offset
    if (ZX_OFFSET_X < 0) ZX_OFFSET_X = 0;
    if (ZX_OFFSET_Y < 0) ZX_OFFSET_Y = 0;
    if (ZX_OFFSET_X + ZX_VIEW_W > ZX_WIDTH) ZX_OFFSET_X = ZX_WIDTH - ZX_VIEW_W;
    if (ZX_OFFSET_Y + ZX_VIEW_H > ZX_HEIGHT) ZX_OFFSET_Y = ZX_HEIGHT - ZX_VIEW_H;
  }
  
  // ═══ РЕНДЕРИНГ С СОХРАНЕНИЕМ 4:3 ═══
  int bufferIdx = 0;
  for (int dy = 0; dy < DISPLAY_HEIGHT; dy++) {
    for (int dx = 0; dx < DISPLAY_WIDTH; dx++) {
      // Проверяем: попадаем ли в рендер область (180×135)?
      if (dx < OFFSET_X || dx >= OFFSET_X + RENDER_WIDTH) {
        // За пределами рендер области → черная полоса
        frameBuffer[bufferIdx++] = 0x0000;
        continue;
      }
      
      // Координата внутри рендер области (0..179)
      int render_x = dx - OFFSET_X;
      int render_y = dy;
      
      // Масштабируем в координаты ZX с учетом ZOOM
      int zx = ZX_OFFSET_X + (render_x * ZX_VIEW_W) / RENDER_WIDTH;
      int zy = ZX_OFFSET_Y + (render_y * ZX_VIEW_H) / RENDER_HEIGHT;
      
      // Проверка границ
      if (zx >= ZX_WIDTH || zy >= ZX_HEIGHT) {
        frameBuffer[bufferIdx++] = 0x0000;
        continue;
      }
      
      // ZX Spectrum bitmap layout
      int y2 = (zy >> 6) & 0x03;
      int y1 = (zy >> 3) & 0x07;
      int y0 = zy & 0x07;
      int col = zx >> 3;
      
      // Читаем напрямую из VRAM (vram[0] = адрес 0x4000)
      int offset = (y2 << 11) + (y0 << 8) + (y1 << 5) + col;
      uint8_t byte = vram[offset];
      
      int bit = zx & 0x07;
      bool pixel = (byte & (0x80 >> bit)) != 0;
      
      // Читаем атрибут для этого знакоместа (8×8 пикселей)
      // Атрибуты начинаются с offset 0x1800 (6144 байт после начала bitmap)
      int attrRow = zy >> 3;  // Делим на 8
      int attrCol = zx >> 3;  // Делим на 8
      int attrOffset = 0x1800 + (attrRow * 32) + attrCol;
      uint8_t attr = vram[attrOffset];
      
      // Распаковываем атрибут:
      // Биты 0-2: INK (цвет символа)
      // Биты 3-5: PAPER (цвет фона)
      // Бит 6: BRIGHT
      int ink = attr & 0x07;
      int paper = (attr >> 3) & 0x07;
      bool bright = (attr & 0x40) != 0;
      
      // Если BRIGHT - добавляем 8 к цвету
      if (bright) {
        ink += 8;
        paper += 8;
      }
      
      // Выбираем цвет: INK если пиксель=1, PAPER если пиксель=0
      uint16_t color = pixel ? specpal565[ink] : specpal565[paper];
      
      frameBuffer[bufferIdx++] = color;
    }
  }
  
  // ═══ КРАСНЫЕ ГРАНИЦЫ (рисуем В frameBuffer!) ═══
  if (zoomLevel > 1.05) {
    // При zoom>1.0: используем ВЕСЬ экран (240×135)
    const int RENDER_W = DISPLAY_WIDTH;   // 240
    const int RENDER_H = DISPLAY_HEIGHT;  // 135
    const int OFFSET_X = 0;  // БЕЗ полос при zoom
    
    int ZX_VIEW_W = (int)(RENDER_W / zoomLevel);
    int ZX_VIEW_H = (int)(RENDER_H / zoomLevel);
    
    int maxPanX = (256 - ZX_VIEW_W) / 2;
    int maxPanY = (192 - ZX_VIEW_H) / 2;
    
    // Определяем где упёрлись в границу (±2 для толерантности)
    bool atLeftEdge = (panX <= -maxPanX + 2);
    bool atRightEdge = (panX >= maxPanX - 2);
    bool atTopEdge = (panY <= -maxPanY + 2);
    bool atBottomEdge = (panY >= maxPanY - 2);
    
    // ДИАГНОСТИКА: показываем значения PAN
    static int debugCounter = 0;
    if (debugCounter++ % 50 == 0) {  // Каждые 50 кадров
      Serial.printf("PAN DEBUG: panX=%d panY=%d | maxPanX=%d maxPanY=%d | L=%d R=%d T=%d B=%d\n",
                    panX, panY, maxPanX, maxPanY, atLeftEdge, atRightEdge, atTopEdge, atBottomEdge);
    }
    
    // Рисуем КРАСНЫЕ линии НА КРАЮ ЭКРАНА! (1 ПИКСЕЛЬ!)
    const uint16_t RED = TFT_RED;  // Красный
    
    if (atLeftEdge) {
      // Левая граница: x=0 (САМЫЙ КРАЙ!)
      for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        frameBuffer[y * DISPLAY_WIDTH + 0] = RED;
      }
      Serial.println("🔴 LEFT EDGE!");
    }
    
    if (atRightEdge) {
      // Правая граница: x=237
      for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        frameBuffer[y * DISPLAY_WIDTH + 237] = RED;
      }
      Serial.println("🔴 RIGHT EDGE!");
    }
    
    if (atTopEdge) {
      // Верхняя граница: y=0 (САМЫЙ КРАЙ!)
      for (int x = OFFSET_X; x < OFFSET_X + RENDER_W; x++) {
        frameBuffer[0 * DISPLAY_WIDTH + x] = RED;
      }
      Serial.println("🔴 TOP EDGE!");
    }
    
    if (atBottomEdge) {
      // Нижняя граница: y=134 (САМЫЙ КРАЙ!)
      for (int x = OFFSET_X; x < OFFSET_X + RENDER_W; x++) {
        frameBuffer[134 * DISPLAY_WIDTH + x] = RED;
      }
      Serial.println("🔴 BOTTOM EDGE!");
    }
  }
  
  // Отрисовываем буфер ОДНИМ вызовом (с красными линиями внутри!)
  M5Cardputer.Display.pushImage(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, frameBuffer);
  
  // ═══ ВИЗУАЛЬНЫЕ ИНДИКАТОРЫ ═══
  
  // ZOOM ИНДИКАТОР (желтый, правый верхний угол) - рисуем ПОВЕРХ!
  if (zoomLevel > 1.05) {
    // Чёрный фон с белой рамкой
    M5Cardputer.Display.fillRect(185, 2, 53, 14, BLACK);
    M5Cardputer.Display.drawRect(185, 2, 53, 14, WHITE);
    
    // Жёлтый текст
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(189, 5);
    
    // Выводим текст зума
    if (zoomLevel < 1.6) {
      M5Cardputer.Display.print("x1.5");
    } else if (zoomLevel < 2.3) {
      M5Cardputer.Display.print("x2.0");
    } else {
      M5Cardputer.Display.print("x2.5");
    }
  }
  
  // ═══ АСИНХРОННЫЕ УВЕДОМЛЕНИЯ (V3.134) ═══
  drawNotificationOverlay();
  
  // ═══ V3.134: ПЛАШКА "PAUSE" (не рисуем если есть активное уведомление!) ═══
  if (gamePaused && !notificationActive) {
    // Непрозрачная чёрная плашка с жёлтой рамкой
    M5Cardputer.Display.fillRect(60, 55, 120, 30, BLACK);
    M5Cardputer.Display.drawRect(60, 55, 120, 30, TFT_YELLOW);
    M5Cardputer.Display.drawRect(61, 56, 118, 28, TFT_YELLOW);
    
    // Текст "PAUSE" по центру
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(80, 62);
    M5Cardputer.Display.print("PAUSE");
  }
}

// ═══════════════════════════════════════════════════════════
// Обновление ZX Spectrum клавиш из Joystick2 (QAOP + Space)
// ═══════════════════════════════════════════════════════════
void updateJoystickKeys() {
  // Не обрабатываем джойстик если:
  // 1. Джойстик выключен (joystickEnabled = false)
  // 2. Открыто меню, браузер или Information (джойстик используется для навигации)
  if (!joystickEnabled || showMenu || showLoadGameMenu || showBrowser || showInformation) {
    return;  // Ничего не делаем
  }
  
  // Читаем состояние Joystick2
  if (!joystick2Available) {
    return;
  }
  
  Joystick2Data joyData = readJoystick2();
  
  // Мапим джойстик на ZX Spectrum клавиши (ИНВЕРТИРОВАНО):
  // Физический UP → A (SPECKEY_A - вниз в игре)
  // Физический DOWN → Q (SPECKEY_Q - вверх в игре)
  // Физический LEFT → P (SPECKEY_P - вправо в игре)
  // Физический RIGHT → O (SPECKEY_O - влево в игре)
  // FIRE → Space (SPECKEY_SPACE)
  
  // Y-ось: UP/DOWN (ИНВЕРТИРОВАНО!)
  if (joyData.y < 100) {
    // Физический UP → A (вниз в игре)
    spectrum->updateKey(SPECKEY_A, 1);
  }
  if (joyData.y > 155) {
    // Физический DOWN → Q (вверх в игре)
    spectrum->updateKey(SPECKEY_Q, 1);
  }
  
  // X-ось: LEFT/RIGHT (ИНВЕРТИРОВАНО!)
  if (joyData.x < 100) {
    // Физический LEFT → P (вправо в игре)
    spectrum->updateKey(SPECKEY_P, 1);
  }
  if (joyData.x > 155) {
    // Физический RIGHT → O (влево в игре)
    spectrum->updateKey(SPECKEY_O, 1);
  }
  
  // Кнопка: FIRE → Space
  if (joyData.button == 1) {
    spectrum->updateKey(SPECKEY_SPACE, 1);
  }
  
  // Диагностика (раз в 50 кадров, только если есть движение)
  static int debugCounter = 0;
  bool hasMovement = (joyData.x < 100 || joyData.x > 155 || joyData.y < 100 || joyData.y > 155 || joyData.button == 1);
  if (hasMovement && debugCounter++ % 50 == 0) {
    Serial.printf("🕹️  Joystick→Keys: x=%d y=%d btn=%d ", joyData.x, joyData.y, joyData.button);
    if (joyData.y < 100) Serial.print("A ");      // Физический UP → A (вниз)
    if (joyData.y > 155) Serial.print("Q ");      // Физический DOWN → Q (вверх)
    if (joyData.x < 100) Serial.print("P ");      // Физический LEFT → P (вправо)
    if (joyData.x > 155) Serial.print("O ");      // Физический RIGHT → O (влево)
    if (joyData.button == 1) Serial.print("SPACE");
    Serial.println();
  }
}

// ═══════════════════════════════════════════════════════════
// NOTIFICATION SYSTEM - Async Overlay (V3.134)
// ═══════════════════════════════════════════════════════════

void showNotification(const char* text, uint16_t color = TFT_YELLOW, unsigned long duration = 500) {
  notificationText = text;
  notificationColor = color;
  notificationDuration = duration;
  notificationStartTime = millis();
  notificationActive = true;
}

void drawNotificationOverlay() {
  if (!notificationActive) return;
  
  // Проверяем таймаут
  if (millis() - notificationStartTime > notificationDuration) {
    notificationActive = false;
    return;
  }
  
  // Рисуем overlay (поверх экрана эмулятора)
  M5Cardputer.Display.fillRect(40, 60, 160, 20, BLACK);
  M5Cardputer.Display.drawRect(40, 60, 160, 20, WHITE);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(notificationColor);
  M5Cardputer.Display.setCursor(50, 65);
  M5Cardputer.Display.print(notificationText);
}

// ═══════════════════════════════════════════════════════════
// SCREENSHOT SYSTEM (V3.136)
// ═══════════════════════════════════════════════════════════

// ZX Spectrum цветовая палитра (RGB)
const uint8_t zxPalette[16][3] = {
  {0,   0,   0  },  // 0: BLACK
  {0,   0,   192},  // 1: BLUE
  {192, 0,   0  },  // 2: RED
  {192, 0,   192},  // 3: MAGENTA
  {0,   192, 0  },  // 4: GREEN
  {0,   192, 192},  // 5: CYAN
  {192, 192, 0  },  // 6: YELLOW
  {192, 192, 192},  // 7: WHITE
  {0,   0,   0  },  // 8: BRIGHT BLACK (BLACK)
  {0,   0,   255},  // 9: BRIGHT BLUE
  {255, 0,   0  },  // 10: BRIGHT RED
  {255, 0,   255},  // 11: BRIGHT MAGENTA
  {0,   255, 0  },  // 12: BRIGHT GREEN
  {0,   255, 255},  // 13: BRIGHT CYAN
  {255, 255, 0  },  // 14: BRIGHT YELLOW
  {255, 255, 255}   // 15: BRIGHT WHITE
};

// Находит следующий доступный номер для скриншота
int getNextScreenshotNumber() {
  int maxNum = 0;
  
  File dir = SD.open("/ZXscreenshots");
  if (!dir) {
    return 1;  // Папка не существует, начинаем с 1
  }
  
  File file = dir.openNextFile();
  while (file) {
    String name = file.name();
    if (name.startsWith("screenshot_") && name.endsWith(".bmp")) {
      // Извлекаем номер (screenshot_001.bmp → 001)
      int numStart = 11;  // После "screenshot_"
      int numEnd = name.indexOf(".bmp");
      String numStr = name.substring(numStart, numEnd);
      int num = numStr.toInt();
      if (num > maxNum) maxNum = num;
    }
    file.close();
    file = dir.openNextFile();
  }
  dir.close();
  
  return maxNum + 1;
}

// Сохраняет скриншот ZX Spectrum экрана (256×192) в BMP формат
bool saveScreenshotBMP() {
  Serial.println("\n📸 === SCREENSHOT START ===");
  
  // 1) Создаём папку если её нет
  if (!SD.exists("/ZXscreenshots")) {
    Serial.println("📁 Creating /ZXscreenshots/ folder...");
    if (!SD.mkdir("/ZXscreenshots")) {
      Serial.println("❌ Failed to create folder!");
      showNotification("SCREENSHOT FAILED!", TFT_RED, 2000);
      return false;
    }
    Serial.println("✅ Folder created!");
  }
  
  // 2) Находим следующий номер файла
  int num = getNextScreenshotNumber();
  char filename[64];
  snprintf(filename, sizeof(filename), "/ZXscreenshots/screenshot_%03d.bmp", num);
  
  Serial.printf("💾 Saving: %s\n", filename);
  
  // 3) Создаём файл
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("❌ Failed to create file!");
    showNotification("SCREENSHOT FAILED!", TFT_RED, 2000);
    return false;
  }
  
  // 4) BMP Header (54 байта)
  const int WIDTH = 256;
  const int HEIGHT = 192;
  const int PADDING = (4 - (WIDTH * 3) % 4) % 4;  // BMP требует выравнивание по 4 байта
  const int ROW_SIZE = WIDTH * 3 + PADDING;
  const int IMAGE_SIZE = ROW_SIZE * HEIGHT;
  const int FILE_SIZE = 54 + IMAGE_SIZE;
  
  // BMP File Header (14 bytes)
  uint8_t bmpHeader[54] = {
    // Signature "BM"
    0x42, 0x4D,
    // File size (4 bytes, little-endian)
    (uint8_t)(FILE_SIZE), (uint8_t)(FILE_SIZE >> 8), (uint8_t)(FILE_SIZE >> 16), (uint8_t)(FILE_SIZE >> 24),
    // Reserved
    0, 0, 0, 0,
    // Data offset (54)
    54, 0, 0, 0,
    // DIB Header size (40)
    40, 0, 0, 0,
    // Width (4 bytes)
    (uint8_t)(WIDTH), (uint8_t)(WIDTH >> 8), (uint8_t)(WIDTH >> 16), (uint8_t)(WIDTH >> 24),
    // Height (4 bytes) - NEGATIVE для top-down
    (uint8_t)(HEIGHT), (uint8_t)(HEIGHT >> 8), (uint8_t)(HEIGHT >> 16), (uint8_t)(HEIGHT >> 24),
    // Planes (always 1)
    1, 0,
    // Bits per pixel (24)
    24, 0,
    // Compression (0 = none)
    0, 0, 0, 0,
    // Image size
    (uint8_t)(IMAGE_SIZE), (uint8_t)(IMAGE_SIZE >> 8), (uint8_t)(IMAGE_SIZE >> 16), (uint8_t)(IMAGE_SIZE >> 24),
    // X pixels per meter (2835)
    0x13, 0x0B, 0, 0,
    // Y pixels per meter (2835)
    0x13, 0x0B, 0, 0,
    // Colors in palette (0 = default)
    0, 0, 0, 0,
    // Important colors (0 = all)
    0, 0, 0, 0
  };
  
  file.write(bmpHeader, 54);
  
  // 5) Получаем VRAM
  uint8_t* vram = spectrum->mem.getScreenData();
  
  // 6) Записываем пиксели (BMP хранит снизу вверх!)
  uint8_t rowBuffer[ROW_SIZE];
  
  for (int y = HEIGHT - 1; y >= 0; y--) {  // BMP: снизу вверх
    int bufPos = 0;
    
    for (int x = 0; x < WIDTH; x++) {
      // ZX Spectrum memory layout (сложный!)
      // https://www.worldofspectrum.org/faq/reference/48kreference.htm
      
      // Вычисляем адрес пикселя в VRAM
      int yy = y;
      int xx = x;
      
      // Pixel byte address
      int pixelByte = ((yy & 0xC0) << 5) | ((yy & 0x07) << 8) | ((yy & 0x38) << 2) | (xx >> 3);
      int pixelBit = 7 - (xx & 7);
      
      // Attribute address (6144 = 0x1800)
      int attrByte = 0x1800 + ((yy >> 3) << 5) + (xx >> 3);
      
      uint8_t pixel = (vram[pixelByte] >> pixelBit) & 1;
      uint8_t attr = vram[attrByte];
      
      // Извлекаем цвета из атрибута
      uint8_t ink = (attr & 0x07);        // Биты 0-2: INK
      uint8_t paper = (attr >> 3) & 0x07; // Биты 3-5: PAPER
      uint8_t bright = (attr >> 6) & 0x01; // Бит 6: BRIGHT
      
      // Выбираем цвет (pixel=1 → INK, pixel=0 → PAPER)
      uint8_t colorIndex = (pixel ? ink : paper) + (bright ? 8 : 0);
      
      // RGB (BMP хранит в порядке BGR!)
      rowBuffer[bufPos++] = zxPalette[colorIndex][2];  // B
      rowBuffer[bufPos++] = zxPalette[colorIndex][1];  // G
      rowBuffer[bufPos++] = zxPalette[colorIndex][0];  // R
    }
    
    // Padding
    for (int p = 0; p < PADDING; p++) {
      rowBuffer[bufPos++] = 0;
    }
    
    file.write(rowBuffer, ROW_SIZE);
  }
  
  file.close();
  
  Serial.printf("✅ Screenshot saved: %s (%d KB)\n", filename, FILE_SIZE / 1024);
  
  // Уведомление
  char notifText[64];
  snprintf(notifText, sizeof(notifText), "SCREENSHOT %03d.bmp", num);
  showNotification(notifText, TFT_GREEN, 2000);
  
  return true;
}

// ═══════════════════════════════════════════════════════════
// Воспроизведение звука через M5Cardputer.Speaker
// ═══════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════
// 🔊 ВОСПРОИЗВЕДЕНИЕ ЗВУКА (КАК В ДИКТОФОНЕ M5STACK!)
// ═══════════════════════════════════════════════════════════════════
// Частота ZX Spectrum beeper (фиксированная)
// ═══════════════════════════════════════════════════════════
// 🎵 CHATGPT BEEPER SOLUTION - V3.134
// ═══════════════════════════════════════════════════════════

// Легкая огибающая для устранения кликов на стыках
static inline void applyEnvelope(int16_t* dst, int n) {
  for (int i = 0; i < ENV_NS && i < n; ++i) {
    float a = float(i) / ENV_NS;
    int j = n - 1 - i;
    dst[i]  = int16_t(dst[i]  * a);
    dst[j]  = int16_t(dst[j]  * a);
  }
}

// Конвертация: accum[312] → PCM[320] @16kHz
// Duty-cycle + линейная интерполяция = плавный звук!
static void accumToPCM(const uint16_t* accum312, int16_t* out320) {
  // Предрасчёт duty [0..1] для 312 строк
  static float duty[312];
  for (int i = 0; i < 312; ++i) {
    duty[i] = float(accum312[i]) / float(TSTATES_LINE_CONST);  // 0..1
  }
  
  // Ресемплинг 312 → 320 с интерполяцией
  for (int s = 0; s < SPPF; ++s) {
    float pos = (float(s) * (312 - 1)) / float(SPPF - 1);  // в строках
    int i0    = (int)pos;
    int i1    = (i0 + 1 < 312) ? (i0 + 1) : i0;
    float frac = pos - i0;
    float d    = duty[i0] * (1.0f - frac) + duty[i1] * frac;  // интерполяция
    float v    = (2.0f * d - 1.0f);  // duty → [-1..+1]
    out320[s]  = int16_t(v * AMP);
  }
  
  applyEnvelope(out320, SPPF);
}

// ═══ AUDIO TASK: НЕПРЕРЫВНЫЙ ПОТОК! ═══
void Task_Audio(void* pv) {
  while (true) {
    // 1) Текущий буфер
    int16_t* curr = useA ? bufA : bufB;
    
    // 2) Забираем данные кадра от эмулятора
    uint16_t localAccum[312];
    
    if (frameReady) {
      noInterrupts();
      for (int i = 0; i < 312; ++i) localAccum[i] = accumFrame[i];
      frameReady = false;
      interrupts();
      
      // Применяем volume (масштабируем AMP)
      // Генерируем PCM с учетом громкости
      static float duty[312];
      for (int i = 0; i < 312; ++i) {
        duty[i] = float(localAccum[i]) / float(TSTATES_LINE_CONST);
      }
      
      // Volume: 0-10 → 0.0-1.0
      float volScale = soundEnabled ? (float(soundVolume) / 10.0f) : 0.0f;
      
      for (int s = 0; s < SPPF; ++s) {
        float pos = (float(s) * (312 - 1)) / float(SPPF - 1);
        int i0    = (int)pos;
        int i1    = (i0 + 1 < 312) ? (i0 + 1) : i0;
        float frac = pos - i0;
        float d    = duty[i0] * (1.0f - frac) + duty[i1] * frac;
        float v    = (2.0f * d - 1.0f);  // duty → [-1..+1]
        curr[s]  = int16_t(v * AMP * volScale);
      }
      
      applyEnvelope(curr, SPPF);
      
    } else {
      // Тишина (эмулятор не успел)
      memset(curr, 0, sizeof(int16_t) * SPPF);
    }
    
    // 3) Играть (ВАЖНО: количество СЭМПЛОВ, не байтов!)
    M5Cardputer.Speaker.playRaw(curr, SPPF, SAMPLE_RATE, false);
    
    // 4) Ждем окончания воспроизведения
    while (M5Cardputer.Speaker.isPlaying()) {
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    
    useA = !useA;
  }
}

// ═══ API ДЛЯ ЭМУЛЯТОРА ═══
// Вызывать ОДИН РАЗ на кадр (50 Hz)
void ZX_BeeperSubmitFrame(const uint16_t* accum312) {
  noInterrupts();
  for (int i = 0; i < 312; ++i) accumFrame[i] = accum312[i];
  frameReady = true;
  interrupts();
}

// Обработка клавиатуры M5Cardputer → ZX Spectrum
void handleKeyboard() {
  M5Cardputer.update();
  
  // ВСЕГДА обрабатываем клавиатуру (не только при isChange)
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  
  // ===== ДИАГНОСТИКА: Выводим ВСЁ что видит клавиатура =====
  static bool diagnostic = false;  // Отключена
  if (diagnostic && M5Cardputer.Keyboard.isChange()) {
    Serial.println("========== KEYBOARD DEBUG ==========");
    
    // Специальные клавиши
    if (status.enter) Serial.println("  enter = true");
    if (status.space) Serial.println("  space = true");
    if (status.del) Serial.println("  del = true");
    if (status.shift) Serial.println("  shift = true");
    if (status.fn) Serial.println("  fn = true");
    if (status.ctrl) Serial.println("  ctrl = true");
    if (status.opt) Serial.println("  opt = true");
    if (status.alt) Serial.println("  alt = true");
    if (status.tab) Serial.println("  tab = true");
    
    // word[] - массив символов
    if (!status.word.empty()) {
      Serial.print("  word[] = [");
      for (size_t i = 0; i < status.word.size(); i++) {
        char c = status.word[i];
        Serial.printf("'%c' (0x%02X)", c, (uint8_t)c);
        if (i < status.word.size() - 1) Serial.print(", ");
      }
      Serial.println("]");
    }
    
    // hid_keys[] - HID коды клавиш
    if (!status.hid_keys.empty()) {
      Serial.print("  hid_keys[] = [");
      for (size_t i = 0; i < status.hid_keys.size(); i++) {
        Serial.printf("0x%02X", status.hid_keys[i]);
        if (i < status.hid_keys.size() - 1) Serial.print(", ");
      }
      Serial.println("]");
    }
    
    Serial.println("====================================");
  }
  // ===== КОНЕЦ ДИАГНОСТИКИ =====
  
  // ═══ V3.134: ИЗМЕНЕНИЕ КОНТРОЛОВ ═══
  // ESC (без Opt) → Меню
  // Opt + ESC → Reset эмулятора
  
  static unsigned long lastMenuTime = 0;
  
  // 1) ESC (БЕЗ OPT) → ОТКРЫТЬ/ЗАКРЫТЬ МЕНЮ
  if (!status.opt && !status.hid_keys.empty()) {
    for (uint8_t hidKey : status.hid_keys) {
      if (hidKey == 0x35 && (millis() - lastMenuTime > 300)) {  // 0x35 = ` (ESC)
        showMenu = !showMenu;
        emulatorPaused = showMenu;
        gamePaused = false;  // ✅ V3.134: Сбрасываем паузу при открытии меню
        
        if (showMenu) {
          drawMainMenu();
          Serial.println("🔵 MENU OPENED (ESC)");
        } else {
          M5Cardputer.Display.fillScreen(BLACK);
          Serial.println("🔵 MENU CLOSED");
        }
        
        lastMenuTime = millis();
        return;  // Не обрабатываем остальные клавиши
      }
    }
  }
  
  // 2) OPT + ESC → RESET ЭМУЛЯТОРА
  if (status.opt && !status.hid_keys.empty()) {
    for (uint8_t hidKey : status.hid_keys) {
      if (hidKey == 0x35 && (millis() - lastMenuTime > 300)) {  // 0x35 = ` (ESC)
        Serial.println("🔄 RESET EMULATOR (Opt + ESC)");
        
        // Показываем сообщение
        M5Cardputer.Display.fillScreen(BLACK);
        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.setCursor(60, 60);
        M5Cardputer.Display.print("RESET...");
        delay(500);
        
        // Перезагружаем эмулятор
        spectrum->reset();
        
        // Закрываем все подменю и открываем главное меню
        showLoadGameMenu = false;
        showBrowser = false;
        showInformation = false;
        showMenu = true;         // Открываем меню
        emulatorPaused = true;   // Пауза
        gamePaused = false;      // ✅ V3.134: Сбрасываем паузу игры
        
        drawMainMenu();
        Serial.println("✅ EMULATOR RESET COMPLETE → MENU OPENED");
        
        lastMenuTime = millis();
        return;
      }
    }
  }
  
  // 3) TAB (БЕЗ OPT) → ПАУЗА/ВОЗОБНОВЛЕНИЕ ИГРЫ (V3.134)
  // Работает ТОЛЬКО во время игры (не в меню!)
  static unsigned long lastPauseTime = 0;
  if (!status.opt && !status.hid_keys.empty() && !showMenu && !showBrowser && !showInformation) {
    for (uint8_t hidKey : status.hid_keys) {
      if (hidKey == 0x2B && (millis() - lastPauseTime > 300)) {  // 0x2B = TAB
        gamePaused = !gamePaused;
        emulatorPaused = gamePaused;
        
        if (gamePaused) {
          Serial.println("⏸️  GAME PAUSED (TAB)");
        } else {
          Serial.println("▶️  GAME RESUMED (TAB)");
        }
        
        lastPauseTime = millis();
        return;  // Не обрабатываем остальные клавиши
      }
    }
  }
  
  // 4) CTRL (БЕЗ OPT) → SCREENSHOT (V3.136)
  // Работает ТОЛЬКО во время игры (не в меню!)
  static unsigned long lastScreenshotTime = 0;
  if (!status.opt && status.ctrl && !showMenu && !showLoadGameMenu && !showBrowser && !showInformation) {
    if (millis() - lastScreenshotTime > 500) {  // Debounce 500ms
      Serial.println("📸 SCREENSHOT (Ctrl)");
      saveScreenshotBMP();
      lastScreenshotTime = millis();
      return;  // Не обрабатываем остальные клавиши
    }
  }
  
  // ═══ ЕСЛИ INFORMATION ОТКРЫТ - ОБРАБОТКА НАВИГАЦИИ ═══
  if (showInformation) {
    // Обработка клавиш , и / для переключения страниц
    static unsigned long lastInfoNavTime = 0;
    
    for (char key : status.word) {
      if (millis() - lastInfoNavTime > 200) {  // Debounce
        if (key == ',') {
          // , (←) = Предыдущая страница
          informationPage--;
          if (informationPage < 0) informationPage = 5;  // Циклическая (0-5)
          drawInformationScreen(informationPage);
          lastInfoNavTime = millis();
          return;
        } else if (key == '/') {
          // / (→) = Следующая страница
          informationPage++;
          if (informationPage > 5) informationPage = 0;  // Циклическая (0-5)
          drawInformationScreen(informationPage);
          lastInfoNavTime = millis();
          return;
        }
      }
    }
    
    // Обработка ` для выхода
    for (uint8_t hid : status.hid_keys) {
      if (hid == 0x35) {  // Backtick (ESC на Cardputer)
        Serial.println("📌 INFO: Back to menu");
        showInformation = false;
        showMenu = true;
        emulatorPaused = true;
        drawMainMenu();
        return;
      }
    }
    
    // Joystick2 навигация (если подключён)
    if (joystick2Available) {
      Joystick2Data joyData = readJoystick2();
      static unsigned long lastJoyInfoTime = 0;
      
      // ✅ V3.134: УБРАЛИ кнопку джойстика для выхода - только ESC!
      // Навигация left/right для смены страниц
      if (millis() - lastJoyInfoTime > 500) {  // ✅ Увеличили debounce с 300 до 500ms
        if (joyData.x > 175) {  // ✅ RIGHT = предыдущая страница (инвертировано!)
          informationPage--;
          if (informationPage < 0) informationPage = 5;  // Циклическая (0-5)
          drawInformationScreen(informationPage);
          lastJoyInfoTime = millis();
          return;
        } else if (joyData.x < 80) {  // ✅ LEFT = следующая страница (инвертировано!)
          informationPage++;
          if (informationPage > 5) informationPage = 0;  // Циклическая (0-5)
          drawInformationScreen(informationPage);
          lastJoyInfoTime = millis();
          return;
        }
      }
    }
    
    return;  // Блокируем другие клавиши
  }
  
  // ═══ ЕСЛИ БРАУЗЕР ОТКРЫТ - ОБРАБОТКА НАВИГАЦИИ ═══
  if (showBrowser) {
    // ═══ JOYSTICK2 НАВИГАЦИЯ (ЕСЛИ ПОДКЛЮЧЁН) ═══
    if (joystick2Available) {
      Joystick2Data joyData = readJoystick2();
      
      // Вертикальная навигация
      static unsigned long lastJoyNavTime = 0;
      if (millis() - lastJoyNavTime > 200) {  // Debounce 200ms
        
        // ВНИМАНИЕ! Y-ось ИНВЕРТИРОВАНА!
        // Физически ВВЕРХ (y < 100) → список ВНИЗ (selectedFile++)
        // Физически ВНИЗ (y > 155) → список ВВЕРХ (selectedFile--)
        
        if (joyData.y < 100) {
          // Джойстик ВВЕРХ → список ВНИЗ
          if (selectedFile < filteredCount - 1) {
            selectedFile++;
            drawFileBrowser();
            Serial.printf("🕹️  JOY UP → File %d/%d\n", selectedFile + 1, filteredCount);
            lastJoyNavTime = millis();
          }
        } else if (joyData.y > 155) {
          // Джойстик ВНИЗ → список ВВЕРХ
          if (selectedFile > 0) {
            selectedFile--;
            drawFileBrowser();
            Serial.printf("🕹️  JOY DOWN → File %d/%d\n", selectedFile + 1, filteredCount);
            lastJoyNavTime = millis();
          }
        }
      }
      
      // Кнопка = Загрузка файла (с ГЛОБАЛЬНЫМ debounce)
      bool buttonPressed = (joyData.button == 1);
      
      // СТРОГИЙ debounce: 500ms И проверка "кнопка отпущена" (ГЛОБАЛЬНО!)
      if (buttonPressed && !globalJoyButtonState && (millis() - globalJoyButtonPressTime > JOY_BUTTON_DEBOUNCE)) {
        String fileName = filteredFiles[selectedFile];
        Serial.printf("🕹️  JOY BUTTON → Loading: %s\n", fileName.c_str());
        globalJoyButtonPressTime = millis();
        globalJoyButtonState = true;  // Запоминаем что кнопка нажата
        
        // Показываем "Loading..."
        M5Cardputer.Display.fillScreen(BLACK);
        M5Cardputer.Display.setCursor(40, 60);
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.print("Loading...");
        
        // Загружаем файл
        bool success = false;
        if (browserFilter == ".SNA") {
          success = loadSNAFile(fileName);
        } else if (browserFilter == ".TAP") {
          success = loadTAPFile(fileName);
        } else if (browserFilter == ".Z80") {
          success = loadZ80File(fileName);  // ✅ Z80 LOADER! (V3.134)
        }
        
        if (success) {
          // Игра загружена - закрываем браузер и запускаем!
          showBrowser = false;
          showMenu = false;
          emulatorPaused = false;
          M5Cardputer.Display.fillScreen(BLACK);
        } else {
          // Ошибка загрузки - возвращаемся в меню
          showBrowser = false;
          showMenu = true;
          drawMainMenu();
        }
        
        return;
      }
      
      // Обновляем глобальное состояние
      if (!buttonPressed) {
        globalJoyButtonState = false;  // Кнопка отпущена
      }
    }
    
    // ═══ КЛАВИАТУРНАЯ НАВИГАЦИЯ (АЛЬТЕРНАТИВА) ═══
    // Обработка ` (ESC) - выход в меню
    if (!status.hid_keys.empty()) {
      for (uint8_t hidKey : status.hid_keys) {
        if (hidKey == 0x35) {  // 0x35 = ` (backtick)
          Serial.println("📂 BROWSER: ESC pressed - back to menu");
          showBrowser = false;
          showMenu = true;
          drawMainMenu();
          return;
        }
      }
    }
    
    // Обработка навигации (`;` / `.`)
    static unsigned long lastKeyNavTime = 0;
    if (!status.word.empty() && (millis() - lastKeyNavTime > 200)) {  // Debounce 200ms
      char key = status.word[0];
      
      if (key == ';') {
        // ; = Вверх по списку
        if (selectedFile > 0) {
          selectedFile--;
          drawFileBrowser();
          Serial.printf("📂 BROWSER: UP -> File %d/%d\n", selectedFile + 1, filteredCount);
          lastKeyNavTime = millis();
        }
        return;
        
      } else if (key == '.') {
        // . = Вниз по списку
        if (selectedFile < filteredCount - 1) {
          selectedFile++;
          drawFileBrowser();
          Serial.printf("📂 BROWSER: DOWN -> File %d/%d\n", selectedFile + 1, filteredCount);
          lastKeyNavTime = millis();
        }
        return;
      }
    }
    
    // Обработка Enter (отдельно от word!) с ГЛОБАЛЬНЫМ DEBOUNCE
    bool enterPressed = status.enter;
    
    // СТРОГИЙ debounce: 500ms И проверка "кнопка отпущена" (ГЛОБАЛЬНО!)
    if (enterPressed && !globalEnterButtonState && (millis() - globalEnterButtonPressTime > ENTER_BUTTON_DEBOUNCE)) {
      // Enter = Загрузка файла
      String fileName = filteredFiles[selectedFile];
      Serial.printf("⌨️  BROWSER: ENTER → Loading: %s\n", fileName.c_str());
      globalEnterButtonPressTime = millis();
      globalEnterButtonState = true;  // Запоминаем что кнопка нажата
      
      // Показываем "Loading..."
      M5Cardputer.Display.fillScreen(BLACK);
      M5Cardputer.Display.setCursor(40, 60);
      M5Cardputer.Display.setTextColor(TFT_YELLOW);
      M5Cardputer.Display.print("Loading...");
      
      // Загружаем файл
      bool success = false;
      if (browserFilter == ".SNA") {
        success = loadSNAFile(fileName);
      } else if (browserFilter == ".TAP") {
        success = loadTAPFile(fileName);
      } else if (browserFilter == ".Z80") {
        success = loadZ80File(fileName);  // ✅ Z80 LOADER! (V3.134)
      }
      
      if (success) {
        // Игра загружена - закрываем браузер и запускаем!
        showBrowser = false;
        showMenu = false;
        emulatorPaused = false;
        M5Cardputer.Display.fillScreen(BLACK);
      } else {
        // Ошибка загрузки - возвращаемся в меню
        showBrowser = false;
        showMenu = true;
        drawMainMenu();
      }
      return;
    }
    
    // Обновляем глобальное состояние Enter (ВАЖНО: отслеживаем отпускание кнопки!)
    if (!enterPressed) {
      globalEnterButtonState = false;  // Кнопка отпущена
    }
    
    // Если браузер открыт - не обрабатываем ZX клавиши!
    for (int i = 0; i < 8; i++) {
      speckey[i] = 0xFF;
    }
    return;
  }
  
  // ═══ V3.134: ЕСЛИ МЕНЮ ОТКРЫТО - ОБРАБОТКА НАВИГАЦИИ И ВЫБОРА ═══
  if (showMenu || showLoadGameMenu) {
    // ═══ ОБРАБОТКА ESC (`) ДЛЯ ВОЗВРАТА ИЗ ПОДМЕНЮ ═══
    if (showLoadGameMenu && !status.hid_keys.empty()) {
      for (uint8_t hidKey : status.hid_keys) {
        if (hidKey == 0x35) {  // 0x35 = ` (ESC на Cardputer)
          Serial.println("📌 SUBMENU: ESC → Back to main menu");
          showLoadGameMenu = false;
          showMenu = true;
          drawMainMenu();
          return;
        }
      }
    }
    
    // ═══ НАВИГАЦИЯ ДЖОЙСТИКОМ (ЕСЛИ ПОДКЛЮЧЁН) ═══
    if (joystick2Available) {
      Joystick2Data joyData = readJoystick2();
      
      // Вертикальная навигация
      static unsigned long lastMenuJoyNavTime = 0;
      if (millis() - lastMenuJoyNavTime > 200) {  // Debounce 200ms
        
        // ВНИМАНИЕ! Y-ось ИНВЕРТИРОВАНА (как в браузере)!
        // Физически ВВЕРХ (y < 100) → пункт ВНИЗ (selectedMenuItem++)
        // Физически ВНИЗ (y > 155) → пункт ВВЕРХ (selectedMenuItem--)
        
        if (joyData.y < 100) {
          // Джойстик физически ВВЕРХ → пункт ВНИЗ
          if (showMenu) {
            selectedMenuItem++;
            if (selectedMenuItem > 3) selectedMenuItem = 0;  // V3.134: 4 пункта (0-3)
            drawMainMenu();
            Serial.printf("🕹️  MENU: JOY UP → Item %d\n", selectedMenuItem);
          } else if (showLoadGameMenu) {
            selectedLoadGameItem++;
            if (selectedLoadGameItem > 3) selectedLoadGameItem = 0;  // V3.134: 4 пункта (0-3)
            drawLoadGameMenu();
            Serial.printf("🕹️  LOAD GAME: JOY UP → Item %d\n", selectedLoadGameItem);
          }
          lastMenuJoyNavTime = millis();
          
        } else if (joyData.y > 155) {
          // Джойстик физически ВНИЗ → пункт ВВЕРХ
          if (showMenu) {
            selectedMenuItem--;
            if (selectedMenuItem < 0) selectedMenuItem = 3;  // V3.134: 4 пункта (0-3)
            drawMainMenu();
            Serial.printf("🕹️  MENU: JOY DOWN → Item %d\n", selectedMenuItem);
          } else if (showLoadGameMenu) {
            selectedLoadGameItem--;
            if (selectedLoadGameItem < 0) selectedLoadGameItem = 3;  // V3.134: 4 пункта (0-3)
            drawLoadGameMenu();
            Serial.printf("🕹️  LOAD GAME: JOY DOWN → Item %d\n", selectedLoadGameItem);
          }
          lastMenuJoyNavTime = millis();
        }
      }
      
      // Кнопка = Выбор пункта (с ГЛОБАЛЬНЫМ debounce)
      bool buttonPressed = (joyData.button == 1);
      
      // СТРОГИЙ debounce: 500ms И проверка "кнопка отпущена" (ГЛОБАЛЬНО!)
      if (buttonPressed && !globalJoyButtonState && (millis() - globalJoyButtonPressTime > JOY_BUTTON_DEBOUNCE)) {
        Serial.printf("🕹️  MENU: BUTTON → Execute item %d\n", selectedMenuItem);
        globalJoyButtonPressTime = millis();
        globalJoyButtonState = true;  // Запоминаем что кнопка нажата
        // Выполняем действие для выбранного пункта (код ниже)
        goto executeMenuItem;  // Переход к обработке выбранного пункта
      }
      
      // Обновляем глобальное состояние
      if (!buttonPressed) {
        globalJoyButtonState = false;  // Кнопка отпущена
      }
    }
    
    // ═══ НАВИГАЦИЯ КЛАВИАТУРОЙ (СТРЕЛКИ) ═══
    static unsigned long lastMenuKeyNavTime = 0;
    if (!status.word.empty() && (millis() - lastMenuKeyNavTime > 200)) {  // Debounce 200ms
      char key = status.word[0];
      
      if (key == ';') {
        // ; = Вверх
        if (showMenu) {
          selectedMenuItem--;
          if (selectedMenuItem < 0) selectedMenuItem = 3;  // V3.134: 4 пункта (0-3)
          drawMainMenu();
          Serial.printf("⌨️  MENU: UP → Item %d\n", selectedMenuItem);
        } else if (showLoadGameMenu) {
          selectedLoadGameItem--;
          if (selectedLoadGameItem < 0) selectedLoadGameItem = 3;  // V3.134: 4 пункта (0-3)
          drawLoadGameMenu();
          Serial.printf("⌨️  LOAD GAME: UP → Item %d\n", selectedLoadGameItem);
        }
        lastMenuKeyNavTime = millis();
        return;
        
      } else if (key == '.') {
        // . = Вниз
        if (showMenu) {
          selectedMenuItem++;
          if (selectedMenuItem > 3) selectedMenuItem = 0;  // V3.134: 4 пункта (0-3)
          drawMainMenu();
          Serial.printf("⌨️  MENU: DOWN → Item %d\n", selectedMenuItem);
        } else if (showLoadGameMenu) {
          selectedLoadGameItem++;
          if (selectedLoadGameItem > 3) selectedLoadGameItem = 0;  // V3.134: 4 пункта (0-3)
          drawLoadGameMenu();
          Serial.printf("⌨️  LOAD GAME: DOWN → Item %d\n", selectedLoadGameItem);
        }
        lastMenuKeyNavTime = millis();
        return;
      }
    }
    
    // ═══ ВЫБОР ПУНКТА (ENTER) с ГЛОБАЛЬНЫМ DEBOUNCE ═══
    bool enterPressed = status.enter;
    
    // СТРОГИЙ debounce: 500ms И проверка "кнопка отпущена" (ГЛОБАЛЬНО!)
    if (enterPressed && !globalEnterButtonState && (millis() - globalEnterButtonPressTime > ENTER_BUTTON_DEBOUNCE)) {
      Serial.printf("⌨️  MENU: ENTER → Execute item %d\n", selectedMenuItem);
      globalEnterButtonPressTime = millis();
      globalEnterButtonState = true;  // Запоминаем что кнопка нажата
      goto executeMenuItem;
    }
    
    // Обновляем глобальное состояние Enter
    if (!enterPressed) {
      globalEnterButtonState = false;  // Кнопка отпущена
    }
    
    // ═══ V3.134: БЫСТРЫЙ ВЫБОР ЦИФРАМИ (1-4) с DEBOUNCE ═══
    static unsigned long lastDigitKeyTime = 0;
    if (!status.word.empty() && (millis() - lastDigitKeyTime > 300)) {  // Debounce 300ms
      char key = status.word[0];
      
      if (key >= '1' && key <= '4') {  // V3.134: 4 пункта (1-4)
        lastDigitKeyTime = millis();  // Обновляем время последнего нажатия
        
        if (showMenu) {
          selectedMenuItem = key - '1';  // '1'->0, '2'->1, '3'->2, '4'->3
          goto executeMenuItem;
        } else if (showLoadGameMenu) {
          selectedLoadGameItem = key - '1';
          goto executeMenuItem;
        }
      }
    }
    
    // Если меню открыто, но не выполнили действие - выходим
    for (int i = 0; i < 8; i++) {
      speckey[i] = 0xFF;
    }
    return;
  }
  
  // ═══ ВЫПОЛНЕНИЕ ДЕЙСТВИЯ ДЛЯ ВЫБРАННОГО ПУНКТА ═══
  executeMenuItem:
  // ═══ V3.134: ГЛАВНОЕ МЕНЮ (5 пунктов) ═══
  if (showMenu) {
    if (selectedMenuItem == 0) {
      // 1. Basic - возврат к BASIC
      Serial.println("📌 MENU: Basic selected");
      showMenu = false;
      emulatorPaused = false;
      M5Cardputer.Display.fillScreen(BLACK);
      return;
      
    } else if (selectedMenuItem == 1) {
      // 2. Load Game - открываем подменю
      Serial.println("📌 MENU: Load Game selected → opening submenu");
      showMenu = false;
      showLoadGameMenu = true;
      selectedLoadGameItem = 0;  // Сброс выбора в подменю
      drawLoadGameMenu();
      return;
      
    } else if (selectedMenuItem == 2) {
      // 3. Reset Emulator
      Serial.println("📌 MENU: Reset Emulator");
      M5Cardputer.Display.fillScreen(BLACK);
      M5Cardputer.Display.setTextSize(2);
      M5Cardputer.Display.setCursor(50, 60);
      M5Cardputer.Display.setTextColor(TFT_YELLOW);
      M5Cardputer.Display.print("RESET...");
      delay(500);
      
      // Сброс эмулятора
      spectrum->reset_spectrum();
      
      // ✅ V3.134: Открываем меню после сброса
      showMenu = true;
      emulatorPaused = true;
      showLoadGameMenu = false;
      showBrowser = false;
      showInformation = false;
      
      drawMainMenu();
      Serial.println("✅ Emulator reset complete → MENU OPENED");
      return;
      
    } else if (selectedMenuItem == 3) {
      // 4. Information
      Serial.println("📌 MENU: Information selected");
      showMenu = false;
      showInformation = true;
      informationPage = 0;  // Начинаем с первой страницы
      drawInformationScreen(informationPage);
      return;
    }
    // V3.134: "Back" убран! Используй ESC (`) для возврата
  }
  
  // ═══ V3.134: ПОДМЕНЮ "LOAD GAME" (5 пунктов) ═══
  if (showLoadGameMenu) {
    if (selectedLoadGameItem == 0) {
      // 1. .SNA files
      Serial.println("📌 LOAD GAME: .SNA selected");
      browserFilter = ".SNA";
      filterFiles(".SNA");
      showLoadGameMenu = false;
      showBrowser = true;
      drawFileBrowser();
      return;
      
    } else if (selectedLoadGameItem == 1) {
      // 2. .Z80 files
      Serial.println("📌 LOAD GAME: .Z80 selected");
      browserFilter = ".Z80";
      filterFiles(".Z80");
      showLoadGameMenu = false;
      showBrowser = true;
      drawFileBrowser();
      return;
      
    } else if (selectedLoadGameItem == 2) {
      // 3. .TAP files
      Serial.println("📌 LOAD GAME: .TAP selected");
      browserFilter = ".TAP";
      filterFiles(".TAP");
      showLoadGameMenu = false;
      showBrowser = true;
      drawFileBrowser();
      return;
      
    } else if (selectedLoadGameItem == 3) {
      // 4. Audio TAP - NEW FEATURE! (V3.134)
      Serial.println("📌 LOAD GAME: Audio TAP selected");
      showLoadGameMenu = false;
      
      // TODO V3.134: Implement Audio TAP Loading
      // Пока заглушка
      M5Cardputer.Display.fillScreen(BLACK);
      M5Cardputer.Display.setCursor(30, 60);
      M5Cardputer.Display.setTextColor(TFT_YELLOW);
      M5Cardputer.Display.print("Audio TAP Loading");
      M5Cardputer.Display.setCursor(50, 80);
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.setTextColor(TFT_WHITE);
      M5Cardputer.Display.print("Coming soon!");
      delay(2000);
      
      // Возвращаемся в подменю
      showLoadGameMenu = true;
      drawLoadGameMenu();
      return;
    }
    // V3.134: "Back" убран! Используй ESC (`) для возврата в главное меню
  }
  
  // ═══ ОБРАБОТКА ZOOM/PAN (ПЕРЕД ZX Spectrum клавишами!) ═══
  
  // Флаг: если обработали системную команду (ZOOM/PAN), НЕ передавать клавиши в ZX
  bool skipZXKeys = false;
  
  // OPT + Z → ПЕРЕКЛЮЧЕНИЕ ZOOM
  static unsigned long lastZoomTime = 0;
  if (status.opt && !status.word.empty()) {
    for (char key : status.word) {
      if ((key == 'z' || key == 'Z') && (millis() - lastZoomTime > 200)) {
        // Циклический переключатель: 1.0 → 1.5 → 2.0 → 2.5 → 1.0
        if (zoomLevel < 1.3) {
          zoomLevel = 1.5;
        } else if (zoomLevel < 1.8) {
          zoomLevel = 2.0;
        } else if (zoomLevel < 2.3) {
          zoomLevel = 2.5;
        } else {
          // Возврат к ×1.0
          zoomLevel = 1.0;
          panX = 0;  // Сброс PAN
          panY = 0;
        }
        
        // Автопозиционирование на нижний левый угол (где ZX текст!)
        if (zoomLevel > 1.0) {
          // При zoom>1.0: используем ВЕСЬ экран (240×135)
          const int RENDER_W = 240;  // Весь экран!
          const int RENDER_H = 135;
          int ZX_VIEW_W = (int)(RENDER_W / zoomLevel);
          int ZX_VIEW_H = (int)(RENDER_H / zoomLevel);
          
          int maxPanX = (256 - ZX_VIEW_W) / 2;
          int maxPanY = (192 - ZX_VIEW_H) / 2;
          panX = -maxPanX;  // Максимально влево
          panY = maxPanY;   // Максимально вниз
        }
        
        Serial.printf("🔍 Zoom: x%.1f (bottom-left corner)\n", zoomLevel);
        lastZoomTime = millis();
        skipZXKeys = true;  // НЕ передавать 'z' в ZX Spectrum!
      }
      
      // OPT + P → ПЕРЕКЛЮЧЕНИЕ РЕЖИМА (ZOOM ↔ PIXEL-PERFECT)
      if ((key == 'p' || key == 'P') && (millis() - lastZoomTime > 200)) {
        if (renderMode == MODE_ZOOM) {
          // Переключаемся на PIXEL-PERFECT
          renderMode = MODE_PIXEL_PERFECT;
          pixelPerfectPanX = 0;             // V3.134: левый край
          pixelPerfectPanY = PP_MAX_PAN_Y;  // По низу экрана (левый нижний угол!)
          Serial.printf("🎯 Переключение: ZOOM → PIXEL-PERFECT (1:1, x=%d, y=%d)\n", pixelPerfectPanX, pixelPerfectPanY);
        } else {
          // Переключаемся обратно на ZOOM
          renderMode = MODE_ZOOM;
          zoomLevel = 1.0;  // Сброс зума
          panX = 0;
          panY = 0;
          Serial.println("🎯 Переключение: PIXEL-PERFECT → ZOOM");
        }
        lastZoomTime = millis();
        skipZXKeys = true;  // НЕ передавать 'p' в ZX Spectrum!
      }
      
      // OPT + J → ПЕРЕКЛЮЧЕНИЕ JOYSTICK (вкл/выкл)
      if ((key == 'j' || key == 'J') && (millis() - lastZoomTime > 200)) {
        joystickEnabled = !joystickEnabled;
        Serial.printf("🕹️  Joystick→Keys: %s\n", joystickEnabled ? "ENABLED" : "DISABLED");
        
        // Показываем уведомление на экране (если не в меню/браузере)
        if (!showMenu && !showBrowser) {
          M5Cardputer.Display.fillRect(40, 60, 160, 20, BLACK);
          M5Cardputer.Display.drawRect(40, 60, 160, 20, WHITE);
          M5Cardputer.Display.setTextSize(1);
          M5Cardputer.Display.setTextColor(joystickEnabled ? TFT_GREEN : TFT_RED);
          M5Cardputer.Display.setCursor(50, 65);
          M5Cardputer.Display.print(joystickEnabled ? "Joystick: ON" : "Joystick: OFF");
        }
        lastZoomTime = millis();
        skipZXKeys = true;
      }
      
      // OPT + [+] (=) → VOLUME UP
      if ((key == '=' || key == '+') && (millis() - lastZoomTime > 200)) {
        if (soundVolume < 10) soundVolume++;
        Serial.printf("🔊 Volume: %d/10\n", soundVolume);
        
        // ✅ V3.134: Асинхронное уведомление (БЕЗ delay!)
        if (!showMenu && !showBrowser) {
          char msg[32];
          snprintf(msg, sizeof(msg), "Volume: %d/10", soundVolume);
          showNotification(msg, TFT_YELLOW, 500);
        }
        lastZoomTime = millis();
        skipZXKeys = true;
      }
      
      // OPT + [-] → VOLUME DOWN
      if ((key == '-' || key == '_') && (millis() - lastZoomTime > 200)) {
        if (soundVolume > 0) soundVolume--;
        Serial.printf("🔉 Volume: %d/10\n", soundVolume);
        
        // ✅ V3.134: Асинхронное уведомление (БЕЗ delay!)
        if (!showMenu && !showBrowser) {
          char msg[32];
          snprintf(msg, sizeof(msg), "Volume: %d/10", soundVolume);
          showNotification(msg, TFT_YELLOW, 500);
        }
        lastZoomTime = millis();
        skipZXKeys = true;
      }
      
      // OPT + M → MUTE ON/OFF
      if ((key == 'm' || key == 'M') && (millis() - lastZoomTime > 200)) {
        soundEnabled = !soundEnabled;
        Serial.printf("🔇 Sound: %s\n", soundEnabled ? "ENABLED" : "MUTED");
        
        // ✅ V3.134: Асинхронное уведомление (БЕЗ delay!)
        if (!showMenu && !showBrowser) {
          showNotification(
            soundEnabled ? "Sound: ON" : "Sound: MUTED",
            soundEnabled ? TFT_GREEN : TFT_RED,
            1000  // 1 секунда для mute/unmute
          );
        }
        
        lastZoomTime = millis();
        skipZXKeys = true;  // НЕ передавать 'm' в ZX Spectrum!
      }
    }
  }
  
  // OPT + СТРЕЛКИ → PAN (для ZOOM и PIXEL-PERFECT)
  static unsigned long lastPanTime = 0;
  
  // РЕЖИМ PIXEL-PERFECT: вертикальный + горизонтальный PAN (V3.134!)
  if (renderMode == MODE_PIXEL_PERFECT && status.opt && !status.word.empty() && (millis() - lastPanTime > 100)) {
    for (char key : status.word) {
      if (key == ';') {  // Opt + ; = Вверх
        pixelPerfectPanY -= PAN_STEP;
        if (pixelPerfectPanY < 0) pixelPerfectPanY = 0;
        Serial.printf("⬆️ PP PAN UP (x=%d, y=%d)\n", pixelPerfectPanX, pixelPerfectPanY);
        lastPanTime = millis();
        skipZXKeys = true;
        
      } else if (key == '.') {  // Opt + . = Вниз
        pixelPerfectPanY += PAN_STEP;
        if (pixelPerfectPanY > PP_MAX_PAN_Y) pixelPerfectPanY = PP_MAX_PAN_Y;
        Serial.printf("⬇️ PP PAN DOWN (x=%d, y=%d)\n", pixelPerfectPanX, pixelPerfectPanY);
        lastPanTime = millis();
        skipZXKeys = true;
        
      } else if (key == ',') {  // V3.134: Opt + , = Влево
        pixelPerfectPanX -= PAN_STEP;
        if (pixelPerfectPanX < 0) pixelPerfectPanX = 0;
        Serial.printf("⬅️ PP PAN LEFT (x=%d, y=%d)\n", pixelPerfectPanX, pixelPerfectPanY);
        lastPanTime = millis();
        skipZXKeys = true;
        
      } else if (key == '/') {  // V3.134: Opt + / = Вправо
        pixelPerfectPanX += PAN_STEP;
        if (pixelPerfectPanX > PP_MAX_PAN_X) pixelPerfectPanX = PP_MAX_PAN_X;
        Serial.printf("➡️ PP PAN RIGHT (x=%d, y=%d)\n", pixelPerfectPanX, pixelPerfectPanY);
        lastPanTime = millis();
        skipZXKeys = true;
      }
    }
  }
  
  // РЕЖИМ ZOOM: полный PAN (вверх/вниз/влево/вправо)
  if (renderMode == MODE_ZOOM && zoomLevel > 1.0 && status.opt && !status.word.empty() && (millis() - lastPanTime > 100)) {
    // Вычисляем максимальное смещение (используем ВЕСЬ экран при zoom!)
    const int RENDER_W = 240;  // Весь экран!
    const int RENDER_H = 135;
    int ZX_VIEW_W = (int)(RENDER_W / zoomLevel);
    int ZX_VIEW_H = (int)(RENDER_H / zoomLevel);
    
    int maxPanX = (256 - ZX_VIEW_W) / 2;
    int maxPanY = (192 - ZX_VIEW_H) / 2;
    
    for (char key : status.word) {
      if (key == ';') {  // Opt + ; = Вверх
        panY -= PAN_STEP;
        if (panY < -maxPanY) panY = -maxPanY;
        Serial.println("⬆️ PAN UP");
        lastPanTime = millis();
        skipZXKeys = true;  // НЕ передавать ';' в ZX Spectrum!
        
      } else if (key == '.') {  // Opt + . = Вниз
        panY += PAN_STEP;
        if (panY > maxPanY) panY = maxPanY;
        Serial.println("⬇️ PAN DOWN");
        lastPanTime = millis();
        skipZXKeys = true;  // НЕ передавать '.' в ZX Spectrum!
        
      } else if (key == ',') {  // Opt + , = Влево
        panX -= PAN_STEP;
        if (panX < -maxPanX) panX = -maxPanX;
        Serial.println("⬅️ PAN LEFT");
        lastPanTime = millis();
        skipZXKeys = true;  // НЕ передавать ',' в ZX Spectrum!
        
      } else if (key == '/') {  // Opt + / = Вправо
        panX += PAN_STEP;
        if (panX > maxPanX) panX = maxPanX;
        Serial.println("➡️ PAN RIGHT");
        lastPanTime = millis();
        skipZXKeys = true;  // НЕ передавать '/' в ZX Spectrum!
      }
    }
  }
  
  // ═══ ОБРАБОТКА ZX SPECTRUM КЛАВИШ ═══
  
  // ⚠️ КРИТИЧНО: Если нажат OPT - это СИСТЕМНАЯ кнопка, НЕ передаем клавиши в ZX!
  if (status.opt) {
    // Отпускаем все клавиши ZX Spectrum
    for (int i = 0; i < 8; i++) {
      speckey[i] = 0xFF;
    }
    return;  // Выходим, не обрабатывая ZX клавиши
  }
  
  // ⚠️ ВАЖНО: Если обработали ZOOM/PAN, НЕ передаем клавиши в ZX Spectrum!
  if (skipZXKeys) {
    // Отпускаем все клавиши ZX Spectrum
    for (int i = 0; i < 8; i++) {
      speckey[i] = 0xFF;
    }
    return;  // Выходим из функции, не обрабатывая ZX клавиши
  }
  
  // Сначала отпускаем ВСЕ клавиши
  for (int i = 0; i < 8; i++) {
    speckey[i] = 0xFF;
  }
  
  // ДОПОЛНИТЕЛЬНАЯ ДИАГНОСТИКА: показываем КАЖДЫЙ кадр (если клавиша нажата)
  static int diagFrames = 0;
  if (diagnostic && (status.shift || !status.word.empty())) {
    if (diagFrames++ < 50) {  // Только первые 50 кадров
      Serial.printf("Frame #%d: shift=%d word.size=%d\n", 
                    diagFrames, status.shift, status.word.size());
    }
  }
  
  // Теперь нажимаем только те, которые РЕАЛЬНО нажаты СЕЙЧАС
  
  // ENTER
  if (status.enter) spectrum->updateKey(SPECKEY_ENTER, 1);
  
  // SPACE
  if (status.space) spectrum->updateKey(SPECKEY_SPACE, 1);
  
  // SHIFT → CAPS SHIFT (только если явно нажат shift!)
  if (status.shift) {
    spectrum->updateKey(SPECKEY_SHIFT, 1);
    if (diagnostic) Serial.println("  >>> SHIFT PRESSED <<<");
  }
  
  // Fn → SYMBOL SHIFT
  if (status.fn) spectrum->updateKey(SPECKEY_SYMB, 1);
  
  // BACKSPACE → DELETE (CAPS SHIFT + 0)
  if (status.del) {
    spectrum->updateKey(SPECKEY_SHIFT, 1);
    spectrum->updateKey(SPECKEY_0, 1);
  }
  
  // Обрабатываем буквы и цифры из word[]
  for (char c : status.word) {
    // ═══ СТРЕЛКИ ДЛЯ ИГР (QAOP - стандартная схема ZX Spectrum) ═══
    // ; . , / - те же кнопки что и для PAN, но БЕЗ Opt
    if (c == ';') {
      // ; → Q (вверх)
      spectrum->updateKey(SPECKEY_Q, 1);
      continue;  // Не обрабатывать дальше
    } else if (c == '.') {
      // . → A (вниз)
      spectrum->updateKey(SPECKEY_A, 1);
      continue;
    } else if (c == ',') {
      // , → O (влево)
      spectrum->updateKey(SPECKEY_O, 1);
      continue;
    } else if (c == '/') {
      // / → P (вправо)
      spectrum->updateKey(SPECKEY_P, 1);
      continue;
    }
    
    // Преобразуем заглавные в строчные (Shift обрабатывается отдельно)
    if (c >= 'A' && c <= 'Z') {
      c = c - 'A' + 'a';  // A→a, B→b, etc.
    }
    
    // Проверяем letterToSpecKeys для символов
    auto it = letterToSpecKeys.find(c);
    if (it != letterToSpecKeys.end()) {
      // Нашли маппинг - нажимаем все нужные клавиши
      for (SpecKeys key : it->second) {
        spectrum->updateKey(key, 1);
      }
    }
  }
}

void loop() {
  static unsigned long lastFrameTime = 0;
  static int renderCounter = 0;
  unsigned long frameStart = micros();
  
  // ═══ V3.137: ПОКАЗЫВАЕМ УВЕДОМЛЕНИЕ О ПАПКЕ (ОДИН РАЗ!) ═══
  if (!folderNotificationShown && gameFolderStatus >= 0) {
    if (gameFolderStatus == 0) {
      // Папка создана
      showNotification("FOLDER CREATED: /ZXgames/", TFT_GREEN, 3000);
    } else if (gameFolderStatus == 1) {
      // Папка уже существовала
      showNotification("FOLDER EXISTS: /ZXgames/", TFT_CYAN, 2000);
    }
    folderNotificationShown = true;
  }
  
  // Обрабатываем клавиатуру ПЕРЕД каждым кадром
  handleKeyboard();
  
  // Обновляем джойстик → клавиши (если включен и не в меню/браузере)
  updateJoystickKeys();
  
  // ЕСЛИ МЕНЮ ИЛИ БРАУЗЕР ОТКРЫТЫ (ПАУЗА) - НЕ ЗАПУСКАЕМ ЭМУЛЯЦИЮ!
  if (emulatorPaused || showBrowser || showInformation) {
    // V3.134: Если игра на паузе (gamePaused) - РИСУЕМ экран с плашкой!
    if (gamePaused) {
      renderCounter++;
      if (renderCounter >= 5) {
        renderScreen();  // ✅ Рисуем экран + плашку "PAUSE"
        renderCounter = 0;
      }
      delay(50);
      return;
    }
    
    // Меню/Браузер/Information - просто ждём
    delay(50);  // Экономим CPU
    return;
  }
  
  // Запускаем эмуляцию одного кадра (69888 tstates)
  // runForFrame() заполняет accumBuffer (312 значений 0-224)
  int cycles = spectrum->runForFrame(accumBuffer);
  
  // ✅ V3.134: Отправляем данные в Audio Task (ChatGPT!)
  ZX_BeeperSubmitFrame(accumBuffer);
  
  frameCount++;
  intCount++;
  
  // Рендерим экран каждый 5й frame (10 FPS для дисплея - безопасно)
  renderCounter++;
  if (renderCounter >= 5) {
    renderScreen();
    renderCounter = 0;
  }

  // Throttling для 50 FPS (20000 микросекунд = 20ms = 50 FPS)
  unsigned long frameTime = micros() - frameStart;
  if (frameTime < 20000) {
    delayMicroseconds(20000 - frameTime);
  }

  // Телеметрия каждую секунду (используем mid-frame snapshot!)
  unsigned long currentTime = millis();
  if (currentTime - lastStatsTime >= 1000) {
    float fps = frameCount / ((currentTime - lastStatsTime) / 1000.0);
    float intRate = intCount / ((currentTime - lastStatsTime) / 1000.0);
    
    Serial.printf("FPS: %.2f | INT: %.2f/s | PC: 0x%04X | SP: 0x%04X | IM: %d | IFF1: %d | Heap: %d\n",
                  fps, intRate, 
                  spectrum->getHudPC(),
                  spectrum->getHudSP(),
                  spectrum->getHudIM(),
                  spectrum->getHudIFF1(),
                  ESP.getFreeHeap());
    
    
    // Проверяем критерии (только warning для INT rate, IM=0 нормально в начале)
    if (intRate < 45 || intRate > 55) {
      Serial.println("⚠️  WARNING: INT rate not ~50/s!");
    }

    frameCount = 0;
    intCount = 0;
    lastStatsTime = currentTime;
  }
}
