cmake_minimum_required(VERSION 3.15)
project(wsjtx_lib_nodejs)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include cmake-js
include_directories(${CMAKE_JS_INC})

# Add Node.js include path for node_api.h
execute_process(
    COMMAND node -p "require('path').dirname(process.execPath) + '/../include/node'"
    OUTPUT_VARIABLE NODE_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
include_directories(${NODE_INCLUDE_DIR})

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED)

# Find FFTW3
pkg_check_modules(FFTW3F REQUIRED fftw3f)

# Manually check for FFTW3 threads support since pkg-config may not find it
find_library(FFTW3F_THREADS_LIB 
    NAMES fftw3f_threads libfftw3f_threads
    PATHS /opt/homebrew/lib /usr/local/lib
    NO_DEFAULT_PATH
)

if(FFTW3F_THREADS_LIB)
    message(STATUS "FFTW3 with threads support found: ${FFTW3F_THREADS_LIB}")
    set(FFTW_THREADS_LIBRARIES fftw3f_threads)
    set(FFTW_HAS_THREADS TRUE)
else()
    message(STATUS "FFTW3 threads not found, using single-threaded version")
    set(FFTW_THREADS_LIBRARIES "")
    set(FFTW_HAS_THREADS FALSE)
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/wsjtx_lib
    ${CMAKE_SOURCE_DIR}/native
    ${CMAKE_SOURCE_DIR}/node_modules/node-addon-api
    ${FFTW3F_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# Define LIBRARIES_FROM_REFERENCES for wsjtx_lib submodule
set(LIBRARIES_FROM_REFERENCES 
    ${FFTW3F_LIBRARIES}
    ${FFTW_THREADS_LIBRARIES}
)

# Find gfortran library path
execute_process(
    COMMAND gfortran --print-file-name=libgfortran.dylib
    OUTPUT_VARIABLE GFORTRAN_LIB_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
get_filename_component(GFORTRAN_LIB_DIR ${GFORTRAN_LIB_PATH} DIRECTORY)

# Platform-specific libraries for wsjtx_lib
if(APPLE)
    # Find libgcc_s.1 for nested function support
    execute_process(
        COMMAND gfortran --print-file-name=libgcc_s.1.dylib
        OUTPUT_VARIABLE LIBGCC_S_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    get_filename_component(LIBGCC_S_DIR ${LIBGCC_S_PATH} DIRECTORY)
    
    list(APPEND LIBRARIES_FROM_REFERENCES 
        "-framework Accelerate"
        gfortran 
        gcc_s.1
        fftw3f_threads
    )
    # Add gfortran library directory
    link_directories(${GFORTRAN_LIB_DIR})
    link_directories(${LIBGCC_S_DIR})
elseif(UNIX)
    list(APPEND LIBRARIES_FROM_REFERENCES 
        gfortran 
        gcc_s 
        pthread
        fftw3f_threads
    )
elseif(WIN32)
    list(APPEND LIBRARIES_FROM_REFERENCES gfortran)
endif()

# Add wsjtx_lib as subdirectory
add_subdirectory(wsjtx_lib)

# Link directories (must be before creating target)
link_directories(${FFTW3F_LIBRARY_DIRS})

# Source files for the Node.js addon
file(GLOB_RECURSE NATIVE_SOURCES "native/*.cpp" "native/*.h")

# Create the Node.js addon
add_library(${PROJECT_NAME} SHARED 
    ${NATIVE_SOURCES}
    ${CMAKE_JS_SRC}
)

# Set properties for Node.js addon
set_target_properties(${PROJECT_NAME} PROPERTIES 
    PREFIX "" 
    SUFFIX ".node"
    CXX_VISIBILITY_PRESET hidden
)

# Set Node.js addon include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC})

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    ${CMAKE_JS_LIB}
    wsjtx_lib
    ${FFTW3F_LIBRARIES}
    ${FFTW_THREADS_LIBRARIES}
    ${Boost_LIBRARIES}
)

# Platform-specific settings
if(APPLE)
    target_link_libraries(${PROJECT_NAME} 
        "-framework Accelerate"
        gfortran 
        gcc_s.1
        fftw3f_threads
    )
    # Add gfortran library directory for linking
    target_link_directories(${PROJECT_NAME} PRIVATE ${GFORTRAN_LIB_DIR})
    target_link_directories(${PROJECT_NAME} PRIVATE ${LIBGCC_S_DIR})
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} 
        gfortran 
        gcc_s 
        pthread
        fftw3f_threads
    )
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} gfortran)
endif()

# Compiler-specific options
target_compile_definitions(${PROJECT_NAME} PRIVATE NAPI_DISABLE_CPP_EXCEPTIONS)

# Add compile flags
target_compile_options(${PROJECT_NAME} PRIVATE ${FFTW3F_CFLAGS_OTHER})
