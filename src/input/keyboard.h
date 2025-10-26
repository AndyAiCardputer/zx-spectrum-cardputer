#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <M5Unified.h>
#include "../spectrum/keyboard_defs.h"
#include "../spectrum/spectrum_mini.h"

// Map Cardputer keyboard to ZX Spectrum keys
class Keyboard {
private:
  ZXSpectrum *spectrum;

  // Map ASCII char to SpecKeys
  SpecKeys charToSpecKey(char c) {
    switch (c) {
      // Numbers
      case '0': return SPECKEY_0;
      case '1': return SPECKEY_1;
      case '2': return SPECKEY_2;
      case '3': return SPECKEY_3;
      case '4': return SPECKEY_4;
      case '5': return SPECKEY_5;
      case '6': return SPECKEY_6;
      case '7': return SPECKEY_7;
      case '8': return SPECKEY_8;
      case '9': return SPECKEY_9;

      // Letters
      case 'a': case 'A': return SPECKEY_A;
      case 'b': case 'B': return SPECKEY_B;
      case 'c': case 'C': return SPECKEY_C;
      case 'd': case 'D': return SPECKEY_D;
      case 'e': case 'E': return SPECKEY_E;
      case 'f': case 'F': return SPECKEY_F;
      case 'g': case 'G': return SPECKEY_G;
      case 'h': case 'H': return SPECKEY_H;
      case 'i': case 'I': return SPECKEY_I;
      case 'j': case 'J': return SPECKEY_J;
      case 'k': case 'K': return SPECKEY_K;
      case 'l': case 'L': return SPECKEY_L;
      case 'm': case 'M': return SPECKEY_M;
      case 'n': case 'N': return SPECKEY_N;
      case 'o': case 'O': return SPECKEY_O;
      case 'p': case 'P': return SPECKEY_P;
      case 'q': case 'Q': return SPECKEY_Q;
      case 'r': case 'R': return SPECKEY_R;
      case 's': case 'S': return SPECKEY_S;
      case 't': case 'T': return SPECKEY_T;
      case 'u': case 'U': return SPECKEY_U;
      case 'v': case 'V': return SPECKEY_V;
      case 'w': case 'W': return SPECKEY_W;
      case 'x': case 'X': return SPECKEY_X;
      case 'y': case 'Y': return SPECKEY_Y;
      case 'z': case 'Z': return SPECKEY_Z;

      // Special
      case ' ': return SPECKEY_SPACE;
      case '\n': case '\r': return SPECKEY_ENTER;

      default: return SPECKEY_NONE;
    }
  }

public:
  Keyboard(ZXSpectrum *spec) : spectrum(spec) {}

  void poll() {
    // TODO: Implement M5Cardputer keyboard polling
    // M5Cardputer.update() requires proper M5Stack integration
    // For now, this is a stub
    
    // Simple test: press 'A' on startup for testing
    // In real implementation, we would:
    // 1. M5Cardputer.update()
    // 2. Check M5Cardputer.Keyboard.isChange()
    // 3. Get keys from M5Cardputer.Keyboard.keysState()
    // 4. Map to ZX keys
  }

  // Manual key press for testing
  void pressKey(SpecKeys key) {
    spectrum->updateKey(key, 1);
  }

  void releaseKey(SpecKeys key) {
    spectrum->updateKey(key, 0);
  }
};

#endif // KEYBOARD_H

