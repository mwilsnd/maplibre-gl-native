cmake_minimum_required(VERSION 3.19)

include(${PROJECT_SOURCE_DIR}/vendor/icu.cmake)

message(STATUS "MacOS minimum target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")

function(external_bazel_target _NAME _FILE _BZL_TARGET)
    add_library(_NAME STATIC IMPORTED GLOBAL)
    set_target_properties(
        _NAME PROPERTIES
        IMPORTED_LOCATION _FILE
    )
    add_custom_command(
        TARGET mbgl-core-bazel-gen
        PRE_BUILD
        COMMAND bazelisk build _BZL_TARGET --compilation_mode=dbg --//:renderer=metal
            --//:maplibre_platform=ios --macos_minimum_os=${CMAKE_OSX_DEPLOYMENT_TARGET}
    )
endfunction()

external_bazel_target(
    mbgl-macos-default
    ${MLN_CMAKE_BAZEL_LIB_ROOT}/platform/default/libmbgl-default.a
    //platform/default:mbgl-default
)

add_library(mbgl-macos-objcpp STATIC IMPORTED GLOBAL)
set_target_properties(
    mbgl-macos-objcpp PROPERTIES
    IMPORTED_LOCATION ${MLN_CMAKE_BAZEL_BIN_ROOT}/platform/libmacos-objcpp.a
)
add_custom_command(
    TARGET mbgl-core-bazel-gen
    PRE_BUILD
    COMMAND bazelisk build //platform:macos-objcpp --compilation_mode=dbg --//:renderer=metal
        --//:maplibre_platform=ios --macos_minimum_os=${CMAKE_OSX_DEPLOYMENT_TARGET}
)

add_library(mbgl-macos-objc STATIC IMPORTED GLOBAL)
set_target_properties(
    mbgl-macos-objc PROPERTIES
    IMPORTED_LOCATION ${MLN_CMAKE_BAZEL_BIN_ROOT}/platform/libmacos-objc.a
)
add_custom_command(
    TARGET mbgl-core-bazel-gen
    PRE_BUILD
    COMMAND bazelisk build //platform:macos-objc --compilation_mode=dbg --//:renderer=metal
        --//:maplibre_platform=ios --macos_minimum_os=${CMAKE_OSX_DEPLOYMENT_TARGET}
)

target_link_libraries(
    mbgl-core
    INTERFACE
        mbgl-macos-objcpp
        mbgl-macos-objc
        mbgl-macos-default
        "-framework Metal"
        "-framework MetalKit"
        "-framework Foundation"
        "-framework AppKit"
        "-framework CoreGraphics"
        "-framework CoreLocation"
        "-framework SystemConfiguration"
        mbgl-vendor-icu
        sqlite3
        z
)

add_subdirectory(${PROJECT_SOURCE_DIR}/bin)
add_subdirectory(${PROJECT_SOURCE_DIR}/expression-test)
add_subdirectory(${PROJECT_SOURCE_DIR}/platform/glfw)
if(MLN_WITH_NODE)
    add_subdirectory(${PROJECT_SOURCE_DIR}/platform/node)
endif()

# add_executable(
#     mbgl-test-runner
#     ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/test/main.cpp
# )
# 
# target_include_directories(
#     mbgl-test-runner
#     PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/test/include
# )
# 
# target_compile_definitions(
#     mbgl-test-runner
#     PRIVATE WORK_DIRECTORY=${PROJECT_SOURCE_DIR}
# )
# 
# target_link_libraries(
#     mbgl-test-runner
#     PRIVATE mbgl-compiler-options -Wl,-force_load mbgl-test
# )
# 
# add_executable(
#     mbgl-benchmark-runner
#     ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/benchmark/main.cpp
# )
# 
# target_include_directories(
#     mbgl-benchmark-runner
#     PUBLIC ${PROJECT_SOURCE_DIR}/benchmark/include
# )
# 
# target_link_libraries(
#     mbgl-benchmark-runner
#     PRIVATE mbgl-compiler-options -Wl,-force_load mbgl-benchmark
# )
# 
# add_executable(
#     mbgl-render-test-runner
#     ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/render-test/main.cpp
# )
# 
# target_link_libraries(
#     mbgl-render-test-runner
#     PRIVATE mbgl-compiler-options mbgl-render-test
# )

# set_property(TARGET mbgl-benchmark-runner PROPERTY FOLDER Executables)
# set_property(TARGET mbgl-test-runner PROPERTY FOLDER Executables)
# set_property(TARGET mbgl-render-test-runner PROPERTY FOLDER Executables)
