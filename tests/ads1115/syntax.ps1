# Target-compiler syntax checks only. No linking/build/upload.
$ErrorActionPreference = 'Stop'
$captureRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
foreach ($captureTarget in @('main', 'hmi')) {
    $captureDir = if ($captureTarget -eq 'main') { $captureRoot } else { Join-Path $captureRoot 'hmi' }
    Push-Location $captureDir
    try {
        $captureText = pio project metadata --json-output
        if ($LASTEXITCODE -ne 0) { throw 'PlatformIO metadata failed' }
        $captureMeta = ($captureText | ConvertFrom-Json).PSObject.Properties.Value
        $captureArgs = @($captureMeta.cxx_flags | Where-Object { $_ -ne '-MMD' })
        $captureArgs += @($captureMeta.defines | ForEach-Object { '-D' + $_ })
        $captureArgs += @($captureMeta.includes.build | ForEach-Object { '-I' + $_ })
        $captureArgs += '-fsyntax-only'
        $captureFiles = if ($captureTarget -eq 'main') {
            @('src/hardware/ads1115_interface.cpp', 'src/hardware/pressure_capture.cpp',
              'src/communication/hmi_link.cpp', 'src/communication/handlers/hmi_pressure_capture_handler.cpp',
              'src/communication/handlers/hmi_calibration_handler.cpp',
              'src/hardware/adc_probe.cpp', 'src/hardware/pump_control.cpp',
              'src/communication/handlers/hmi_adc_probe_handler.cpp')
        } else {
            @('src/communication/main_link.cpp', 'src/web/routes/pressure_diagnostic_routes.cpp',
              'src/web/routes/pump_diagnostic_routes.cpp', 'src/web/routes/dashboard_routes.cpp', 'src/web/hmi_web_server.cpp',
              'src/web/routes/adc_probe_routes.cpp')
        }
        & $captureMeta.cxx_path $captureArgs $captureFiles
        if ($LASTEXITCODE -ne 0) { throw "$captureTarget source syntax failed" }
        if ($captureTarget -eq 'main') {
            & $captureMeta.cxx_path $captureArgs -DADC_MEASURED_FAST_ACQUISITION=0 src/hardware/ads1115_interface.cpp
            if ($LASTEXITCODE -ne 0) { throw 'M15 ADC baseline syntax failed' }
            & $captureMeta.cxx_path $captureArgs -DADS1115_SINGLE_SHOT_ACQUISITION=0 src/hardware/ads1115_interface.cpp src/hardware/pressure_capture.cpp
            if ($LASTEXITCODE -ne 0) { throw 'Legacy ADC source syntax failed' }
        }
        Write-Output "PASS: $captureTarget changed-source syntax with installed target libraries"
    } finally { Pop-Location }
}
