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
- **所有平台统一采用同级目录方案**，避免嵌套路径导致的 `@loader_path/native/native/` 等问题。

参考命令均已集成到 GitHub Actions，避免自定义脚本堆叠；特殊场景请在本机按上述工具命令单步调试验证。
