cmake_minimum_required(VERSION 3.15)

# 禁用 vcpkg Manifest 模式，使用经典模式
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "Disable vcpkg manifest mode" FORCE)
    set(VCPKG_MANIFEST_INSTALL OFF CACHE BOOL "Disable vcpkg manifest install" FORCE)
    message(STATUS "Detected vcpkg toolchain, disabling manifest mode")
endif()


project(wsjtx_lib_nodejs LANGUAGES C CXX Fortran)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable position independent code for all targets (including subprojects)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Include cmake-js - this must come first to get CMAKE_JS_* variables
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

# **关键修复**: Windows + MinGW 环境检测并清除 MSVC 特定标志
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(STATUS "Detected MinGW-w64 environment on Windows")
    
    # 设置 PKG_CONFIG_PATH 以便查找 MSYS2 库
    if(EXISTS "C:/msys64/mingw64/lib/pkgconfig")
        set(ENV{PKG_CONFIG_PATH} "C:/msys64/mingw64/lib/pkgconfig")
        message(STATUS "Set PKG_CONFIG_PATH to C:/msys64/mingw64/lib/pkgconfig")
    endif()
    
    # 添加 MSYS2 的 include 和 library 路径
    if(EXISTS "C:/msys64/mingw64")
        include_directories("C:/msys64/mingw64/include")
        link_directories("C:/msys64/mingw64/lib")
        message(STATUS "Added MSYS2 include and library paths")
    endif()
    
    # **关键**: 清空 cmake-js 可能添加的 MSVC 专属链接标志 (/DELAYLOAD:NODE.EXE)
    set(CMAKE_SHARED_LINKER_FLAGS "")
    message(STATUS "Cleared MSVC-specific linker flags for MinGW compatibility")
endif()

# Force avoid MSVC detection on Windows when using MinGW
if(WIN32 AND MSVC)
    message(FATAL_ERROR "MSVC compiler detected. This project requires MinGW-w64. Please use MSYS2/MinGW-w64 environment and specify -G \"MinGW Makefiles\"")
endif()

# Additional check for cmake-js on Windows - prevent Visual Studio generator
if(WIN32 AND CMAKE_GENERATOR MATCHES "Visual Studio")
    message(FATAL_ERROR "Visual Studio generator detected. This project requires MinGW-w64. Please set CMAKE_GENERATOR=MinGW Makefiles")
endif()

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
elseif(WIN32)
    # Windows with MinGW-w64 specific flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
    
    # Set CMake policy for newer Boost versions
    if(POLICY CMP0167)
        cmake_policy(SET CMP0167 NEW)
    endif()
endif()

# Use pkg-config for all platforms (including Windows with MSYS2)
find_package(PkgConfig REQUIRED)

# Set CMake policy for newer Boost versions
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# Find Boost - use different methods based on platform
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # Windows with MinGW-w64: try both find_package and manual detection
    find_package(Boost QUIET)
    if(NOT Boost_FOUND)
        # Manual detection for MSYS2 Boost
        message(STATUS "Boost not found via find_package, trying manual detection...")
        
        # Look for Boost in MSYS2 locations
        find_path(Boost_INCLUDE_DIRS 
            NAMES boost/version.hpp
            PATHS /mingw64/include C:/msys64/mingw64/include
            NO_DEFAULT_PATH
        )
        
        if(Boost_INCLUDE_DIRS)
            message(STATUS "Found Boost headers at: ${Boost_INCLUDE_DIRS}")
            set(Boost_FOUND TRUE)
            # For header-only libraries, we don't need to link anything
            set(BOOST_LIBRARIES "")
        else()
            message(WARNING "Boost headers not found")
            set(BOOST_LIBRARIES "")
        endif()
    else()
        message(STATUS "Boost found via find_package: ${Boost_VERSION}")
        set(BOOST_LIBRARIES ${Boost_LIBRARIES})
    endif()
else()
    # Other platforms: use standard find_package
    find_package(Boost REQUIRED)
    set(BOOST_LIBRARIES ${Boost_LIBRARIES})
endif()

# Find FFTW3 using pkg-config
pkg_check_modules(FFTW3F REQUIRED fftw3f)

# Check for FFTW3 threads support
if(UNIX AND NOT APPLE)
    # Linux: search in standard locations
    find_library(FFTW3F_THREADS_LIB 
        NAMES fftw3f_threads libfftw3f_threads
        PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu
    )
elseif(APPLE)
    # macOS: search in homebrew locations
    find_library(FFTW3F_THREADS_LIB 
        NAMES fftw3f_threads libfftw3f_threads
        PATHS /opt/homebrew/lib /usr/local/lib
        NO_DEFAULT_PATH
    )
