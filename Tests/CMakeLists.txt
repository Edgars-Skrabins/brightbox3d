file(GLOB_RECURSE BRIGHT_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(BrightTests ${BRIGHT_TEST_SOURCES})
target_link_libraries(BrightTests PRIVATE BrightBox3D BrightEditorCore GTest::gtest_main BrightStb)
target_compile_definitions(BrightTests PRIVATE BRIGHT_UFBX_DATA_DIR="${ufbx_SOURCE_DIR}/data" BRIGHT_ENGINE_CONTENT_DIR="${CMAKE_SOURCE_DIR}/Engine/Content" BRIGHT_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/Samples" BRIGHT_LUCIDE_INFO="${lucide_SOURCE_DIR}/font/info.json")
target_include_directories(BrightTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(BrightTests PRIVATE BRIGHT_MODULE_NAME="BrightTests")
target_compile_definitions(BrightTests PRIVATE
    BRIGHT_CXX_COMPILER="${CMAKE_CXX_COMPILER}"
    BRIGHT_OSX_SYSROOT="${CMAKE_OSX_SYSROOT}"
    BRIGHT_ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}/Engine/Source"
    BRIGHT_ENGINE_GENERATED_DIR="${BRIGHT_GENERATED_DIR}/Engine"
    BRIGHT_COMPILE_ERROR_SNIPPETS="${CMAKE_CURRENT_SOURCE_DIR}/Reflection/CompileErrors"
    BRIGHT_HEADER_TOOL="$<TARGET_FILE:BrightHeaderTool>")
bright_generate_reflection(BrightTests ${CMAKE_CURRENT_SOURCE_DIR})

include(GoogleTest)
gtest_discover_tests(BrightTests DISCOVERY_MODE PRE_TEST)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(BrightTests PRIVATE -Werror=reorder-init-list)
endif()
