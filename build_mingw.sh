#!/bin/bash

echo "=== Building with MSYS2/MinGW-w64 toolchain ==="
echo "Using improved CMakeLists.txt with MinGW-specific MSVC flag clearing"

# 1. 验证基础工具是否可用
echo "=== Verifying essential tools ==="
echo "Checking basic tools..."
which cmake && cmake --version || { echo "❌ CMake not found"; exit 1; }
which gcc && gcc --version || { echo "❌ GCC not found"; exit 1; }
which g++ && g++ --version || { echo "❌ G++ not found"; exit 1; }
which gfortran && gfortran --version || { echo "❌ gfortran not found"; exit 1; }
which pkg-config && pkg-config --version || { echo "❌ pkg-config not found"; exit 1; }
which node && node --version || { echo "❌ Node.js not found"; exit 1; }
which npm && npm --version || { echo "❌ npm not found"; exit 1; }
which npx && npx --version || { echo "❌ npx not found"; exit 1; }

# 2. 验证 Node.js 环境
echo -e "\n=== Verifying Node.js environment ==="
echo "Node.js version: $(node --version)"
echo "npm version: $(npm --version)"
echo "npx version: $(npx --version)"

# 3. 检查 node-addon-api 头文件
echo -e "\n=== Checking node-addon-api headers ==="
NODE_ADDON_API_PATH="node_modules/node-addon-api"
if [ -d "$NODE_ADDON_API_PATH" ]; then
    echo "✅ node-addon-api package found"
    if [ -f "$NODE_ADDON_API_PATH/napi.h" ]; then
        echo "✅ napi.h found"
    else
        echo "❌ napi.h not found"
        exit 1
    fi
else
    echo "❌ node-addon-api package not found"
    exit 1
fi

# 4. 设置环境变量强制使用 MinGW 工具链
echo -e "\n=== Setting up MinGW environment ==="
export PKG_CONFIG_PATH="/mingw64/lib/pkgconfig"
export CMAKE_PREFIX_PATH="/mingw64"
export CC="gcc"
export CXX="g++"
export FC="gfortran"
export CMAKE_MAKE_PROGRAM="mingw32-make"

# 5. 禁用 Visual Studio 检测
echo "Disabling Visual Studio detection..."
unset VSINSTALLDIR
unset VCINSTALLDIR
unset WindowsSDKDir
unset VCPKG_ROOT

# 6. 打印环境信息
echo -e "\n=== Environment variables ==="
echo "PKG_CONFIG_PATH=$PKG_CONFIG_PATH"
echo "CMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH"
echo "CC=$CC"
echo "CXX=$CXX"
echo "FC=$FC"
echo "CMAKE_MAKE_PROGRAM=$CMAKE_MAKE_PROGRAM"

# 7. 验证 FFTW3 库
echo -e "\n=== Checking FFTW3 availability ==="
if pkg-config --exists fftw3f; then
    echo "✅ FFTW3F found"
    echo "FFTW3F libraries: $(pkg-config --libs fftw3f)"
else
    echo "❌ FFTW3F not found"
    exit 1
fi

# 8. 验证 Boost 安装
echo -e "\n=== Checking Boost installation ==="
if [ -d "/mingw64/include/boost" ]; then
    echo "✅ Boost headers found"
    ls -la /mingw64/include/boost/ | head -5
else
    echo "❌ Boost headers not found"
    exit 1
fi

if ls /mingw64/lib/libboost* >/dev/null 2>&1; then
    echo "✅ Boost libraries found"
    ls -la /mingw64/lib/libboost* | head -3
else
    echo "❌ Boost libraries not found"
    exit 1
fi

# 9. 清理构建目录
echo -e "\n=== Cleaning build directory ==="
rm -rf build/

# 10. 运行构建
echo -e "\n=== Starting build process ==="
echo "Running cmake-js with MinGW..."
npx cmake-js compile --arch=x64 \
            --generator="MinGW Makefiles" \
            --no-retry \
            --verbose