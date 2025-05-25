@echo off
echo === Windows MSVC Build Test ===

echo Checking system information...
echo OS: %OS%
echo PROCESSOR_ARCHITECTURE: %PROCESSOR_ARCHITECTURE%

echo Checking Node.js...
node --version
npm --version

echo Checking Visual Studio Build Tools...
where msbuild || echo "MSBuild not found"

echo Checking Intel Fortran...
where ifort || echo "ifort not found"
where ifx || echo "ifx not found"

echo Setting up Intel oneAPI environment...
if exist "C:\Program Files (x86)\Intel\oneAPI\setvars.bat" (
    echo Found Intel oneAPI setvars.bat
    call "C:\Program Files (x86)\Intel\oneAPI\setvars.bat"
) else (
    echo Intel oneAPI setvars.bat not found
)

echo Re-checking Fortran compilers...
where ifort && echo "ifort available" || echo "ifort not available"
where ifx && echo "ifx available" || echo "ifx not available"

echo Checking vcpkg...
if exist "vcpkg\vcpkg.exe" (
    echo vcpkg found
    vcpkg\vcpkg.exe list
) else (
    echo vcpkg not found, setting up...
    git clone https://github.com/Microsoft/vcpkg.git
    .\vcpkg\bootstrap-vcpkg.bat
    .\vcpkg\vcpkg install fftw3:x64-windows boost:x64-windows
)

echo Installing npm dependencies...
npm ci --ignore-scripts

echo Building TypeScript...
npm run build:ts

echo Building native module...
set IFORT_PATH=C:\Program Files (x86)\Intel\oneAPI\compiler\latest\windows\bin\intel64\ifort.exe
npx cmake-js compile --arch=x64 ^
  -G "Visual Studio 17 2022" -A x64 ^
  --CDCMAKE_TOOLCHAIN_FILE=vcpkg\scripts\buildsystems\vcpkg.cmake ^
  --CDCMAKE_Fortran_COMPILER="%IFORT_PATH%" ^
  --verbose

echo Checking build results...
if exist "build\Release\wsjtx_lib_nodejs.node" (
    echo Build successful: build\Release\wsjtx_lib_nodejs.node
    dir "build\Release\wsjtx_lib_nodejs.node"
) else (
    echo Build failed: .node file not found
    echo Build directory contents:
    dir build\Release\ 2>nul || echo "Release directory not found"
)

echo === Build test complete ===
pause 