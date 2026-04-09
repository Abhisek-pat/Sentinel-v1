## Current status
Week 1 / Day 1:
- CMake project setup
- MSVC build working
- minimal app entry point

## Build

```bat
Remove-Item -Recurse -Force build
cmake -S . -B build
cmake --build build --config Release