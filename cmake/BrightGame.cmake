set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT BRIGHT_SDK)
    message(FATAL_ERROR "BRIGHT_SDK must point to BrightBox3DSDK.cmake")
endif()
include(${BRIGHT_SDK})
include(${BRIGHT_ENGINE_SOURCE_DIR}/cmake/BrightReflection.cmake)

if(NOT TARGET BrightBox3D)
    add_library(BrightBox3D SHARED IMPORTED)
    set_target_properties(BrightBox3D PROPERTIES
        IMPORTED_LOCATION "${BRIGHT_ENGINE_LIBRARY}"
        IMPORTED_IMPLIB "${BRIGHT_ENGINE_LINKER_FILE}"
        INTERFACE_INCLUDE_DIRECTORIES "${BRIGHT_ENGINE_SOURCE_DIR}/Engine/Source;${BRIGHT_RAPIDJSON_INCLUDE}"
        INTERFACE_COMPILE_DEFINITIONS "RAPIDJSON_HAS_STDSTRING=1;RAPIDJSON_NOMEMBERITERATORCLASS")
    if(WIN32)
        set_property(TARGET BrightBox3D APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "NOMINMAX;WIN32_LEAN_AND_MEAN")
    endif()
endif()

if(NOT TARGET BrightHeaderTool)
    add_executable(BrightHeaderTool IMPORTED)
    set_target_properties(BrightHeaderTool PROPERTIES IMPORTED_LOCATION "${BRIGHT_HEADER_TOOL}")
endif()

function(bright_add_game_module name source_dir output_dir)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${source_dir}/*.cpp ${source_dir}/*.h)
    add_library(${name} SHARED ${sources})
    target_link_libraries(${name} PRIVATE BrightBox3D)
    target_include_directories(${name} PRIVATE ${source_dir})
    bright_generate_reflection(${name} ${source_dir})
    set_target_properties(${name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${output_dir}
        RUNTIME_OUTPUT_DIRECTORY ${output_dir}
        BUILD_RPATH "${BRIGHT_ENGINE_BINARY_DIR}")
    foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${config} ${output_dir} RUNTIME_OUTPUT_DIRECTORY_${config} ${output_dir})
    endforeach()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${name} PRIVATE -Wall -Wno-unused-parameter)
    elseif(MSVC)
        target_compile_options(${name} PRIVATE /utf-8 /W3)
    endif()
endfunction()
