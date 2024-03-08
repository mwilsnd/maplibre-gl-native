cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

# MacOS build target
set(CMAKE_OSX_DEPLOYMENT_TARGET 13.6 CACHE STRING "" FORCE)
add_compile_options(-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET})
add_link_options(-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET})

option(MLN_WITH_CORE_ONLY "Build only the core bits, no platform code" OFF)
option(MLN_WITH_CLANG_TIDY "Build with clang-tidy checks enabled" OFF)
option(MLN_WITH_COVERAGE "Enable code coverage collection" OFF)
option(MLN_WITH_QT "Build MapLibre Native Qt bindings" OFF)
option(MLN_WITH_NODE "Build MapLibre Native Node.js bindings" OFF)
option(MLN_WITH_SANITIZER "Use [address|thread|undefined] here" OFF)
option(MLN_WITH_RTTI "Compile with runtime type information" OFF)
option(MLN_WITH_OPENGL "Build with OpenGL renderer" ON)
option(MLN_WITH_METAL "Build with Metal renderer" ON)
option(MLN_WITH_EGL "Build with EGL renderer" OFF)
option(MLN_WITH_OSMESA "Build with OSMesa (Software) renderer" OFF)
option(MLN_WITH_WERROR "Make all compilation warnings errors" ON)
option(MLN_LEGACY_RENDERER "Include the legacy rendering pathway" ON)
option(MLN_DRAWABLE_RENDERER "Include the drawable rendering pathway" OFF)
option(MLN_USE_UNORDERED_DENSE "Use ankerl dense containers for performance" ON)

if(MLN_WITH_QT)
    set(MLN_CORE_SHARED_LIBRARY ON)
else()
    set(MLN_CORE_SHARED_LIBRARY OFF)
endif()

if (MLN_WITH_CLANG_TIDY)
    find_program(CLANG_TIDY_COMMAND NAMES clang-tidy)
    if(NOT CLANG_TIDY_COMMAND)
        message(FATAL_ERROR "ENABLE_CLANG_TIDY is ON but clang-tidy is not found!")
    else()
        message(STATUS "Found clang-tidy at ${CLANG_TIDY_COMMAND}")
    endif()
    # TODO: there are options which are only available on GCC(e.g. -Werror=maybe-uninitialized), 
    # that's why we need to disable this `unknown-warning-option` here. 
    # We could check if current compiler supports particular flag before enabling it.
    set(CLANG_TIDY_COMMAND "${CLANG_TIDY_COMMAND};--extra-arg=-Wno-unknown-warning-option;--extra-arg=-Wno-pragmas")
endif()

if(WIN32 AND NOT MLN_WITH_QT)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/platform/windows/custom-toolchain.cmake)
endif()

project("Mapbox GL Native" LANGUAGES CXX C)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER Core)

set(
    MEMORYCHECK_COMMAND_OPTIONS
    "--leak-check=full --gen-suppressions=all --error-exitcode=1 --suppressions=${PROJECT_SOURCE_DIR}/scripts/valgrind.sup"
)

include(CTest)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(
    mbgl-compiler-options INTERFACE
)

set(UBSAN_BLACKLIST ${PROJECT_SOURCE_DIR}/scripts/ubsan.blacklist)

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(MLN_DARWIN_WITH_BAZEL ON)
    set(MLN_WITH_OPENGL OFF)
    set(MLN_WITH_METAL ON)
    set(MLN_DRAWABLE_RENDERER ON)
    message(STATUS "Linking externally with bazel")
else()
    set(MLN_DARWIN_WITH_BAZEL OFF)
endif() 

if (MLN_DRAWABLE_RENDERER)
    set(MLN_LEGACY_RENDERER OFF)
endif()

