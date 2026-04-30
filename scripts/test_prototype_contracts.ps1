$ErrorActionPreference = "Stop"

function Assert-Contains {
  param(
    [string]$Path,
    [string]$Pattern,
    [string]$Message
  )

  $content = Get-Content $Path -Raw
  if ($content -notmatch $Pattern) {
    throw $Message
  }
}

function Assert-NotContains {
  param(
    [string]$Path,
    [string]$Pattern,
    [string]$Message
  )

  $content = Get-Content $Path -Raw
  if ($content -match $Pattern) {
    throw $Message
  }
}

$root = Split-Path -Parent $PSScriptRoot
$firmware = Join-Path $root "firmware\kc868_a6_lpg_controller"

Assert-Contains "$firmware\include\WeightService.h" "kHx711DoutPin\s*=\s*32" "HX711 DOUT must stay on GPIO32 / IO-1."
Assert-Contains "$firmware\include\WeightService.h" "kHx711SckPin\s*=\s*33" "HX711 SCK must stay on GPIO33 / IO-2."

Assert-Contains "$firmware\src\FillController.cpp" "writeRelay\(0,\s*true\)" "Fast fill must energize Relay 1."
Assert-Contains "$firmware\src\FillController.cpp" "writeRelay\(2,\s*true\)" "Fast fill must energize the main supply relay."
Assert-Contains "$firmware\src\FillController.cpp" "writeRelay\(0,\s*false\)" "Slow fill transition must turn fast valve off."
Assert-Contains "$firmware\src\FillController.cpp" "writeRelay\(1,\s*true\)" "Slow fill transition must energize Relay 2."
Assert-Contains "$firmware\src\FillController.cpp" "writeAllSafe\(\)" "Controller must include all-relay-safe transitions."

Assert-Contains "$firmware\src\WebPortal.cpp" 'server_\.on\("/api/transactions"' "Transactions API route is required."
Assert-Contains "$firmware\src\WebPortal.cpp" 'server_\.on\("/api/relay"' "Manual relay API route is required."
Assert-Contains "$firmware\src\WebPortal.cpp" "manual relay control blocked during fill" "Manual relay control must be blocked during active fill."

Assert-Contains "$firmware\src\TransactionLog.cpp" 'kTransactionPrefix' "TransactionLog must use per-transaction records."
Assert-Contains "$firmware\src\TransactionLog.cpp" 'recordPath\(record\.id\)' "TransactionLog must save by transaction id."
Assert-Contains "$firmware\src\TransactionLog.cpp" 'exportJson' "TransactionLog must expose JSON history."

Assert-Contains "$firmware\data\index.html" 'data-role="operator"' "UI must include User/Operator role."
Assert-Contains "$firmware\data\index.html" 'data-role="admin"' "UI must include Admin role."
Assert-Contains "$firmware\data\index.html" 'data-role="manufacturer"' "UI must include Manufacturer role."
Assert-NotContains "$firmware\data\index.html" "Apply Simulated Weight" "Production UI must not expose simulated weight control."

Write-Host "Prototype contract checks passed."
