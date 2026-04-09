cmake_minimum_required(VERSION 3.20)

project(Sentinel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)

add_executable(sentinel
    src/main.cpp
    src/app/pipeline.cpp
    src/capture/video_source.cpp
)

target_include_directories(sentinel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(sentinel PRIVATE
    ${OpenCV_LIBS}
)

if(MSVC)
    target_compile_options(sentinel PRIVATE /W4 /EHsc)
endif()