target_compile_options(
    mbgl-compiler-options
    INTERFACE
        $<$<BOOL:${MLN_WITH_COVERAGE}>:--coverage>
        $<$<BOOL:${MLN_WITH_SANITIZER}>:-DSANITIZE>
        $<$<BOOL:${MLN_WITH_SANITIZER}>:-fno-omit-frame-pointer>
        $<$<BOOL:${MLN_WITH_SANITIZER}>:-fno-optimize-sibling-calls>
        $<$<STREQUAL:${MLN_WITH_SANITIZER},address>:-fsanitize=address>
        $<$<STREQUAL:${MLN_WITH_SANITIZER},thread>:-fsanitize=thread>
        $<$<STREQUAL:${MLN_WITH_SANITIZER},undefined>:-fsanitize=implicit-conversion,
        -fsanitize-blacklist=${UBSAN_BLACKLIST}>
        $<$<STREQUAL:${MLN_WITH_SANITIZER},undefined>:-fsanitize=undefined,
        -fsanitize-blacklist=${UBSAN_BLACKLIST}>
        $<$<STREQUAL:${MLN_WITH_SANITIZER},undefined>:-fsanitize=unsigned-integer-overflow,
        -fsanitize-blacklist=${UBSAN_BLACKLIST}>
        $<$<STREQUAL:${MLN_WITH_SANITIZER},undefined>:-fsanitize=nullability,
        -fsanitize-blacklist=${UBSAN_BLACKLIST}>
        $<$<STREQUAL:${MLN_WITH_SANITIZER},undefined>:-fsanitize=float-divide-by-zero,
        -fsanitize-blacklist=${UBSAN_BLACKLIST}>
        $<$<PLATFORM_ID:iOS>:-fembed-bitcode>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<OR:$<BOOL:${MLN_WITH_RTTI}>,$<CXX_COMPILER_ID:MSVC>>>>:-fno-rtti>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<PLATFORM_ID:Windows>>>:-Wall>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<PLATFORM_ID:Windows>>>:-Wshadow>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<PLATFORM_ID:Windows>>>:-Wextra>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<PLATFORM_ID:Windows>>,$<BOOL:${MLN_WITH_WERROR}>>:-Werror>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<PLATFORM_ID:Android>>:-Wno-error=tautological-constant-compare>
        $<$<CXX_COMPILER_ID:GNU>:-Wno-error=maybe-uninitialized>
        $<$<CXX_COMPILER_ID:GNU>:-Wno-error=return-type>
        $<$<CXX_COMPILER_ID:GNU>:-Wno-error=unknown-pragmas>
        $<$<CXX_COMPILER_ID:GNU>:-Wno-error=pragmas>
        $<$<CXX_COMPILER_ID:AppleClang>:-Wno-unknown-warning-option>
        $<$<AND:$<CXX_COMPILER_ID:AppleClang>,$<NOT:$<BOOL:${MLN_WITH_QT}>>>:-Wno-error=deprecated-declarations>
        $<$<AND:$<CXX_COMPILER_ID:AppleClang>,$<NOT:$<BOOL:${MLN_WITH_QT}>>>:-Wno-error=unused-parameter>
        $<$<AND:$<CXX_COMPILER_ID:AppleClang>,$<NOT:$<BOOL:${MLN_WITH_QT}>>>:-Wno-error=unused-property-ivar>
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
        $<$<CXX_COMPILER_ID:MSVC>:/WX>      # all warnings as errors
        $<$<CXX_COMPILER_ID:MSVC>:/EHsc>    # exceptions
        $<$<CXX_COMPILER_ID:MSVC>:/wd4068>  # pragma
        $<$<CXX_COMPILER_ID:MSVC>:/D_USE_MATH_DEFINES>
        $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<PLATFORM_ID:Windows>>:-D_USE_MATH_DEFINES>
        $<$<CXX_COMPILER_ID:MSVC>:/D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS>
        $<$<CXX_COMPILER_ID:MSVC>:/D_CRT_SECURE_NO_WARNINGS>
)

target_link_libraries(
    mbgl-compiler-options
    INTERFACE
        $<$<BOOL:${MLN_WITH_COVERAGE}>:--coverage>
        $<$<STREQUAL:${MLN_WITH_SANITIZER},address>:-fsanitize=address>
        $<$<STREQUAL:${MLN_WITH_SANITIZER},thread>:-fsanitize=thread>
        $<$<STREQUAL:${MLN_WITH_SANITIZER},undefined>:-fsanitize=undefined>
)

cmake_policy(SET CMP0063 NEW)
cmake_policy(SET CMP0079 NEW) # target_link_libraries from other directories
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
if (CMAKE_GENERATOR STREQUAL "Ninja Multi-Config")
    set(CMAKE_CROSS_CONFIGS "all")
    set(CMAKE_DEFAULT_CONFIGS "all")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

if(NOT MLN_DARWIN_WITH_BAZEL)
    if(MLN_CORE_SHARED_LIBRARY)
        add_library(mbgl-core OBJECT)
    else()
        add_library(mbgl-core STATIC)
    endif()
endif()

