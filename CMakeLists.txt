cmake_minimum_required(VERSION 3.16)

project(json_stream_viewer VERSION 1.0.0 LANGUAGES C CXX)

# Find ncurses library for terminal handling
find_package(Curses REQUIRED)

add_executable(json_stream_viewer
    "main.cpp"
    "JsonBlockPager.cpp"
)

target_compile_features(json_stream_viewer PRIVATE cxx_std_20)
target_include_directories(json_stream_viewer PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(json_stream_viewer ${CURSES_LIBRARIES})
target_compile_definitions(json_stream_viewer PRIVATE HAS_CURSES)

if(MSVC)
    target_compile_options(json_stream_viewer PRIVATE /W4 /permissive-)
else()
    target_compile_options(json_stream_viewer PRIVATE -Wall -Wextra -Wpedantic)
endif()
