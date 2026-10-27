@echo off
setlocal
set "CHOIR_TARGET=%CommonProgramFiles%\VST3\NOORI Voice Choir.vst3"
echo Close Cubase before installing NOORI Voice Choir.
pause
robocopy "%~dp0NOORI Voice Choir.vst3" "%CHOIR_TARGET%" /E
if errorlevel 8 (echo Install failed. Run as administrator or copy the VST3 folder manually.) else (echo Installed. Reopen Cubase and rescan plugins.)
pause
