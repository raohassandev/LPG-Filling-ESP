"""
Generate history_screen_background_800x480.png
Dark industrial theme matching the actual LPG Operator screen.
Run: python gen_background.py
"""
from PIL import Image, ImageDraw, ImageFont
import os, sys

W, H = 800, 480

# ── Colors ─────────────────────────────────────────────────────────────────────
BG          = '#0D1117'   # page background
PANEL_BG    = '#111827'   # panel fill
PANEL_EDGE  = '#1E3A5F'   # panel border
HEADER_BG   = '#0A0E1A'   # top bar
TEAL        = '#17B4C8'   # accent / active tab / footer
TEAL_DIM    = '#0D6B7A'   # darker teal for button fill
TEAL_LIGHT  = '#4DD8EC'   # bright teal text highlight
WHITE       = '#FFFFFF'
LABEL       = '#8899BB'   # dim label colour
INPUT_BG    = '#0D1828'   # input box fill
INPUT_EDGE  = '#2A3F60'   # input box border
FOOTER_BG   = '#17B4C8'   # teal footer
DIVIDER     = '#1A2E4A'   # subtle divider line
WARN_EDGE   = '#CC4444'   # alarm box border
ORANGE      = '#E65100'   # pending indicator

# ── Fonts ──────────────────────────────────────────────────────────────────────
def load(path, size):
    try:
        return ImageFont.truetype(path, size)
    except Exception:
        return ImageFont.load_default()

FONT_PATH_BOLD   = 'C:/Windows/Fonts/arialbd.ttf'
FONT_PATH_NORMAL = 'C:/Windows/Fonts/arial.ttf'

F_TITLE  = load(FONT_PATH_BOLD,   20)
F_H2     = load(FONT_PATH_BOLD,   13)
F_LABEL  = load(FONT_PATH_NORMAL, 11)
F_SMALL  = load(FONT_PATH_NORMAL,  9)
F_FOOT   = load(FONT_PATH_BOLD,   11)
F_BTN    = load(FONT_PATH_BOLD,   13)

# ── Helpers ────────────────────────────────────────────────────────────────────
img  = Image.new('RGB', (W, H), BG)
draw = ImageDraw.Draw(img)

def rrect(x0, y0, x1, y1, r=5, fill=None, outline=None, lw=1):
    if fill:    draw.rounded_rectangle([x0,y0,x1,y1], radius=r, fill=fill)
    if outline: draw.rounded_rectangle([x0,y0,x1,y1], radius=r, outline=outline, width=lw)

def ibox(x, y, w, h):
    rrect(x, y, x+w, y+h, r=3, fill=INPUT_BG, outline=INPUT_EDGE, lw=1)

