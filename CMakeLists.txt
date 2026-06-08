cmake_minimum_required(VERSION 3.24)

if(CMAKE_HOST_WIN32)
    set(TOOLKITS_DIR "win")
elseif(CMAKE_HOST_UNIX AND NOT CMAKE_HOST_APPLE)
    set(TOOLKITS_DIR "lin")
endif()

if(DEFINED TOOLKITS_DIR)
    set(TOOLKIT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/obj/${TOOLKITS_DIR}/toolkits")
    message(STATUS ">>> TOOLKIT_DIR [${TOOLKIT_DIR}]")
    set(VCPKG_ROOT_DIR "${TOOLKIT_DIR}/vcpkg")
    if(CMAKE_HOST_WIN32)
        set(VCPKG_EXECUTABLE "${VCPKG_ROOT_DIR}/vcpkg.exe")
        set(VCPKG_BOOTSTRAP  "${VCPKG_ROOT_DIR}/bootstrap-vcpkg.bat")
        set(VCPKG_TARGET_TRIPLET "x64-windows")
    else()
        set(VCPKG_EXECUTABLE "${VCPKG_ROOT_DIR}/vcpkg")
        set(VCPKG_BOOTSTRAP  "${VCPKG_ROOT_DIR}/bootstrap-vcpkg.sh")
        set(VCPKG_TARGET_TRIPLET "x64-linux")
    endif()
    # Clone vcpkg if it is not already present.
    if(NOT EXISTS "${VCPKG_ROOT_DIR}/bootstrap-vcpkg.sh" AND NOT EXISTS "${VCPKG_ROOT_DIR}/bootstrap-vcpkg.bat")
        message(STATUS ">>> Cloning vcpkg into [${VCPKG_ROOT_DIR}]")
        execute_process(
            COMMAND git clone "https://github.com/microsoft/vcpkg.git" "${VCPKG_ROOT_DIR}"
            RESULT_VARIABLE _vcpkg_clone_result
        )
        if(NOT _vcpkg_clone_result EQUAL 0)
            message(FATAL_ERROR ">>> Failed to clone vcpkg [${_vcpkg_clone_result}]")
        endif()
    endif()
    # Bootstrap vcpkg if the executable is missing.
    if(NOT EXISTS "${VCPKG_EXECUTABLE}")
        message(STATUS ">>> Bootstrapping vcpkg [${VCPKG_BOOTSTRAP}]")
        execute_process(
            COMMAND "${VCPKG_BOOTSTRAP}" -disableMetrics
            WORKING_DIRECTORY "${VCPKG_ROOT_DIR}"
            RESULT_VARIABLE _vcpkg_bs_result
        )
        if(NOT _vcpkg_bs_result EQUAL 0)
            message(FATAL_ERROR ">>> vcpkg bootstrap failed [${_vcpkg_bs_result}]")
        endif()
    endif()
    
    execute_process(COMMAND ${VCPKG_EXECUTABLE} install 
        --vcpkg-root ${VCPKG_ROOT_DIR}
        pdcurses:${VCPKG_TARGET_TRIPLET}
        RESULT_VARIABLE RESULT
    ) 
    if(NOT RESULT EQUAL 0)
        message(FATAL_ERROR ">>> 'Vcpkg.exe' install failed [${RESULT}]")
    endif()
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT_DIR}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
        if(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
            message(FATAL_ERROR ">>> CMAKE_TOOLCHAIN_FILE not found at [${CMAKE_TOOLCHAIN_FILE}]")
        endif()
        message(STATUS ">>> CMAKE_TOOLCHAIN_FILE [${CMAKE_TOOLCHAIN_FILE}]")
    endif()
endif()

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
    find_package(unofficial-pdcurses CONFIG REQUIRED)
    target_link_libraries(json_stream_viewer PRIVATE unofficial::pdcurses::pdcurses)
    target_compile_definitions(json_stream_viewer PRIVATE HAS_CURSES)
    target_compile_options(json_stream_viewer PRIVATE /W4 /permissive-)
else()
    find_package(Curses REQUIRED)
    if(CURSES_FOUND)
        target_include_directories(json_stream_viewer PRIVATE ${CURSES_INCLUDE_DIR})
        target_link_libraries(json_stream_viewer PRIVATE ${CURSES_LIBRARIES})
        target_compile_definitions(json_stream_viewer PRIVATE HAS_CURSES)
    endif()
    target_compile_options(json_stream_viewer PRIVATE -Wall -Wextra -Wpedantic)
endif()