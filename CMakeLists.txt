project(mp)
cmake_minimum_required(VERSION 2.8.12)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Write all output files to the project's (binary-) root,
# regardless of where they are declared in the folder structure.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

if(NOT CMAKE_BUILD_TYPE)
    message("Defaulting to \"Release\" build type")
    set(CMAKE_BUILD_TYPE "Release"
        CACHE STRING
        "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CXX_FLAGS                   "-std=c++11 -Wall -Wextra -Wno-unused-local-typedefs")
set(CXX_FLAGS                   "${CXX_FLAGS} -pthread")            # All targets link with multithreading enabled
set(CXX_FLAGS                   "${CXX_FLAGS} -Wl,--no-as-needed")  # Required to actually link against pthreads (Workaround for gcc 4.8.2)

set(CMAKE_AR "gcc-ar")
set(CMAKE_RANLIB "gcc-ranlib")
set(CMAKE_NM "gcc-nm")
set(CMAKE_CXX_FLAGS             "${CMAKE_CXX_FLAGS} ${CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE     "${CMAKE_CXX_FLAGS_RELEASE} -flto")
set(CMAKE_CXX_FLAGS_DEBUG       "${CMAKE_CXX_FLAGS_DEBUG} -O0 -gdwarf-3 -Werror -pedantic-errors")

include(ExternalProject)
include(Dependencies.cmake)

# Header files are included in all targets' dependencies
# because otherwise they woudn't appear in QtCreator.
set(INCLUDE_ROOT "${PROJECT_SOURCE_DIR}/include/mp")
set(HEADERS
    ${INCLUDE_ROOT}/defs.hpp
    ${INCLUDE_ROOT}/metrics.hpp
    ${INCLUDE_ROOT}/ground_truth.hpp
    ${INCLUDE_ROOT}/signal_data.hpp
    ${INCLUDE_ROOT}/parser.hpp
    ${INCLUDE_ROOT}/tracing_data.hpp
    ${INCLUDE_ROOT}/feature_computation.hpp
    ${INCLUDE_ROOT}/co_moving_detection.hpp
    ${INCLUDE_ROOT}/following_detection.hpp
    ${INCLUDE_ROOT}/serialization.hpp
    ${INCLUDE_ROOT}/following_graph.hpp
    ${INCLUDE_ROOT}/page_rank.hpp

    ${INCLUDE_ROOT}/tools/array_view.hpp
    ${INCLUDE_ROOT}/tools/array_2d.hpp
    ${INCLUDE_ROOT}/tools/iter.hpp
    ${INCLUDE_ROOT}/tools/strided_array_view.hpp
)
install(DIRECTORY ${INCLUDE_ROOT} DESTINATION include)

include_directories("include")
include_directories(SYSTEM "deps")
include_directories(SYSTEM "deps/dlib-18.12")
include_directories(SYSTEM "deps/cereal-1.0.0/include")
include_directories(SYSTEM "deps/pugixml-1.5/include")
include_directories(SYSTEM "${Boost_INCLUDE_DIR}")

add_subdirectory("src")
add_subdirectory("test")
