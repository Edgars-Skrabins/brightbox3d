add_executable(BrightHeaderTool Main.cpp Tokenizer.cpp Tokenizer.h Parser.cpp Parser.h Generator.cpp Generator.h)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(BrightHeaderTool PRIVATE -Wall -Wextra)
endif()
