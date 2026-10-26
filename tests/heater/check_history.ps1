$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$compiler = Join-Path $env:USERPROFILE '.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-g++.exe'
Push-Location $projectRoot
try {
  & $compiler -std=gnu++11 -fsyntax-only -Ihmi/src tests/heater/shot_save_policy.cpp
  if ($LASTEXITCODE -ne 0) { throw 'Shot save policy assertions failed.' }
  foreach ($test in @('history_recent_test','transport_test','assembled_page_test')) {
    & node "tests/heater/$test.cjs"
    if ($LASTEXITCODE -ne 0) { throw "$test failed." }
  }
} finally { Pop-Location }
