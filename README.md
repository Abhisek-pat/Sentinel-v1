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

pip install fastapi uvicorn openai pydantic
uvicorn app:app --port 8000

Uvicorn running on http://127.0.0.1:8000
Application startup complete

http://127.0.0.1:8000/docs