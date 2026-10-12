# PlatformIO 54.03.21 imports generated sources from the CMake file API but
# does not run target_add_binary_data()'s custom command. Generate these small
# embedded resources at configure time as well. Keep IDF's original build rule
# and symbol naming, including RENAME_TO, intact.
function(target_add_binary_data target embed_file embed_type)
    _target_add_binary_data("${target}" "${embed_file}" "${embed_type}" ${ARGN})

    cmake_parse_arguments(EMBED "" "RENAME_TO" "DEPENDS" ${ARGN})
    idf_build_get_property(build_dir BUILD_DIR)
    idf_build_get_property(idf_path IDF_PATH)
    get_filename_component(data_file "${embed_file}" ABSOLUTE)
    get_filename_component(data_name "${data_file}" NAME)

    # Generated inputs (e.g. mbedTLS's certificate bundle) need their own
    # producer rule. PlatformIO already handles that bundle separately.
    if(NOT EXISTS "${data_file}")
        return()
    endif()

    set(rename_arg)
    if(EMBED_RENAME_TO)
        set(rename_arg "-DVARIABLE_BASENAME=${EMBED_RENAME_TO}")
    endif()

    # Changing the input must rerun configure, since SCons skips the IDF rule.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${data_file}" ${EMBED_DEPENDS})
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DDATA_FILE=${data_file}"
            "-DSOURCE_FILE=${build_dir}/${data_name}.S"
            "-DFILE_TYPE=${embed_type}"
            ${rename_arg}
            -P "${idf_path}/tools/cmake/scripts/data_file_embed_asm.cmake"
        RESULT_VARIABLE embed_result
    )
    if(NOT embed_result EQUAL 0)
        message(FATAL_ERROR "Failed to embed ${data_file}: ${embed_result}")
    endif()
endfunction()
