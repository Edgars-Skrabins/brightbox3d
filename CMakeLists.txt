cmake_minimum_required(VERSION 3.24)
project(BrightBox3D LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

set(BRIGHT_BIN_DIR ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BRIGHT_BIN_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BRIGHT_BIN_DIR})
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} ${BRIGHT_BIN_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config} ${BRIGHT_BIN_DIR})
endforeach()

option(BRIGHT_BUILD_TESTS "Build BrightBox3D unit tests" ON)

include(cmake/Dependencies.cmake)
include(cmake/BrightReflection.cmake)
include(cmake/GenerateIcons.cmake)

set(BRIGHT_GENERATED_DIR ${CMAKE_BINARY_DIR}/Generated)
bright_generate_icons(${lucide_SOURCE_DIR}/font/info.json ${BRIGHT_GENERATED_DIR}/Engine/Core/Icon.h ${BRIGHT_GENERATED_DIR}/Editor/IconGlyphs.h)

add_subdirectory(Tools/BrightHeaderTool)
add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Player)

if(BRIGHT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
