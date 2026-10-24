param([string]$BuildDir='build-delay',[string]$OutputDir='dist-delay')
$ErrorActionPreference='Stop'
$bundle=Join-Path $BuildDir 'NooriDoubleDelay_artefacts/Release/VST3/NOORI Double Delay.vst3'
$module=Join-Path $bundle 'Contents/x86_64-win/NOORI Double Delay.vst3'
if (!(Test-Path $module)) {throw 'Missing VST3 binary'}
$bytes=[IO.File]::ReadAllBytes($module)
$pe=[BitConverter]::ToInt32($bytes,0x3c)
if ([BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x8664) {throw 'Not a Windows AMD64 binary'}
if (!(Test-Path (Join-Path $BuildDir 'delay-tests.ok'))) {throw 'Missing processor test success marker'}
if (!(Test-Path (Join-Path $BuildDir 'Preview.png'))) {throw 'Missing actual GUI preview'}
$package=Join-Path $OutputDir 'Noori-Double-Delay-Windows-x64'
New-Item -ItemType Directory -Path $package -Force | Out-Null
Copy-Item -Recurse $bundle $package
Copy-Item -Recurse 'double-delay/licenses' $package
Copy-Item 'double-delay/scripts/Install.bat','double-delay/README.md','double-delay/LICENSE' $package
Copy-Item (Join-Path $BuildDir 'Preview.png') $package
@{product='NOORI Double Delay';version='0.1.0';platform='Windows x64';format='VST3';commit=(& git rev-parse HEAD);built_utc=[DateTime]::UtcNow.ToString('o');automated_tests='passed';pe_amd64_verified=$true;cubase_15_tested=$false;original_dll_audio_comparison='not run - uploaded Nuendo DLL rejects available emulated host';release_status='experimental-development-build';sha256=(Get-FileHash $module -Algorithm SHA256).Hash} | ConvertTo-Json | Set-Content (Join-Path $package 'BUILD-INFO.json')
Compress-Archive -Path $package -DestinationPath (Join-Path $OutputDir 'Noori-Double-Delay-Windows-x64.zip') -Force
