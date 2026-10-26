$ErrorActionPreference = 'Stop'
$heaterRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$heaterCompiler = Join-Path $env:USERPROFILE '.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-g++.exe'
& $heaterCompiler -std=gnu++17 -fsyntax-only "-I$heaterRoot" (Join-Path $PSScriptRoot 'core.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Heater state-machine assertions failed' }
Write-Output 'PASS: heater actual-core constexpr scenarios.'
& $heaterCompiler -std=gnu++17 -fsyntax-only "-I$heaterRoot" (Join-Path $PSScriptRoot 'heater_control_model.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Heater PID assertions failed' }
Write-Output 'PASS: Heater PID equations, saturation, feedforward and cadence.'
& $heaterCompiler -std=gnu++17 -fsyntax-only "-I$heaterRoot" (Join-Path $PSScriptRoot 'max31855.cpp')
if ($LASTEXITCODE -ne 0) { throw 'MAX31855 frame assertions failed' }
Write-Output 'PASS: MAX31855 signed temperatures and fault/frame decoding.'
& $heaterCompiler -std=gnu++17 -fsyntax-only "-I$heaterRoot" (Join-Path $PSScriptRoot 'error_stats.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Temperature rolling-window assertions failed' }
Write-Output 'PASS: temperature 5s windows, 20% boundary, bursts, gaps and clock wrap.'
foreach ($heaterTarget in @('main','hmi')) {
  $heaterDir = if ($heaterTarget -eq 'main') { $heaterRoot } else { Join-Path $heaterRoot 'hmi' }
  Push-Location $heaterDir
  try {
    $heaterMetaText = pio project metadata --json-output
    if ($LASTEXITCODE -ne 0) { throw 'Metadata failed' }
    $heaterMeta = ($heaterMetaText | ConvertFrom-Json).PSObject.Properties.Value
    $heaterArgs = @($heaterMeta.cxx_flags | Where-Object { $_ -ne '-MMD' })
    # Preserve quoted macro values when PowerShell builds the native command line
    # (for example MBEDTLS_CONFIG_FILE and BUILD_VERSION).
    $heaterArgs += @($heaterMeta.defines |
      Where-Object { $_ -notmatch '^ARDUINO_BOARD=' } |
      ForEach-Object { '-D' + ($_ -replace '"', '\"') })
    $heaterArgs += @($heaterMeta.includes.build | ForEach-Object { '-I' + $_ })
    $heaterArgs += '-fsyntax-only'
    $heaterFiles = if ($heaterTarget -eq 'main') {
      @('src/main.cpp','src/app/orchestrator.cpp','src/communication/handlers/hmi_command_handler.cpp','src/hardware/heater_control.cpp','src/hardware/pump_control.cpp','src/hardware/sensor_reader.cpp','src/communication/hmi_link.cpp','src/communication/handlers/hmi_firmware_receiver.cpp','src/recovery/recovery_portal.cpp')
    } else {
      @('src/main.cpp','src/ui/screen_manager.cpp','src/ui/screens/live_screens.cpp','src/communication/main_link.cpp','src/web/routes/heater_routes.cpp','src/web/routes/firmware_routes.cpp','src/web/routes/dashboard_routes.cpp','src/web/hmi_web_server.cpp')
    }
    & $heaterMeta.cxx_path $heaterArgs $heaterFiles
    if ($LASTEXITCODE -ne 0) { throw "$heaterTarget syntax failed" }
    Write-Output "PASS: $heaterTarget heater integration target syntax (no link/upload)."
  } finally { Pop-Location }
}
