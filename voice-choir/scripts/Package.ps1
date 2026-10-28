param([string]$BuildDir='build-choir',[string]$OutputDir='dist-choir')
$ErrorActionPreference='Stop'
$bundle=Join-Path $BuildDir 'NooriVoiceChoir_artefacts/Release/VST3/NOORI CHOIR.vst3'
$module=Join-Path $bundle 'Contents/x86_64-win/NOORI CHOIR.vst3'
if (!(Test-Path $module)) {throw 'Missing VST3 binary'}
$bytes=[IO.File]::ReadAllBytes($module)
$pe=[BitConverter]::ToInt32($bytes,0x3c)
if ([BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x8664) {throw 'Not a Windows AMD64 binary'}
if (!(Test-Path (Join-Path $BuildDir 'choir-tests.ok'))) {throw 'Missing processor test success marker'}
if (!(Test-Path (Join-Path $BuildDir 'Preview.png'))) {throw 'Missing actual GUI preview'}
$package=Join-Path $OutputDir 'Noori-Choir-0.2.0-Windows-x64'
if (Test-Path $package) {Remove-Item -Recurse -Force $package}
New-Item -ItemType Directory -Path $package -Force | Out-Null
Copy-Item -Recurse $bundle $package
Copy-Item -Recurse 'voice-choir/licenses' $package
Copy-Item 'voice-choir/scripts/Install.bat','voice-choir/README.md','voice-choir/LICENSE' $package
Copy-Item (Join-Path $BuildDir 'Preview*.png') $package
if (!(Test-Path (Join-Path $BuildDir 'choir-files.ok'))) {throw 'Missing WAV I/O test marker'}
@{product='NOORI CHOIR';version='0.2.0';platform='Windows x64';format='VST3';commit=(& git rev-parse HEAD);built_utc=[DateTime]::UtcNow.ToString('o');automated_tests='passed';pe_amd64_verified=$true;cubase_15_tested=$false;ai_model=$false;logo_asset_bundled=$true;installer_verified=$false;release_status='experimental-development-build';sha256=(Get-FileHash $module -Algorithm SHA256).Hash} | ConvertTo-Json | Set-Content (Join-Path $package 'BUILD-INFO.json')
# Bundle exact project sources with every experimental binary.
$source=Join-Path $package 'Source';New-Item -ItemType Directory -Path $source -Force | Out-Null
Copy-Item -Recurse 'voice-choir' $source
Copy-Item -Recurse '.github' $source
Compress-Archive -Path $package -DestinationPath (Join-Path $OutputDir 'Noori-Choir-0.2.0-Windows-x64.zip') -Force
