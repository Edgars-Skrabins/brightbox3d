file(GLOB_RECURSE BRIGHT_ENGINE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h)

file(GLOB_RECURSE BRIGHT_ENGINE_APPLE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.mm)

if(APPLE)
    list(FILTER BRIGHT_ENGINE_SOURCES EXCLUDE REGEX ".*Windows\\.cpp$")
    list(FILTER BRIGHT_ENGINE_SOURCES EXCLUDE REGEX ".*/DX11/.*")
    list(APPEND BRIGHT_ENGINE_SOURCES ${BRIGHT_ENGINE_APPLE_SOURCES})
elseif(WIN32)
    list(FILTER BRIGHT_ENGINE_SOURCES EXCLUDE REGEX ".*Apple\\.cpp$")
    list(FILTER BRIGHT_ENGINE_SOURCES EXCLUDE REGEX ".*/Metal/.*")
endif()

add_library(BrightBox3D SHARED ${BRIGHT_ENGINE_SOURCES})
target_include_directories(BrightBox3D PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_link_libraries(BrightBox3D PRIVATE glfw)
target_link_libraries(BrightBox3D PUBLIC BrightRapidJson)
target_link_libraries(BrightBox3D PRIVATE BrightStb BrightUfbx)
bright_generate_reflection(BrightBox3D ${CMAKE_CURRENT_SOURCE_DIR}/Source)

if(APPLE)
    set_source_files_properties(${BRIGHT_ENGINE_APPLE_SOURCES} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_compile_definitions(BrightBox3D PRIVATE ENABLE_HLSL)
    target_link_libraries(BrightBox3D PRIVATE glslang SPIRV glslang-default-resource-limits spirv-cross-msl spirv-cross-glsl spirv-cross-core)
    target_link_libraries(BrightBox3D PUBLIC "-framework Cocoa" "-framework CoreServices" "-framework IOKit" "-framework QuartzCore" "-framework Metal")
elseif(WIN32)
    target_link_libraries(BrightBox3D PUBLIC d3d11 dxgi d3dcompiler)
    target_compile_definitions(BrightBox3D PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Source/Rendering/StbImplementation.cpp PROPERTIES COMPILE_OPTIONS "-w")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(BrightBox3D PRIVATE -Wall -Wextra -Wno-unused-parameter)
elseif(MSVC)
    target_compile_options(BrightBox3D PRIVATE /W4 /wd4100 /utf-8)
endif()

set(BRIGHT_SDK_CONTENT "set(BRIGHT_ENGINE_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\")
set(BRIGHT_ENGINE_BINARY_DIR \"${BRIGHT_BIN_DIR}\")
set(BRIGHT_ENGINE_LIBRARY \"$<TARGET_FILE:BrightBox3D>\")
set(BRIGHT_ENGINE_LINKER_FILE \"$<TARGET_LINKER_FILE:BrightBox3D>\")
set(BRIGHT_HEADER_TOOL \"$<TARGET_FILE:BrightHeaderTool>\")
set(BRIGHT_RAPIDJSON_INCLUDE \"${rapidjson_SOURCE_DIR}/include\")
set(BRIGHT_ENGINE_BUILD_TYPE \"$<CONFIG>\")
")
file(GENERATE OUTPUT ${BRIGHT_BIN_DIR}/BrightBox3DSDK.cmake CONTENT "${BRIGHT_SDK_CONTENT}")
