# 预构建二进制依赖打包与路径修正（全平台）

目标

- 为 Node 原生扩展生成可直接分发的预构建（prebuilds），并随包携带必要的运行库，做到“开箱即用”。
- 采用成熟的现成工具（dylibbundler、patchelf、objdump/ldd），在 CI 中自动完成依赖复制与加载路径修正，尽量减少自定义脚本。
- 避免把系统自带的基础库（如 macOS 的系统框架、Linux 的 glibc）打包进来，保持包体和合规性。

目录结构

- 预构建产物放在 `prebuilds/<platform>-<arch>/` 下：
  - **所有平台统一**：将三方运行库与 `.node` 文件放在同级目录，通过 `@loader_path/`（macOS）、`$ORIGIN`（Linux）或默认加载器（Windows）加载。
  - 这样可以避免嵌套路径问题，简化配置，三平台保持一致。

通用原则

- 只随包携带非系统库：
  - 常见需要携带：`libfftw3f(.threads)`、`libgfortran`、`libquadmath`、（Linux 上可选）`libstdc++`、`libgcc_s`。
  - 不携带：macOS 系统框架（如 Accelerate）、Linux glibc、Windows 系统 DLL。
- 加载路径改写：
  - macOS：改为 `@loader_path/<name>.dylib`（同级目录）。
  - Linux：设置（或覆盖）RPATH/RUNPATH 为 `$ORIGIN`（同级目录）。
  - Windows：无需 rpath，DLL 与 `.node` 同级即可被加载器找到。
- 验证：在 CI 中使用 `otool -L`（macOS）、`ldd`（Linux）、`objdump -p`（Windows）输出检查结果，确保无绝对路径残留且依赖可解析。

平台做法与所用工具

1) macOS（使用 `dylibbundler`）

- 依赖：`brew install dylibbundler`
- 执行要点：
  - 仅声明 Homebrew 的搜索目录，避免复制系统库：
    - `/opt/homebrew/opt/fftw/lib`、`/opt/homebrew/opt/gcc@14/lib/gcc/14`（Intel 则 `/usr/local/opt/...`）。
  - 目标目录：`prebuilds/darwin-*`（与 .node 同级），前缀：`@loader_path/`。
- 示例命令：
```
dylibbundler \
  -x prebuilds/darwin-arm64/wsjtx_lib_nodejs.node \
  -d prebuilds/darwin-arm64 \
  -p "@loader_path/" \
  -s /opt/homebrew/opt/fftw/lib \
  -s /opt/homebrew/opt/gcc/lib/gcc/current \
  -s /opt/homebrew/opt/gcc@14/lib/gcc/14 \
  -s /usr/local/opt/fftw/lib \
  -s /usr/local/opt/gcc/lib/gcc/current \
  -s /usr/local/opt/gcc@14/lib/gcc/14 \
  -b
```
- 验证：
```
otool -L prebuilds/darwin-arm64/wsjtx_lib_nodejs.node
```
应看到 `@loader_path/libfftw3f.3.dylib` 等（同级目录），无 Homebrew 绝对路径。

2) Linux（使用 `patchelf` + `ldd`）

- 依赖：`apt-get install -y patchelf`（以及 `ldd`）
- 复制依赖：用 `ldd` 列出 `.node` 的依赖，筛选并复制目标库到与 `.node` 同级目录：
  - 包括 `libfftw3f*`、`libgfortran*`、`libquadmath*`、`libgcc_s*`（可酌情包含 `libstdc++*`）。
  - 排除 glibc（`libc`, `libm`, `libpthread`, `ld-linux` 等）。
- 设置 RPATH：
```
patchelf --set-rpath '$ORIGIN' prebuilds/linux-*/wsjtx_lib_nodejs.node
```
- 验证：
```
ldd prebuilds/linux-*/wsjtx_lib_nodejs.node | grep -v 'linux-vdso\|ld-linux\|libc\|libm\|libpthread\|libdl'
```
应无 "not found"，关键库解析到同目录。

3) Windows（MinGW/MSYS2 工具链）

