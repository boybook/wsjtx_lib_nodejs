cmake_minimum_required(VERSION 3.15)

# 禁用 vcpkg Manifest 模式，使用经典模式
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "Disable vcpkg manifest mode" FORCE)
    set(VCPKG_MANIFEST_INSTALL OFF CACHE BOOL "Disable vcpkg manifest install" FORCE)
    message(STATUS "Detected vcpkg toolchain, disabling manifest mode")
endif()


project(wsjtx_lib_nodejs LANGUAGES C CXX Fortran)

# Honour RC compiler path injected from CI to avoid node‑rc conflicts
if(DEFINED ENV{CMAKE_RC_COMPILER})
    set(CMAKE_RC_COMPILER "$ENV{CMAKE_RC_COMPILER}" CACHE FILEPATH "RC compiler" FORCE)
endif()

# Use MSVC on Windows by default, else keep existing
if(WIN32)
    message(STATUS "Windows platform detected")
    
    # Force use of MSVC compilers for C/C++, Intel for Fortran only
    if(NOT CMAKE_C_COMPILER)
        find_program(MSVC_C_COMPILER
            NAMES cl.exe
            PATHS
                "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/*/bin/Hostx64/x64"
                "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/*/bin/Hostx64/x64"
                "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC/*/bin/Hostx64/x64"
                "C:/Program Files (x86)/Microsoft Visual Studio/*/VC/Tools/MSVC/*/bin/Hostx64/x64"
            DOC "MSVC C Compiler"
        )
        if(MSVC_C_COMPILER)
            set(CMAKE_C_COMPILER ${MSVC_C_COMPILER} CACHE FILEPATH "C compiler" FORCE)
            message(STATUS "Found MSVC C compiler: ${MSVC_C_COMPILER}")
        endif()
    endif()
    
    if(NOT CMAKE_CXX_COMPILER)
        find_program(MSVC_CXX_COMPILER
            NAMES cl.exe
            PATHS
                "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/*/bin/Hostx64/x64"
                "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/*/bin/Hostx64/x64"
                "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC/*/bin/Hostx64/x64"
                "C:/Program Files (x86)/Microsoft Visual Studio/*/VC/Tools/MSVC/*/bin/Hostx64/x64"
            DOC "MSVC CXX Compiler"
        )
        if(MSVC_CXX_COMPILER)
            set(CMAKE_CXX_COMPILER ${MSVC_CXX_COMPILER} CACHE FILEPATH "CXX compiler" FORCE)
            message(STATUS "Found MSVC C++ compiler: ${MSVC_CXX_COMPILER}")
        endif()
    endif()
    
    # Intel Fortran compiler should be available in PATH after setvars.bat
    # Let CMake find it automatically if not explicitly set
    if(NOT CMAKE_Fortran_COMPILER)
        message(STATUS "CMAKE_Fortran_COMPILER not set, trying to find Intel Fortran compiler")
        
        # Try to find Intel Fortran compiler in common locations
        find_program(INTEL_FORTRAN_COMPILER
            NAMES ifort.exe ifort ifx.exe ifx
            PATHS
                "C:/Program Files (x86)/Intel/oneAPI/compiler/latest/windows/bin/intel64"
                "C:/Program Files (x86)/Intel/oneAPI/compiler/latest/windows/bin"
                "C:/Program Files/Intel/oneAPI/compiler/latest/windows/bin/intel64"
                "C:/Program Files/Intel/oneAPI/compiler/latest/windows/bin"
            DOC "Intel Fortran Compiler"
        )
        
        if(INTEL_FORTRAN_COMPILER)
            message(STATUS "Found Intel Fortran compiler: ${INTEL_FORTRAN_COMPILER}")
            set(CMAKE_Fortran_COMPILER ${INTEL_FORTRAN_COMPILER} CACHE FILEPATH "Fortran compiler" FORCE)
        else()
            message(WARNING "Intel Fortran compiler not found automatically")
            message(STATUS "Please ensure Intel oneAPI HPC Toolkit is installed and CMAKE_Fortran_COMPILER is set")
        endif()
    else()
        message(STATUS "Using specified Fortran compiler: ${CMAKE_Fortran_COMPILER}")
    endif()
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable position independent code for all targets (including subprojects)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Set compiler flags for better compatibility
if(UNIX AND NOT APPLE)
    # Linux specific flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fvisibility=hidden")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fPIC")
    
    # Suppress some warnings that are common in the wsjtx_lib code
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-overflow -Wno-deprecated-declarations")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-stringop-overflow -Wno-deprecated-declarations")
    
    # Force PIC for all targets in subdirectories
    set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Build position independent code" FORCE)
    
    # Additional flags to ensure PIC compilation
    add_compile_options(-fPIC)
    set(CMAKE_SHARED_LIBRARY_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_CXX_FLAGS} -fPIC")
    set(CMAKE_SHARED_LIBRARY_C_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS} -fPIC")
