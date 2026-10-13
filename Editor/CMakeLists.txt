file(GLOB_RECURSE BRIGHT_EDITOR_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h)
list(FILTER BRIGHT_EDITOR_SOURCES EXCLUDE REGEX ".*/Source/Main\\.cpp$")

add_library(BrightEditorCore STATIC ${BRIGHT_EDITOR_SOURCES})
target_include_directories(BrightEditorCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_link_libraries(BrightEditorCore PUBLIC BrightBox3D BrightImGui BrightStb)

add_executable(BrightEditor ${CMAKE_CURRENT_SOURCE_DIR}/Source/Main.cpp)
target_link_libraries(BrightEditor PRIVATE BrightEditorCore)
set_target_properties(BrightEditor PROPERTIES BUILD_RPATH "${BRIGHT_BIN_DIR}")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(BrightEditorCore PRIVATE -Wall -Wextra -Wno-unused-parameter)
elseif(MSVC)
    target_compile_options(BrightEditorCore PRIVATE /W4 /wd4100 /utf-8)
endif()
