# WSJTX-Lib 发布指南

## 发布流程

```bash
# 1. 确保代码在 main 分支且 CI 通过
# 2. bump 版本（自动创建 commit + v* tag）
npm version patch   # bug 修复
npm version minor   # 新功能
npm version major   # 破坏性更改

# 3. 推送代码和 tag
git push && git push --tags
```

推送 tag 后 CI 自动完成：
1. 5 平台构建 + 测试（linux-x64, linux-arm64, darwin-arm64, darwin-x64, win32-x64）
2. 汇总 prebuilds
3. 验证全部平台二进制完整
4. `npm publish` 发布到 npm registry
5. 创建 GitHub Release 并上传各平台预构建压缩包

## 前置条件

在 GitHub repo settings → Secrets and variables → Actions 中添加：
- `NPM_TOKEN`：npm access token（`npm token create` 生成）

## 预构建包结构

```
prebuilds/
├── linux-x64/
│   ├── wsjtx_lib_nodejs.node
│   ├── libwsjtx_core.so
│   ├── libfftw3f.so.3, libgfortran.so.5, ...
│   └── build-info.json
├── darwin-arm64/
│   ├── wsjtx_lib_nodejs.node
│   ├── libwsjtx_core.dylib
│   ├── libfftw3f.3.dylib, libgfortran.5.dylib, ...
│   └── build-info.json
├── win32-x64/
│   ├── wsjtx_lib_nodejs.node
│   ├── wsjtx_core.dll
│   ├── libfftw3f-3.dll, libgfortran-5.dll, ...
│   └── build-info.json
└── ...
```

运行时通过 `node-gyp-build` 自动加载对应平台的预构建二进制。

## 本地调试

```bash
# 查看 prebuilds 状态（不影响发布流程）
npm run package
```

## 用户安装

```bash
npm install wsjtx-lib
# 预构建二进制随包安装，无需编译
```

无对应预构建的平台需从源码构建（需要 cmake、gfortran、fftw3、boost）。
