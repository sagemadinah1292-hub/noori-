@echo off
setlocal
set "SOURCE=%~dp0NOORI Double Delay.vst3"
set "TARGET=%CommonProgramFiles%\VST3\NOORI Double Delay.vst3"
if not exist "%SOURCE%\Contents\x86_64-win\NOORI Double Delay.vst3" (
 echo Extract the entire package first, including the VST3 folder.
 pause
 exit /b 1
)
echo Close Cubase before installing. This installs NOORI Double Delay only.
if not exist "%CommonProgramFiles%\VST3" mkdir "%CommonProgramFiles%\VST3"
xcopy "%SOURCE%" "%TARGET%\" /E /I /Y >nul
if errorlevel 1 (
 echo Installation failed. Run Install.bat as administrator, or copy the VST3 folder manually.
 pause
 exit /b 1
)
echo Installed. Reopen Cubase and search for NOORI Double Delay in the Delay category.
pause
