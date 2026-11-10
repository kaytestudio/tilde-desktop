cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
project(zterminal C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# macOS specific settings
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)
endif()

# Header files
set(HEADERS
    src/terminal.h
    src/pty.h
    src/tabs.h
    src/mouse.h
    src/clipboard.h
    src/config.h
    src/scrollback.h
    src/search.h
    src/themes.h
    src/session.h
    src/serial.h
)

# C source files
set(SOURCES
    src/main.c
    src/terminal.c
    src/pty.c
    src/tabs.c
    src/mouse.c
    src/config.c
    src/scrollback.c
    src/search.c
    src/themes.c
    src/session.c
    src/serial.c
)

# ---------------------------------------------------------------------------
# clipboard.m: compiled manually via clang -x objective-c because CMake's
# ObjC language support is broken on this toolchain.  We produce a .o and
# link it directly into the executable.
# ---------------------------------------------------------------------------
set(CLIPBOARD_OBJ "${CMAKE_BINARY_DIR}/clipboard.o")

add_custom_command(
    OUTPUT   ${CLIPBOARD_OBJ}
    COMMAND  clang -x objective-c -c
                -I${CMAKE_SOURCE_DIR}/src
                -arch arm64
                -o ${CLIPBOARD_OBJ}
                ${CMAKE_SOURCE_DIR}/src/clipboard.m
    DEPENDS  ${CMAKE_SOURCE_DIR}/src/clipboard.m
             ${CMAKE_SOURCE_DIR}/src/clipboard.h
    COMMENT  "Compiling clipboard.m (Objective-C)"
)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(zterminal ${SOURCES} ${HEADERS} ${CLIPBOARD_OBJ}
    README.md)

target_include_directories(zterminal PRIVATE src)

if(APPLE)
    target_link_libraries(zterminal
        ${COCOA_LIBRARY}
        ${CORE_FOUNDATION}
        ${APPKIT_LIBRARY}
    )
endif()

# Strict warnings + explicit C11 on all C sources
target_compile_options(zterminal PRIVATE -std=c11 -Wall -Wextra -pedantic)
