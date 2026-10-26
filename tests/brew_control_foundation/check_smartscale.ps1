param([string]$ScaleDir = 'D:/DIY_HUSTERY/smartscale_main.ino')
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$compiler = Join-Path $env:USERPROFILE '.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-g++.exe'
if (!(Test-Path $compiler)) { throw 'Install the Main PlatformIO toolchain first.' }
function Get-NormalizedHash([string]$path) {
    $source = [IO.File]::ReadAllText($path).Replace("`r`n", "`n")
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($source))) }
    finally { $sha.Dispose() }
}
$canonical = Get-NormalizedHash (Join-Path $projectRoot 'protocol/hustery_scale_v2.h')
$installed = Get-NormalizedHash (Join-Path $ScaleDir 'hustery_scale_v2.h')
if ($canonical -ne $installed) { throw 'SmartScale/HMI protocol headers differ (excluding CRLF/LF).' }
& $compiler -std=gnu++17 -fsyntax-only "-I$projectRoot" "-I$ScaleDir" (Join-Path $PSScriptRoot 'smartscale_tare.cpp')
if ($LASTEXITCODE -ne 0) { throw 'SmartScale tare assertions failed.' }
Write-Output 'PASS: actual SmartScale tare header, 7 behavior assertions, shared wire header hash.'
