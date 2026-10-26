$ErrorActionPreference = 'Stop'
$thermalRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
foreach ($thermalTarget in @('main','hmi')) {
  $thermalDir = if ($thermalTarget -eq 'main') { $thermalRoot } else { Join-Path $thermalRoot 'hmi' }
  Push-Location $thermalDir
  try {
    $thermalMetaText = pio project metadata --json-output
    if ($LASTEXITCODE -ne 0) { throw 'Metadata failed' }
    $thermalMeta = ($thermalMetaText | ConvertFrom-Json).PSObject.Properties.Value
    $thermalArgs = @($thermalMeta.cxx_flags | Where-Object { $_ -ne '-MMD' })
    $thermalArgs += @($thermalMeta.defines | ForEach-Object { '-D' + $_ })
    $thermalArgs += @($thermalMeta.includes.build | ForEach-Object { '-I' + $_ })
    $thermalArgs += '-fsyntax-only'
    $thermalFiles = if ($thermalTarget -eq 'main') {
      @('src/main.cpp','src/app/calibration_store.cpp','src/app/orchestrator.cpp','src/model/thermal_model.cpp','src/hardware/sensor_reader.cpp','src/communication/handlers/hmi_calibration_handler.cpp')
    } else {
      @('src/main.cpp','src/ui/screen_manager.cpp','src/ui/screens/settings_screens.cpp','src/communication/main_link.cpp','src/web/calibration_web_bridge.cpp','src/web/routes/dashboard_routes.cpp')
    }
    $thermalLog = Join-Path $PSScriptRoot ($thermalTarget + '-syntax.log')
    $ErrorActionPreference = 'Continue'
    & $thermalMeta.cxx_path $thermalArgs $thermalFiles *> $thermalLog
    $thermalExit = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($thermalExit -ne 0) {
      Select-String -Path $thermalLog -Pattern 'error:' | Select-Object -First 10
      throw "$thermalTarget syntax failed; see $thermalLog"
    }
    Write-Output "PASS: $thermalTarget thermal calibration syntax (no firmware build/link/upload)."
  } finally { Pop-Location }
}
Push-Location $thermalRoot
try {
  node tests/thermal/web_test.cjs
  if ($LASTEXITCODE -ne 0) { throw 'Thermal Web tests failed' }
  node tests/heater/assembled_page_test.cjs
  if ($LASTEXITCODE -ne 0) { throw 'Assembled Web tests failed' }
} finally { Pop-Location }
