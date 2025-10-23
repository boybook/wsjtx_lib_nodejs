# wsjtx_bridge - 跨平台C API桥接库
# 提供统一的C接口来访问wsjtx_lib的C++功能

cmake_minimum_required(VERSION 3.15)

# 此CMakeLists.txt被主项目的CMakeLists.txt通过add_subdirectory()调用
# 继承主项目的配置（编译器、标准、PIC等）

message(STATUS "=== Building wsjtx_bridge (dynamic library) ===")

# 创建动态库
add_library(wsjtx_bridge SHARED
    wsjtx_bridge.cpp
    wsjtx_bridge.h
)

# 链接wsjtx_lib静态库
target_link_libraries(wsjtx_bridge PRIVATE wsjtx_lib)

# 设置C++标准
set_property(TARGET wsjtx_bridge PROPERTY CXX_STANDARD 17)

# Windows: 设置导出宏
if(WIN32)
    target_compile_definitions(wsjtx_bridge PRIVATE WSJTX_BRIDGE_EXPORTS)
endif()

# 包含wsjtx_lib头文件
target_include_directories(wsjtx_bridge PRIVATE
    ${CMAKE_SOURCE_DIR}/wsjtx_lib
)

# 设置输出属性
set_target_properties(wsjtx_bridge PROPERTIES
    OUTPUT_NAME "wsjtx_bridge"
    PREFIX ""  # 不添加lib前缀（即使在Unix上）
    POSITION_INDEPENDENT_CODE ON
)

# 平台特定的输出名称和RPATH设置
if(WIN32)
    # Windows: wsjtx_bridge.dll
    set_target_properties(wsjtx_bridge PROPERTIES SUFFIX ".dll")
elseif(APPLE)
    # macOS: wsjtx_bridge.dylib
    set_target_properties(wsjtx_bridge PROPERTIES SUFFIX ".dylib")

    # 设置install_name，让.node可以在同目录找到这个dylib
    # @loader_path 表示加载此dylib的模块（.node）所在目录
    set_target_properties(wsjtx_bridge PROPERTIES
        INSTALL_NAME_DIR "@loader_path"
        BUILD_WITH_INSTALL_NAME_DIR ON
    )
else()
    # Linux: wsjtx_bridge.so
    set_target_properties(wsjtx_bridge PROPERTIES SUFFIX ".so")

    # 设置RPATH，让.node可以在同目录找到这个.so
    set_target_properties(wsjtx_bridge PROPERTIES
        BUILD_RPATH "\$ORIGIN"
        INSTALL_RPATH "\$ORIGIN"
    )
endif()

# 输出到与.node相同的目录
set_target_properties(wsjtx_bridge PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
)

# 为多配置生成器设置各配置的输出目录
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} CFG_UPPER)
    set_target_properties(wsjtx_bridge PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
        RUNTIME_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
        ARCHIVE_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}/${cfg}"
    )
endforeach()

message(STATUS "wsjtx_bridge will be built as a dynamic library")
message(STATUS "  - Output directory: ${CMAKE_BINARY_DIR}/Release")
if(WIN32)
    message(STATUS "  - Library name: wsjtx_bridge.dll")
elseif(APPLE)
    message(STATUS "  - Library name: wsjtx_bridge.dylib")
else()
    message(STATUS "  - Library name: wsjtx_bridge.so")
endif()
