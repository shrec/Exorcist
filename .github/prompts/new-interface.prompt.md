---
description: "Create a new core interface abstraction (IFileSystem, IProcess style) with Qt implementation"
agent: "developer"
argument-hint: "Interface name and purpose, e.g. 'ISettings — persistent key-value configuration'"
---
Create a new core interface following the Exorcist pattern:

1. Create the interface header in `src/core/` with:
   - `#pragma once`
   - Pure virtual destructor
   - `QString *error` output parameters for fallible operations
   - PascalCase class name with `I` prefix

2. Create the Qt implementation in `src/core/` as `qt<name>.h` and `qt<name>.cpp`

3. Add both files to `src/CMakeLists.txt`

4. Follow the existing patterns from `IFileSystem`/`QtFileSystem` and `IProcess`/`QtProcess`

Reference files:
- [IFileSystem interface](src/core/ifilesystem.h)
- [QtFileSystem implementation](src/core/qtfilesystem.cpp)
- [CMakeLists.txt](src/CMakeLists.txt)
