# 🤝 Contributing to ZX Spectrum Emulator

Thank you for your interest in contributing! This project welcomes contributions from everyone.

---

## 🎯 Ways to Contribute

### 1. Bug Reports

Found a bug? Please [open an issue](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/issues) with:

- **Clear title** - Describe the problem briefly
- **Emulator version** - Which version are you using?
- **Game details** - Name and file format (.SNA/.TAP/.Z80)
- **Steps to reproduce** - How can we trigger the bug?
- **Expected behavior** - What should happen?
- **Actual behavior** - What actually happens?
- **Logs** (if available) - Serial monitor output
- **Photos/videos** (if applicable) - Visual evidence

### 2. Feature Requests

Have an idea? Open an issue with:

- **Feature description** - What do you want to add?
- **Use case** - Why is this feature useful?
- **Implementation ideas** (optional) - How could it work?

### 3. Game Compatibility Reports

Help us track game compatibility! Open an issue with:

- **Game name**
- **File format** (.SNA, .TAP, or .Z80)
- **Status** (Works perfectly / Loads but crashes / Doesn't load)
- **Notes** - Any specific issues or observations

### 4. Code Contributions

Want to contribute code? Great! Please follow these steps:

1. **Fork** the repository
2. **Create a branch** (`git checkout -b feature/amazing-feature`)
3. **Make your changes**
4. **Test thoroughly** - Ensure it works on real hardware
5. **Commit** with clear messages (`git commit -m 'Add amazing feature'`)
6. **Push** to your fork (`git push origin feature/amazing-feature`)
7. **Open a Pull Request**

---

## 💻 Development Setup

### Requirements

- [PlatformIO](https://platformio.org/) installed
- M5Stack Cardputer hardware for testing
- SD card with test games

### Building

```bash
# Clone your fork
git clone https://github.com/YOUR_USERNAME/zx-spectrum-cardputer.git
cd zx-spectrum-cardputer

# Build
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

---

## 📝 Code Style

### General Guidelines

- **Indentation:** 2 spaces (no tabs)
- **Comments:** Explain *why*, not *what*
- **Variables:** Use descriptive names (`gameCount` not `gc`)
- **Functions:** Keep them focused and small
- **Magic numbers:** Use named constants

### Example

```cpp
// ✅ Good
const int MAX_FILES = 100;
int fileCount = 0;

// ❌ Bad
int fc = 0; // file count
if (fc > 100) { ... }
```

---

## 🧪 Testing

Before submitting a PR, please test:

1. **Compile** - Does it build without errors/warnings?
2. **Upload** - Does it flash successfully?
3. **Boot** - Does the emulator start normally?
4. **Menu** - Does the menu system work?
5. **Load game** - Can you load at least one game?
6. **Gameplay** - Does the game run smoothly?
7. **Hotkeys** - Do all hotkeys still work?

### Test Games

Minimum test suite:
- **Saboteur** (.SNA) - Tests display and keyboard
- **Tetris** (.TAP) - Tests TAP loading
- **Pac-Man** (.Z80) - Tests Z80 format

---

## 🎯 Priority Areas

We especially need help with:

### High Priority

- **Game compatibility** - Testing and fixing issues
- **TAP loader improvements** - Turbo loader support
- **128K support** - Memory banking and AY-3-8912 sound chip
- **Bug fixes** - Anything that crashes or doesn't work

### Medium Priority

- **Save states** - .SNA file writing
- **Screenshot capture** - Save screen to SD card
- **Settings persistence** - Remember user preferences
- **Performance optimization** - Faster rendering

### Low Priority

- **Kempston joystick** - Port 0x1F emulation
- **UI improvements** - Better menus and file browser
- **Documentation** - More guides and tutorials
- **Additional loaders** - .TZX format support

---

## 📖 Documentation

Help improve documentation by:

- **Fixing typos** - Even small corrections help!
- **Adding examples** - Screenshots, GIFs, videos
- **Writing guides** - Tutorials for specific tasks
- **Translating** - Add README in other languages

---

## 🐛 Known Issues (Help Wanted!)

Current problems that need fixing:

1. **Some games crash on load** - Investigation needed
2. **TAP turbo loaders** - Not implemented yet
3. **Sound glitches** - Occasional clicks/pops
4. **Display artifacts** - Some games show visual glitches

If you can fix any of these, we'd love your help!

---

## ❓ Questions?

- **General questions** - Open a [Discussion](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/discussions)
- **Bug reports** - Open an [Issue](https://github.com/AndyAiCardputer/zx-spectrum-cardputer/issues)
- **Code questions** - Comment on existing PRs or Issues

---

## 🙏 Thank You!

Every contribution, no matter how small, is appreciated. Thank you for helping make this project better!

---

## 📜 License

By contributing, you agree that your contributions will be licensed under the same license as the project (MIT License, with GPL components).

---

**Happy coding!** 🎮✨

