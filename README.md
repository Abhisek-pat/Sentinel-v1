# Sentinel

Sentinel is a real-time person-monitoring application built with C++17, OpenCV,
ONNX Runtime, and a YOLOv8 ONNX model. It supports Windows and Raspberry Pi
Linux builds.

The CMake configuration detects the target automatically:

- Windows builds use the `windows` variant and run inference on every frame.
- ARM Linux builds use the `raspberrypi` variant and run inference every third
  frame to reduce Raspberry Pi CPU load.

The selected variant is printed during CMake configuration and when Sentinel
starts.

## Requirements

- CMake 3.20 or newer
- OpenCV development files
- ONNX Runtime development package
- `models/yolo/model_320.onnx`

Set `ONNXRUNTIME_DIR` to the ONNX Runtime installation root. The directory must
contain `include/onnxruntime_cxx_api.h` and the platform library under `lib/`.

## Windows

Open PowerShell in the repository root.

```powershell
$env:ONNXRUNTIME_DIR = "D:\path\to\onnxruntime"

cmake -S . -B build
cmake --build build --config Release
```

Ensure the OpenCV and ONNX Runtime DLL directories are on `PATH` before
starting Sentinel:

```powershell
$env:PATH = "$env:ONNXRUNTIME_DIR\lib;D:\path\to\opencv\build\x64\vc16\bin;$env:PATH"
```

Run with the default webcam:

```powershell
.\build\Release\sentinel.exe
```

Run with a specific webcam index:

```powershell
.\build\Release\sentinel.exe "1"
```

Run with an RTSP stream:

```powershell
.\build\Release\sentinel.exe "rtsp://username:password@camera-ip:554/stream"
```

## Raspberry Pi

Use a 64-bit Raspberry Pi OS installation. Install OpenCV and build tools:

```bash
sudo apt update
sudo apt install -y build-essential cmake libopencv-dev
```

Install the ARM64 ONNX Runtime package and set its location:

```bash
wget https://github.com/microsoft/onnxruntime/releases/download/v1.24.4/onnxruntime-linux-aarch64-1.24.4.tgz
tar -xzf onnxruntime-linux-aarch64-1.24.4.tgz
sudo mv onnxruntime-linux-aarch64-1.24.4 /opt/onnxruntime

export ONNXRUNTIME_DIR=/opt/onnxruntime
export LD_LIBRARY_PATH="$ONNXRUNTIME_DIR/lib:$LD_LIBRARY_PATH"
```

Build Sentinel:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Run with the default webcam:

```bash
./build/sentinel
```

Run with a specific webcam index:

```bash
./build/sentinel "1"
```

Run with an RTSP stream:

```bash
./build/sentinel "rtsp://username:password@camera-ip:554/stream"
```

## Other Video Sources

A video-file path can be passed instead of a webcam index or RTSP URL:

```text
sentinel path/to/video.mp4
```

Press `Q` or `Esc` in the Sentinel window to exit. Loitering-triggered clips
are saved under `data/clips/`.

## Notes

- Run Sentinel from the repository root so the relative model path resolves.
- Generic desktop Linux and macOS targets are currently rejected by CMake.
- The optional FastAPI/OpenAI reasoning service exists under `llm_service/`,
  but it is not connected to the current C++ runtime.
