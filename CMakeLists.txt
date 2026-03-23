cmake_minimum_required(VERSION 3.15)

# ============================================================================
# Build modes (for Windows two-phase build):
#   WSJTX_BUILD_CORE_ONLY  - Build only wsjtx_core shared library (MinGW)
#   WSJTX_BUILD_NODE_ONLY  - Build only .node module (MSVC, needs wsjtx_core)
#   Neither                - Build both targets (Linux/macOS, same compiler)
# ============================================================================
option(WSJTX_BUILD_CORE_ONLY "Build only wsjtx_core shared library" OFF)
option(WSJTX_BUILD_NODE_ONLY "Build only .node module (requires pre-built wsjtx_core)" OFF)

# Disable vcpkg manifest mode if detected
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "" FORCE)
    set(VCPKG_MANIFEST_INSTALL OFF CACHE BOOL "" FORCE)
endif()

# Project languages depend on build mode
if(WSJTX_BUILD_NODE_ONLY)
    project(wsjtx_lib_nodejs LANGUAGES C CXX)
else()
    project(wsjtx_lib_nodejs LANGUAGES C CXX Fortran)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# CMake policy for newer Boost versions
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# ============================================================================
# Platform-specific compiler flags
# ============================================================================
if(UNIX AND NOT APPLE)
    add_compile_options(-fPIC -fvisibility=hidden)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-overflow -Wno-deprecated-declarations")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-stringop-overflow -Wno-deprecated-declarations")
elseif(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -Wno-deprecated-declarations -Wno-unqualified-std-cast-call")
elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # MinGW environment
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -Wno-deprecated-declarations")
    set(CMAKE_SHARED_LINKER_FLAGS "")

    # MSYS2 paths (check both C: and D: drives)
    foreach(_msys_root "C:/msys64" "D:/msys64")
        if(EXISTS "${_msys_root}/mingw64")
            include_directories("${_msys_root}/mingw64/include")
            link_directories("${_msys_root}/mingw64/lib")
            if(EXISTS "${_msys_root}/mingw64/lib/pkgconfig")
                set(ENV{PKG_CONFIG_PATH} "${_msys_root}/mingw64/lib/pkgconfig")
            endif()
            break()
        endif()
    endforeach()
endif()

# ============================================================================
# NODE-ONLY build path (MSVC on Windows)
# Only builds the .node module, links against pre-built wsjtx_core
# ============================================================================
if(WSJTX_BUILD_NODE_ONLY)
    if(NOT WSJTX_CORE_DIR)
        message(FATAL_ERROR "WSJTX_CORE_DIR must be set to the directory containing wsjtx_core.lib and wsjtx_core.dll")
    endif()

    # cmake-js headers
    if(CMAKE_JS_INC)
        include_directories(${CMAKE_JS_INC})
    endif()

    # Node.js include path
    execute_process(
        COMMAND node -p "require('path').dirname(process.execPath) + '/../include/node'"
        OUTPUT_VARIABLE NODE_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NODE_INCLUDE_DIR AND EXISTS "${NODE_INCLUDE_DIR}")
        include_directories(${NODE_INCLUDE_DIR})
    endif()

    # node-addon-api
    set(NODE_ADDON_API_PATHS
        "${CMAKE_SOURCE_DIR}/node_modules/node-addon-api"
        "${CMAKE_SOURCE_DIR}/../../node_modules/node-addon-api"
    )
    foreach(path ${NODE_ADDON_API_PATHS})
        if(EXISTS "${path}/napi.h")
            set(NODE_ADDON_API_PATH "${path}")
            break()
        endif()
    endforeach()
    if(NOT NODE_ADDON_API_PATH)
        message(FATAL_ERROR "Could not find node-addon-api")
    endif()

    include_directories(
        ${CMAKE_SOURCE_DIR}/native
        ${NODE_ADDON_API_PATH}
    )

    # .node target
    set(NODE_SOURCES native/wsjtx_wrapper.cpp native/wsjtx_wrapper.h)
    if(CMAKE_JS_SRC)
        list(APPEND NODE_SOURCES ${CMAKE_JS_SRC})
    endif()
    add_library(${PROJECT_NAME} SHARED ${NODE_SOURCES})

    set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NAPI_DISABLE_CPP_EXCEPTIONS
        BUILDING_NODE_EXTENSION
        NAPI_VERSION=4
        NODE_GYP_MODULE_NAME=${PROJECT_NAME}
    )

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_JS_INC}
        "${NODE_ADDON_API_PATH}"
        "${WSJTX_CORE_DIR}"
    )

    # Link wsjtx_core import library + Node.js
    find_library(WSJTX_CORE_LIB
        NAMES wsjtx_core
        PATHS "${WSJTX_CORE_DIR}"
        NO_DEFAULT_PATH
    )
    if(NOT WSJTX_CORE_LIB)
        message(FATAL_ERROR "wsjtx_core.lib not found in ${WSJTX_CORE_DIR}")
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE ${WSJTX_CORE_LIB})
    if(CMAKE_JS_LIB)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_JS_LIB})
    endif()

    # Output to build/Release (handle multi-config generators like VS)
    set(_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Release")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
    )
    foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER ${cfg} CFG_UPPER)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${_OUTPUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_${CFG_UPPER} "${_OUTPUT_DIR}"
            ARCHIVE_OUTPUT_DIRECTORY_${CFG_UPPER} "${_OUTPUT_DIR}"
        )
    endforeach()

    return()  # Skip the rest of the file