if(MLN_DARWIN_WITH_BAZEL)
    add_compile_definitions(
        MLN_DRAWABLE_RENDERER=1
        MLN_RENDER_BACKEND_METAL=1
    )
elseif(MLN_WITH_OPENGL)
    target_compile_definitions(
        mbgl-core
        PRIVATE MLN_RENDER_BACKEND_OPENGL=1
        PUBLIC
            "MLN_LEGACY_RENDERER=$<BOOL:${MLN_LEGACY_RENDERER}>"
            "MLN_DRAWABLE_RENDERER=$<BOOL:${MLN_DRAWABLE_RENDERER}>"
            "MLN_USE_UNORDERED_DENSE=$<BOOL:${MLN_USE_UNORDERED_DENSE}>"
    )
elseif(MLN_WITH_METAL)
    target_compile_definitions(
        mbgl-core
        PRIVATE MLN_RENDER_BACKEND_METAL=1
    )
endif()

# File listings
include(${PROJECT_SOURCE_DIR}/cmake/core.cmake)

# External linkage support with Bazel on Darwin
include(${PROJECT_SOURCE_DIR}/cmake/bazel.cmake)

if(MLN_DARWIN_WITH_BAZEL)
    # mbgl-core is built by Bazel
    external_bazel_target(
        mbgl-core
        //:mbgl-core
        dbg
        MLN_CORE_SHARED_LIBRARY
    )

    set_target_properties(
        mbgl-core
        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/include
    )
