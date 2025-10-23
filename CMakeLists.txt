cmake_minimum_required(VERSION 3.15)

# 禁用 vcpkg Manifest 模式，使用经典模式
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "Disable vcpkg manifest mode" FORCE)
    set(VCPKG_MANIFEST_INSTALL OFF CACHE BOOL "Disable vcpkg manifest install" FORCE)
    message(STATUS "Detected vcpkg toolchain, disabling manifest mode")
endif()

# 初始化项目，暂时只声明C和C++
# Fortran将在需要时通过enable_language()启用
project(wsjtx_lib_nodejs LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable position independent code for all targets (including subprojects)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Include cmake-js - this must come first to get CMAKE_JS_* variables
if(CMAKE_JS_INC)
    include_directories(${CMAKE_JS_INC})
endif()

# Add Node.js include path for node_api.h (for all platforms)
execute_process(
    COMMAND node -p "require('path').dirname(process.execPath) + '/../include/node'"
    OUTPUT_VARIABLE NODE_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NODE_INCLUDE_DIR AND EXISTS "${NODE_INCLUDE_DIR}")
    include_directories(${NODE_INCLUDE_DIR})
    message(STATUS "Added Node.js include directory: ${NODE_INCLUDE_DIR}")
endif()

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

# Windows平台编译器检测和配置
if(WIN32)
    if(MSVC)
        # MSVC编译模式：仅编译Native扩展层
        message(STATUS "=== Windows MSVC Build Mode ===")
        message(STATUS "Building native extension only (wsjtx_lib.dll must be pre-built with MinGW)")
        set(WSJTX_WINDOWS_MSVC_MODE ON)
        set(WSJTX_SKIP_LIB_BUILD ON)

        # MSVC编译选项
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /EHsc /std:c++17")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi")

        # 使用动态链接的MSVC运行时库 (/MD)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

        message(STATUS "MSVC compiler flags configured")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # MinGW编译模式：完整构建
        message(STATUS "=== Windows MinGW Build Mode ===")
        message(STATUS "Building complete project including wsjtx_lib.dll")
        set(WSJTX_WINDOWS_MSVC_MODE OFF)
        set(WSJTX_SKIP_LIB_BUILD OFF)
    elseif(CMAKE_GENERATOR MATCHES "Visual Studio")
        message(FATAL_ERROR "Visual Studio generator detected without MSVC compiler. Please use either MSVC or MinGW-w64.")
    else()
        message(WARNING "Unknown Windows compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

# Set compiler flags for better compatibility
if(UNIX AND NOT APPLE)
    # Linux specific flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fvisibility=hidden")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    # Fortran flags will be set after enable_language(Fortran) is called
    
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

# Check for FFTW3 threads support - 改进的检测逻辑
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
elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # Windows with MSYS2/MinGW: use pkg-config if available, otherwise force enable
    execute_process(
        COMMAND pkg-config --exists fftw3f
        RESULT_VARIABLE PKG_CONFIG_FFTW_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )
    
    if(PKG_CONFIG_FFTW_RESULT EQUAL 0)
        # FFTW3 found via pkg-config, assume threads support exists
        message(STATUS "FFTW3 found via pkg-config, enabling threads support")
        set(FFTW3F_THREADS_LIB "fftw3f_threads")
        set(FFTW_THREADS_LIBRARIES "fftw3f_threads")
        set(FFTW_HAS_THREADS TRUE)
    else()
        message(STATUS "FFTW3 threads support detection failed, disabling")
        set(FFTW_THREADS_LIBRARIES "")
        set(FFTW_HAS_THREADS FALSE)
    endif()
else()
    # Other Windows: search in mingw64 locations
    find_library(FFTW3F_THREADS_LIB 
        NAMES fftw3f_threads libfftw3f_threads
        PATHS /mingw64/lib D:/msys64/mingw64/lib
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

# Add wsjtx_lib as subdirectory (skip in MSVC mode)
if(NOT WSJTX_SKIP_LIB_BUILD)
    # 启用Fortran语言（仅在需要构建wsjtx_lib时）
    enable_language(Fortran)
    message(STATUS "Fortran compiler enabled for wsjtx_lib build")

    # 设置Fortran编译标志（在启用Fortran后）
    if(UNIX AND NOT APPLE)
        # Linux specific Fortran flags
        set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fPIC")
    endif()

    add_subdirectory(wsjtx_lib)

    # Windows MinGW模式: 构建wsjtx_bridge.dll
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        message(STATUS "=== Building wsjtx_bridge.dll (MinGW) ===")

        # 创建Bridge DLL
        add_library(wsjtx_bridge SHARED
            native/wsjtx_bridge.cpp
            native/wsjtx_bridge.h
        )

        # 链接wsjtx_lib静态库
        target_link_libraries(wsjtx_bridge PRIVATE
            wsjtx_lib
        )

        # 设置编译选项
        set_property(TARGET wsjtx_bridge PROPERTY CXX_STANDARD 17)
        target_compile_definitions(wsjtx_bridge PRIVATE WSJTX_BRIDGE_EXPORTS)

        # 包含子模块头文件目录
        target_include_directories(wsjtx_bridge PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/wsjtx_lib
        )

        # 设置输出属性
        set_target_properties(wsjtx_bridge PROPERTIES
            OUTPUT_NAME "wsjtx_bridge"
            PREFIX ""
            SUFFIX ".dll"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
        )

        # 对于多配置生成器,设置各配置的输出目录
        foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
            string(TOUPPER ${cfg} CFG_UPPER)
            set_target_properties(wsjtx_bridge PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
            )
        endforeach()

        message(STATUS "wsjtx_bridge.dll will be built with MinGW and linked with wsjtx_lib")

        # MinGW阶段只构建wsjtx_bridge.dll,不构建.node扩展
        # .node扩展应该在MSVC阶段构建
        message(STATUS "MinGW stage complete. Skipping .node build (will be built in MSVC stage)")
        return()
    endif()
else()
    message(STATUS "Skipping wsjtx_lib build (MSVC mode)")
endif()

# Link directories (must be before creating target)
link_directories(${FFTW3F_LIBRARY_DIRS})

# ==================== 构建 Node.js Addon (.node) ====================
# 注意: MinGW模式在上面已经return(),不会执行到这里
# 这部分只在 MSVC 模式或 Linux/macOS 平台执行

# Source files for the Node.js addon
file(GLOB_RECURSE NATIVE_SOURCES "native/*.cpp" "native/*.h")

# MSVC模式: 排除wsjtx_bridge.cpp (这是MinGW专用的C API桥接层)
if(WIN32 AND MSVC)
    list(FILTER NATIVE_SOURCES EXCLUDE REGEX ".*wsjtx_bridge\\.(cpp|h)$")
    message(STATUS "MSVC mode: excluding wsjtx_bridge.cpp from .node build")
endif()

# Create the Node.js addon
# 注意: 此时MinGW模式已经return,只剩MSVC/Linux/macOS执行到这里
if(CMAKE_JS_SRC)
    add_library(${PROJECT_NAME} SHARED
        ${NATIVE_SOURCES}
        ${CMAKE_JS_SRC}
    )
    message(STATUS "Building ${PROJECT_NAME} with CMAKE_JS_SRC (delay-load hook)")
else()
    add_library(${PROJECT_NAME} SHARED
        ${NATIVE_SOURCES}
    )
    message(STATUS "Building ${PROJECT_NAME} without CMAKE_JS_SRC")
endif()

# Set properties for Node.js addon
set_target_properties(${PROJECT_NAME} PROPERTIES 
    PREFIX "" 
    SUFFIX ".node"
    CXX_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
)

# Ensure output goes to build/Release across generators and platforms
set(_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Release")
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
)

# Also set configuration-specific output directories for multi-config generators
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} CFG_UPPER)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
        RUNTIME_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
        ARCHIVE_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
    )
