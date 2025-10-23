cmake_minimum_required(VERSION 3.15)

# 禁用 vcpkg Manifest 模式，使用经典模式
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "Disable vcpkg manifest mode" FORCE)
    set(VCPKG_MANIFEST_INSTALL OFF CACHE BOOL "Disable vcpkg manifest install" FORCE)
    message(STATUS "Detected vcpkg toolchain, disabling manifest mode")
endif()

# 构建选项（用于Windows两阶段构建）
option(WSJTX_BUILD_LIBS "Build wsjtx_lib and wsjtx_bridge" ON)
option(WSJTX_BUILD_NODE_ADDON "Build Node.js addon (.node)" ON)

# 初始化项目
project(wsjtx_lib_nodejs LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable position independent code for all targets
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ==================== Node.js 和 cmake-js 配置 ====================

# Include cmake-js
if(CMAKE_JS_INC)
    include_directories(${CMAKE_JS_INC})
endif()

# Add Node.js include path
execute_process(
    COMMAND node -p "require('path').dirname(process.execPath) + '/../include/node'"
    OUTPUT_VARIABLE NODE_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NODE_INCLUDE_DIR AND EXISTS "${NODE_INCLUDE_DIR}")
    include_directories(${NODE_INCLUDE_DIR})
    message(STATUS "Node.js include directory: ${NODE_INCLUDE_DIR}")
endif()

# ==================== 平台特定编译选项 ====================

if(UNIX AND NOT APPLE)
    # Linux
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fvisibility=hidden")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-overflow -Wno-deprecated-declarations")

    add_compile_options(-fPIC)
elseif(APPLE)
    # macOS
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-unqualified-std-cast-call")

    if(POLICY CMP0167)
        cmake_policy(SET CMP0167 NEW)
    endif()
elseif(WIN32)
    if(MSVC)
        # MSVC
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /EHsc /std:c++17")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    else()
        # MinGW
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -Wno-deprecated-declarations")

        # 设置 PKG_CONFIG_PATH
        if(EXISTS "C:/msys64/mingw64/lib/pkgconfig")
            set(ENV{PKG_CONFIG_PATH} "C:/msys64/mingw64/lib/pkgconfig")
            message(STATUS "Set PKG_CONFIG_PATH for MSYS2")
        endif()

        # 添加 MSYS2 路径
        if(EXISTS "C:/msys64/mingw64")
            include_directories("C:/msys64/mingw64/include")
            link_directories("C:/msys64/mingw64/lib")
        endif()

        # 清空 MSVC 特定的链接标志
        set(CMAKE_SHARED_LINKER_FLAGS "")
    endif()

    if(POLICY CMP0167)
        cmake_policy(SET CMP0167 NEW)
    endif()
endif()

# ==================== 依赖库查找 ====================

# Boost (header-only)
if(WIN32 AND NOT MSVC)
    # MinGW: 尝试find_package和手动查找
    find_package(Boost QUIET)
    if(NOT Boost_FOUND)
        find_path(Boost_INCLUDE_DIRS
            NAMES boost/version.hpp
            PATHS /mingw64/include C:/msys64/mingw64/include
            NO_DEFAULT_PATH
        )
        if(Boost_INCLUDE_DIRS)
            message(STATUS "Found Boost headers: ${Boost_INCLUDE_DIRS}")
            set(Boost_FOUND TRUE)
        endif()
    endif()
else()
    # 其他平台
    find_package(Boost REQUIRED)
endif()

# pkg-config 和 FFTW3（仅在构建库时需要）
if(WSJTX_BUILD_LIBS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFTW3F REQUIRED fftw3f)

    # FFTW3 threads support
    if(UNIX AND NOT APPLE)
        # Linux
        find_library(FFTW3F_THREADS_LIB
            NAMES fftw3f_threads
            PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu
        )
    elseif(APPLE)
        # macOS
        find_library(FFTW3F_THREADS_LIB
            NAMES fftw3f_threads
            PATHS /opt/homebrew/lib /usr/local/lib
            NO_DEFAULT_PATH
        )
    elseif(WIN32 AND NOT MSVC)
        # Windows MinGW
        execute_process(
            COMMAND pkg-config --exists fftw3f
            RESULT_VARIABLE PKG_CONFIG_FFTW_RESULT
            OUTPUT_QUIET ERROR_QUIET
        )
        if(PKG_CONFIG_FFTW_RESULT EQUAL 0)
            set(FFTW3F_THREADS_LIB "fftw3f_threads")
        endif()
    endif()

    if(FFTW3F_THREADS_LIB)
        message(STATUS "FFTW3 with threads: ${FFTW3F_THREADS_LIB}")
        set(FFTW_THREADS_LIBRARIES fftw3f_threads)
        set(FFTW_HAS_THREADS TRUE)
    else()
        message(STATUS "FFTW3 threads not found, using single-threaded")
        set(FFTW_THREADS_LIBRARIES "")
        set(FFTW_HAS_THREADS FALSE)
    endif()
endif()

# node-addon-api
set(NODE_ADDON_API_PATHS
    "${CMAKE_SOURCE_DIR}/node_modules/node-addon-api"
    "${CMAKE_SOURCE_DIR}/../../node_modules/node-addon-api"
    "${CMAKE_SOURCE_DIR}/../../../node_modules/node-addon-api"
)

set(NODE_ADDON_API_PATH "")
foreach(path ${NODE_ADDON_API_PATHS})
    if(EXISTS "${path}/napi.h")
        set(NODE_ADDON_API_PATH "${path}")
        message(STATUS "Found node-addon-api: ${NODE_ADDON_API_PATH}")
        break()
    endif()
endforeach()

