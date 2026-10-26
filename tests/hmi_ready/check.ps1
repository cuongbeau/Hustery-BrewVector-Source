$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$compiler = Join-Path $env:USERPROFILE '.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-g++.exe'
& $compiler -std=gnu++11 -fsyntax-only "-I$projectRoot" "-I$projectRoot/hmi/src" (Join-Path $PSScriptRoot 'layout_contract.cpp')
if ($LASTEXITCODE -ne 0) { throw 'HMI standby touch layout assertions failed.' }
& $compiler -std=gnu++17 -fsyntax-only "-I$projectRoot/hmi/src" "-I$projectRoot/protocol" (Join-Path $PSScriptRoot 'result_snapshot.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Frozen result session assertions failed.' }
Write-Output 'PASS: touch layouts, result yield policy and frozen stop/session behavior.'
& $compiler -std=gnu++17 -fsyntax-only "-I$projectRoot" (Join-Path $PSScriptRoot 'quick_yield_session.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Quick yield session assertions failed.' }
Write-Output 'PASS: one-shot yield, token, lease, cancellation and bounds.'
& $compiler -std=gnu++17 -fsyntax-only "-I$projectRoot" (Join-Path $PSScriptRoot 'yield_stop.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Whole-shot yield stop assertions failed.' }
Write-Output 'PASS: shot 490 yield crossing, pre-tare guard, scale loss, OFF and legacy behavior.'
