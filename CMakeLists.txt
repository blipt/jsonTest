cmake_minimum_required(VERSION 3.16)

project(json_stream_viewer VERSION 1.0.0 LANGUAGES CXX)

add_executable(json_stream_viewer
    src/main.cpp
    src/JsonBlockPager.cpp
)

target_include_directories(json_stream_viewer PRIVATE include)
target_compile_features(json_stream_viewer PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(json_stream_viewer PRIVATE /W4 /permissive-)
else()
    target_compile_options(json_stream_viewer PRIVATE -Wall -Wextra -Wpedantic)
endif()
