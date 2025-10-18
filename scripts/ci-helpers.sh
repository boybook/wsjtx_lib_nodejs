#!/bin/bash
# CI Helper Functions for GitHub Actions
# 提供可复用的函数,减少 workflow 文件的重复代码

set -e  # Exit on error

# ============================================================================
# 平台名称映射
# ============================================================================
# 将 GitHub Actions 的 runner OS 名称映射到 Node.js 标准平台名称
get_platform_name() {
  local os_name="$1"

  case "$os_name" in
    ubuntu-latest|ubuntu-*) echo "linux" ;;
    macos-latest|macos-*)   echo "darwin" ;;
    windows-latest|windows-*) echo "win32" ;;
    *) echo "$os_name" ;;
  esac
}

# ============================================================================
# 创建构建信息文件
# ============================================================================
create_build_info() {
  local target_dir="$1"
  local platform="$2"
  local github_runner="$3"
  local arch="$4"
  local node_version="$5"
  local cmake_arch="$6"

  # 获取 .node 文件大小
  local node_file=$(find "$target_dir" -maxdepth 1 -name "*.node" | head -1)
  local file_size="unknown"
  if [ -f "$node_file" ]; then
    file_size=$(stat -c%s "$node_file" 2>/dev/null || stat -f%z "$node_file" 2>/dev/null || echo "unknown")
  fi

  # 统计捆绑的库文件数量
  local bundled_libs=$(ls "$target_dir" 2>/dev/null | grep -E '\.(so|dylib|dll)$' | wc -l | tr -d ' ')

  # 生成 JSON 文件
  cat > "$target_dir/build-info.json" << EOF
{
  "platform": "$platform",
  "github_runner": "$github_runner",
  "arch": "$arch",
  "node_version": "$node_version",
  "build_time": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "cmake_arch": "$cmake_arch",
  "file_size": $file_size,
  "bundled_libraries": $bundled_libs
}
EOF

  echo "✅ Created build-info.json"
  cat "$target_dir/build-info.json"
}

# ============================================================================
# 捆绑 Linux 共享库
# ============================================================================
bundle_linux_dependencies() {
  local node_file="$1"
  local target_dir="$2"

  echo "🔍 Checking Linux shared libraries..."

  # 提取需要捆绑的库(排除系统库)
  ldd "$node_file" | \
    grep -v "linux-vdso\|ld-linux\|libc\|libm\|libpthread\|libdl" | \
    awk '{print $3}' | \
    grep -v "not found" | \
    while read lib; do
      if [ -f "$lib" ] && [[ "$lib" == *"libfftw"* || "$lib" == *"libgfortran"* || "$lib" == *"libgcc"* || "$lib" == *"libquadmath"* || "$lib" == *"libstdc++"* ]]; then
        lib_name=$(basename "$lib")
        echo "  📦 Bundling: $lib_name"
        cp -n "$lib" "$target_dir/" 2>/dev/null || cp "$lib" "$target_dir/"
      fi
    done

  # 设置 RPATH
  echo "🔧 Setting RPATH to \$ORIGIN"
  patchelf --set-rpath '$ORIGIN' "$node_file" || true

  # 验证
  echo "✅ ldd verification:"
  ldd "$node_file" || true
}

# ============================================================================
# 捆绑 macOS dylibs
# ============================================================================
bundle_macos_dependencies() {
  local node_file="$1"
  local target_dir="$2"

  echo "🔍 Bundling macOS dylibs with dylibbundler..."

  # 构建搜索路径参数
  local brew_prefix=$(brew --prefix 2>/dev/null || echo "/opt/homebrew")
  local search_paths=(
    "/opt/homebrew/opt/fftw/lib"
    "/opt/homebrew/opt/gcc@14/lib/gcc/14"
    "/opt/homebrew/opt/gcc/lib/gcc/current"
    "/usr/local/opt/fftw/lib"
    "/usr/local/opt/gcc@14/lib/gcc/14"
    "/usr/local/opt/gcc/lib/gcc/current"
  )

  local sp_args=""
  for p in "${search_paths[@]}"; do
    [ -d "$p" ] && sp_args="$sp_args -s $p"
  done

  # 动态添加 Cellar 路径
  for p in "$brew_prefix/Cellar/fftw"/*/lib "$brew_prefix/Cellar/gcc"/*/lib/gcc/current; do
    [ -d "$p" ] && sp_args="$sp_args -s $p"
  done

  # 运行 dylibbundler (所有依赖与 .node 文件同级)
  dylibbundler -x "$node_file" -d "$target_dir" -p "@loader_path/" $sp_args -b

  echo "✅ Bundled dylibs:"
  ls -lh "$target_dir"/*.dylib 2>/dev/null || echo "  (no dylib files)"

  # 验证路径
  echo "✅ otool -L verification:"
  otool -L "$node_file"

  # 检查是否有绝对路径残留
  if otool -L "$node_file" | grep -E '^\s+(/opt/homebrew|/usr/local)'; then
    echo "❌ ERROR: Found absolute Homebrew paths in binary!"
    return 1
  fi

  echo "✅ All dependency paths are relative"
}

# ============================================================================
# 捆绑 Windows DLLs
# ============================================================================
bundle_windows_dependencies() {
  local node_file="$1"
  local target_dir="$2"

  echo "🔍 Analyzing DLL dependencies..."

  # 获取依赖的 DLL 列表
  local required_dlls=$(objdump -p "$node_file" | grep "DLL Name" | awk '{print $3}' | grep -E "(libfftw|libgfortran|libgcc|libwinpthread|libstdc)")

  echo "📦 Required DLLs:"
  for dll in $required_dlls; do
    echo "  - $dll"
  done

  # 复制 DLLs
  local bundled_count=0
  local missing_dlls=""

  for dll in $required_dlls; do
    if [ -f "/mingw64/bin/$dll" ]; then
      if cp "/mingw64/bin/$dll" "$target_dir/" 2>/dev/null; then
        echo "  ✅ Bundled: $dll"
        ((bundled_count++))
      else
        echo "  ❌ Copy failed: $dll"
        missing_dlls="$missing_dlls $dll"
      fi
    else
      echo "  ❌ Missing: $dll"
      missing_dlls="$missing_dlls $dll"
    fi
  done

  echo ""
  echo "📊 Summary: $bundled_count DLLs bundled"
  [ -n "$missing_dlls" ] && echo "⚠️  Missing DLLs:$missing_dlls"

  return 0
}

# ============================================================================
# 显示打包结果摘要
# ============================================================================
show_package_summary() {
  local target_dir="$1"

  echo ""
  echo "========================================="
  echo "📦 Package Contents"
  echo "========================================="
  ls -lh "$target_dir"
  echo "========================================="
}

# ============================================================================
# 主函数 - 如果直接执行此脚本
# ============================================================================
if [ "${BASH_SOURCE[0]}" = "${0}" ]; then
  echo "CI Helper Functions - Usage:"
  echo "  source scripts/ci-helpers.sh"
  echo ""
  echo "Available functions:"
  echo "  - get_platform_name <os_name>"
  echo "  - create_build_info <target_dir> <platform> <runner> <arch> <node_ver> <cmake_arch>"
  echo "  - bundle_linux_dependencies <node_file> <target_dir>"
  echo "  - bundle_macos_dependencies <node_file> <target_dir>"
  echo "  - bundle_windows_dependencies <node_file> <target_dir>"
  echo "  - show_package_summary <target_dir>"
fi
