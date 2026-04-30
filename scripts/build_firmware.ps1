param(
  [string]$Fqbn = $(if ($env:FQBN) { $env:FQBN } else { "esp32:esp32:esp32" }),
  [string]$SketchDir = $(Join-Path $PSScriptRoot "..\firmware\kc868_a6_lpg_controller")
)

$ErrorActionPreference = "Stop"

$repoLocalCandidates = @(
  (Join-Path $PSScriptRoot "..\tools\local\arduino-cli.exe"),
  (Join-Path $PSScriptRoot "..\tools\local\arduino-cli-0.35.3.exe"),
  (Join-Path $PSScriptRoot "..\tools\local\arduino-cli-0.35.3\arduino-cli.exe")
)

$arduinoCli = $null
foreach ($candidate in $repoLocalCandidates) {
  if (Test-Path $candidate) {
    $arduinoCli = (Resolve-Path $candidate).Path
    break
  }
}

if (-not $arduinoCli) {
  $command = Get-Command arduino-cli -ErrorAction SilentlyContinue
  if ($command) {
    $arduinoCli = $command.Source
  }
}

if (-not $arduinoCli) {
  throw "arduino-cli was not found. Install it or place arduino-cli.exe under tools\local."
}

$resolvedSketchDir = (Resolve-Path $SketchDir).Path

Write-Host "Building sketch: $resolvedSketchDir"
Write-Host "Using FQBN: $Fqbn"
Write-Host "Using arduino-cli: $arduinoCli"

& $arduinoCli compile --fqbn $Fqbn $resolvedSketchDir
exit $LASTEXITCODE
