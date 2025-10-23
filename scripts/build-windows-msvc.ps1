# Windows MSVC/MinGW 分离架构构建脚本
# 用于本地开发环境

param(
    [string]$MSYSPath = "C:\msys64",
    [string]$VSVersion = "2022",
    [string]$VSEdition = "Community"
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Windows MSVC/MinGW Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查MSYS2安装
if (-not (Test-Path $MSYSPath)) {
    Write-Host "❌ Error: MSYS2 not found at $MSYSPath" -ForegroundColor Red
    Write-Host "Please install MSYS2 from https://www.msys2.org/" -ForegroundColor Yellow
    exit 1
}

# 检查Visual Studio安装
$VSPath = "C:\Program Files\Microsoft Visual Studio\$VSVersion\$VSEdition"
if (-not (Test-Path $VSPath)) {
    Write-Host "❌ Error: Visual Studio $VSVersion $VSEdition not found" -ForegroundColor Red
    Write-Host "Please install Visual Studio or adjust -VSVersion and -VSEdition parameters" -ForegroundColor Yellow
    exit 1
}

# 步骤1: 使用MinGW编译wsjtx_lib.dll
Write-Host ""
Write-Host "=== Step 1: Building wsjtx_lib.dll with MinGW ===" -ForegroundColor Green
Write-Host ""

$BuildScript = @"
cd /c/Users/`$USERNAME/Documents/coding/wsjtx_lib_nodejs/wsjtx_lib || cd /d/path/to/wsjtx_lib_nodejs/wsjtx_lib
mkdir -p build && cd build

# 配置CMake
cmake .. -G 'MinGW Makefiles' \
         -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_C_COMPILER=gcc \
         -DCMAKE_CXX_COMPILER=g++ \
         -DCMAKE_Fortran_COMPILER=gfortran \
         -DCMAKE_PREFIX_PATH=/mingw64 \
         -DPKG_CONFIG_EXECUTABLE=/mingw64/bin/pkg-config

# 编译
cmake --build . --config Release --verbose

# 验证
if [ ! -f wsjtx_lib.dll ]; then
    echo "❌ Error: wsjtx_lib.dll not found!"
    exit 1
fi

echo "✅ wsjtx_lib.dll built successfully"
ls -la wsjtx_lib.dll
"@

$BuildScriptPath = Join-Path $env:TEMP "build_mingw_temp.sh"
$BuildScript | Out-File -FilePath $BuildScriptPath -Encoding UTF8

& "$MSYSPath\usr\bin\bash.exe" -lc $BuildScriptPath

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ MinGW build failed!" -ForegroundColor Red
    exit 1
}

# 复制DLL到prebuilds
Write-Host ""
Write-Host "📦 Copying DLL and dependencies to prebuilds..." -ForegroundColor Green

$PrebuildsDir = "prebuilds\win32-x64"
if (-not (Test-Path $PrebuildsDir)) {
    New-Item -ItemType Directory -Path $PrebuildsDir -Force | Out-Null
}

Copy-Item "wsjtx_lib\build\wsjtx_lib.dll" -Destination $PrebuildsDir -Force
Write-Host "✅ Copied wsjtx_lib.dll"

# 复制MinGW运行时依赖
$MinGWBin = "$MSYSPath\mingw64\bin"
$DLLs = @(
    "libfftw3f-3.dll",
    "libgfortran-5.dll",
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll"
)

foreach ($dll in $DLLs) {
    $srcPath = Join-Path $MinGWBin $dll
    if (Test-Path $srcPath) {
        Copy-Item $srcPath -Destination $PrebuildsDir -Force
        Write-Host "✅ Copied $dll"
    } else {
        Write-Host "⚠️  Warning: $dll not found" -ForegroundColor Yellow
    }
}

# 步骤2: 使用MSVC编译native扩展
Write-Host ""
Write-Host "=== Step 2: Building native extension with MSVC ===" -ForegroundColor Green
Write-Host ""

# 初始化MSVC环境
$VCVarsPath = "$VSPath\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $VCVarsPath)) {
    Write-Host "❌ Error: vcvars64.bat not found at $VCVarsPath" -ForegroundColor Red
    exit 1
}

Write-Host "Initializing MSVC environment..."
cmd /c "`"$VCVarsPath`" && set" | ForEach-Object {
    if ($_ -match "^([^=]+)=(.*)$") {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
    }
}

# 清理之前的构建
if (Test-Path "build") {
    Remove-Item -Recurse -Force "build"
}

Write-Host "Running cmake-js with MSVC..."
npx cmake-js compile --arch=x64 --CDCMAKE_BUILD_TYPE=Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ MSVC build failed!" -ForegroundColor Red
    exit 1
}

# 验证.node文件
if (-not (Test-Path "build\Release\wsjtx_lib_nodejs.node")) {
    Write-Host "❌ Error: wsjtx_lib_nodejs.node not found!" -ForegroundColor Red
    exit 1
}

Write-Host "✅ wsjtx_lib_nodejs.node built successfully"

# 复制到prebuilds
Copy-Item "build\Release\wsjtx_lib_nodejs.node" -Destination $PrebuildsDir -Force
Write-Host "✅ Copied wsjtx_lib_nodejs.node to prebuilds"

# 完成
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "✅ Build completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "📁 Package contents:"
Get-ChildItem $PrebuildsDir | Format-Table Name, Length -AutoSize

Write-Host ""
Write-Host "Next steps:"
Write-Host "1. Run tests: npm test"
Write-Host "2. Package: npm pack"
