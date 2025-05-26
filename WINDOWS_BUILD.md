# Windows MSVC 构建指南

本项目已从MinGW工具链迁移到MSVC工具链，以获得更好的Windows兼容性和性能。

## 前置要求

### 1. Visual Studio Build Tools
安装 Visual Studio 2022 Build Tools 或 Visual Studio 2022 Community：
- 下载地址：https://visualstudio.microsoft.com/downloads/
- 确保安装了 "C++ build tools" 工作负载

### 2. Intel oneAPI HPC Toolkit
安装 Intel oneAPI HPC Toolkit 以获得 Fortran 编译器支持：
- 下载地址：https://www.intel.com/content/www/us/en/developer/tools/oneapi/hpc-toolkit.html
- 选择免费版本即可
- 安装后确保运行 `setvars.bat` 设置环境变量

### 3. vcpkg 包管理器
项目使用 vcpkg 管理 C++ 依赖库：
```cmd
git clone https://github.com/Microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install fftw3:x64-windows boost:x64-windows
```

### 4. Node.js
安装 Node.js 18 或更高版本：
- 下载地址：https://nodejs.org/

## 构建步骤

### 1. 设置环境
```cmd
# 设置 Intel oneAPI 环境变量
call "C:\Program Files (x86)\Intel\oneAPI\setvars.bat"

# 验证编译器可用性
where ifort
where ifx
ifort /help
```

### 2. 安装依赖
```cmd
npm ci --ignore-scripts
```

### 3. 构建 TypeScript
```cmd
npm run build:ts
```

### 4. 构建原生模块
```cmd
# 使用专门的 Windows 构建脚本
npm run build:native:win

# 或者手动运行（确保明确指定编译器并避免工具冲突）
for /f "delims=" %i in ('where cl') do set "MSVC_CL_PATH=%i"

# 临时重命名Node.js的rc工具以避免与Windows SDK冲突
if exist "node_modules\.bin\rc.exe" ren "node_modules\.bin\rc.exe" "rc.exe.bak"
if exist "node_modules\.bin\rc" ren "node_modules\.bin\rc" "rc.bak"

npx cmake-js compile -G "NMake Makefiles" --CDCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake --CDCMAKE_C_COMPILER="%MSVC_CL_PATH%" --CDCMAKE_CXX_COMPILER="%MSVC_CL_PATH%" --CDCMAKE_Fortran_COMPILER="C:/Program Files (x86)/Intel/oneAPI/compiler/latest/windows/bin/intel64/ifort.exe"

# 恢复Node.js的rc工具
if exist "node_modules\.bin\rc.exe.bak" ren "node_modules\.bin\rc.exe.bak" "rc.exe"
if exist "node_modules\.bin\rc.bak" ren "node_modules\.bin\rc.bak" "rc"
```

### 5. 运行测试
```cmd
npm test
```

## 故障排除

### Fortran 编译器未找到
确保：
1. Intel oneAPI HPC Toolkit 已正确安装
2. 已运行 `setvars.bat` 设置环境变量
3. `ifort` 或 `ifx` 在 PATH 中可用

### vcpkg 依赖问题
确保：
1. vcpkg 已正确安装和初始化
2. 已安装所需的包：`fftw3:x64-windows` 和 `boost:x64-windows`
3. CMake 工具链文件路径正确

### Visual Studio 集成问题
如果遇到Intel Fortran与Visual Studio集成问题：
1. 使用NMake生成器而不是Visual Studio生成器
2. 确保同时设置了Intel oneAPI和MSVC环境变量
3. 使用完整路径指定Fortran编译器

### 编译器混合问题
如果CMake选择了Intel C/C++编译器（icl.exe）而不是MSVC（cl.exe）：
1. 确保在调用cmake-js之前运行了`vcvars64.bat`
2. 明确指定C/C++编译器路径：
   ```cmd
   --CDCMAKE_C_COMPILER="path/to/cl.exe" --CDCMAKE_CXX_COMPILER="path/to/cl.exe"
   ```
3. 只有Fortran代码使用Intel编译器，C/C++代码应该使用MSVC

### 链接器错误
如果遇到`xilink.exe`或`unknown error`链接问题：
- 这通常是因为Intel链接器与MSVC环境不兼容
- 确保使用MSVC的cl.exe编译器，它会自动使用正确的链接器
- 检查环境变量设置顺序：先设置MSVC，再设置Intel oneAPI

### RC工具冲突
如果遇到`RC Pass 1: command ... failed (exit code 0) with the following output: unknown error`：
- 这是因为CMake使用了Node.js项目中的`rc`工具而不是Windows SDK的`rc.exe`
- 解决方案：临时重命名`node_modules\.bin\rc`和`node_modules\.bin\rc.exe`
- 构建完成后记得恢复这些文件
- 这个问题在GitHub Actions中已自动处理

## 自动化构建

项目的 GitHub Actions 工作流已配置为自动在 Windows 上使用 MSVC 工具链构建。工作流会：
1. 安装 Visual Studio Build Tools
2. 安装 Intel Fortran 编译器
3. 设置 vcpkg 并安装依赖
4. 使用 NMake 生成器构建项目并运行测试

## 与其他平台的兼容性

- **Linux**: 继续使用 GCC + gfortran
- **macOS**: 继续使用 Clang + gfortran (通过 Homebrew)
- **Windows**: 现在使用 MSVC + Intel Fortran (通过 NMake)

这种配置确保了在所有平台上的最佳兼容性和性能。