file(GLOB_RECURSE BRIGHT_EDITOR_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h)

add_executable(BrightEditor ${BRIGHT_EDITOR_SOURCES})
target_include_directories(BrightEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_link_libraries(BrightEditor PRIVATE BrightBox3D BrightImGui)
set_target_properties(BrightEditor PROPERTIES BUILD_RPATH "${BRIGHT_BIN_DIR}")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(BrightEditor PRIVATE -Wall -Wextra -Wno-unused-parameter)
elseif(MSVC)
    target_compile_options(BrightEditor PRIVATE /W4 /wd4100 /utf-8)
endif()