def ctext(draw, cx, y, text, font, fill):
    bbox = draw.textbbox((0,0), text, font=font)
    tw = bbox[2] - bbox[0]
    draw.text((cx - tw//2, y), text, font=font, fill=fill)

def ltext(x, y, text, font=None, fill=LABEL):
    draw.text((x, y), text, font=font or F_LABEL, fill=fill)

# ══════════════════════════════════════════════════════════════════════════════
# HEADER  y 0–50
# ══════════════════════════════════════════════════════════════════════════════
draw.rectangle([0, 0, W, 50], fill=HEADER_BG)

# Brand mark (small red bar + text — replace with actual logo PNG if available)
draw.rectangle([7, 8, 34, 42], fill='#8B0000')
draw.rectangle([8, 9, 33, 41], fill='#CC0000')
draw.text((38, 12), 'LPG Filling Reports', font=F_TITLE, fill=WHITE)

# Record # chip
ltext(390, 7, 'RECORD  #', font=F_SMALL, fill=LABEL)
rrect(390, 18, 490, 42, r=3, fill=INPUT_BG, outline=INPUT_EDGE)

# Pending ACK chip
ltext(502, 7, 'PENDING ACK', font=F_SMALL, fill=ORANGE)
rrect(502, 18, 618, 42, r=3, fill='#1A100A', outline=ORANGE)

# Date / Time (top-right)
ltext(628, 10, '24/05/2026', font=F_SMALL, fill=LABEL)
rrect(628, 20, 748, 36, r=2, fill=INPUT_BG, outline=INPUT_EDGE)
ltext(628, 38, '10:30:45 AM', font=F_SMALL, fill=LABEL)

# ══════════════════════════════════════════════════════════════════════════════
# LEFT PANEL — "Record Info"   x 5–248, y 57–402
# ══════════════════════════════════════════════════════════════════════════════
PL_X, PL_Y, PL_W, PL_H = 5, 57, 243, 345
rrect(PL_X, PL_Y, PL_X+PL_W, PL_Y+PL_H, r=6, fill=PANEL_BG, outline=PANEL_EDGE, lw=1)
ctext(draw, PL_X+PL_W//2, PL_Y+8, 'RECORD INFO', F_H2, WHITE)
draw.line([PL_X+1, PL_Y+28, PL_X+PL_W-1, PL_Y+28], fill=DIVIDER)

rows_l = [
    ('Record #',   PL_Y+56),
    ('Status',     PL_Y+88),
    ('Mode',       PL_Y+120),
    ('Error Code', PL_Y+152),
    ('Duration',   PL_Y+184),
]
for lbl, ry in rows_l:
    ltext(PL_X+10, ry-14, lbl, fill=LABEL)
    ibox(PL_X+110, ry-18, 125, 22)
    if lbl == 'Duration':
        ltext(PL_X+237, ry-14, 'sec', font=F_SMALL, fill=LABEL)

# Pending ACK lamp area
draw.line([PL_X+1, PL_Y+216, PL_X+PL_W-1, PL_Y+216], fill=DIVIDER)
ctext(draw, PL_X+PL_W//2, PL_Y+228, 'New Record', F_LABEL, LABEL)
CX, CY = PL_X+PL_W//2, PL_Y+280
draw.ellipse([CX-30, CY-30, CX+30, CY+30], outline=TEAL, width=2)
ctext(draw, CX, CY-10, 'LAMP', F_SMALL, TEAL_DIM)
ctext(draw, CX, CY+4,  '4x,175', F_SMALL, DIVIDER)
ltext(CX-15, PL_Y+320, 'LB 1506', font=F_SMALL, fill=LABEL)

# ══════════════════════════════════════════════════════════════════════════════
# CENTER PANEL — "Fill Data"   x 256–538, y 57–402
# ══════════════════════════════════════════════════════════════════════════════
PC_X, PC_Y, PC_W, PC_H = 256, 57, 283, 345
rrect(PC_X, PC_Y, PC_X+PC_W, PC_Y+PC_H, r=6, fill=PANEL_BG, outline=PANEL_EDGE, lw=1)
ctext(draw, PC_X+PC_W//2, PC_Y+8, 'FILL DATA', F_H2, WHITE)
draw.line([PC_X+1, PC_Y+28, PC_X+PC_W-1, PC_Y+28], fill=DIVIDER)

# Large Actual Net KG display
ctext(draw, PC_X+PC_W//2, PC_Y+36, 'Actual Net KG', F_LABEL, LABEL)
rrect(PC_X+10, PC_Y+50, PC_X+PC_W-10, PC_Y+110, r=5, fill=INPUT_BG, outline=INPUT_EDGE, lw=2)

# Target + Tare pair
ltext(PC_X+22, PC_Y+120, 'Target KG', font=F_SMALL, fill=LABEL)
ibox(PC_X+10, PC_Y+132, 120, 24)
ltext(PC_X+163, PC_Y+120, 'Tare KG', font=F_SMALL, fill=LABEL)
ibox(PC_X+148, PC_Y+132, 120, 24)

# Delta vs target
ctext(draw, PC_X+PC_W//2, PC_Y+170, 'Δ vs Target (kg)', F_LABEL, LABEL)
ibox(PC_X+60, PC_Y+183, 163, 24)

# Fill progress bar
ltext(PC_X+10, PC_Y+220, 'Fill Progress', fill=LABEL)
rrect(PC_X+10, PC_Y+234, PC_X+PC_W-10, PC_Y+252, r=9, fill=INPUT_BG, outline=INPUT_EDGE)

# Machine Status lamps
draw.line([PC_X+1, PC_Y+264, PC_X+PC_W-1, PC_Y+264], fill=DIVIDER)
ltext(PC_X+10, PC_Y+271, 'Machine Status', fill=LABEL)
lamp_data = [('E-Stop', PC_X+46), ('Cylinder', PC_X+118), ('Nozzle', PC_X+192), ('Stable', PC_X+256)]
for ll, lx in lamp_data:
    draw.ellipse([lx-18, PC_Y+293, lx+18, PC_Y+329], outline=INPUT_EDGE, width=2)
    bbox = draw.textbbox((0,0), ll, font=F_SMALL)
    tw = bbox[2]-bbox[0]
    draw.text((lx-tw//2, PC_Y+333), ll, font=F_SMALL, fill=LABEL)

# ══════════════════════════════════════════════════════════════════════════════
# RIGHT PANEL — "Amount Data"   x 549–792, y 57–402
# ══════════════════════════════════════════════════════════════════════════════
PR_X, PR_Y, PR_W, PR_H = 549, 57, 244, 345
rrect(PR_X, PR_Y, PR_X+PR_W, PR_Y+PR_H, r=6, fill=PANEL_BG, outline=PANEL_EDGE, lw=1)
ctext(draw, PR_X+PR_W//2, PR_Y+8, 'AMOUNT DATA', F_H2, WHITE)
draw.line([PR_X+1, PR_Y+28, PR_X+PR_W-1, PR_Y+28], fill=DIVIDER)

ltext(PR_X+10, PR_Y+42, 'Rate / KG', fill=LABEL)
ibox(PR_X+90, PR_Y+38, 130, 22)
ltext(PR_X+222, PR_Y+42, 'PKR', font=F_SMALL, fill=LABEL)

ltext(PR_X+10, PR_Y+74, 'Target Amt', fill=LABEL)
ibox(PR_X+90, PR_Y+70, 130, 22)
ltext(PR_X+222, PR_Y+74, 'PKR', font=F_SMALL, fill=LABEL)

ctext(draw, PR_X+PR_W//2, PR_Y+106, 'Final Amount (PKR)', F_LABEL, LABEL)
rrect(PR_X+10, PR_Y+120, PR_X+PR_W-10, PR_Y+178, r=5, fill=INPUT_BG, outline=INPUT_EDGE, lw=2)

draw.line([PR_X+10, PR_Y+188, PR_X+PR_W-10, PR_Y+188], fill=DIVIDER)

# ACKNOWLEDGE button (teal style, like the action buttons in Operator)
rrect(PR_X+10, PR_Y+198, PR_X+PR_W-10, PR_Y+250, r=8, fill=TEAL_DIM, outline=TEAL, lw=2)
ctext(draw, PR_X+PR_W//2, PR_Y+213, 'ACKNOWLEDGE', F_BTN, TEAL_LIGHT)
ctext(draw, PR_X+PR_W//2, PR_Y+230, 'Enabled when pending', F_SMALL, TEAL_DIM)

# Go to Operator (grey style)
rrect(PR_X+10, PR_Y+262, PR_X+PR_W-10, PR_Y+302, r=8, fill=PANEL_BG, outline=PANEL_EDGE, lw=1)
ctext(draw, PR_X+PR_W//2, PR_Y+276, '◀  Operator Screen', F_LABEL, LABEL)

# ══════════════════════════════════════════════════════════════════════════════
# NAVIGATION BAR   y 408–452
# ══════════════════════════════════════════════════════════════════════════════
draw.rectangle([0, 408, W, 452], fill=HEADER_BG)
nav_tabs = ['Alarm', 'Reports', 'Service', 'Diagnostics', 'Operator']
for i, tab in enumerate(nav_tabs):
    tx = i * 160
    if tab == 'Reports':
        draw.rectangle([tx, 408, tx+160, 452], fill=TEAL_DIM)
        draw.rectangle([tx, 408, tx+161, 413], fill=TEAL)
        ctext(draw, tx+80, 427, tab, F_H2, TEAL_LIGHT)
    else:
        ctext(draw, tx+80, 427, tab, F_LABEL, LABEL)
    if i > 0:
        draw.line([tx, 410, tx, 450], fill=DIVIDER)

# ══════════════════════════════════════════════════════════════════════════════
# FOOTER   y 453–480
# ══════════════════════════════════════════════════════════════════════════════
draw.rectangle([0, 453, W, 480], fill=FOOTER_BG)
draw.text((10, 461), 'RECOMMENDED NEXT STEP', font=F_FOOT, fill=HEADER_BG)
rrect(225, 458, W-10, 475, r=3, fill=TEAL_DIM, outline='#0A5A6A')

out = os.path.join(os.path.dirname(__file__), 'history_screen_background_800x480.png')
img.save(out)
print(f'PNG saved: {out}')