- 列出依赖：`objdump -p build/Release/wsjtx_lib_nodejs.node | grep "DLL Name"`
- 复制到与 `.node` 同级目录（Windows 加载器不搜索子目录）：
  - 常见 DLL：`libfftw3f-3.dll`, `libfftw3f_threads-3.dll`, `libgfortran-5.dll`, `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`。
- 验证：运行 `npm test` 或在 CI 中尝试直接 `require()` 加载。

CI 集成（简化、可重复）

- **所有平台统一方案**：将依赖库与 `.node` 文件放在同级目录
- macOS：用 `dylibbundler` 自动复制并改写到 `prebuilds/darwin-*/`；输出 `otool -L` 结果到日志。
- Linux：安装 `patchelf`；复制 `ldd` 识别出的目标库到 `prebuilds/linux-*/` 并 `patchelf --set-rpath '$ORIGIN'`；输出 `ldd` 结果。
- Windows：用 `objdump` 枚举 DLL 并从 MinGW 目录复制到与 `.node` 同级；输出依赖列表。

发布前校验

- 生成 `prebuilds/*/` 后：
  - macOS：`otool -L` 检查路径应为 `@loader_path/<lib>.dylib`（同级目录）。
  - Linux：`ldd` 无 "not found"，并且关键库解析到同目录。
  - Windows：`objdump -p` 的 DLL 在同级目录存在。

注意事项

- macOS 改写依赖会使原签名失效，dylibbundler 会进行 ad-hoc 签名；若集成到 Electron/App，需对最终产物统一签名/公证。
- Linux 不要尝试捆绑或静态链接 glibc；可考虑 `-static-libstdc++ -static-libgcc` 减少 .so 数量。
- **所有平台统一采用同级目录方案**，避免嵌套路径导致的 `@loader_path/native/native/` 等问题（详见下方"常见陷阱"）。

## 常见陷阱与解决方案

### ⚠️ 陷阱1: dylibbundler 修改了错误的文件

**问题描述**:
在 GitHub Actions 或 CI 环境中，可能出现以下情况：
1. 日志显示 `otool -L build/Release/wsjtx_lib_nodejs.node` 的依赖路径已正确修改为 `@loader_path/xxx.dylib`
2. 但下载到本地的 `prebuilds/darwin-arm64/wsjtx_lib_nodejs.node` 仍然是绝对路径（如 `/opt/homebrew/...`）

**根本原因**:
构建脚本的执行顺序错误：
```bash
# ❌ 错误的顺序
cp build/Release/wsjtx_lib_nodejs.node "$TARGET_DIR/"  # 先复制
NODE_FILE="build/Release/wsjtx_lib_nodejs.node"         # 指向原始文件
dylibbundler -x "$NODE_FILE" ...                        # 修改的是原始文件
# 结果: prebuilds/ 中的文件是修改前复制的,从未被 dylibbundler 处理过!
```

**解决方案**:
确保 `dylibbundler` 处理的是**最终要打包的文件**：
```bash
# ✅ 正确的顺序
cp build/Release/wsjtx_lib_nodejs.node "$TARGET_DIR/"  # 先复制
NODE_FILE="$TARGET_DIR/wsjtx_lib_nodejs.node"          # 指向目标文件
dylibbundler -x "$NODE_FILE" ...                        # 修改目标文件
# 结果: prebuilds/ 中的文件就是被 dylibbundler 处理过的!
```

**验证方法**:
```bash
# 在 CI 日志中,应该看到处理的是 prebuilds/ 中的文件
echo "Processing: $NODE_FILE"  # 应该输出 prebuilds/darwin-arm64/wsjtx_lib_nodejs.node

# 验证时检查的也应该是 prebuilds/ 中的文件
otool -L "$NODE_FILE"  # 而不是 build/Release/wsjtx_lib_nodejs.node
```

### ⚠️ 陷阱2: 嵌套子目录导致的路径解析错误

**问题描述**:
在 Electron 应用中加载模块时报错：
```
Error: dlopen(...): Library not loaded: @loader_path/native/libfftw3f.3.dylib
Referenced from: .../native/libfftw3f_threads.3.dylib
Reason: tried: '.../native/native/libfftw3f.3.dylib' (no such file)
```

