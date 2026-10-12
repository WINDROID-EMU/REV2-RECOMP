# Resident Evil: Revelations 2 — Static Recompilation (REV2-RECOMP)

[![Platform: Linux x86_64](https://img.shields.io/badge/Platform-Linux%20x86__64-blue.svg)](https://kernel.org)
[![C++23](https://img.shields.io/badge/Language-C%2B%2B23-orange.svg)](https://en.cppreference.com/w/cpp/23)
[![Recompiler: ReXGlue](https://img.shields.io/badge/Recompiler-ReXGlue%20v0.10.0-green.svg)](https://github.com)
[![Reverse Engineering: Ghidra](https://img.shields.io/badge/Analysis-Ghidra%2012.1.4-red.svg)](https://ghidra-sre.org/)

End-to-end static recompilation of the complete Xbox 360 retail release of **Resident Evil: Revelations 2** (*Biohazard Revelations 2*, Title ID: `5841144C`, Media ID: `639D55D4`, Version `0.0.0.3`) to native Linux 64-bit x86_64 code.

---

## 🚀 Key Highlights & Technical Metrics

- **4,417,487 PowerPC Instructions Analyzed**: Extracted and cataloged across 99,085 functions via Ghidra 12.1.4.
- **14,063,986 Lines of C++23 Code**: 71 generated source and header files (`generated/default/`) encompassing the complete MT Framework engine logic.
- **149,258 Functions Registered**: 100% control-flow graph coverage across 282,208 direct calls, 84,649 branches, and 799 switch tables.
- **Dynamic PPC Thunk Dispatcher**: Interposition of runtime indirect interface thunks (`bctr`, `lwz`, `subfic`, `subfe`, `addi`, `rlwinm`, etc.) in `src/ppc_thunk_dispatcher.h` for seamless virtual method dispatch with zero translation lag.
- **MT Framework Parallel Engine**: Multithreaded execution across 19 concurrent threads (Main XThread, Audio Worker, XMA Decoder, Kernel APC Dispatcher, and 12 engine worker threads).
- **Direct Native Execution**: Produces an optimized Linux ELF binary without runtime emulation overhead.

---

## 📁 Repository Structure

```
REV2-RECOMP/
├── CMakeLists.txt              # CMake build configuration
├── CMakePresets.json           # Compiler presets
├── RERev2_manifest.toml        # ReXGlue project manifest
├── default_functions.toml      # Tail-call target declarations
├── src/
│   ├── main.cpp                # App entrypoint and ELF symbol interposition
│   ├── RERev2_app.h            # ReXApp runtime configuration
│   └── ppc_thunk_dispatcher.h  # Dynamic PowerPC thunk interpreter & dispatcher
├── generated/                  # Recompiled C++23 code (~398 MB)
│   ├── default/
│   │   ├── RERev2_pch.h        # Precompiled header and memory macros
│   │   ├── RERev2_register.cpp # Function registration table (149k functions)
│   │   ├── RERev2_recomp.*.cpp # 33 split translation units (14M lines)
│   │   └── ...
│   └── rexglue.cmake           # ReXGlue CMake helper
├── docs/
│   └── recompilation_report.md # Comprehensive technical reverse-engineering report
└── .gitignore
```

---

## 🛠️ Building the Native Executable

### Prerequisites
- **OS**: Linux x86_64 (Ubuntu 22.04+, Arch, Fedora, Debian 12+)
- **Compiler**: Clang 18+ or GCC 14+ with full C++23 support
- **Build System**: CMake 3.25+ and Ninja (or GNU Make)
- **Dependencies**: SDL2, Vulkan / X11 dev packages
- **ReXGlue SDK**: ReXGlue SDK `v0.10.0` or later

### Build Steps

```bash
# Clone the repository
git clone https://github.com/WINDROID-EMU/REV2-RECOMP.git
cd REV2-RECOMP

# Configure the build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++

# Build the binary
cmake --build build --target RERev2 -j$(nproc)
```

The compiled binary will be placed at `build/RERev2` (approx. 445 MB ELF executable).

---

## 🎮 Running the Game

Resident Evil: Revelations 2 requires original retail game assets (`Game.arc`, `Core.arc`, movies, shaders, audio) extracted from the Xbox 360 release (STFS package `5841144C`).

### Quick Launch (Using Included Runtime)
The repository bundles the complete native ReXGlue Linux x86_64 runtime libraries in `runtime/linux-amd64/`. You can launch the game directly using the provided launcher:

```bash
# Executar diretamente usando o script launcher
./run.sh --game_data_root "/path/to/extracted/game_assets"
```

### Manual Execution
```bash
./build/RERev2 --game_data_root "/path/to/extracted/game_assets"
```
*(The binary automatically resolves runtime libraries from its directory via `$ORIGIN` or from `runtime/linux-amd64`).*

---

## 📊 Technical Analysis

For detailed analysis on section layouts, Ghidra control-flow metrics, and PPC instruction mapping, refer to:
- [`docs/recompilation_report.md`](docs/recompilation_report.md)

---

## ⚖️ Legal & Disclaimer

This project is a static recompilation research and preservation effort.
No copyrighted game assets, audio files, textures, models, or proprietary game media are distributed within this repository.
Users must supply their own legally acquired copy of the game assets to run the recompiled binary.
Resident Evil: Revelations 2 is a trademark of CAPCOM CO., LTD.