if(NOT NODE_ADDON_API_PATH)
    message(FATAL_ERROR "Could not find node-addon-api")
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/wsjtx_lib
    ${CMAKE_SOURCE_DIR}/wsjtx_bridge
    ${CMAKE_SOURCE_DIR}/native
    ${NODE_ADDON_API_PATH}
    ${FFTW3F_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# ==================== 库依赖定义 ====================

set(LIBRARIES_FROM_REFERENCES
    ${FFTW3F_LIBRARIES}
    ${FFTW_THREADS_LIBRARIES}
)

# 平台特定库
if(APPLE)
    # macOS: 查找 gfortran
    set(GFORTRAN_SEARCH_PATHS
        "/opt/homebrew/bin"
        "/usr/local/bin"
    )

    find_program(GFORTRAN_EXECUTABLE
        NAMES gfortran gfortran-14 gfortran-13 gfortran-12
        PATHS ${GFORTRAN_SEARCH_PATHS}
    )

    if(GFORTRAN_EXECUTABLE)
        message(STATUS "Found gfortran: ${GFORTRAN_EXECUTABLE}")

        execute_process(
            COMMAND ${GFORTRAN_EXECUTABLE} --print-file-name=libgfortran.dylib
            OUTPUT_VARIABLE GFORTRAN_LIB_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        if(GFORTRAN_LIB_PATH AND NOT GFORTRAN_LIB_PATH STREQUAL "libgfortran.dylib")
            get_filename_component(GFORTRAN_LIB_DIR "${GFORTRAN_LIB_PATH}" DIRECTORY)
            link_directories(${GFORTRAN_LIB_DIR})

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
        endif()
    endif()

    list(APPEND LIBRARIES_FROM_REFERENCES
        "-framework Accelerate"
        gfortran
        gcc_s.1
    )
elseif(UNIX)
    # Linux
    list(APPEND LIBRARIES_FROM_REFERENCES
        gfortran
        gcc_s
        pthread
    )
elseif(WIN32 AND NOT MSVC)
    # Windows MinGW
    list(APPEND LIBRARIES_FROM_REFERENCES
        gfortran
        gcc_s
        pthread
    )
endif()

# ==================== 构建wsjtx_lib 和 wsjtx_bridge ====================

if(WSJTX_BUILD_LIBS)
    message(STATUS "=== Building wsjtx_lib and wsjtx_bridge ===")

    # 启用Fortran
    enable_language(Fortran)
    message(STATUS "Fortran compiler enabled")

    # Linux Fortran flags
    if(UNIX AND NOT APPLE)
        set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fPIC")
    endif()

    # Link directories
    link_directories(${FFTW3F_LIBRARY_DIRS})

    # 添加wsjtx_lib子目录
    add_subdirectory(wsjtx_lib)

    # 添加wsjtx_bridge子目录
    add_subdirectory(wsjtx_bridge)
else()
    message(STATUS "=== Skipping wsjtx_lib and wsjtx_bridge (WSJTX_BUILD_LIBS=OFF) ===")
endif()

# ==================== 构建 Node.js Addon (.node) ====================

if(WSJTX_BUILD_NODE_ADDON)
    message(STATUS "=== Building Node.js addon (.node) ===")

    # 源文件（排除bridge文件，已移动到wsjtx_bridge/）
    file(GLOB_RECURSE NATIVE_SOURCES "native/*.cpp" "native/*.h")

    # 创建Node.js addon
    if(CMAKE_JS_SRC)
        add_library(${PROJECT_NAME} SHARED
            ${NATIVE_SOURCES}
            ${CMAKE_JS_SRC}
        )
    else()
        add_library(${PROJECT_NAME} SHARED
            ${NATIVE_SOURCES}
        )
    endif()

    # 设置属性
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".node"
        CXX_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON
    )

    # 输出目录
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
    )

    # 多配置生成器
    foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER ${cfg} CFG_UPPER)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
            RUNTIME_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
            ARCHIVE_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
        )
    endforeach()

    # RPATH设置
    if(UNIX AND NOT APPLE)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_RPATH "\$ORIGIN"
            INSTALL_RPATH "\$ORIGIN"
        )
    elseif(APPLE)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_RPATH "@loader_path"
            INSTALL_RPATH "@loader_path"
        )
    endif()

    # Include directories
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_JS_INC}
        "${NODE_ADDON_API_PATH}"
        "${CMAKE_CURRENT_SOURCE_DIR}/native"
        "${CMAKE_CURRENT_SOURCE_DIR}/wsjtx_bridge"
    )

    # 编译定义
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NAPI_DISABLE_CPP_EXCEPTIONS
        BUILDING_NODE_EXTENSION
        NAPI_VERSION=4
        NODE_GYP_MODULE_NAME=${PROJECT_NAME}
    )

    # 编译选项（跳过FFTW相关，因为.node不直接链接FFTW）
    if(WSJTX_BUILD_LIBS AND FFTW3F_CFLAGS_OTHER)
        target_compile_options(${PROJECT_NAME} PRIVATE ${FFTW3F_CFLAGS_OTHER})
    endif()

    # 链接Node.js库
    if(CMAKE_JS_LIB)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_JS_LIB})
    endif()

    # 链接库（仅链接Boost，运行时动态加载wsjtx_bridge）
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${BOOST_LIBRARIES}
    )

    message(STATUS ".node extension will dynamically load wsjtx_bridge at runtime")
    message(STATUS "Build complete: wsjtx_lib (static) -> wsjtx_bridge (dynamic) -> .node (loads bridge)")
else()
    message(STATUS "=== Skipping Node.js addon (.node) (WSJTX_BUILD_NODE_ADDON=OFF) ===")
endif()