endif()

# ============================================================================
# Full build path (Linux/macOS) or core-only build (Windows MinGW)
# ============================================================================

# cmake-js headers (needed for full build)
if(CMAKE_JS_INC)
    include_directories(${CMAKE_JS_INC})
endif()

# Node.js include path
execute_process(
    COMMAND node -p "require('path').dirname(process.execPath) + '/../include/node'"
    OUTPUT_VARIABLE NODE_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NODE_INCLUDE_DIR AND EXISTS "${NODE_INCLUDE_DIR}")
    include_directories(${NODE_INCLUDE_DIR})
endif()

# ---- Dependencies ----

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3F REQUIRED fftw3f)

# FFTW3 threads support
if(UNIX AND NOT APPLE)
    find_library(FFTW3F_THREADS_LIB NAMES fftw3f_threads
        PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu)
elseif(APPLE)
    find_library(FFTW3F_THREADS_LIB NAMES fftw3f_threads
        PATHS /opt/homebrew/lib /usr/local/lib NO_DEFAULT_PATH)
elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    execute_process(COMMAND pkg-config --exists fftw3f RESULT_VARIABLE _r OUTPUT_QUIET ERROR_QUIET)
    if(_r EQUAL 0)
        set(FFTW3F_THREADS_LIB "fftw3f_threads")
    endif()
endif()

if(FFTW3F_THREADS_LIB)
    set(FFTW_THREADS_LIBRARIES fftw3f_threads)
    set(FFTW_HAS_THREADS TRUE)
else()
    set(FFTW_THREADS_LIBRARIES "")
    set(FFTW_HAS_THREADS FALSE)
endif()

# Boost
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    find_package(Boost QUIET)
    if(NOT Boost_FOUND)
        find_path(Boost_INCLUDE_DIRS NAMES boost/version.hpp
            PATHS /mingw64/include C:/msys64/mingw64/include D:/msys64/mingw64/include NO_DEFAULT_PATH)
        if(Boost_INCLUDE_DIRS)
            set(Boost_FOUND TRUE)
        endif()
    endif()
else()
    find_package(Boost REQUIRED)
endif()

