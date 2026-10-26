$ErrorActionPreference = 'Stop'
$adcRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$adcCompiler = Join-Path $env:USERPROFILE '.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-g++.exe'
foreach ($adcMode in @(0,1)) {
    & $adcCompiler -std=gnu++17 -fsyntax-only "-I$adcRoot" "-DADS1115_SINGLE_SHOT_ACQUISITION=$adcMode" (Join-Path $PSScriptRoot 'acquisition.cpp') (Join-Path $PSScriptRoot 'pressure_capture.cpp') (Join-Path $PSScriptRoot 'adc_probe.cpp')
    if ($LASTEXITCODE -ne 0) { throw "ADC acquisition contract failed, mode=$adcMode" }
}
Write-Output 'PASS: ADC busy/ready/rearm ordering, signed sample and legacy A/B behavior.'
Write-Output 'PASS: capture lifecycle, averaging, publish cadence/wrap and read bounds.'
Write-Output 'PASS: ADC probe owner ACK, timing, abort/restore, fresh sample, failure lock and wrap.'
