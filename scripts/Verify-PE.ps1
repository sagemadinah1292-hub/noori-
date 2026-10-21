param([Parameter(Mandatory=$true)][string]$Module)
$ErrorActionPreference = 'Stop'
if (![Environment]::Is64BitOperatingSystem) { throw 'Windows x64 is required' }
$bytes = [IO.File]::ReadAllBytes((Resolve-Path $Module))
if ($bytes.Length -lt 256 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) { throw 'Missing MZ DOS signature' }
$pe = [BitConverter]::ToInt32($bytes, 0x3c)
if ($pe -lt 64 -or ($pe + 26) -gt $bytes.Length) { throw 'Invalid PE header offset' }
if ([BitConverter]::ToUInt32($bytes,$pe) -ne 0x00004550) { throw 'Missing PE signature' }
if ([BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x8664) { throw 'Module is not AMD64' }
if ([BitConverter]::ToUInt16($bytes,$pe+24) -ne 0x20b) { throw 'Module is not PE32+' }
if (([BitConverter]::ToUInt16($bytes,$pe+22) -band 0x2000) -eq 0) { throw 'Module is not a DLL' }
Write-Host "Verified genuine PE32+ AMD64 DLL: $Module"
