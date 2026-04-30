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
Assert-Contains "$firmware\src\FillController.cpp" "status\.netWeightKg\s*>=\s*status\.targetWeightKg" "Fill completion must use net weight."
Assert-Contains "$firmware\src\FillController.cpp" "status\.netWeightKg\s*>=\s*status\.targetWeightKg\s*\*" "Fast-to-slow transition must use net weight."

Assert-Contains "$firmware\src\WebPortal.cpp" 'server_\.on\("/api/transactions"' "Transactions API route is required."
Assert-Contains "$firmware\src\WebPortal.cpp" 'server_\.on\("/api/relay"' "Manual relay API route is required."
Assert-Contains "$firmware\src\WebPortal.cpp" 'server_\.on\("/api/tare"' "Tare weight API route is required."
Assert-Contains "$firmware\src\WebPortal.cpp" 'server_\.on\("/api/modbus"' "Modbus register map API route is required."
Assert-Contains "$firmware\src\WebPortal.cpp" "manual relay control blocked during fill" "Manual relay control must be blocked during active fill."

Assert-Contains "$firmware\src\TransactionLog.cpp" 'kTransactionPrefix' "TransactionLog must use per-transaction records."
Assert-Contains "$firmware\src\TransactionLog.cpp" 'recordPath\(record\.id\)' "TransactionLog must save by transaction id."
Assert-Contains "$firmware\src\TransactionLog.cpp" 'exportJson' "TransactionLog must expose JSON history."
Assert-Contains "$firmware\src\TransactionLog.cpp" "record\.finalAmount\s*=\s*record\.netWeightKg\s*\*\s*record\.ratePerKg" "Transaction amount must be calculated from net weight."

Assert-Contains "$firmware\include\ModbusRegisterMap.h" "kLiveWeight\s*=\s*0x1001" "Modbus live weight register must be 0x1001."
Assert-Contains "$firmware\include\ModbusRegisterMap.h" "kTareWeight\s*=\s*0x1002" "Modbus tare weight register must be 0x1002."
Assert-Contains "$firmware\include\ModbusRegisterMap.h" "kNetWeight\s*=\s*0x1003" "Modbus net weight register must be 0x1003."
Assert-Contains "$firmware\include\ModbusRegisterMap.h" "kFillingStatus\s*=\s*0x1004" "Modbus filling status register must be 0x1004."
Assert-Contains "$firmware\include\ModbusRegisterMap.h" "kTargetWeight\s*=\s*0x1005" "Modbus target weight register must be 0x1005."
Assert-Contains "$firmware\include\ModbusRegisterMap.h" "kEstopStatus\s*=\s*0x1006" "Modbus E-stop register must be 0x1006."

Assert-Contains "$firmware\data\index.html" 'data-role="operator"' "UI must include User/Operator role."
Assert-Contains "$firmware\data\index.html" 'data-role="admin"' "UI must include Admin role."
Assert-Contains "$firmware\data\index.html" 'data-role="manufacturer"' "UI must include Manufacturer role."
Assert-Contains "$firmware\data\index.html" "tareWeightInput" "Operator UI must include tare weight input."
Assert-Contains "$firmware\data\index.html" "netWeightKg" "Operator UI must display net weight."
Assert-Contains "$firmware\data\index.html" "adminRateInput" "Admin UI must include rate management."
Assert-NotContains "$firmware\data\index.html" "Apply Simulated Weight" "Production UI must not expose simulated weight control."

Write-Host "Prototype contract checks passed."
