include(FetchContent)
set(FETCHCONTENT_QUIET ON)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE)

FetchContent_Declare(rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG 24b5e7a8b27f42fa16b96fc70aade9106cf7102f
    SOURCE_SUBDIR BrightNoBuild)

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
    SOURCE_SUBDIR BrightNoBuild)

FetchContent_Declare(ufbx
    GIT_REPOSITORY https://github.com/ufbx/ufbx.git
    GIT_TAG v0.23.1
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR BrightNoBuild)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9-docking
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR BrightNoBuild)

FetchContent_Declare(imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG 105e4fde45e4e74aae8973d98750820ef7aca7e2
    SOURCE_SUBDIR BrightNoBuild)

FetchContent_Declare(pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG v1.15
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR BrightNoBuild)

FetchContent_Declare(yoga
    GIT_REPOSITORY https://github.com/facebook/yoga.git
    GIT_TAG v3.2.1
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR BrightNoBuild)

FetchContent_Declare(freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG VER-2-14-3
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR BrightNoBuild)

FetchContent_Declare(msdfatlasgen
    GIT_REPOSITORY https://github.com/Chlumsky/msdf-atlas-gen.git
    GIT_TAG v1.4
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR BrightNoBuild)

FetchContent_Declare(jolt
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.6.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR BrightNoBuild)

FetchContent_Declare(smaa
    GIT_REPOSITORY https://github.com/iryoku/smaa.git
    GIT_TAG 71c806a838bdd7d517df19192a20f0c61b3ca29d
    SOURCE_SUBDIR BrightNoBuild)

FetchContent_Declare(ninja
    GIT_REPOSITORY https://github.com/ninja-build/ninja.git
    GIT_TAG v1.12.1
    GIT_SHALLOW TRUE)

FetchContent_Declare(lucide
    URL https://registry.npmjs.org/lucide-static/-/lucide-static-1.48.0.tgz
    URL_HASH SHA256=3c2ecda3d25f6a9692d83f8036d9a526f7da584a51af74cd16eda4498c5c33d8
    SOURCE_SUBDIR BrightNoBuild)

FetchContent_MakeAvailable(glfw rapidjson stb ufbx imgui imguizmo pugixml yoga freetype msdfatlasgen jolt smaa lucide)

file(COPY ${smaa_SOURCE_DIR}/SMAA.hlsl ${smaa_SOURCE_DIR}/LICENSE.txt DESTINATION ${CMAKE_SOURCE_DIR}/Engine/Content/Shaders/ThirdParty/SMAA)
file(COPY ${lucide_SOURCE_DIR}/font/lucide.ttf ${lucide_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_SOURCE_DIR}/Editor/Content/Fonts/ThirdParty/Lucide)
add_library(BrightSmaaTextures INTERFACE)
target_include_directories(BrightSmaaTextures SYSTEM INTERFACE ${smaa_SOURCE_DIR}/Textures)

add_library(BrightImGui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp)
target_include_directories(BrightImGui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/misc/cpp ${imguizmo_SOURCE_DIR}/src)
set_target_properties(BrightImGui PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(BrightImGui PRIVATE -w)
endif()

add_library(BrightStb INTERFACE)
target_include_directories(BrightStb SYSTEM INTERFACE ${stb_SOURCE_DIR})

add_library(BrightUfbx STATIC ${ufbx_SOURCE_DIR}/ufbx.c)
target_include_directories(BrightUfbx SYSTEM PUBLIC ${ufbx_SOURCE_DIR})
set_target_properties(BrightUfbx PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(BrightUfbx PRIVATE -w)
endif()

if(APPLE)
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
    set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
    set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
    set(ENABLE_HLSL ON CACHE BOOL "" FORCE)
    set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
    set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_SHARED OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
        GIT_TAG vulkan-sdk-1.4.363.0
        GIT_SHALLOW TRUE)

    FetchContent_Declare(spirv_cross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG vulkan-sdk-1.4.363.0
        GIT_SHALLOW TRUE)

    FetchContent_MakeAvailable(glslang spirv_cross)
endif()

add_library(BrightRapidJson INTERFACE)
target_include_directories(BrightRapidJson SYSTEM INTERFACE ${rapidjson_SOURCE_DIR}/include)
target_compile_definitions(BrightRapidJson INTERFACE RAPIDJSON_HAS_STDSTRING=1 RAPIDJSON_NOMEMBERITERATORCLASS)

if(BRIGHT_BUILD_TESTS)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ninja)

add_library(BrightPugiXml STATIC ${pugixml_SOURCE_DIR}/src/pugixml.cpp)
target_include_directories(BrightPugiXml SYSTEM PUBLIC ${pugixml_SOURCE_DIR}/src)
set_target_properties(BrightPugiXml PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE BRIGHT_YOGA_SOURCES ${yoga_SOURCE_DIR}/yoga/*.cpp)
add_library(BrightYoga STATIC ${BRIGHT_YOGA_SOURCES})
target_include_directories(BrightYoga SYSTEM PUBLIC ${yoga_SOURCE_DIR})
set_target_properties(BrightYoga PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(BrightYoga PRIVATE -w)
endif()

set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
add_subdirectory(${freetype_SOURCE_DIR} ${CMAKE_BINARY_DIR}/_deps/freetype-cmake EXCLUDE_FROM_ALL)
set_target_properties(freetype PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(freetype PRIVATE -w)
endif()

set(BRIGHT_MSDF_DIR ${msdfatlasgen_SOURCE_DIR})
file(GLOB BRIGHT_MSDFGEN_SOURCES ${BRIGHT_MSDF_DIR}/msdfgen/core/*.cpp)
file(GLOB BRIGHT_MSDF_ATLAS_SOURCES ${BRIGHT_MSDF_DIR}/msdf-atlas-gen/*.cpp)
list(FILTER BRIGHT_MSDF_ATLAS_SOURCES EXCLUDE REGEX ".*(main|artery-font-export|image-encode|shadron-preview-generator|json-export|csv-export)\\.cpp$")
add_library(BrightMsdf STATIC ${BRIGHT_MSDFGEN_SOURCES} ${BRIGHT_MSDF_DIR}/msdfgen/ext/import-font.cpp ${BRIGHT_MSDF_ATLAS_SOURCES})
target_include_directories(BrightMsdf SYSTEM PUBLIC ${BRIGHT_MSDF_DIR} ${BRIGHT_MSDF_DIR}/msdfgen)
target_compile_definitions(BrightMsdf PUBLIC MSDFGEN_PUBLIC= MSDF_ATLAS_PUBLIC= MSDFGEN_DISABLE_PNG MSDF_ATLAS_NO_ARTERY_FONT MSDFGEN_USE_CPP11 MSDF_ATLAS_NO_PNG)
target_link_libraries(BrightMsdf PUBLIC freetype)
set_target_properties(BrightMsdf PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(BrightMsdf PRIVATE -w)
endif()

set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(${jolt_SOURCE_DIR}/Build ${CMAKE_BINARY_DIR}/_deps/jolt-cmake EXCLUDE_FROM_ALL)
set_target_properties(Jolt PROPERTIES POSITION_INDEPENDENT_CODE ON)
