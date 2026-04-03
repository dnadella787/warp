include_guard(GLOBAL)

function(_warp_resolve_codegen_cli out_var)
    if (TARGET warp_codegen_cli)
        set(${out_var} warp_codegen_cli PARENT_SCOPE)
        return()
    endif ()

    if (TARGET warp::warp_codegen_cli)
        set(${out_var} warp::warp_codegen_cli PARENT_SCOPE)
        return()
    endif ()

    if (DEFINED warp_CODEGEN_EXECUTABLE)
        set(${out_var} "${warp_CODEGEN_EXECUTABLE}" PARENT_SCOPE)
        return()
    endif ()

    message(FATAL_ERROR
        "warp_generate_stubs requires the warp codegen CLI. "
        "Build the source tree with the warp_codegen_cli target or install the package that provides warp_codegen.")
endfunction()

function(warp_generate_stubs)
    set(options)
    set(one_value_args
        TARGET
        SPEC
        OUTPUT_DIR
        NAMESPACE
        MODEL_HEADER
        RESOURCE_HEADER
        OUT_MODEL_HEADER
        OUT_RESOURCE_HEADER)
    set(multi_value_args DEPENDS)
    cmake_parse_arguments(WARP "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    foreach(required_arg TARGET SPEC OUTPUT_DIR)
        if (NOT WARP_${required_arg})
            message(FATAL_ERROR "warp_generate_stubs requires ${required_arg}.")
        endif ()
    endforeach()

    if (TARGET "${WARP_TARGET}")
        message(FATAL_ERROR "warp_generate_stubs target '${WARP_TARGET}' already exists.")
    endif ()

    if (NOT WARP_MODEL_HEADER)
        set(WARP_MODEL_HEADER "generated_api_types.hpp")
    endif ()

    if (NOT WARP_RESOURCE_HEADER)
        set(WARP_RESOURCE_HEADER "generated_api_resources.hpp")
    endif ()

    get_filename_component(_warp_spec "${WARP_SPEC}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(_warp_output_dir "${WARP_OUTPUT_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

    set(_warp_model_output "${_warp_output_dir}/${WARP_MODEL_HEADER}")
    set(_warp_resource_output "${_warp_output_dir}/${WARP_RESOURCE_HEADER}")

    get_filename_component(_warp_model_output_dir "${_warp_model_output}" DIRECTORY)
    get_filename_component(_warp_resource_output_dir "${_warp_resource_output}" DIRECTORY)

    _warp_resolve_codegen_cli(_warp_codegen_cli)
    if (TARGET "${_warp_codegen_cli}")
        set(_warp_codegen_cli_command "$<TARGET_FILE:${_warp_codegen_cli}>")
        set(_warp_codegen_cli_dependency "${_warp_codegen_cli}")
    else ()
        set(_warp_codegen_cli_command "${_warp_codegen_cli}")
        set(_warp_codegen_cli_dependency "${_warp_codegen_cli}")
    endif ()

    set(_warp_codegen_args
        --spec "${_warp_spec}"
        --output-dir "${_warp_output_dir}"
        --model-header "${WARP_MODEL_HEADER}"
        --resource-header "${WARP_RESOURCE_HEADER}")

    if (WARP_NAMESPACE)
        list(APPEND _warp_codegen_args --namespace "${WARP_NAMESPACE}")
    endif ()

    add_custom_command(
        OUTPUT "${_warp_model_output}" "${_warp_resource_output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_warp_model_output_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_warp_resource_output_dir}"
        COMMAND ${_warp_codegen_cli_command} ${_warp_codegen_args}
        DEPENDS "${_warp_spec}" ${WARP_DEPENDS} ${_warp_codegen_cli_dependency}
        VERBATIM
        COMMENT "Generating warp API stubs from ${_warp_spec}")

    add_custom_target("${WARP_TARGET}"
        DEPENDS "${_warp_model_output}" "${_warp_resource_output}")

    set_source_files_properties("${_warp_model_output}" "${_warp_resource_output}" PROPERTIES GENERATED TRUE)
    set_target_properties("${WARP_TARGET}" PROPERTIES
        WARP_MODEL_HEADER "${_warp_model_output}"
        WARP_RESOURCE_HEADER "${_warp_resource_output}"
        WARP_OUTPUT_DIR "${_warp_output_dir}")

    if (WARP_OUT_MODEL_HEADER)
        set(${WARP_OUT_MODEL_HEADER} "${_warp_model_output}" PARENT_SCOPE)
    endif ()

    if (WARP_OUT_RESOURCE_HEADER)
        set(${WARP_OUT_RESOURCE_HEADER} "${_warp_resource_output}" PARENT_SCOPE)
    endif ()
endfunction()
