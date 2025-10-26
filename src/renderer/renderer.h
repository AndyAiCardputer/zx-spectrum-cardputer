#ifndef RENDERER_H
#define RENDERER_H

#include <M5Unified.h>
#include "../spectrum/spectrum_mini.h"

// Downscale/crop 256x192 ZX screen to 240x135 Cardputer display
// Strategy: 
// - X: 256 → 240 (drop every 16th pixel, or scale)
// - Y: 192 → 135 (crop top/bottom by ~28 pixels each)

class Renderer {
private:
  M5GFX *display;
  uint16_t *frameBuffer = nullptr;  // RGB565 buffer for one frame
  int displayWidth = TFT_WIDTH;
  int displayHeight = TFT_HEIGHT;
  
  // Flash counter (for FLASH attribute)
  uint16_t flashCounter = 0;
  bool flashState = false;

public:
  Renderer(M5GFX *disp) : display(disp) {
    // Allocate frame buffer
    frameBuffer = (uint16_t *)malloc(displayWidth * displayHeight * sizeof(uint16_t));
    if (!frameBuffer) {
      Serial.println("Failed to allocate frame buffer");
    }
  }

  ~Renderer() {
    if (frameBuffer) free(frameBuffer);
  }

  // Render ZX Spectrum screen (6144 bytes bitmap + 768 bytes attributes)
  // to 240x135 display with simple crop/downscale
  void renderFrame(const uint8_t *screen, const uint8_t *borderColors) {
    // Update flash state every 16 frames (~0.32s at 50fps)
    flashCounter++;
    if (flashCounter >= 16) {
      flashCounter = 0;
      flashState = !flashState;
    }

    const uint8_t *bitmap = screen;           // 6144 bytes (256x192 pixels)
    const uint8_t *attrs = screen + 0x1800;   // 768 bytes (32x24 chars)

    // Strategy: crop top 28 lines and bottom 29 lines (192 - 135 = 57)
    // Then downscale X from 256 to 240
    int cropTop = 28;
    int cropBottom = 29;
    int visibleLines = 192 - cropTop - cropBottom; // 135 lines

    for (int y = 0; y < displayHeight; y++) {
      int srcY = y + cropTop;
      if (srcY >= 192) break;

      // Calculate ZX screen address (interleaved format)
      int third = srcY / 64;
      int line = srcY % 8;
      int chunk = (srcY % 64) / 8;
      int bitmapOffset = (third * 2048) + (line * 256) + (chunk * 32);

      // Attribute row
      int attrRow = srcY / 8;
      const uint8_t *attrLine = attrs + (attrRow * 32);

      for (int x = 0; x < displayWidth; x++) {
        // Map 240 pixels to 256 source pixels (simple: x * 256 / 240)
        int srcX = (x * 256) / 240;
        int charX = srcX / 8;
        int pixelBit = 7 - (srcX % 8);

        uint8_t bitmapByte = bitmap[bitmapOffset + charX];
        uint8_t attr = attrLine[charX];

        // Extract colors from attribute
        uint8_t ink = attr & 0x07;
        uint8_t paper = (attr >> 3) & 0x07;
        bool bright = attr & 0x40;
        bool flash = attr & 0x80;

        // Apply flash
        if (flash && flashState) {
          uint8_t temp = ink;
          ink = paper;
          paper = temp;
        }

        // Apply bright bit
        if (bright) {
          ink |= 0x08;
          paper |= 0x08;
        }

        // Get pixel value
        bool pixelOn = (bitmapByte & (1 << pixelBit)) != 0;
        uint8_t colorIndex = pixelOn ? ink : paper;

        // Convert to RGB565
        uint16_t color = specpal565[colorIndex];
        frameBuffer[y * displayWidth + x] = color;
      }
    }

    // Push to display
    display->startWrite();
    display->pushImage(0, 0, displayWidth, displayHeight, frameBuffer);
    display->endWrite();
  }

  // Draw a simple border color (optional, can be skipped for now)
  void renderBorder(uint8_t borderColor) {
    // For now, just fill with border color
    // TODO: actual border rendering if needed
  }
};

#endif // RENDERER_H

