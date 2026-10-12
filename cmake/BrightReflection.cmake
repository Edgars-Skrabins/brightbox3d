function(bright_generate_reflection target source_root)
    file(GLOB_RECURSE headers CONFIGURE_DEPENDS ${source_root}/*.h)
    set(generated_files "")
    foreach(header ${headers})
        file(RELATIVE_PATH relative ${source_root} ${header})
        string(REGEX REPLACE "\\.h$" ".gen.cpp" generated_relative ${relative})
        set(generated ${CMAKE_CURRENT_BINARY_DIR}/Generated/${generated_relative})
        add_custom_command(
            OUTPUT ${generated}
            COMMAND $<TARGET_FILE:BrightHeaderTool> --header ${header} --include ${relative} --output ${generated}
            DEPENDS ${header} BrightHeaderTool
            COMMENT "BrightHeaderTool ${relative}"
            VERBATIM)
        list(APPEND generated_files ${generated})
    endforeach()
    target_sources(${target} PRIVATE ${generated_files})
endfunction()
