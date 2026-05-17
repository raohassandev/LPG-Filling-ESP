# LPG Operator HMI Development Guide - Direct Read V2

## 1. Purpose

This is the rebuilt operator-screen package for the Weintek MT8071iE 800x480 HMI.
It uses direct controller tags for live values and keeps macro use limited to UI logic and command sequencing.

## 2. Core Decisions

### Direct read values
Use Weintek native tags directly for all live values:

- `Live Weight` = `4x_Double`, address `0`, 32-bit Float
- `Tare Weight` = `4x_Double`, address `2`, 32-bit Float
- `Net Weight` = `4x_Double`, address `4`, 32-bit Float
- `Target KG` = `4x_Double`, address `6`, 32-bit Float
- `Rate Per KG` = `4x_Double`, address `8`, 32-bit Float
- `Target Amount` = `4x_Double`, address `10`, 32-bit Float
- `Current Amount` = `4x_Double`, address `12`, 32-bit Float

Your HMI test proved direct live-weight reading is stable with this method.

### Macros are not for live-data copying
Macros are used only for:

- By KG / By Amount mode selection
- Auto-calculation between KG and amount
- Capture tare command
- Ready / Apply Setup command
- Start / Stop / Complete / Reset commands
- Progress percentage calculation

## 3. Device and Conversion Settings

Recommended HMI device:

- Device type: `MODBUS RTU (Zero-based Addressing)`
- Station: `1`
- COM: `115200, N, 8, 1`
- Addressing: zero-based

Data conversion:

- `4x_Double`: keep conversion OFF because direct float was verified with `4x_Double` + `32-bit Float`.
- `4x`: if 16-bit status values read as `256` instead of `1`, enable `4x AB -> BA` only.
- Do not enable random double-word swapping.

## 4. Operator Workflow

The operator should not see engineering words like `Write Presets` or `Prepare`.

Final user flow:

1. Choose `By KG` or `By Amount`.
2. Enter `Price per KG`.
3. Enter either `Desired KG` or `Desired Amount`.
4. Press `Capture Tare` when the cylinder is placed and stable.
5. Press `Ready / Apply Setup`.
6. When the machine shows ready, press `Start Filling`.
7. Use `Stop Filling` only when needed.
8. When complete, press `Complete / Next Cylinder`.

## 5. By KG / By Amount Logic

Local HMI tags:

- `UI Mode`: `0 = By KG`, `1 = By Amount`
- `UI Price Per KG`: operator input
- `UI Desired KG`: operator input or calculated value
- `UI Desired Amount`: operator input or calculated value

Calculation:

- By KG: `Desired Amount = Desired KG * Price per KG`
- By Amount: `Desired KG = Desired Amount / Price per KG`

## 6. Tare Logic

Do not allow manual tare entry for normal operators.

Use button:

- Label: `Capture Tare`
- Macro: `M03_Capture_Tare`
- Command: `13`

Display current tare using direct tag:

- `Tare Weight`, `4x_Double`, address `2`, 32-bit Float

## 7. Apply Setup Logic

Use button:

- Label: `Ready / Apply Setup`
- Macro: `M04_Ready_Apply_Setup`

This macro:

1. Calculates the missing value based on mode.
2. Writes preset target kg, rate, and amount to controller preset registers.
3. Sends mode command.
4. Sends prepare/apply command.

## 8. Required Files

### Background
Use:

- `01_Background/operator_screen_background_800x480.png`

### Preview
Use this only to understand final result:

- `01_Background/operator_screen_preview_with_assets_800x480.png`

### Placement map
Use:

- `05_Documentation/OBJECT_PLACEMENT_MAP.csv`
- `01_Background/operator_screen_placement_overlay_800x480.png`

### Tags
Import:

- `03_Tags/LPG_OPERATOR_DIRECT_REQUIRED_TAGS_IMPORT_NO_HEADER.csv`

### Macros
Use:

- `04_Macros/GlobalLibrary_DirectRead.txt`
- `04_Macros/OperatorScreenMacros_DirectRead.txt`

## 9. HMI Object Rules

- Numeric displays: use direct controller tags.
- Numeric inputs: use Local HMI UI tags.
- Buttons: use PNG picture + transparent Function Key over the picture.
- Fill state and alarm: use Word Lamp.
- E-Stop/Cylinder/Nozzle/Stable: use Bit Lamp.
- Progress bar: use Local HMI `UI Progress Percent`.

## 10. Quality Check Before Field Use

Check these before operator handover:

- Live Weight directly shows stable value.
- Net Weight directly shows stable value.
- E-Stop/Cylinder/Nozzle/Stable show 0/1 correctly, not 0/256.
- If 1 shows as 256, apply `4x AB -> BA` conversion only.
- Capture Tare changes tare/net behavior.
- By KG auto-calculates amount.
- By Amount auto-calculates kg.
- Ready / Apply Setup makes Ready To Start true.
- Start Filling is disabled until Ready To Start = 1.
- Stop Filling is disabled until Can Stop = 1.
