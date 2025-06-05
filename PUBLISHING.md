# WSJTX-Lib 发布指南

本文档说明如何发布带有预构建二进制文件的 npm 包。

## 📦 依赖库捆绑策略

### 为什么需要捆绑依赖库？

我们的Node.js原生模块依赖这些外部库：
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

### 有预构建包的情况

用户执行 `npm install wsjtx-lib` 时：

1. npm检查是否有匹配的预构建包
2. 如果有，直接使用预构建的`.node`文件和捆绑的依赖库
3. 安装完成，无需编译

### 无预构建包的情况

如果用户的平台没有预构建包：

1. `prebuild-install`会尝试下载预构建包（失败）
2. 回退到源码编译模式
3. 要求用户安装构建依赖（cmake, gcc, gfortran, FFTW3等）
4. 使用cmake-js进行编译

## 🔧 模块加载逻辑

我们的`src/index.ts`中的模块查找逻辑：

```typescript
function findNativeModule(): string {
  const possiblePaths = [
    // 预构建包路径
    path.resolve(__dirname, '..', 'prebuilds', `${process.platform}-${process.arch}`, 'wsjtx_lib_nodejs.node'),
    // 源码构建路径
    path.resolve(__dirname, '..', 'build', 'wsjtx_lib_nodejs.node'),
    path.resolve(__dirname, '..', 'build', 'Release', 'wsjtx_lib_nodejs.node'),
  ];
  // ...
}
```

这确保无论是预构建包还是源码编译，都能正确找到原生模块。

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

- [prebuild-install](https://github.com/prebuild/prebuild-install): 预构建包安装器
- [cmake-js](https://github.com/cmake-js/cmake-js): Node.js C++编译工具
- [GitHub Actions](https://github.com/features/actions): 自动化构建 