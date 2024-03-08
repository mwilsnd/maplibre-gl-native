cmake_minimum_required(VERSION 3.19)

include(${PROJECT_SOURCE_DIR}/vendor/icu.cmake)

message(STATUS "MacOS minimum target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")

link_libraries(sqlite3 z mbgl-vendor-csscolorparser mbgl-core mbgl-macos-default mbgl-macos-objc mbgl-macos-objcpp)

external_bazel_target(
    boost
    //vendor:boost # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)
external_bazel_target(
    csscolorparser
    //vendor:csscolorparser # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)
external_bazel_target(
    "earcut.hpp"
    "//vendor:earcut.hpp" # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)
external_bazel_target(
    bzl-mapbox-base
    //vendor:mapbox-base # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)
external_bazel_target(
    parsedate
    //vendor:parsedate # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)
external_bazel_target(
    polylabel
    //vendor:polylabel # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)
external_bazel_target(
    unique_resource
    //vendor:unique_resource # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)
external_bazel_target(
    vector-tile
    //vendor:vector-tile # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)
external_bazel_target(
    wagyu
    //vendor:wagyu # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)
external_bazel_target(
    icu
    //vendor:icu # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)
external_bazel_target(
    nunicode
    //vendor:nunicode # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)





external_bazel_target(
    mbgl-macos-default-collator
    //platform/default:default-collator # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)

external_bazel_target(
    mbgl-macos-default
    //platform/default:mbgl-default # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)

set_target_properties(
    mbgl-macos-default
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
        ${PROJECT_SOURCE_DIR}/platform/default/include
)

external_bazel_target(
    mbgl-macos-objcpp
    //platform:macos-objcpp # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)

external_bazel_target(
    mbgl-macos-objc
    //platform:macos-objc # Bazel target
    dbg # Build Mode: dbg, opt
    OFF # Shared library, OFF = STATIC
)


add_subdirectory(${PROJECT_SOURCE_DIR}/bin)
add_subdirectory(${PROJECT_SOURCE_DIR}/expression-test)
add_subdirectory(${PROJECT_SOURCE_DIR}/platform/glfw)
if(MLN_WITH_NODE)
    add_subdirectory(${PROJECT_SOURCE_DIR}/platform/node)
endif()

add_executable(
    mbgl-test-runner
    ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/test/main.cpp
)

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
