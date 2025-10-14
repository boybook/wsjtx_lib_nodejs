# WSJTX-Lib 发布指南

本文档说明如何以 prebuildify 风格发布带有预构建二进制的 npm 包，并由 node-gyp-build 在运行时自动查找与加载。

## 📦 依赖库捆绑策略

### 为什么需要捆绑依赖库？

我们的 Node.js 原生模块依赖这些外部库：
- **FFTW3**: 快速傅里叶变换库
- **Fortran运行时**: gfortran库
- **GCC运行时**: libgcc, libstdc++等

这些库在不同系统上的位置和版本可能不同，为了确保用户安装后能正常使用，我们将必要的依赖库与`.node`文件一起打包。

### 捆绑的库文件

#### Windows (MinGW构建)
```
prebuilds/windows-latest-x64/
├── wsjtx_lib_nodejs.node       # 主模块
├── libfftw3f-3.dll            # FFTW3单精度
├── libfftw3f_threads-3.dll    # FFTW3线程支持
├── libgfortran-5.dll          # Fortran运行时
├── libgcc_s_seh-1.dll         # GCC运行时
├── libwinpthread-1.dll        # 线程支持
├── libstdc++-6.dll            # C++标准库
└── build-info.json            # 构建信息
```

#### Linux
```
prebuilds/ubuntu-latest-x64/
├── wsjtx_lib_nodejs.node       # 主模块
├── libfftw3f.so.3             # FFTW3库
├── libgfortran.so.5           # Fortran运行时
└── build-info.json            # 构建信息
```

#### macOS
```
prebuilds/macos-latest-arm64/
├── wsjtx_lib_nodejs.node       # 主模块
├── libfftw3f.3.dylib          # FFTW3库
├── libgfortran.5.dylib        # Fortran运行时
└── build-info.json            # 构建信息
```

## 🚀 发布流程

### 1. 准备发布

确保所有测试通过并且GitHub Actions构建成功：

```bash
# 检查构建状态
git status
npm test

# 下载GitHub Actions构建的预构建文件
# (从Actions artifacts中下载all-prebuilds.zip并解压到项目根目录)
```

### 2. 验证预构建包

运行打包验证脚本：

```bash
npm run package
```

这会显示类似输出：
```
📦 Packaging prebuilt binaries for npm...

✅ linux-x64:
   • Native module: 1.06 MB
   • Bundled libraries: 2
   • Total package: 3.2 MB

✅ darwin-arm64:
   • Native module: 0.96 MB
   • Bundled libraries: 1
   • Total package: 2.1 MB

✅ windows-latest-x64:
   • Native module: 1.64 MB
   • Bundled libraries: 6
   • Total package: 8.7 MB
   • Additional files: libfftw3f-3.dll, libfftw3f_threads-3.dll, ...

📊 Summary:
   • Valid packages: 3/3
   • Total size: 3.66 MB
```

### 3. 版本管理

更新版本号：

```bash
# 补丁版本 (bug修复)
npm version patch

# 次要版本 (新功能)
npm version minor

# 主要版本 (破坏性更改)
npm version major
```

### 4. 发布到npm

```bash
# 发布 (会自动运行prepublishOnly脚本)
npm publish

# 或者发布beta版本
npm publish --tag beta
```

### 5. 创建GitHub Release

1. 在GitHub上创建新的Release
2. 上传预构建的压缩包供直接下载
3. 包含发布说明和更新日志

## 📋 用户安装体验

### 有预构建包的情况（默认）

用户执行 `npm install wsjtx-lib` 后，运行时代码通过 `node-gyp-build` 在以下位置查找：

1. `prebuilds/<platform>-<arch>/*.node`
2. 回退到 `build/Release/*.node`（本地开发场景）

预构建二进制已随 npm 包内置，安装完成后无需编译与网络下载。

### 无预构建包的情况

如果用户的平台没有对应目录（例如非列出的 CPU/OS 组合或 musl/Alpine），运行时将报错并提示已尝试的搜索路径。

此时用户可选择从源码构建：

1. 安装构建依赖（cmake、gfortran、FFTW3、Boost 等）
2. 执行 `npm run build` 生成 `build/Release/*.node`
3. 运行时会自动从 `build/Release` 回退加载

## 🔧 模块加载逻辑

运行时加载逻辑使用 `node-gyp-build`，并带有回退路径：

```ts
const load = require('node-gyp-build');
const pkgRoot = path.resolve(__dirname, '..', '..');
const binding = load(pkgRoot); // 优先按 prebuildify 规范加载
// 若失败，则回退到 prebuilds/<platform>-<arch>/ 与 build/Release 路径
```

## 📊 包大小优化

虽然捆绑依赖库会增加包大小，但考虑到：

1. **用户体验**: 安装即用，无需配置环境
2. **兼容性**: 避免版本冲突问题
3. **维护成本**: 减少支持请求

这是一个合理的权衡。

对于关注包大小的用户，我们提供了源码编译选项。

## 🚨 注意事项

1. **许可证兼容性**: 确保捆绑的库的许可证与项目兼容
2. **安全更新**: 定期更新依赖库版本
3. **平台测试**: 在目标平台上测试预构建包
4. **版本一致性**: 确保所有平台使用相同版本的依赖库

## 📚 相关工具

- [prebuildify](https://github.com/prebuild/prebuildify): 预构建产物目录规范与工作流
- [node-gyp-build](https://github.com/prebuild/node-gyp-build): 运行时自动加载预构建二进制
- [cmake-js](https://github.com/cmake-js/cmake-js): 使用 CMake 构建 Node.js C++ 扩展
- [GitHub Actions](https://github.com/features/actions): 自动化构建
