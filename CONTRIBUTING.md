# Contributing

Thanks for helping build Exorcist.

## Setup
- Install Qt 6 (Widgets)
- Install CMake 3.21+
- Use a C++17 compiler
- LLVM installed and discoverable in your toolchain

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Code Style
- Prefer modern C++ (C++17)
- Keep headers minimal and compile times low
- Avoid heavy dependencies unless justified
- Document non-obvious logic with short comments

## Pull Requests
- Keep changes focused
- Add tests or demos when applicable
- Update docs if behavior changes
