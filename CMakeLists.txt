cmake_minimum_required(VERSION 3.21)  # notcurses requires 3.21

project(vitals VERSION 0.8.8 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# notcurses is linked statically and never installed; skip its C++ bindings, multimedia, tests, demos and docs; zlib instead of libdeflate
set(USE_PANDOC        OFF  CACHE BOOL   "" FORCE)
set(USE_CXX           OFF  CACHE BOOL   "" FORCE)
set(USE_MULTIMEDIA    none CACHE STRING "" FORCE)
set(USE_DEFLATE       OFF  CACHE BOOL   "" FORCE)
set(USE_DOCTEST       OFF  CACHE BOOL   "" FORCE)
set(USE_STATIC        ON   CACHE BOOL   "" FORCE)
set(USE_POC           OFF  CACHE BOOL   "" FORCE)
set(BUILD_EXECUTABLES OFF  CACHE BOOL   "" FORCE)
set(BUILD_FFI_LIBRARY OFF  CACHE BOOL   "" FORCE)
add_subdirectory(notcurses EXCLUDE_FROM_ALL)

set(SOURCES
    main.cpp
    src/procfs.cpp
    src/sysinfo.cpp
    src/draw.cpp
    src/draw_tty.cpp
    src/format.cpp
    src/theme.cpp
    src/config.cpp
    src/paths.cpp
    src/tty.cpp
    src/gpu.cpp
    src/panel_cpu.cpp
    src/panel_memory.cpp
    src/panel_network.cpp
    src/panel_storage.cpp
    src/panel_thermal.cpp
    src/panel_gpu.cpp
    src/panel_settings.cpp
    src/layout.cpp
    src/render.cpp
    src/selftest.cpp
    src/selftest_checks.cpp
    src/selftest_fixtures.cpp
)

add_executable(vitals ${SOURCES})

target_include_directories(vitals PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/notcurses/include
)

target_compile_definitions(vitals PRIVATE
    VITALS_VERSION="${PROJECT_VERSION}"
    VITALS_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/vitals"
)

# notcurses v3.0.17 doesn't pass zlib on to its static library
find_package(ZLIB REQUIRED)
target_link_libraries(notcurses-core-static PRIVATE ZLIB::ZLIB)

target_link_libraries(vitals PRIVATE
    notcurses-static
)

# GCC < 9 keeps std::filesystem in libstdc++fs
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
    target_link_libraries(vitals PRIVATE stdc++fs)
endif()

target_compile_options(vitals PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

install(TARGETS vitals
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET vitals POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/themes $<TARGET_FILE_DIR:vitals>/themes
)

install(DIRECTORY themes/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/vitals/themes
    FILES_MATCHING PATTERN "*.theme"
)
