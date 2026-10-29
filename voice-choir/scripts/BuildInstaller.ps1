param([string]$OutputDir='dist-choir')
$ErrorActionPreference='Stop'
$package=Join-Path $OutputDir 'Noori-Choir-0.4.2-Windows-x64'
$compiler=Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6/ISCC.exe'
if (!(Test-Path $compiler)) {throw 'Inno Setup 6 is required to compile the installer.'}
$buildInfo=Get-Content (Join-Path $package 'BUILD-INFO.json') -Raw | ConvertFrom-Json
$activationRequired=if ($buildInfo.licensing_enforced) {1} else {0}
& $compiler "/DActivationRequired=$activationRequired" "/DSourceDir=$((Resolve-Path $package).Path)" "/O$((Resolve-Path $OutputDir).Path)" 'voice-choir/scripts/Installer.iss'
if ($LASTEXITCODE -ne 0) {throw 'Installer compilation failed'}
$setup=Join-Path $OutputDir 'NOORI_CHOIR_Setup_x64.exe'
$testRoot=Join-Path $env:RUNNER_TEMP 'NooriInstallerSmoke'
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
$sentinel=Join-Path $testRoot 'OtherPlugin-Sentinel.txt'
Set-Content $sentinel 'Preserve unrelated files'
$process=Start-Process -FilePath (Resolve-Path $setup) -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/DIR=`"$testRoot`"" -Wait -PassThru
if ($process.ExitCode -ne 0) {throw "Installer exited $($process.ExitCode)"}
$relative='NOORI CHOIR.vst3/Contents/x86_64-win/NOORI CHOIR.vst3'
$installed=Join-Path $testRoot $relative
if (!(Test-Path $installed)) {throw 'Installed VST3 module is missing'}
if ((Get-FileHash $installed).Hash -ne (Get-FileHash (Join-Path $package $relative)).Hash) {throw 'Installed binary hash mismatch'}
$uninstaller=Join-Path $testRoot 'NOORI CHOIR Uninstall/unins000.exe'
$process=Start-Process -FilePath $uninstaller -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -Wait -PassThru
if ($process.ExitCode -ne 0) {throw 'Uninstall failed'}
if (Test-Path $installed) {throw 'Uninstall did not remove the plugin'}
if (!(Test-Path $sentinel)) {throw 'Uninstall removed an unrelated file'}
@{installer_compiled=$true;silent_install_tested=$true;installed_binary_hash_verified=$true;silent_uninstall_tested=$true;unrelated_files_preserved=$true;cubase_15_tested=$false;interactive_install_tested=$false;code_signed=$false} | ConvertTo-Json | Set-Content (Join-Path $OutputDir 'INSTALLER-TEST-RESULTS.json')