**根本原因**:
使用 `native/` 子目录时，dylib 之间的依赖路径解析出现嵌套：

```
目录结构:
prebuilds/darwin-arm64/
├── wsjtx_lib_nodejs.node
└── native/
    ├── libfftw3f.3.dylib
    └── libfftw3f_threads.3.dylib

libfftw3f_threads.3.dylib 的依赖:
  @loader_path/native/libfftw3f.3.dylib

解析过程:
  @loader_path = native/ (libfftw3f_threads.3.dylib 所在目录)
  @loader_path/native/libfftw3f.3.dylib = native/native/libfftw3f.3.dylib ❌
```

这是因为 `dylibbundler` 在处理时：
- 修改 `.node` 文件的依赖 → `@loader_path/native/xxx.dylib` ✅ 正确
- 修改 `native/` 目录下的 dylib 依赖 → **也是** `@loader_path/native/xxx.dylib` ❌ 错误!

但当 dylib **自己在 native/ 目录**时，`@loader_path` 就是 `native/` 本身，再加上 `/native/` 就变成了 `native/native/`。

**解决方案**:
**不使用子目录**，将所有 dylib 与 `.node` 文件放在同级目录：

```bash
# macOS
dylibbundler -x "$TARGET_DIR/wsjtx_lib_nodejs.node" \
             -d "$TARGET_DIR" \              # 同级目录
             -p "@loader_path/" \            # 不加 native/
             -s /opt/homebrew/opt/fftw/lib \
             -b

# Linux
patchelf --set-rpath '$ORIGIN' "$TARGET_DIR/wsjtx_lib_nodejs.node"  # 不加 /native
```

这样所有依赖都是 `@loader_path/xxx.dylib`，解析到同目录，简单清晰。

### ⚠️ 陷阱3: Windows prebuildify spawn EINVAL 错误

**问题描述**:
在 Windows CI 环境中运行 `prebuildify --napi --strip` 时失败：
```
Error: spawn EINVAL
    at ChildProcess.spawn (node:internal/child_process:414:11)
    at Object.spawn (node:child_process:761:9)
    at prebuildify/index.js:212:22
  errno: -4071,
  code: 'EINVAL',
  syscall: 'spawn'
```

**根本原因**:
1. Windows 上的 `node-gyp` 是 `.cmd` 批处理文件，不是可执行程序
2. Node.js 的 `child_process.spawn()` 默认**不能**直接执行 `.cmd` 文件
3. 需要通过 `cmd.exe /c` 包装执行，或设置 `{shell: true}` 选项
4. `prebuildify` 内部使用 `proc.spawn(opts.nodeGyp, args, { stdio: 'inherit' })` 调用 node-gyp
5. 没有设置 `shell: true`，导致在 Windows 上抛出 `EINVAL` 错误

**技术细节**:
```javascript
// prebuildify 内部代码（简化）
var child = proc.spawn(opts.nodeGyp, args, {
  cwd: opts.cwd,
  env: opts.env,
  stdio: opts.quiet ? 'ignore' : 'inherit'
  // ❌ 缺少 shell: true
});

// 在 Windows 上，opts.nodeGyp 通常是 "node-gyp.cmd"
// spawn 无法直接执行 .cmd 文件，导致 EINVAL 错误
```

**为什么常规方案无效**:

尝试 1 - 在 workflow 中设置 shell:
```yaml
# ❌ 无效：只影响 workflow 步骤，不影响 prebuildify 内部的 spawn
- name: Generate prebuilds
  shell: cmd
  run: npm run prebuild
```

尝试 2 - 设置环境变量:
```yaml
# ❌ 无效：只改变路径，不解决 spawn 无法执行 .cmd 的问题
- run: |
    set PREBUILD_NODE_GYP=node-gyp.cmd
    npx prebuildify --napi --strip
```

