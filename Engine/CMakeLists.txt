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

include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

set(BRIGHT_ENGINE_SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Source)

set(BRIGHT_MODULE_LIBRARIES_Platform glfw)
set(BRIGHT_MODULE_LIBRARIES_Geometry BrightUfbx)
set(BRIGHT_MODULE_LIBRARIES_Rendering BrightStb BrightSmaaTextures)
set(BRIGHT_MODULE_LIBRARIES_Physics Jolt)
set(BRIGHT_MODULE_LIBRARIES_UI BrightPugiXml BrightYoga BrightMsdf)
if(APPLE)
    set(BRIGHT_MODULE_LIBRARIES_RHI glslang SPIRV glslang-default-resource-limits spirv-cross-msl spirv-cross-glsl spirv-cross-core)
endif()

add_custom_target(BrightModuleCheck
    COMMAND ${CMAKE_COMMAND} -DBRIGHT_ENGINE_SOURCE=${BRIGHT_ENGINE_SOURCE_ROOT} -P ${CMAKE_SOURCE_DIR}/cmake/CheckModuleDependencies.cmake
    COMMENT "Checking engine module dependencies"
    VERBATIM)

add_library(BrightBox3D SHARED)
add_dependencies(BrightBox3D BrightModuleCheck)
target_include_directories(BrightBox3D PUBLIC ${BRIGHT_ENGINE_SOURCE_ROOT} ${CMAKE_CURRENT_SOURCE_DIR}/Content ${BRIGHT_GENERATED_DIR}/Engine)
target_link_libraries(BrightBox3D PUBLIC BrightRapidJson)
if(APPLE)
    target_link_libraries(BrightBox3D PUBLIC "-framework Cocoa" "-framework CoreServices" "-framework IOKit" "-framework QuartzCore" "-framework Metal")
elseif(WIN32)
    target_link_libraries(BrightBox3D PUBLIC d3d11 dxgi d3dcompiler shell32)
    target_compile_definitions(BrightBox3D PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

foreach(module ${BRIGHT_MODULES})
    set(module_sources "")
    foreach(source ${BRIGHT_ENGINE_SOURCES})
        if(source MATCHES "^${BRIGHT_ENGINE_SOURCE_ROOT}/${module}/")
            list(APPEND module_sources ${source})
        endif()
    endforeach()
    set(module_target BrightModule${module})
    add_library(${module_target} OBJECT ${module_sources})
    target_include_directories(${module_target} PUBLIC ${BRIGHT_ENGINE_SOURCE_ROOT} ${CMAKE_CURRENT_SOURCE_DIR}/Content ${BRIGHT_GENERATED_DIR}/Engine)
    target_link_libraries(${module_target} PUBLIC BrightRapidJson ${BRIGHT_MODULE_LIBRARIES_${module}})
    target_compile_definitions(${module_target} PRIVATE BRIGHT_MODULE_NAME="BrightBox3D")
    if(APPLE)
        target_compile_definitions(${module_target} PRIVATE ENABLE_HLSL)
    elseif(WIN32)
        target_compile_definitions(${module_target} PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${module_target} PRIVATE -Wall -Wextra -Wno-unused-parameter -Werror=reorder-init-list)
    elseif(MSVC)
        target_compile_options(${module_target} PRIVATE /W4 /wd4100 /utf-8)
    endif()
    add_dependencies(${module_target} BrightModuleCheck)
    bright_generate_reflection(${module_target} ${BRIGHT_ENGINE_SOURCE_ROOT}/${module} ${BRIGHT_ENGINE_SOURCE_ROOT})
    target_link_libraries(BrightBox3D PRIVATE ${module_target})
endforeach()

if(APPLE)
    set_source_files_properties(${BRIGHT_ENGINE_APPLE_SOURCES} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Source/Rendering/StbImplementation.cpp PROPERTIES COMPILE_OPTIONS "-w")

set(BRIGHT_SDK_CONTENT "set(BRIGHT_ENGINE_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\")
set(BRIGHT_ENGINE_BINARY_DIR \"${BRIGHT_BIN_DIR}\")
set(BRIGHT_ENGINE_LIBRARY \"$<TARGET_FILE:BrightBox3D>\")
set(BRIGHT_ENGINE_LINKER_FILE \"$<TARGET_LINKER_FILE:BrightBox3D>\")
set(BRIGHT_HEADER_TOOL \"$<TARGET_FILE:BrightHeaderTool>\")
set(BRIGHT_RAPIDJSON_INCLUDE \"${rapidjson_SOURCE_DIR}/include\")
set(BRIGHT_ENGINE_GENERATED_INCLUDE \"${BRIGHT_GENERATED_DIR}/Engine\")
set(BRIGHT_ENGINE_BUILD_TYPE \"$<CONFIG>\")
")
file(GENERATE OUTPUT ${BRIGHT_BIN_DIR}/BrightBox3DSDK.cmake CONTENT "${BRIGHT_SDK_CONTENT}")
