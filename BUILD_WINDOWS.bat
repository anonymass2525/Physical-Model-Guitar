@echo off
setlocal
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake was not found. Install CMake and Visual Studio 2022 C++ Desktop Development.
  pause
  exit /b 1
)
echo [1/3] Configure Windows x64 Release and JUCE 9.0.4...
cmake -S . -B build-windows -A x64 -DPMG_BUILD_PLUGIN=ON -DBUILD_TESTING=ON
if errorlevel 1 goto :failed
echo [2/3] Build new v0.4 VST3 and audio tests...
cmake --build build-windows --config Release --parallel 2
if errorlevel 1 goto :failed
echo [3/3] Test native physical DSP...
ctest --test-dir build-windows -C Release --output-on-failure
if errorlevel 1 goto :failed
echo.
echo Build completed. Plugin in build-windows\PhysicalModelGuitar_artefacts\Release\VST3\
echo Build an EXE installer using Inno Setup 6 ISCC.exe:
echo   Installer\PMG04.iss
echo Backup VST3 folder: Physical Model Guitar 0.4.vst3 under:
echo    %%CommonProgramFiles%%\VST3\
echo v0.2 remains untouched: v0.4 uses a new VST3 plugin identifier.
pause
exit /b 0
:failed
echo.
echo Build failed. Read the log above and send it to ChatGPT.
pause
exit /b 1
