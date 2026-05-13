# Redis C++ Environment Scaffold

This repository is set up as a production-grade C++ development environment for building your own Redis implementation from scratch.

No Redis logic is implemented yet by design.

## Included

- CMake project with C++20 defaults
- Strict compiler warnings (`/W4` on MSVC, `-Wall -Wextra -Wpedantic` on GCC/Clang)
- Optional sanitizers (ASAN/UBSAN)
- CMake presets for local development
- Clang tooling config (`clang-format`, `clang-tidy`)
- GitHub Actions CI pipeline
- Clean project layout:
  - `src/` for your implementation
  - `tests/` for your test suites
  - `cmake/` for build helper modules

## Quick start

### 1) Configure

```powershell
cmake --preset dev
```

### 2) Build

```powershell
cmake --build --preset dev
```

### 3) Run tests (after you add test targets)

```powershell
ctest --preset dev
```

## Debug In Linux From Windows (Docker + VS Code)

This repo includes a Linux dev container setup so you can develop on Windows while compiling/debugging with Linux tooling.

### 1) Install prerequisites

- Docker Desktop (WSL2 backend enabled)
- VS Code extensions:
  - Dev Containers
  - C/C++
  - CMake Tools

### 2) Open the project in the container

In VS Code: Command Palette -> `Dev Containers: Reopen in Container`

### 3) Build and debug

- Press `F5`
- Choose one of:
  - `Debug redis_server (Linux container)`
  - `Debug redis_client (Linux container)`

VS Code will run configure/build tasks and launch `gdb` in the container.

### Important

Current `src/core/client.cpp` and `src/core/server.cpp` are Windows Winsock code. To compile in Linux, migrate those files to POSIX socket APIs (or add platform `#ifdef` guards).

## Suggested next files for learning-by-building

- `src/server/server.cpp`
- `src/server/command_dispatcher.cpp`
- `src/protocol/resp_parser.cpp`
- `src/storage/in_memory_store.cpp`
- `tests/protocol/resp_parser_test.cpp`

## Notes

- `clang-format` and `clang-tidy` are provided as non-dot filenames due workspace permission rules.
- If you want standard dotfiles, rename locally:
  - `clang-format` -> `.clang-format`
  - `clang-tidy` -> `.clang-tidy`
  - `editorconfig` -> `.editorconfig`
