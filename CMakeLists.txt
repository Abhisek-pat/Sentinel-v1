cmake_minimum_required(VERSION 3.20)

project(Sentinel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)

set(ONNXRUNTIME_DIR $ENV{ONNXRUNTIME_DIR})
if(NOT ONNXRUNTIME_DIR)
    message(FATAL_ERROR "ONNXRUNTIME_DIR environment variable is not set.")
endif()

set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_DIR}/include")
set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_DIR}/lib")
set(ONNXRUNTIME_LIB "${ONNXRUNTIME_LIB_DIR}/onnxruntime.lib")

add_executable(sentinel
    src/main.cpp
    src/app/pipeline.cpp

    src/capture/video_source.cpp

    src/detection/detector.cpp
    src/detection/yolo_onnx.cpp

    src/tracking/tracker.cpp
    src/events/event_engine.cpp
    src/zones/zone_manager.cpp

    src/reasoning/scene_state.cpp
    src/reasoning/llm_client.cpp

    src/ui/overlay_renderer.cpp
    src/utils/timer.cpp
)

target_include_directories(sentinel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(sentinel PRIVATE
    ${OpenCV_LIBS}
    ${ONNXRUNTIME_LIB}
    winhttp
)

if(MSVC)
    target_compile_options(sentinel PRIVATE /W4 /EHsc)
endif()