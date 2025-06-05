# 构建说明

本项目是一个 Node.js 原生扩展，支持在 Linux、macOS 和 Windows 平台上构建。

## 系统要求

### 通用要求
- Node.js >= 16.0.0
- CMake >= 3.15
- FFTW3 库（单精度浮点版本）
- Boost 库
- Fortran 编译器

### 平台特定要求

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y \
  cmake \
  build-essential \
  gfortran \
  libfftw3-dev \
  libboost-all-dev \
  pkg-config
```

#### macOS
```bash
brew install cmake fftw boost gcc pkg-config
```

#### Windows (MSYS2/MinGW-w64)
安装 MSYS2，然后在 MSYS2 MINGW64 终端中运行：
```bash
pacman -S --needed \
  base-devel \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-pkg-config \
  mingw-w64-x86_64-fftw \
  mingw-w64-x86_64-boost \
  mingw-w64-x86_64-gcc-fortran
```

**注意**：在 GitHub Actions 中，我们使用系统安装的 Node.js 配合 MSYS2 的编译工具链。如果在本地开发，你可以选择：
1. 在 MSYS2 中安装 Node.js：`pacman -S mingw-w64-x86_64-nodejs`
2. 或者使用系统 Node.js 配合 MSYS2 编译工具

## 构建步骤

### 通用步骤

1. 克隆仓库并初始化子模块：
```bash
git clone --recursive https://github.com/boybook/wsjtx_lib_nodejs.git
cd wsjtx-lib-nodejs
```

2. 安装 npm 依赖：
```bash
npm ci
```

3. 构建项目：
```bash
npm run build
```

### 平台特定构建

#### Linux/macOS
```bash
npm run build:native
npm run build:ts
```

#### Windows (MSYS2)
在 MSYS2 MINGW64 终端中：
```bash
npm run build:native:win
npm run build:ts
```

## 测试

运行基础测试：
```bash
npm test
```

运行完整测试：
```bash
npm run test:full
```

## 故障排除

### Windows 常见问题

1. **CMake 检测到 MSVC 而不是 MinGW**
   - 确保在 MSYS2 MINGW64 终端中运行构建命令
   - 检查环境变量 PATH 中没有 Visual Studio 的路径干扰
   - 使用 `cmake-js compile -- -G "MinGW Makefiles"` 显式指定生成器

2. **找不到 gfortran**
   - 确保安装了 `mingw-w64-x86_64-gcc-fortran`
   - 运行 `which gfortran` 验证编译器路径

3. **Node.js 头文件找不到**
   - 使用 MSYS2 安装的 Node.js：`pacman -S mingw-w64-x86_64-nodejs`
   - 或确保系统 Node.js 安装正确

4. **pkg-config 找不到库**
   - 确保 PKG_CONFIG_PATH 包含 MSYS2 路径：`export PKG_CONFIG_PATH="/mingw64/lib/pkgconfig"`
   - 验证库是否正确安装：`pkg-config --list-all | grep fftw`

5. **npx 命令找不到**
   - 如果使用系统 Node.js，确保 npm 全局安装目录在 PATH 中
   - 或在 MSYS2 中安装 Node.js：`pacman -S mingw-w64-x86_64-nodejs`

### macOS 常见问题

1. **gfortran 找不到**
   - 安装 GCC：`brew install gcc`
   - 检查 gfortran 版本：`gfortran --version`

2. **Boost 库找不到**
   - 重新安装 Boost：`brew reinstall boost`

### Linux 常见问题

1. **FFTW3 开发文件缺失**
   - 安装开发包：`sudo apt-get install libfftw3-dev`

2. **交叉编译 ARM64 失败**
   - 安装交叉编译工具链：`sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu gfortran-aarch64-linux-gnu`

## GitHub Actions 构建

项目配置了自动化 CI/CD 流程，支持：
- Linux x64
- macOS ARM64
- Windows x64 (使用 MSYS2/MinGW-w64)

构建产物将会自动上传到 GitHub Actions Artifacts。 