endforeach()

# On Linux, ensure the module can find bundled .so beside the .node via RPATH
if(UNIX AND NOT APPLE)
    # Use $ORIGIN so the loader searches the .node's directory
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "\$ORIGIN"
        INSTALL_RPATH "\$ORIGIN"
    )
endif()

# 设置 Node.js 和 node-addon-api 头文件路径
# 注意: MinGW模式已经return,这里只处理MSVC和其他平台
if(WIN32 AND MSVC)
    # Windows MSVC: 需要包含native目录(用于wsjtx_bridge.h声明)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_JS_INC}
        "${NODE_ADDON_API_PATH}"
        "${CMAKE_CURRENT_SOURCE_DIR}/native"
    )
    message(STATUS "MSVC build: including native directory for wsjtx_bridge.h")
else()
    # Linux/macOS: 标准配置
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_JS_INC}
        "${NODE_ADDON_API_PATH}"
    )
    message(STATUS "Non-Windows build: using standard cmake-js headers")
endif()

# Compiler-specific options
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    NAPI_DISABLE_CPP_EXCEPTIONS
    BUILDING_NODE_EXTENSION
    NAPI_VERSION=4
    NODE_GYP_MODULE_NAME=${PROJECT_NAME}
)

# Add compile flags
target_compile_options(${PROJECT_NAME} PRIVATE ${FFTW3F_CFLAGS_OTHER})

# Link Node.js library (MSVC/Linux/macOS only, MinGW已return)
if(CMAKE_JS_LIB)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_JS_LIB})
    message(STATUS "Linking cmake-js library: ${CMAKE_JS_LIB}")
endif()

# Link libraries based on build mode
if(WSJTX_WINDOWS_MSVC_MODE)
    # MSVC mode: don't link wsjtx_lib (will be loaded dynamically)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${BOOST_LIBRARIES}
    )
    message(STATUS "MSVC mode: wsjtx_lib will be loaded dynamically via LoadLibrary")
else()
    # Other platforms: link wsjtx_lib statically
    target_link_libraries(${PROJECT_NAME} PRIVATE
        wsjtx_lib
        ${FFTW3F_LIBRARIES}
        ${BOOST_LIBRARIES}
    )
endif()

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
    # Windows MSVC: 不需要链接Fortran库,会通过wsjtx_bridge.dll动态加载
    message(STATUS "Windows MSVC: Fortran libraries will be loaded via wsjtx_bridge.dll")
endif()