else()
    # Windows with MSYS2: search in mingw64 locations
    find_library(FFTW3F_THREADS_LIB 
        NAMES fftw3f_threads libfftw3f_threads
        PATHS /mingw64/lib
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

# Auto-detect node-addon-api path (monorepo vs standalone)
set(NODE_ADDON_API_PATHS
    "${CMAKE_SOURCE_DIR}/node_modules/node-addon-api"           # Standalone project
    "${CMAKE_SOURCE_DIR}/../../node_modules/node-addon-api"    # Monorepo (2 levels up)
    "${CMAKE_SOURCE_DIR}/../../../node_modules/node-addon-api" # Monorepo (3 levels up)
)

set(NODE_ADDON_API_PATH "")
foreach(path ${NODE_ADDON_API_PATHS})
    if(EXISTS "${path}/napi.h")
        set(NODE_ADDON_API_PATH "${path}")
        message(STATUS "Found node-addon-api at: ${NODE_ADDON_API_PATH}")
        break()
    endif()
endforeach()

if(NOT NODE_ADDON_API_PATH)
    message(FATAL_ERROR "Could not find node-addon-api. Searched paths: ${NODE_ADDON_API_PATHS}")
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/wsjtx_lib
    ${CMAKE_SOURCE_DIR}/native
    ${NODE_ADDON_API_PATH}
    ${FFTW3F_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# Define LIBRARIES_FROM_REFERENCES for wsjtx_lib submodule
set(LIBRARIES_FROM_REFERENCES 
    ${FFTW3F_LIBRARIES}
    ${FFTW_THREADS_LIBRARIES}
)

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
elseif(WIN32)
    # Windows with MinGW-w64 specific libraries
    list(APPEND LIBRARIES_FROM_REFERENCES 
        gfortran 
        gcc_s 
        pthread
    )
    if(FFTW_HAS_THREADS)
        list(APPEND LIBRARIES_FROM_REFERENCES fftw3f_threads)
    endif()
endif()

# Add wsjtx_lib as subdirectory
add_subdirectory(wsjtx_lib)

# Link directories (must be before creating target)
link_directories(${FFTW3F_LIBRARY_DIRS})

# Source files for the Node.js addon
file(GLOB_RECURSE NATIVE_SOURCES "native/*.cpp" "native/*.h")

# Create the Node.js addon with MinGW-specific handling
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # MinGW build: exclude Windows delay load hook
    add_library(${PROJECT_NAME} SHARED 
        ${NATIVE_SOURCES}
    )
    message(STATUS "MinGW build: excluding CMAKE_JS_SRC (Windows delay load hook)")
else()
    # Other platforms: include CMAKE_JS_SRC if available
    if(CMAKE_JS_SRC)
        add_library(${PROJECT_NAME} SHARED 
            ${NATIVE_SOURCES}
            ${CMAKE_JS_SRC}
        )
        message(STATUS "Non-MinGW build: including CMAKE_JS_SRC for delay-load hook")
    else()
        add_library(${PROJECT_NAME} SHARED 
            ${NATIVE_SOURCES}
        )
    endif()
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
target_compile_options(${PROJECT_NAME} PRIVATE ${FFTW3F_CFLAGS_OTHER})

# Link libraries with MinGW-specific handling
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # MinGW: don't link CMAKE_JS_LIB as it may cause issues
    message(STATUS "MinGW build: skipping CMAKE_JS_LIB linking")
else()
    # Other platforms: link CMAKE_JS_LIB if available
    if(CMAKE_JS_LIB)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_JS_LIB})
    endif()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    wsjtx_lib
    ${FFTW3F_LIBRARIES}
    ${BOOST_LIBRARIES}
)

# Platform-specific linking
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Accelerate"
        gfortran 
        gcc_s.1
    )
    if(FFTW_HAS_THREADS)
        target_link_libraries(${PROJECT_NAME} PRIVATE fftw3f_threads)
    endif()
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        gfortran 
        gcc_s 
        pthread
    )
    if(FFTW_HAS_THREADS)
        target_link_libraries(${PROJECT_NAME} PRIVATE fftw3f_threads)
    endif()
    
    # Linux specific linker flags for Node.js extensions
    # Note: We don't use --no-undefined because Node.js extensions
    # have symbols that are resolved at runtime by the Node.js process
    target_link_options(${PROJECT_NAME} PRIVATE 
        -Wl,--as-needed
    )
elseif(WIN32)
    # Windows with MinGW-w64 linking
    target_link_libraries(${PROJECT_NAME} PRIVATE
        gfortran 
        gcc_s 
        pthread
    )
    if(FFTW_HAS_THREADS)
        target_link_libraries(${PROJECT_NAME} PRIVATE fftw3f_threads)
    endif()
endif()
