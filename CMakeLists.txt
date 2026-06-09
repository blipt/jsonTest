cmake_minimum_required(VERSION 3.24)

project(json_stream_viewer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(json_stream_viewer
    "main.cpp"
    "JsonBlockPager.cpp"
)

target_compile_features(json_stream_viewer PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(json_stream_viewer PRIVATE /W4 /permissive-)
else()
    target_compile_options(json_stream_viewer PRIVATE -Wall -Wextra -Wpedantic)
endif()