尝试 3 - 直接调用 node-gyp:
```yaml
# ⚠️ 部分有效：绕过 prebuildify，但失去其便利性
- run: |
    node-gyp rebuild
    # 需要手动创建 prebuilds 目录结构
```

**正确解决方案**:
通过 **monkey-patching** `child_process.spawn` 来拦截和修复 `.cmd` 调用。

创建 `scripts/run-prebuildify.js`:
```javascript
#!/usr/bin/env node
// Windows-safe prebuildify runner that wraps child_process.spawn for node-gyp
const os = require('os');
const path = require('path');
const cp = require('child_process');

// 保存原始的 spawn 函数
const origSpawn = cp.spawn;

// 替换 spawn 函数
cp.spawn = function patchedSpawn(command, args, options) {
  if (process.platform === 'win32') {
    // 检测是否是 .cmd 文件或 node-gyp
    const isCmd = /\.cmd$/i.test(command) || /node-gyp(\.js)?$/i.test(command);
    if (isCmd) {
      // 通过 cmd.exe 包装执行
      const cmdExe = process.env.ComSpec || 'cmd.exe';
      const cmdline = [command].concat(args || []).join(' ');
      return origSpawn(cmdExe, ['/d', '/s', '/c', cmdline],
        Object.assign({ stdio: 'inherit' }, options, { shell: false }));
    }
  }
  // 非 Windows 或非 .cmd 文件，使用原始 spawn
  return origSpawn(command, args, options);
};

// 在 patch 之后调用 prebuildify
const prebuildify = require('prebuildify');

const opts = {
  napi: true,
  strip: true,
  cwd: process.cwd(),
  arch: process.env.PREBUILD_ARCH || os.arch(),
  platform: process.env.PREBUILD_PLATFORM || os.platform(),
  nodeGyp: process.env.PREBUILD_NODE_GYP || path.join(
    process.cwd(),
    'node_modules',
    '.bin',
    process.platform === 'win32' ? 'node-gyp.cmd' : 'node-gyp'
  )
};

prebuildify(opts, (err) => {
  if (err) {
    console.error(err.stack || err.message || String(err));
    process.exit(1);
  }
  console.log('prebuildify completed successfully');
});
```

在 GitHub Actions workflow 中使用:
```yaml
- name: Generate prebuilds (Windows)
  if: matrix.os == 'windows-latest'
  run: node scripts/run-prebuildify.js
```

**工作流程**:
1. `run-prebuildify.js` 首先 patch `child_process.spawn` 函数
2. 然后调用 `prebuildify(opts, callback)`
3. `prebuildify` 内部尝试 `spawn('node-gyp.cmd', args, ...)`
4. 被 patch 的 `spawn` 自动检测到 `.cmd` 文件
5. 自动转换为 `spawn('cmd.exe', ['/c', 'node-gyp.cmd', ...args], ...)`
6. 成功执行 ✅

**为什么这个方案有效**:
- ✅ 在 prebuildify 执行之前注入补丁
- ✅ 所有通过 prebuildify 的 spawn 调用都会被自动修复
- ✅ 对 Linux/macOS 透明（不影响其他平台）
- ✅ 无需修改 prebuildify 源码
- ✅ 经过生产环境验证（node-hamlib v0.1.22+）

**注意事项**:
1. 这个脚本**必须**在调用 prebuildify 之前执行 patch
2. 不能使用 `npm run prebuild` 直接调用 prebuildify，因为那样 patch 不会生效
3. 必须使用 `node scripts/run-prebuildify.js` 来执行
4. 这是 Windows 平台特有的解决方案，Linux/macOS 不需要

**验证方法**:
```bash
# 本地 Windows 测试
node scripts/run-prebuildify.js

# 应该看到输出：
# prebuildify completed successfully
# prebuilds/win32-x64/node.napi.node 已创建
```

**历史教训**:
在重构打包流程时，如果删除了这个脚本，Windows 构建会立即失败。这是一个**不可替代**的关键脚本，直到 prebuildify 官方修复这个问题为止。

参考命令均已集成到 GitHub Actions，避免自定义脚本堆叠；特殊场景请在本机按上述工具命令单步调试验证。