elseif(APPLE)
    # macOS specific flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-unqualified-std-cast-call")
    
    # Set CMake policy for newer Boost versions
    if(POLICY CMP0167)
        cmake_policy(SET CMP0167 NEW)
    endif()
endif()

# Include cmake-js
if(CMAKE_JS_INC)
include_directories(${CMAKE_JS_INC})
endif()

# Add Node.js include path for node_api.h
execute_process(
    COMMAND node -p "require('path').dirname(process.execPath) + '/../include/node'"
    OUTPUT_VARIABLE NODE_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
include_directories(${NODE_INCLUDE_DIR})

# Platform-specific package finding
if(WIN32 AND MSVC)
    # Windows with MSVC: Use vcpkg for FFTW3
    find_package(FFTW3 CONFIG REQUIRED)            # vcpkg provides FFTW3Config.cmake
    # Prefer the single‑precision (float) library that matches wsjtx requirements.
    if(TARGET FFTW3::fftw3f)
        set(FFTW3_LIB_TARGET FFTW3::fftw3f)
    elseif(TARGET FFTW3::fftw3)
        set(FFTW3_LIB_TARGET FFTW3::fftw3)
    else()
        message(FATAL_ERROR "FFTW3 import targets not found – ensure vcpkg installed fftw3[float,threads]:x64-windows")
    endif()

    # Set CMake policy for newer Boost versions
    if(POLICY CMP0167)
        cmake_policy(SET CMP0167 NEW)
    endif()

    # Find Boost
    find_package(Boost QUIET)
    if(Boost_FOUND)
        message(STATUS "Boost found: ${Boost_VERSION}")
        set(BOOST_LIBRARIES ${Boost_LIBRARIES})
    else()
        message(WARNING "Boost not found via find_package, continuing...")
        set(BOOST_LIBRARIES "")
    endif()

    # MSVC doesn't need separate threads library for FFTW
    set(FFTW_HAS_THREADS FALSE)
    set(FFTW_THREADS_LIBRARIES "")
elseif(WIN32)
    # Windows with MSYS2: Use pkg-config like Unix systems
    find_package(PkgConfig REQUIRED)
    
    # Set CMake policy for newer Boost versions
    if(POLICY CMP0167)
        cmake_policy(SET CMP0167 NEW)
    endif()
    
    # Find Boost
    find_package(Boost QUIET)
    if(Boost_FOUND)
        message(STATUS "Boost found: ${Boost_VERSION}")
        set(BOOST_LIBRARIES ${Boost_LIBRARIES})
    else()
        message(WARNING "Boost not found via find_package, continuing...")
        set(BOOST_LIBRARIES "")
    endif()
    
    # Find FFTW3 using pkg-config
    pkg_check_modules(FFTW3F REQUIRED fftw3f)
    
    # MSYS2/MinGW doesn't typically have separate threads library
    set(FFTW_HAS_THREADS FALSE)
    set(FFTW_THREADS_LIBRARIES "")
else()
    # Unix (Linux/macOS): Use pkg-config
    find_package(PkgConfig REQUIRED)
    find_package(Boost REQUIRED)
    set(BOOST_LIBRARIES ${Boost_LIBRARIES})
    
    # Find FFTW3
    pkg_check_modules(FFTW3F REQUIRED fftw3f)
    
    # Check for FFTW3 threads support
    if(UNIX AND NOT APPLE)
        # Linux: search in standard locations
        find_library(FFTW3F_THREADS_LIB 
            NAMES fftw3f_threads libfftw3f_threads
            PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu
        )
    else()
        # macOS: search in homebrew locations
        find_library(FFTW3F_THREADS_LIB 
            NAMES fftw3f_threads libfftw3f_threads
            PATHS /opt/homebrew/lib /usr/local/lib
            NO_DEFAULT_PATH
        )
    endif()
    
    if(FFTW3F_THREADS_LIB)
        message(STATUS "FFTW3 with threads support found: ${FFTW3F_THREADS_LIB}")
        set(FFTW_THREADS_LIBRARIES fftw3f_threads)
        set(FFTW_HAS_THREADS TRUE)
    else()
        message(STATUS "FFTW3 threads not found, using single-threaded version")
        set(FFTW_THREADS_LIBRARIES "")
        set(FFTW_HAS_THREADS FALSE)
    endif()
endif()

# Include directories
if(WIN32 AND MSVC)
    include_directories(
        ${CMAKE_SOURCE_DIR}/wsjtx_lib
        ${CMAKE_SOURCE_DIR}/native
        ${CMAKE_SOURCE_DIR}/node_modules/node-addon-api
    )
else()
include_directories(
    ${CMAKE_SOURCE_DIR}/wsjtx_lib
    ${CMAKE_SOURCE_DIR}/native
    ${CMAKE_SOURCE_DIR}/node_modules/node-addon-api
    ${FFTW3F_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)
endif()

# Define LIBRARIES_FROM_REFERENCES for wsjtx_lib submodule
if(WIN32 AND MSVC)
    # Determine the correct FFTW3 target name for wsjtx_lib
    if(NOT TARGET FFTW3_LIB_TARGET)
        if(TARGET FFTW3::fftw3f)
            set(FFTW3_LIB_TARGET FFTW3::fftw3f)
        elseif(TARGET FFTW3::fftw3)
            set(FFTW3_LIB_TARGET FFTW3::fftw3)
        else()
            message(FATAL_ERROR "No valid FFTW3 target found during link step")
        endif()
    endif()

    set(LIBRARIES_FROM_REFERENCES 
        ${FFTW3_LIB_TARGET}
    )
else()
set(LIBRARIES_FROM_REFERENCES 
    ${FFTW3F_LIBRARIES}
    ${FFTW_THREADS_LIBRARIES}
)
endif()

# Platform-specific library setup
if(APPLE)
    # Find gfortran library path for macOS - support both Intel and ARM64
    set(GFORTRAN_SEARCH_PATHS
        "/opt/homebrew/bin"  # ARM64 (Apple Silicon)
        "/usr/local/bin"     # x64 (Intel)
    )
    
    # Try to find gfortran in common locations
    find_program(GFORTRAN_EXECUTABLE 
        NAMES gfortran gfortran-14 gfortran-13 gfortran-12 gfortran-11
        PATHS ${GFORTRAN_SEARCH_PATHS}
        NO_DEFAULT_PATH
    )
    
    # Also try system PATH as fallback
    if(NOT GFORTRAN_EXECUTABLE)
        find_program(GFORTRAN_EXECUTABLE 
            NAMES gfortran gfortran-14 gfortran-13 gfortran-12 gfortran-11
        )
    endif()
    
    if(GFORTRAN_EXECUTABLE)
        message(STATUS "Found gfortran: ${GFORTRAN_EXECUTABLE}")
        
        # Get library path (but don't set CMAKE_Fortran_COMPILER here to avoid cache conflicts)
        execute_process(
            COMMAND ${GFORTRAN_EXECUTABLE} --print-file-name=libgfortran.dylib
            OUTPUT_VARIABLE GFORTRAN_LIB_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        
        if(GFORTRAN_LIB_PATH AND NOT GFORTRAN_LIB_PATH STREQUAL "libgfortran.dylib")
            get_filename_component(GFORTRAN_LIB_DIR "${GFORTRAN_LIB_PATH}" DIRECTORY)
            
            # Find libgcc_s.1 for nested function support
            execute_process(
                COMMAND ${GFORTRAN_EXECUTABLE} --print-file-name=libgcc_s.1.dylib
                OUTPUT_VARIABLE LIBGCC_S_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            
            if(LIBGCC_S_PATH AND NOT LIBGCC_S_PATH STREQUAL "libgcc_s.1.dylib")
                get_filename_component(LIBGCC_S_DIR "${LIBGCC_S_PATH}" DIRECTORY)
                link_directories(${LIBGCC_S_DIR})
            endif()
            
            link_directories(${GFORTRAN_LIB_DIR})
        endif()
    else()
        message(WARNING "gfortran not found, Fortran compilation may fail")
        message(STATUS "Searched in paths: ${GFORTRAN_SEARCH_PATHS}")
        message(STATUS "You may need to install gfortran: brew install gcc")
    endif()
    
    list(APPEND LIBRARIES_FROM_REFERENCES 
        "-framework Accelerate"
        gfortran 
        gcc_s.1
    )
    if(FFTW_HAS_THREADS)
        list(APPEND LIBRARIES_FROM_REFERENCES fftw3f_threads)
    endif()
elseif(UNIX)
    # Linux specific libraries
    list(APPEND LIBRARIES_FROM_REFERENCES 
        gfortran 
        gcc_s 
        pthread
    )
    if(FFTW_HAS_THREADS)
        list(APPEND LIBRARIES_FROM_REFERENCES fftw3f_threads)
    endif()
elseif(WIN32 AND NOT MSVC)
    # Windows specific libraries - MSYS2/MinGW environment
    list(APPEND LIBRARIES_FROM_REFERENCES 
        gfortran 
        gcc_s 
        pthread
    )
endif()

# Add wsjtx_lib as subdirectory
add_subdirectory(wsjtx_lib)

# Link directories (must be before creating target)
if(NOT WIN32 OR NOT MSVC)
if(NOT WIN32)
    link_directories(${FFTW3F_LIBRARY_DIRS})
    endif()
endif()

# Source files for the Node.js addon
file(GLOB_RECURSE NATIVE_SOURCES "native/*.cpp" "native/*.h")

# Create the Node.js addon
if(MINGW)
    message(STATUS "MINGW: Compiling Node.js addon without CMAKE_JS_SRC")
    add_library(${PROJECT_NAME} SHARED 
        ${NATIVE_SOURCES}
        # Exclude CMAKE_JS_SRC for MinGW
    )
else()
    # For other platforms (MSVC, Linux, macOS)
    add_library(${PROJECT_NAME} SHARED 
        ${NATIVE_SOURCES}
        ${CMAKE_JS_SRC}
    )
endif()

# Set properties for Node.js addon
set_target_properties(${PROJECT_NAME} PROPERTIES 
    PREFIX "" 
    SUFFIX ".node"
    CXX_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
)

# Set Node.js addon include directories
if(CMAKE_JS_INC)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC})
endif()

# Compiler-specific options
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    NAPI_DISABLE_CPP_EXCEPTIONS
    BUILDING_NODE_EXTENSION
)

# Add compile flags
if(NOT WIN32 OR NOT MSVC)
if(NOT WIN32)
    target_compile_options(${PROJECT_NAME} PRIVATE ${FFTW3F_CFLAGS_OTHER})
    endif()
endif()

# Link libraries
if(MINGW)
    message(STATUS "MINGW: Linking Node.js addon. Assuming node.exe/libnode.dll is in PATH or standard lib dirs for MinGW.")
    target_link_libraries(${PROJECT_NAME} 
        wsjtx_lib
        # Potentially add -lnode or the path to libnode.dll.a if needed
    )
elseif(WIN32 AND MSVC)
    # MSVC linking
    if(CMAKE_JS_LIB)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_JS_LIB})
    endif()

    # Determine the correct FFTW3 target name
    if(NOT TARGET FFTW3_LIB_TARGET)
        if(TARGET FFTW3::fftw3f)
            set(FFTW3_LIB_TARGET FFTW3::fftw3f)
        elseif(TARGET FFTW3::fftw3)
            set(FFTW3_LIB_TARGET FFTW3::fftw3)
        else()
            message(FATAL_ERROR "No valid FFTW3 target found during link step")
        endif()
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE
        wsjtx_lib
        ${FFTW3_LIB_TARGET}
        ${BOOST_LIBRARIES}
    )
