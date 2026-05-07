param(
  [string]$Port = $(if ($env:PORT) { $env:PORT } else { "COM5" }),
  [string]$SketchDir = $(Join-Path $PSScriptRoot "..\firmware\lpg_controller"),
  [string]$Offset = "0x290000",
  [string]$Size = "0x160000"
)

$ErrorActionPreference = "Stop"

$arduino15Tools = Join-Path $env:LOCALAPPDATA "Arduino15\packages\esp32\tools"

$mkspiffs = $null
if (Test-Path $arduino15Tools) {
  $mkspiffs = Get-ChildItem "$arduino15Tools\mkspiffs" -Recurse -Filter "mkspiffs.exe" -ErrorAction SilentlyContinue |
              Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $mkspiffs) {
  $found = Get-Command mkspiffs -ErrorAction SilentlyContinue
  if ($found) { $mkspiffs = $found.Source }
}
if (-not $mkspiffs) {
  throw "mkspiffs not found. Install the esp32 Arduino core via Board Manager."
}

$esptool = $null
if (Test-Path $arduino15Tools) {
  $esptool = Get-ChildItem "$arduino15Tools\esptool_py" -Recurse -Filter "esptool.exe" -ErrorAction SilentlyContinue |
             Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $esptool) {
  $found = Get-Command esptool -ErrorAction SilentlyContinue
  if ($found) { $esptool = $found.Source }
}
if (-not $esptool) {
  throw "esptool not found. Install the esp32 Arduino core via Board Manager."
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
& $esptool --chip esp32 --port $Port --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode keep --flash_freq keep --flash_size keep $Offset $imagePath
exit $LASTEXITCODE
