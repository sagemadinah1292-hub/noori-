@echo off
setlocal
set "CHOIR_TARGET=%CommonProgramFiles%\VST3\NOORI CHOIR.vst3"
echo Close Cubase before installing NOORI CHOIR.
pause
robocopy "%~dp0NOORI CHOIR.vst3" "%CHOIR_TARGET%" /E
if errorlevel 8 (echo Install failed. Run as administrator or copy the VST3 folder manually.) else (echo Installed. Reopen Cubase and rescan plugins.)
pause

