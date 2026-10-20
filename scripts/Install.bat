@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "BUNDLE=%~dp0NooriTurabTempo.vst3"
set "MODULE=%BUNDLE%\Contents\x86_64-win\NooriTurabTempo.vst3"
set "DEST=%CommonProgramFiles%\VST3\NooriTurabTempo.vst3"
if not exist "%MODULE%" (
  echo ERROR: Windows x64 VST3 bundle is missing: "%MODULE%"
  exit /b 2
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Verify-PE.ps1" -Module "%MODULE%"
if errorlevel 1 (
 echo ERROR: Module did not pass PE x64 verification. Installation stopped.
 exit /b 3
)
net session >nul 2>&1
if errorlevel 1 (
 echo ERROR: Run Install.bat as Administrator, then try again.
 exit /b 5
)
if exist "%DEST%" (
 echo ERROR: "%DEST%" already exists. Close Cubase and move the old bundle to a backup location first.
 exit /b 6
)
robocopy "%BUNDLE%" "%DEST%" /E /COPY:DAT /DCOPY:DAT /R:1 /W:1
if errorlevel 8 (
 echo ERROR: Could not copy VST3 bundle. Check permissions and close Cubase.
 exit /b 7
)
if not exist "%DEST%\Contents\x86_64-win\NooriTurabTempo.vst3" (
 echo ERROR: Installed module is missing.
 exit /b 8
)
echo Installed to "%DEST%". Open Cubase and rescan VST3 plugins.
exit /b 0
