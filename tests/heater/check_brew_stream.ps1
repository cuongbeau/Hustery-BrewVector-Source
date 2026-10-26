$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$compiler = Join-Path $env:USERPROFILE '.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-g++.exe'
Push-Location $projectRoot
try {
  & $compiler -std=gnu++17 -fsyntax-only -Ihmi/src -Iprotocol tests/heater/brew_stream.cpp
  if ($LASTEXITCODE -ne 0) { throw 'Brew stream ring assertions failed.' }
  & $compiler -std=gnu++11 -fsyntax-only -x c++ -Iprotocol hmi/src/model/brew_stream.h
  if ($LASTEXITCODE -ne 0) { throw 'Arduino C++11 compatibility failed.' }
  foreach ($test in @('brew_stream_test','dashboard_poll_test','dashboard_test','assembled_page_test','transport_test','web_test')) {
    & node "tests/heater/$test.cjs"
    if ($LASTEXITCODE -ne 0) { throw "$test failed." }
  }
} finally { Pop-Location }