else()
    target_sources(
        mbgl-core PRIVATE
        ${INCLUDE_FILES}
        ${SRC_FILES}
    )

    if (MLN_WITH_CLANG_TIDY)
        set_target_properties(mbgl-core PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
    endif()

    source_group(TREE ${PROJECT_SOURCE_DIR}/include FILES ${INCLUDE_FILES})
    source_group(TREE ${PROJECT_SOURCE_DIR}/src FILES ${SRC_FILES})

    if(EXISTS ${PROJECT_SOURCE_DIR}/.git/HEAD)
        execute_process(
            COMMAND
                git
                rev-parse
                --short=8
                HEAD
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE MLN_VERSION_REV
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else()
        set(MLN_VERSION_REV 00000000)
    endif()

    set_source_files_properties(
        ${PROJECT_SOURCE_DIR}/src/mbgl/util/version.cpp
        PROPERTIES
        COMPILE_DEFINITIONS
        MBGL_VERSION_REV="${MLN_VERSION_REV}"
    )

    target_include_directories(
        mbgl-core
        PRIVATE ${PROJECT_SOURCE_DIR}/src
    )

    target_include_directories(
        mbgl-core
        PUBLIC ${PROJECT_SOURCE_DIR}/include
    )
endif()

include(${PROJECT_SOURCE_DIR}/vendor/boost.cmake)
include(${PROJECT_SOURCE_DIR}/vendor/csscolorparser.cmake)
include(${PROJECT_SOURCE_DIR}/vendor/earcut.hpp.cmake)
include(${PROJECT_SOURCE_DIR}/vendor/eternal.cmake)
include(${PROJECT_SOURCE_DIR}/vendor/mapbox-base.cmake)
include(${PROJECT_SOURCE_DIR}/vendor/parsedate.cmake)
include(${PROJECT_SOURCE_DIR}/vendor/polylabel.cmake)
include(${PROJECT_SOURCE_DIR}/vendor/protozero.cmake)
include(${PROJECT_SOURCE_DIR}/vendor/unique_resource.cmake)
include(${PROJECT_SOURCE_DIR}/vendor/vector-tile.cmake)
include(${PROJECT_SOURCE_DIR}/vendor/wagyu.cmake)

add_subdirectory(${PROJECT_SOURCE_DIR}/vendor/unordered_dense)
set_target_properties(
    unordered_dense
    PROPERTIES
        INTERFACE_MAPBOX_NAME "unordered_dense"
        INTERFACE_MAPBOX_URL "https://github.com/martinus/unordered_dense"
        INTERFACE_MAPBOX_AUTHOR "Martin Leitner-Ankerl"
        INTERFACE_MAPBOX_LICENSE ${PROJECT_SOURCE_DIR}/vendor/unordered_dense/LICENSE
)

set(MLN_CORE_PUBLIC_DEPS
    Mapbox::Base
    Mapbox::Base::Extras::expected-lite
    Mapbox::Base::Extras::rapidjson
    Mapbox::Base::geojson.hpp
    Mapbox::Base::geometry.hpp
    Mapbox::Base::variant
    mbgl-vendor-csscolorparser
    unordered_dense
)

if(NOT MLN_DARWIN_WITH_BAZEL)
    target_link_libraries(
        mbgl-core
        PRIVATE
            Mapbox::Base::Extras::kdbush.hpp
            Mapbox::Base::supercluster.hpp
            Mapbox::Base::shelf-pack-cpp
            Mapbox::Base::geojson-vt-cpp
            Mapbox::Base::cheap-ruler-cpp
            mbgl-compiler-options
            mbgl-vendor-boost
            mbgl-vendor-csscolorparser
            mbgl-vendor-earcut.hpp
            mbgl-vendor-eternal
            mbgl-vendor-parsedate
            mbgl-vendor-polylabel
            mbgl-vendor-protozero
            mbgl-vendor-unique_resource
            mbgl-vendor-vector-tile
            mbgl-vendor-wagyu
        PUBLIC
            ${MLN_CORE_PUBLIC_DEPS}
    )

    export(TARGETS mbgl-core)
else()
    #target_link_libraries(
    #    mbgl-core
    #    INTERFACE
    #        mbgl-vendor-boost
    #        mbgl-vendor-earcut.hpp
    #        mbgl-vendor-eternal
    #        mbgl-vendor-parsedate
    #        mbgl-vendor-polylabel
    #        mbgl-vendor-protozero
    #        mbgl-vendor-unique_resource
    #        mbgl-vendor-vector-tile
    #        mbgl-vendor-wagyu
    #        ${MLN_CORE_PUBLIC_DEPS}
    #)

    #foreach(dep ${MLN_CORE_PUBLIC_DEPS})
    #    get_target_property(INCLUDES ${dep} INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
    #    list(APPEND mbgl-core-public-dep-includes ${INCLUDES})
    #endforeach()

    #target_include_directories(
    #    mbgl-core
    #    INTERFACE
    #        ${mbgl-core-public-dep-includes}
    #)
endif()

export(TARGETS
    mapbox-base
    mapbox-base-cheap-ruler-cpp
    mapbox-base-extras-expected-lite
    mapbox-base-extras-kdbush.hpp
    mapbox-base-extras-rapidjson
    mapbox-base-geojson-vt-cpp
    mapbox-base-geojson.hpp
    mapbox-base-geometry.hpp
    mapbox-base-jni.hpp
    mapbox-base-optional
    mapbox-base-pixelmatch-cpp
    mapbox-base-shelf-pack-cpp
    mapbox-base-supercluster.hpp
    mapbox-base-variant
    mbgl-compiler-options
    mbgl-vendor-boost
    mbgl-vendor-csscolorparser
    mbgl-vendor-earcut.hpp
    mbgl-vendor-eternal
    mbgl-vendor-parsedate
    mbgl-vendor-polylabel
    mbgl-vendor-protozero
    mbgl-vendor-unique_resource
    mbgl-vendor-vector-tile
    mbgl-vendor-wagyu
    unordered_dense

    FILE MapboxCoreTargets.cmake
)

set_target_properties(
    mbgl-core
    PROPERTIES
        INTERFACE_MAPBOX_NAME "Mapbox GL Native"
        INTERFACE_MAPBOX_URL "https://github.com/maplibre/maplibre-native"
        INTERFACE_MAPBOX_AUTHOR "Mapbox"
        INTERFACE_MAPBOX_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE.md
)

set_property(TARGET mbgl-core PROPERTY FOLDER Core)

add_library(
    Mapbox::Map ALIAS mbgl-core
)

if(MLN_WITH_CORE_ONLY)
    return()
endif()

include(${PROJECT_SOURCE_DIR}/scripts/license.cmake)

if(MLN_WITH_QT)
    include(${PROJECT_SOURCE_DIR}/platform/qt/qt.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL Android)
    include(${PROJECT_SOURCE_DIR}/platform/android/android.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
    include(${PROJECT_SOURCE_DIR}/platform/linux/linux.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    include(${PROJECT_SOURCE_DIR}/platform/macos/macos.cmake)
elseif(WIN32)
    include(${PROJECT_SOURCE_DIR}/platform/windows/windows.cmake)
else()
    message(FATAL_ERROR "Unsupported target platform: " ${CMAKE_SYSTEM_NAME})
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/test)
add_subdirectory(${PROJECT_SOURCE_DIR}/benchmark)
add_subdirectory(${PROJECT_SOURCE_DIR}/render-test)
