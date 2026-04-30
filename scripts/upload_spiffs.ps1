param(
  [string]$Port = $(if ($env:PORT) { $env:PORT } else { "COM5" }),
  [string]$SketchDir = $(Join-Path $PSScriptRoot "..\firmware\kc868_a6_lpg_controller"),
  [string]$Offset = "0x290000",
  [string]$Size = "0x160000"
)

$ErrorActionPreference = "Stop"

$mkspiffs = Join-Path $env:LOCALAPPDATA "Arduino15\packages\esp32\tools\mkspiffs\0.2.3\mkspiffs.exe"
$esptool = Join-Path $env:LOCALAPPDATA "Arduino15\packages\esp32\tools\esptool_py\5.2.0\esptool.exe"

if (-not (Test-Path $mkspiffs)) {
  throw "mkspiffs was not found. Install the esp32 Arduino core first."
}

if (-not (Test-Path $esptool)) {
  throw "esptool was not found. Install the esp32 Arduino core first."
}

$resolvedSketchDir = (Resolve-Path $SketchDir).Path
$dataDir = Join-Path $resolvedSketchDir "data"
$buildDir = Join-Path $resolvedSketchDir "build"
$imagePath = Join-Path $buildDir "spiffs.bin"

if (-not (Test-Path $dataDir)) {
  throw "Data directory not found: $dataDir"
}

New-Item -ItemType Directory -Force $buildDir | Out-Null

Write-Host "Building SPIFFS image from: $dataDir"
Write-Host "Image size: $Size"
& $mkspiffs -c $dataDir -b 4096 -p 256 -s $Size $imagePath
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

Write-Host "Uploading SPIFFS image: $imagePath"
Write-Host "Using port: $Port"
Write-Host "Using offset: $Offset"
& $esptool --chip esp32 --port $Port --baud 921600 --before default-reset --after hard-reset write-flash -z --flash-mode keep --flash-freq keep --flash-size keep $Offset $imagePath
exit $LASTEXITCODE
