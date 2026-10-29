@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CLANG_CL=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin\clang-cl.exe"

"%CMAKE%" -B build -G "Ninja" -DCMAKE_C_COMPILER="%CLANG_CL%"
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

"%CMAKE%" --build build --config Release
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

if exist "build\compile_commands.json" copy /y "build\compile_commands.json" "compile_commands.json" >nul

echo Build complete: .\build\nanochess.exe
