file(GLOB_RECURSE BRIGHT_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(BrightTests ${BRIGHT_TEST_SOURCES})
target_link_libraries(BrightTests PRIVATE BrightBox3D GTest::gtest_main BrightStb)
target_compile_definitions(BrightTests PRIVATE BRIGHT_UFBX_DATA_DIR="${ufbx_SOURCE_DIR}/data" BRIGHT_ENGINE_CONTENT_DIR="${CMAKE_SOURCE_DIR}/Engine/Content")
target_include_directories(BrightTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
bright_generate_reflection(BrightTests ${CMAKE_CURRENT_SOURCE_DIR})

include(GoogleTest)
gtest_discover_tests(BrightTests DISCOVERY_MODE PRE_TEST)
