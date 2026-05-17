LPG OPERATOR HMI - PROFESSIONAL DIRECT READ V2
================================================

Start here.

This package replaces the earlier macro-read/duplicated tag approach.
Your latest HMI test proved that the Weintek tag:

    Address type: 4x_Double
    Data format: 32-bit Float
    Address: 0

can read Live Weight directly and correctly from the controller.

Main design decision:
- Direct controller tags are used for live display values.
- Macros are used only for operator calculations, setup write, tare command, and command sequencing.
- No macro-based live data copy layer is used.

Open these files in order:
1. 01_Background/operator_screen_background_800x480.png
2. 01_Background/operator_screen_preview_with_assets_800x480.png
3. 05_Documentation/HMI_DEVELOPMENT_GUIDE_DIRECT_READ_V2.md
4. 05_Documentation/OBJECT_PLACEMENT_MAP.csv
5. 03_Tags/LPG_OPERATOR_DIRECT_REQUIRED_TAGS_IMPORT_NO_HEADER.csv

Do not import all raw registers. Use only the required tag file.
