add_custom_target(build_bazel_targets)

function(external_bazel_target _NAME _BZL_TARGET _BUILD_MODE _IS_SHARED_LIBARY)
    if(_IS_SHARED_LIBARY)
        add_library(${_NAME}-imported OBJECT IMPORTED GLOBAL)
    else()
        add_library(${_NAME}-imported STATIC IMPORTED GLOBAL)
    endif()

    string(CONCAT CMD
        "bazelisk cquery ${_BZL_TARGET}"
        " --output=files --compilation_mode=${_BUILD_MODE}"
        " --//:renderer=metal"
        " --apple_platform_type=macos"
        " --macos_minimum_os=${CMAKE_OSX_DEPLOYMENT_TARGET}"
        " --use_top_level_targets_for_symlinks"
    )

    execute_process(
        COMMAND bash -c ${CMD}
        OUTPUT_VARIABLE ARTIFACT_LOCATION
    )

    string(CONCAT LIB_PATH ${PROJECT_SOURCE_DIR} "/" ${ARTIFACT_LOCATION})
    string(STRIP ${LIB_PATH} LIB_PATH)

    set_property(
        TARGET ${_NAME}-imported
        PROPERTY IMPORTED_LOCATION ${LIB_PATH})

        
    message(STATUS ${LIB_PATH})
    message(STATUS "EOL")



    add_custom_target(
        ${_NAME}-gen
        COMMAND bazelisk build ${_BZL_TARGET}
            --compilation_mode=${_BUILD_MODE}
            --//:renderer=metal
            --apple_platform_type=macos
            --macos_minimum_os=${CMAKE_OSX_DEPLOYMENT_TARGET}
            --use_top_level_targets_for_symlinks
    )

    add_library(${_NAME} INTERFACE)
    target_link_libraries(${_NAME} INTERFACE ${_NAME}-imported)
    add_dependencies(${_NAME} ${_NAME}-gen)
    add_dependencies(build_bazel_targets ${_NAME})
endfunction()