else()
    target_link_libraries(${PROJECT_NAME} 
        ${CMAKE_JS_LIB}
        wsjtx_lib
    )
endif()

# Platform-specific linking
if(APPLE)
    target_link_libraries(${PROJECT_NAME} 
        "-framework Accelerate"
        ${FFTW3F_LIBRARIES}
        ${BOOST_LIBRARIES}
        gfortran 
        gcc_s.1
    )
    if(FFTW_HAS_THREADS)
        target_link_libraries(${PROJECT_NAME} fftw3f_threads)
    endif()
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} 
        ${FFTW3F_LIBRARIES}
        ${BOOST_LIBRARIES}
        gfortran 
        gcc_s 
        pthread
    )
    if(FFTW_HAS_THREADS)
        target_link_libraries(${PROJECT_NAME} fftw3f_threads)
    endif()
    
    # Linux specific linker flags for Node.js extensions
    # Note: We don't use --no-undefined because Node.js extensions
    # have symbols that are resolved at runtime by the Node.js process
    target_link_options(${PROJECT_NAME} PRIVATE 
        -Wl,--as-needed
    )
elseif(WIN32 AND NOT MSVC)
    # Windows linking - MSYS2/MinGW environment
    target_link_libraries(${PROJECT_NAME} 
        ${FFTW3F_LIBRARIES}
        ${BOOST_LIBRARIES}
        gfortran 
        gcc_s 
        pthread
    )
endif()
