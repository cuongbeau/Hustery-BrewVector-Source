# Compile-time contract assertions and upstream hash check, no firmware build.
param([string]$ProjectDir = (Join-Path $PSScriptRoot '../..'))
$ErrorActionPreference = 'Stop'
$foundationRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$foundationManifest = Get-Content (Join-Path $foundationRoot 'lib/BrewProfileRuntime/UPSTREAM.json') -Raw | ConvertFrom-Json
foreach ($entry in $foundationManifest.files) {
    $actual = (Get-FileHash (Join-Path $foundationRoot ('lib/BrewProfileRuntime/' + $entry.local)) -Algorithm SHA256).Hash
    if ($actual -ne $entry.sha256) { throw "Upstream hash mismatch: $($entry.local)" }
}
Push-Location $ProjectDir
try {
    $metadataText = pio project metadata --json-output
    if ($LASTEXITCODE -ne 0) { throw 'PlatformIO metadata failed' }
    $metadata = $metadataText | ConvertFrom-Json
    foreach ($property in $metadata.PSObject.Properties) {
        $envConfig = $property.Value
        $compilerArgs = @($envConfig.cxx_flags | Where-Object { $_ -ne '-MMD' })
        $compilerArgs += @($envConfig.defines | ForEach-Object { '-D' + $_ })
        $compilerArgs += @($envConfig.includes.build | ForEach-Object { '-I' + $_ })
        $compilerArgs += @(( '-I' + $foundationRoot ), '-fsyntax-only', (Join-Path $foundationRoot 'tests/brew_control_foundation/compile_contract.cpp'))
        if ($envConfig.cxx_flags -contains '-std=gnu++17') {
            $compilerArgs += (Join-Path $foundationRoot 'tests/brew_control_foundation/flow_shot_policy.cpp')
            $compilerArgs += (Join-Path $foundationRoot 'tests/brew_control_foundation/scale_session.cpp')
            $compilerArgs += (Join-Path $foundationRoot 'tests/brew_control_foundation/pump_flow_calibration.cpp')
        }
        & $envConfig.cxx_path $compilerArgs
        if ($LASTEXITCODE -ne 0) { throw "Contract compile failed: $($property.Name)" }
        Write-Output "PASS: $($property.Name), upstream SHA256 and schema/contract syntax (behavior static_asserts enabled on C++14+)"
    }
} finally {
    Pop-Location
}
