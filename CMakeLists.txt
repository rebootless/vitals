cmake_minimum_required(VERSION 3.14)

project(vitals LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(USE_PANDOC OFF CACHE BOOL "" FORCE)
add_subdirectory(notcurses)

set(SOURCES
    main.cpp
    src/procfs.cpp
    src/sysinfo.cpp
    src/draw.cpp
    src/format.cpp
    src/panel_cpu.cpp
    src/panel_memory.cpp
    src/panel_network.cpp
    src/panel_storage.cpp
    src/panel_thermal.cpp
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
