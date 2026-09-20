cmake_minimum_required(VERSION 3.21)  # notcurses requires 3.21

project(vitals LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vitals only uses the notcurses C core API: skip the C++ bindings, multimedia, tests,
# demos and docs so the build needs no extra dependencies (or a C++23 compiler for
# notcurses itself), and use zlib instead of libdeflate.
set(USE_PANDOC        OFF  CACHE BOOL   "" FORCE)
set(USE_CXX           OFF  CACHE BOOL   "" FORCE)
set(USE_MULTIMEDIA    none CACHE STRING "" FORCE)
set(USE_DEFLATE       OFF  CACHE BOOL   "" FORCE)
set(USE_DOCTEST       OFF  CACHE BOOL   "" FORCE)
set(USE_STATIC        OFF  CACHE BOOL   "" FORCE)
set(USE_POC           OFF  CACHE BOOL   "" FORCE)
set(BUILD_EXECUTABLES OFF  CACHE BOOL   "" FORCE)
set(BUILD_FFI_LIBRARY OFF  CACHE BOOL   "" FORCE)
add_subdirectory(notcurses)

set(SOURCES
    main.cpp
    src/procfs.cpp
    src/sysinfo.cpp
    src/draw.cpp
    src/draw_tty.cpp
    src/format.cpp
    src/theme.cpp
    src/config.cpp
    src/tty.cpp
    src/gpu.cpp
    src/panel_cpu.cpp
    src/panel_memory.cpp
    src/panel_network.cpp
    src/panel_storage.cpp
    src/panel_thermal.cpp
    src/panel_gpu.cpp
    src/panel_settings.cpp
)

add_executable(vitals ${SOURCES})

target_include_directories(vitals PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/notcurses/include
)

target_link_libraries(vitals PRIVATE
    notcurses
)

target_compile_options(vitals PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

install(TARGETS vitals
    RUNTIME DESTINATION bin
)
