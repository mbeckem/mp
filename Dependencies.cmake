# Boost
set(Boost_USE_STATIC_LIBS       ON)
set(Boost_USE_MULTITHREADED     ON)
set(Boost_USE_STATIC_RUNTIME    OFF)
find_package(Boost 1.55
             COMPONENTS system program_options filesystem
             REQUIRED)

# The Dlib library provides machine learning functionality.
ExternalProject_Add(project_dlib
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/dlib-18.12/dlib
    PREFIX     ${PROJECT_BINARY_DIR}/deps/dlib-18.12
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DDLIB_ISO_CPP_ONLY=1 -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    # Copy static library to installation dir
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy libdlib.a <INSTALL_DIR>
)

# Make linking via cmake possible
ExternalProject_Get_Property(project_dlib INSTALL_DIR)
add_library(dlib STATIC IMPORTED)
add_dependencies(dlib project_dlib)
set_property(TARGET dlib PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/libdlib.a)

# pugixml provides xml parsing/writing
ExternalProject_Add(project_pugixml
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/pugixml-1.5/scripts # CMakeLists.txt is located here
    PREFIX     ${PROJECT_BINARY_DIR}/deps/pugixml-1.5
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
               -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
               -DCMAKE_INSTALL_LIBDIR=lib # Overwrite gnu-folder-nonsense
)

# Make linking via cmake possible
ExternalProject_Get_Property(project_pugixml INSTALL_DIR)
add_library(pugixml STATIC IMPORTED)
add_dependencies(pugixml project_pugixml)
set_property(TARGET pugixml PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libpugixml.a)