# macOS: find gfortran library path
if(APPLE)
    find_program(GFORTRAN_EXECUTABLE
        NAMES gfortran gfortran-14 gfortran-13 gfortran-12 gfortran-11
        PATHS /opt/homebrew/bin /usr/local/bin)
    if(NOT GFORTRAN_EXECUTABLE)
        find_program(GFORTRAN_EXECUTABLE NAMES gfortran gfortran-14 gfortran-13 gfortran-12)
    endif()
    if(GFORTRAN_EXECUTABLE)
        execute_process(COMMAND ${GFORTRAN_EXECUTABLE} --print-file-name=libgfortran.dylib
            OUTPUT_VARIABLE GFORTRAN_LIB_PATH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(GFORTRAN_LIB_PATH AND NOT GFORTRAN_LIB_PATH STREQUAL "libgfortran.dylib")
            get_filename_component(GFORTRAN_LIB_DIR "${GFORTRAN_LIB_PATH}" DIRECTORY)
            link_directories(${GFORTRAN_LIB_DIR})
        endif()
        execute_process(COMMAND ${GFORTRAN_EXECUTABLE} --print-file-name=libgcc_s.1.dylib
            OUTPUT_VARIABLE LIBGCC_S_PATH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(LIBGCC_S_PATH AND NOT LIBGCC_S_PATH STREQUAL "libgcc_s.1.dylib")
            get_filename_component(LIBGCC_S_DIR "${LIBGCC_S_PATH}" DIRECTORY)
            link_directories(${LIBGCC_S_DIR})
        endif()
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/wsjtx_lib
    ${CMAKE_SOURCE_DIR}/native
    ${FFTW3F_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# Dependencies for the core library
set(LIBRARIES_FROM_REFERENCES ${FFTW3F_LIBRARIES} ${FFTW_THREADS_LIBRARIES})

# Build the Fortran/C++ core
add_subdirectory(wsjtx_lib)
link_directories(${FFTW3F_LIBRARY_DIRS})

# ============================================================================
# Target 1: wsjtx_core shared library (pure C API)
# ============================================================================
add_library(wsjtx_core SHARED native/wsjtx_c_api.cpp native/wsjtx_c_api.h)

target_compile_definitions(wsjtx_core PRIVATE WSJTX_CORE_EXPORTS)
set_target_properties(wsjtx_core PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
)

target_include_directories(wsjtx_core PRIVATE
    ${CMAKE_SOURCE_DIR}/wsjtx_lib
    ${FFTW3F_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(wsjtx_core PRIVATE wsjtx_lib)

# Platform-specific linking for wsjtx_core
if(APPLE)
    target_link_libraries(wsjtx_core PRIVATE
        ${FFTW3F_LIBRARIES} "-framework Accelerate" gfortran gcc_s.1)
    if(FFTW_HAS_THREADS)
        target_link_libraries(wsjtx_core PRIVATE fftw3f_threads)
    endif()
elseif(UNIX)
    target_link_libraries(wsjtx_core PRIVATE
        ${FFTW3F_LIBRARIES} gfortran gcc_s pthread)
    if(FFTW_HAS_THREADS)
        target_link_libraries(wsjtx_core PRIVATE fftw3f_threads)
    endif()
elseif(WIN32)
    # Static-link ALL non-system dependencies so wsjtx_core.dll only depends
    # on Windows system DLLs (KERNEL32.dll, msvcrt.dll). This prevents DLL
    # conflicts when the user's PATH contains a different MinGW installation.
    target_link_options(wsjtx_core PRIVATE
        -static-libgcc
        -static-libstdc++
    )
    target_link_libraries(wsjtx_core PRIVATE
        -Wl,-Bstatic
        -lfftw3f -lfftw3f_threads
        -lgfortran -lquadmath -lwinpthread
        -Wl,-Bdynamic
    )
endif()

# Output wsjtx_core to build/Release alongside .node
set(_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Release")
set_target_properties(wsjtx_core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
)

# Linux RPATH
if(UNIX AND NOT APPLE)
    set_target_properties(wsjtx_core PROPERTIES
        BUILD_RPATH "\$ORIGIN"
        INSTALL_RPATH "\$ORIGIN"
    )
endif()

# If core-only build, stop here
if(WSJTX_BUILD_CORE_ONLY)
    return()
endif()

# ============================================================================
# Target 2: .node N-API module (links wsjtx_core, no Fortran/FFTW needed)
# ============================================================================

# node-addon-api
set(NODE_ADDON_API_PATHS
    "${CMAKE_SOURCE_DIR}/node_modules/node-addon-api"
    "${CMAKE_SOURCE_DIR}/../../node_modules/node-addon-api"
    "${CMAKE_SOURCE_DIR}/../../../node_modules/node-addon-api"
)
foreach(path ${NODE_ADDON_API_PATHS})
    if(EXISTS "${path}/napi.h")
        set(NODE_ADDON_API_PATH "${path}")
        break()
    endif()
endforeach()
if(NOT NODE_ADDON_API_PATH)
    message(FATAL_ERROR "Could not find node-addon-api")
endif()

# .node target
set(NODE_SOURCES native/wsjtx_wrapper.cpp native/wsjtx_wrapper.h)

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # MinGW: exclude delay load hook
    add_library(${PROJECT_NAME} SHARED ${NODE_SOURCES})
elseif(CMAKE_JS_SRC)
    add_library(${PROJECT_NAME} SHARED ${NODE_SOURCES} ${CMAKE_JS_SRC})
else()
    add_library(${PROJECT_NAME} SHARED ${NODE_SOURCES})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    CXX_VISIBILITY_PRESET hidden
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    NAPI_DISABLE_CPP_EXCEPTIONS
    BUILDING_NODE_EXTENSION
    NAPI_VERSION=4
    NODE_GYP_MODULE_NAME=${PROJECT_NAME}
)

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_JS_INC}"
        "/mingw64/include/node"
        "/mingw64/include/node/node"
        "${NODE_ADDON_API_PATH}"
    )
else()
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_JS_INC}
        "${NODE_ADDON_API_PATH}"
    )
endif()

# Link wsjtx_core (not wsjtx_lib directly)
target_link_libraries(${PROJECT_NAME} PRIVATE wsjtx_core)

# Node.js library
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(EXISTS "${CMAKE_JS_LIB}")
        set(LOCAL_NODE_LIB "${CMAKE_BINARY_DIR}/node.lib")
        configure_file("${CMAKE_JS_LIB}" "${LOCAL_NODE_LIB}" COPYONLY)
        target_link_libraries(${PROJECT_NAME} PRIVATE "${LOCAL_NODE_LIB}")
    endif()
elseif(CMAKE_JS_LIB)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_JS_LIB})
endif()

# Output directories
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
)
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} CFG_UPPER)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
        RUNTIME_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
        ARCHIVE_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
    )
endforeach()

# Linux RPATH for .node
if(UNIX AND NOT APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "\$ORIGIN"
        INSTALL_RPATH "\$ORIGIN"
    )
endif()
