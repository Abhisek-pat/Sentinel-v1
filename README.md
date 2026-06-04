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


cd ~
wget https://github.com/microsoft/onnxruntime/releases/download/v1.24.4/onnxruntime-linux-aarch64-1.24.4.tgz
tar -xzf onnxruntime-linux-aarch64-1.24.4.tgz
sudo mv onnxruntime-linux-aarch64-1.24.4 /opt/onnxruntime


echo 'export ONNXRUNTIME_DIR=/opt/onnxruntime' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=/opt/onnxruntime/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc

ls $ONNXRUNTIME_DIR/include/onnxruntime_cxx_api.h
ls $ONNXRUNTIME_DIR/lib/libonnxruntime.so

scp models/yolo/model.onnx <pi-user>@<pi-ip>:/home/<pi-user>/Sentinel-v1/models/yolo/model.onnx

scp models/yolo/model.onnx kunmun@192.168.1.223:/home/kunmun/Sentinel-v1/models/yolo/model.onnx

./build/sentinel "rtsp://kunmunTapoCam:LeezaSonali_07@192.168.1.225:554/stream2"