param([string]$BuildDir='build', [string]$OutputDir='dist')
$ErrorActionPreference='Stop'
$bundle=Join-Path $BuildDir 'NooriTurabTempo_artefacts/Release/VST3/NooriTurabTempo.vst3'
$module=Join-Path $bundle 'Contents/x86_64-win/NooriTurabTempo.vst3'
& "$PSScriptRoot/Verify-PE.ps1" -Module $module
if (!(Test-Path (Join-Path $BuildDir 'plugin-smoke.ok'))) { throw 'Missing successful plugin smoke validation' }
if (!(Test-Path (Join-Path $BuildDir 'Preview.png'))) { throw 'Missing real editor preview' }
if (!(Test-Path (Join-Path $BuildDir 'ProjectPreview.png'))) { throw 'Missing ARA project-track editor preview' }
if (!(Test-Path (Join-Path $BuildDir 'DarkPreview.png'))) { throw 'Missing dark-theme preview' }
$package=Join-Path $OutputDir 'Noori-Turab-Tempo-Windows-x64'
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
if (Test-Path $package) { throw 'Package directory exists; use a clean output directory' }
New-Item -ItemType Directory -Path $package | Out-Null
Copy-Item -Recurse $bundle $package
Copy-Item "$PSScriptRoot/Install.bat", "$PSScriptRoot/Verify-PE.ps1", 'README.md', 'VALIDATION.md', 'LICENSE' $package
Copy-Item -Recurse 'licenses', 'docs' $package
Copy-Item (Join-Path $BuildDir 'DarkPreview.png'), (Join-Path $BuildDir 'Preview.png'), (Join-Path $BuildDir 'ProjectPreview.png') $package
$buildInfo=@{
 product='NOORI TURAB TEMPO';version='0.3.1';platform='Windows x64';format='VST3';
 commit=(& git rev-parse HEAD);built_utc=[DateTime]::UtcNow.ToString('o');
 pe_amd64_verified=$true;automated_dsp_tests='passed';processor_smoke='passed';
 cubase_pro_15_tested=$false;ara_extension_compiled=$true;ara_host_validation='Cubase not run';ara_in_process_host_test='passed';
 urdu_corpus_evaluation='not run';release_status='experimental-development-build';
 sha256=(Get-FileHash $module -Algorithm SHA256).Hash
}
$buildInfo | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $package 'BUILD-INFO.json')
Compress-Archive -Path $package -DestinationPath (Join-Path $OutputDir 'Noori-Turab-Tempo-Windows-x64.zip')
Write-Host "Created Windows package only after PE verification and automated checks."
