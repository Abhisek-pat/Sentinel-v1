cmake_minimum_required(VERSION 3.20)

project(Sentinel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

string(TOLOWER "${CMAKE_SYSTEM_NAME}" SENTINEL_SYSTEM_NAME)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SENTINEL_SYSTEM_PROCESSOR)

if(WIN32)
    set(SENTINEL_VARIANT "windows")
    set(SENTINEL_INFERENCE_INTERVAL 1)
    set(SENTINEL_COMPILE_DEFINITION SENTINEL_PLATFORM_WINDOWS=1)
elseif(SENTINEL_SYSTEM_NAME STREQUAL "linux" AND
       SENTINEL_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|armv[6-8].*)$")
    set(SENTINEL_VARIANT "raspberrypi")
    set(SENTINEL_INFERENCE_INTERVAL 3)
    set(SENTINEL_COMPILE_DEFINITION SENTINEL_PLATFORM_RASPBERRY_PI=1)
else()
    message(FATAL_ERROR
        "Unsupported Sentinel target: ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}. "
        "Supported variants are Windows and Raspberry Pi Linux.")
endif()

message(STATUS "Sentinel build variant: ${SENTINEL_VARIANT}")
message(STATUS "Sentinel inference interval: every ${SENTINEL_INFERENCE_INTERVAL} frame(s)")

find_package(OpenCV REQUIRED)

set(ONNXRUNTIME_DIR "$ENV{ONNXRUNTIME_DIR}" CACHE PATH "ONNX Runtime installation root")
if(NOT ONNXRUNTIME_DIR)
    message(FATAL_ERROR
        "ONNXRUNTIME_DIR is not set. Set the environment variable or pass "
        "-DONNXRUNTIME_DIR=<path> when configuring.")
endif()

set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_DIR}/include")

if(WIN32)
    set(ONNXRUNTIME_LIB "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib")
    set(ONNXRUNTIME_DLL "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll")
    set(ONNXRUNTIME_PROVIDERS_DLL "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.dll")
else()
    set(ONNXRUNTIME_LIB "${ONNXRUNTIME_DIR}/lib/libonnxruntime.so")
endif()

if(NOT EXISTS "${ONNXRUNTIME_INCLUDE_DIR}/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "ONNX Runtime headers not found under: ${ONNXRUNTIME_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${ONNXRUNTIME_LIB}")
    message(FATAL_ERROR "ONNX Runtime library not found: ${ONNXRUNTIME_LIB}")
endif()

if(WIN32 AND NOT EXISTS "${ONNXRUNTIME_DLL}")
    message(FATAL_ERROR "ONNX Runtime DLL not found: ${ONNXRUNTIME_DLL}")
endif()

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
    src/recording/frame_ring_buffer.cpp

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
)

target_compile_definitions(sentinel PRIVATE
    ${SENTINEL_COMPILE_DEFINITION}
    SENTINEL_VARIANT="${SENTINEL_VARIANT}"
    SENTINEL_INFERENCE_INTERVAL=${SENTINEL_INFERENCE_INTERVAL}
)

if(WIN32)
    add_custom_command(TARGET sentinel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_DLL}"
            "$<TARGET_FILE_DIR:sentinel>"
        COMMENT "Copying matching ONNX Runtime DLL beside sentinel.exe"
    )

    if(EXISTS "${ONNXRUNTIME_PROVIDERS_DLL}")
        add_custom_command(TARGET sentinel POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ONNXRUNTIME_PROVIDERS_DLL}"
                "$<TARGET_FILE_DIR:sentinel>"
            COMMENT "Copying ONNX Runtime providers DLL beside sentinel.exe"
        )
    endif()
endif()

if(MSVC)
    target_compile_options(sentinel PRIVATE /W4 /EHsc)
    target_compile_definitions(sentinel PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(sentinel PRIVATE -Wall -Wextra -O3)
    find_package(Threads REQUIRED)
    target_link_libraries(sentinel PRIVATE Threads::Threads)
endif()

install(TARGETS sentinel RUNTIME DESTINATION bin)
install(FILES models/yolo/model_320.onnx DESTINATION share/sentinel/models/yolo)

include(CTest)

if(BUILD_TESTING)
    add_executable(tracker_test
        tests/tracker_test.cpp
        src/tracking/tracker.cpp
    )
    target_include_directories(tracker_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(tracker_test PRIVATE ${OpenCV_LIBS})
    add_test(NAME tracker_test COMMAND tracker_test)

    add_executable(event_engine_test
        tests/event_engine_test.cpp
        src/events/event_engine.cpp
    )
    target_include_directories(event_engine_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(event_engine_test PRIVATE ${OpenCV_LIBS})
    add_test(NAME event_engine_test COMMAND event_engine_test)

    add_executable(zone_manager_test
        tests/zone_manager_test.cpp
        src/zones/zone_manager.cpp
    )
    target_include_directories(zone_manager_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(zone_manager_test PRIVATE ${OpenCV_LIBS})
    add_test(NAME zone_manager_test COMMAND zone_manager_test)

    if(MSVC)
        target_compile_options(tracker_test PRIVATE /W4 /EHsc)
        target_compile_options(event_engine_test PRIVATE /W4 /EHsc)
        target_compile_options(zone_manager_test PRIVATE /W4 /EHsc)
    else()
        target_compile_options(tracker_test PRIVATE -Wall -Wextra)
        target_compile_options(event_engine_test PRIVATE -Wall -Wextra)
        target_compile_options(zone_manager_test PRIVATE -Wall -Wextra)
    endif()
endif()
