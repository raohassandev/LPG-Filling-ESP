#include "screens/DashboardScreen.h"
#include "DisplayFormat.h"
#include "Theme.h"
#include "ScreenManager.h"
#include "esp_log.h"
#include <cstdarg>
#include <cstdio>
#include <stdlib.h>
#include <string.h>

extern ScreenManager screenManager;
extern ModbusClient  modbusClient;

static const char* TAG = "DASH";

// ── helpers ───────────────────────────────────────────────────────────────────

static const char* stateLabel(FillState s) {
  switch (s) {
    case FillState::Idle:        return "IDLE";
    case FillState::Ready:       return "READY";
    case FillState::Validating:  return "VALIDATE";
    case FillState::Fast:        return "FAST";
    case FillState::Slow:        return "SLOW";
    case FillState::Settling:    return "SETTLE";
    case FillState::Complete:    return "DONE";
    case FillState::Aborted:     return "ABORT";
    case FillState::Fault:       return "FAULT";
    case FillState::Maintenance: return "SERVICE";
    default:                     return "UNKNOWN";
  }
}

static const char* alertTitle(const ControllerSnapshot& s) {
  if (!s.connected) return "Controller link lost";
  if (!s.eStopOk) return "Emergency stop active";
  if (!s.cylinderPresent) return "Cylinder not detected";
  if (!s.nozzleEngaged) return "Nozzle not engaged";
  if (!s.weightStable && (s.state == FillState::Idle || s.state == FillState::Ready))
    return "Scale is still settling";
  switch (s.state) {
    case FillState::Fault:    return "Fault requires reset";
    case FillState::Aborted:  return "Fill aborted";
    case FillState::Complete: return "Fill complete";
    default:                  return "System ready";
  }
}

static const char* alertBody(const ControllerSnapshot& s) {
  if (!s.connected) return "Check RS485 wiring and controller power.";
  if (!s.eStopOk) return "Release the emergency push button, then press RESET.";
  if (!s.cylinderPresent) return "Place cylinder correctly on the platform.";
  if (!s.nozzleEngaged) return "Connect nozzle securely before filling.";
  if (!s.weightStable && (s.state == FillState::Idle || s.state == FillState::Ready))
    return "Wait until the scale indicator turns ready.";
  switch (s.state) {
    case FillState::Fault:    return "Outputs are safe. Inspect the alarm condition, then press RESET.";
    case FillState::Aborted:  return "Press RESET to return the controller to idle.";
    case FillState::Complete: return "Transaction closed. Press RESET before the next fill if needed.";
    default:                  return "All primary readiness checks are healthy.";
  }
}

static lv_color_t alertColor(const ControllerSnapshot& s) {
  if (!s.connected || s.state == FillState::Fault || !s.eStopOk) return TC::danger();
  if (s.state == FillState::Aborted || !s.cylinderPresent || !s.nozzleEngaged || !s.weightStable) return TC::warning();
  if (s.state == FillState::Complete) return TC::complete();
  return TC::ready();
}

static lv_color_t stateColor(FillState s) {
  switch (s) {
    case FillState::Ready:      return TC::ready();
    case FillState::Fast:       return TC::active();
    case FillState::Slow:       return TC::warning();
    case FillState::Settling:   return TC::settling();
    case FillState::Complete:   return TC::complete();
    case FillState::Fault:
    case FillState::Aborted:    return TC::danger();
    default:                    return TC::muted();
  }
}

static bool fne(float a, float b, float eps = 0.005f) { return (a - b) > eps || (b - a) > eps; }

static void setReadyIcon(lv_obj_t* iconLbl, bool ok, lv_color_t activeColor);

static lv_obj_t* iconRoot(lv_obj_t* parent) {
  lv_obj_t* root = lv_obj_create(parent);
  lv_obj_set_size(root, 34, 30);
  lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(root, 0, 0);
  lv_obj_set_style_pad_all(root, 0, 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(root, LV_ALIGN_TOP_MID, 0, 3);
  return root;
}

static lv_obj_t* iconShape(lv_obj_t* parent, int x, int y, int w, int h, int radius = 2) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_bg_color(o, TC::muted(), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

static lv_obj_t* makeSymbolIcon(lv_obj_t* parent, const char* sym) {
  lv_obj_t* root = iconRoot(parent);
  lv_obj_t* lbl = lv_label_create(root);
  lv_label_set_text(lbl, sym);
  lv_obj_set_style_text_font(lbl, TF::lg(), 0);
  lv_obj_set_style_text_color(lbl, TC::muted(), 0);
  lv_obj_center(lbl);
  return root;
}

static lv_obj_t* makeCylinderIcon(lv_obj_t* parent) {
  lv_obj_t* root = iconRoot(parent);
  iconShape(root, 12, 1, 10, 4, 2);
  iconShape(root, 8, 5, 18, 22, 5);
  iconShape(root, 11, 8, 12, 3, 2);
  iconShape(root, 11, 21, 12, 3, 2);
  return root;
}

static lv_obj_t* makeNozzleIcon(lv_obj_t* parent) {
  lv_obj_t* root = iconRoot(parent);
  iconShape(root, 4, 8, 21, 6, 2);
  iconShape(root, 24, 6, 6, 10, 2);
  iconShape(root, 10, 14, 6, 10, 2);
  iconShape(root, 18, 20, 10, 7, 2);
  lv_obj_t* shackle = lv_obj_create(root);
  lv_obj_set_size(shackle, 8, 8);
  lv_obj_set_pos(shackle, 19, 15);
  lv_obj_set_style_bg_opa(shackle, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(shackle, TC::muted(), 0);
  lv_obj_set_style_border_width(shackle, 2, 0);
  lv_obj_set_style_border_side(shackle, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT, 0);
  lv_obj_set_style_radius(shackle, 4, 0);
  lv_obj_set_style_pad_all(shackle, 0, 0);
  lv_obj_clear_flag(shackle, LV_OBJ_FLAG_SCROLLABLE);
  return root;
}

static void setLabelTextIfChanged(lv_obj_t* label, const char* text) {
  if (!label || !text) return;
  const char* old = lv_label_get_text(label);
  if (old && strcmp(old, text) == 0) return;
  lv_label_set_text(label, text);
}

static void setLabelFmtIfChanged(lv_obj_t* label, const char* fmt, ...) {
  if (!label || !fmt) return;
  char buf[96];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  setLabelTextIfChanged(label, buf);
}

static bool snapChanged(const ControllerSnapshot& a, const ControllerSnapshot& b) {
  return a.state           != b.state
      || a.connected       != b.connected
      || a.eStopOk         != b.eStopOk
      || a.cylinderPresent != b.cylinderPresent
      || a.nozzleEngaged   != b.nozzleEngaged
      || a.weightStable    != b.weightStable
      || fne(a.liveWeightKg, b.liveWeightKg)
      || fne(a.tareWeightKg, b.tareWeightKg)
      || fne(a.netWeightKg,  b.netWeightKg)
      || fne(a.currentAmount, b.currentAmount, 0.5f)
      || a.rtcHour         != b.rtcHour
      || a.rtcMinute       != b.rtcMinute
      || a.rtcSecond       != b.rtcSecond
      || a.todayFills      != b.todayFills
      || fne(a.todayKg,     b.todayKg, 0.05f)
      || fne(a.todayAmount, b.todayAmount, 0.5f);
}

// ── build ─────────────────────────────────────────────────────────────────────

void DashboardScreen::build(ModbusClient& mbus) {
  mbus_ = &mbus;
  scr_  = lv_obj_create(nullptr);
  Theme::applyScreenBg(scr_);

  // ── Status bar (full width, 58 px) ──────────────────────────────────────────
  lv_obj_t* bar = lv_obj_create(scr_);
  lv_obj_set_size(bar, 800, 58);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, TC::surface(), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_pad_hor(bar, 20, 0);
  lv_obj_set_style_pad_ver(bar, 0, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  // Bottom border line on status bar
  lv_obj_t* barLine = lv_obj_create(scr_);
  lv_obj_set_size(barLine, 800, 2);
  lv_obj_set_pos(barLine, 0, 58);
  lv_obj_set_style_bg_color(barLine, TC::border(), 0);
  lv_obj_set_style_bg_opa(barLine, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(barLine, 0, 0);
  lv_obj_set_style_radius(barLine, 0, 0);

  // Title
  Theme::label(bar, "LPG FILLING STATION", TF::lg(), TC::text());
  lv_obj_t* title = lv_obj_get_child(bar, 0);
  lv_obj_set_width(title, 300);
  lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

  // Fixed-position header controls avoid overlap on the physical 800x480 panel.
  stateBadge_ = lv_obj_create(bar);
  lv_obj_set_size(stateBadge_, 102, 32);
  lv_obj_set_style_pad_hor(stateBadge_, 8, 0);
  lv_obj_set_style_pad_ver(stateBadge_, 0, 0);
  lv_obj_set_style_radius(stateBadge_, 16, 0);
  lv_obj_set_style_border_width(stateBadge_, 0, 0);
  lv_obj_set_style_bg_color(stateBadge_, TC::muted(), 0);
  lv_obj_set_style_bg_opa(stateBadge_, LV_OPA_COVER, 0);
  lv_obj_set_pos(stateBadge_, 324, 13);
  lv_obj_clear_flag(stateBadge_, LV_OBJ_FLAG_SCROLLABLE);

  lblState_ = lv_label_create(stateBadge_);
  lv_label_set_text(lblState_, "IDLE");
  lv_obj_set_width(lblState_, 86);
  lv_label_set_long_mode(lblState_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(lblState_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(lblState_, TF::md(), 0);
  lv_obj_set_style_text_color(lblState_, TC::white(), 0);
  lv_obj_center(lblState_);

  // Right side of status bar: wifi | time | role
  lv_obj_t* btnWifi = Theme::button(bar, LV_SYMBOL_WIFI,
                                    TC::surface2(), TC::textSub(), 42, 34);
  lv_obj_set_pos(btnWifi, 446, 12);
  lv_obj_add_event_cb(btnWifi, onWifiPressed, LV_EVENT_CLICKED, this);

  lblTime_ = lv_label_create(bar);
  lv_label_set_text(lblTime_, "--:--");
  lv_obj_set_size(lblTime_, 76, 28);
  lv_label_set_long_mode(lblTime_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(lblTime_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(lblTime_, TF::lg(), 0);
  lv_obj_set_style_text_color(lblTime_, TC::textSub(), 0);
  lv_obj_set_pos(lblTime_, 500, 15);

  lblMbus_ = lv_label_create(bar);
  lv_label_set_text(lblMbus_, LV_SYMBOL_CLOSE " RTU");
  lv_obj_set_size(lblMbus_, 64, 24);
  lv_label_set_long_mode(lblMbus_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(lblMbus_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(lblMbus_, TF::sm(), 0);
  lv_obj_set_style_text_color(lblMbus_, TC::danger(), 0);
  lv_obj_set_pos(lblMbus_, 576, 18);

  // Role button (password-protected role selector)
  btnRole_ = Theme::button(bar, LV_SYMBOL_SETTINGS,
                            TC::surface2(), TC::textSub(), 138, 34);
  lv_obj_set_pos(btnRole_, 644, 12);
  lv_obj_add_event_cb(btnRole_, onRolePressed, LV_EVENT_CLICKED, this);
  // Update the button label text after creation (child 0 of btn is the label)
  lblRoleBtn_ = lv_obj_get_child(btnRole_, 0);
  lv_label_set_text(lblRoleBtn_, LV_SYMBOL_SETTINGS " OPERATOR");
  lv_obj_set_style_text_font(lblRoleBtn_, TF::sm(), 0);

  // ── Left column: Weight card (x=16, y=68, 460×258) ──────────────────────────
  lv_obj_t* wCard = lv_obj_create(scr_);
  lv_obj_set_size(wCard, 460, 198);
  lv_obj_set_pos(wCard, 16, 68);
  Theme::applyCard(wCard);
  lv_obj_set_style_pad_all(wCard, 0, 0);
  lv_obj_set_style_border_width(wCard, 0, 0);
  lv_obj_set_style_bg_color(wCard, TC::surface(), 0);
  // Left accent border updated dynamically
  weightAccent_ = lv_obj_create(wCard);
  lv_obj_set_size(weightAccent_, 4, LV_PCT(100));
  lv_obj_set_pos(weightAccent_, 0, 0);
  lv_obj_set_style_bg_color(weightAccent_, TC::muted(), 0);
  lv_obj_set_style_bg_opa(weightAccent_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(weightAccent_, 0, 0);
  lv_obj_set_style_radius(weightAccent_, 0, 0);
  lv_obj_set_style_radius(wCard, 10, 0);

  lv_obj_t* wInner = lv_obj_create(wCard);
  lv_obj_set_size(wInner, 456 - 4, LV_PCT(100));
  lv_obj_set_pos(wInner, 4, 0);
  lv_obj_set_style_bg_opa(wInner, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(wInner, 0, 0);
  lv_obj_set_style_pad_hor(wInner, 16, 0);
  lv_obj_set_style_pad_ver(wInner, 14, 0);
  lv_obj_clear_flag(wInner, LV_OBJ_FLAG_SCROLLABLE);

  Theme::label(wInner, "LIVE WEIGHT", TF::sm(), TC::textSub());
  lv_obj_t* lWLabel = lv_obj_get_child(wInner, 0);
  lv_obj_align(lWLabel, LV_ALIGN_TOP_LEFT, 0, 0);

  lblLive_ = lv_label_create(wInner);
  lv_label_set_text(lblLive_, "0.000");
  lv_obj_set_width(lblLive_, 280);
  lv_label_set_long_mode(lblLive_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(lblLive_, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_text_font(lblLive_, TF::hero(), 0);
  lv_obj_set_style_text_color(lblLive_, TC::text(), 0);
  lv_obj_align(lblLive_, LV_ALIGN_LEFT_MID, 0, -14);

  lv_obj_t* unitLbl = Theme::label(wInner, "kg", TF::xl(), TC::textSub());
  lv_obj_align(unitLbl, LV_ALIGN_LEFT_MID, 0, 24);

  // Divider line
  lv_obj_t* wDiv = lv_obj_create(wInner);
  lv_obj_set_size(wDiv, LV_PCT(100), 1);
  lv_obj_align(wDiv, LV_ALIGN_BOTTOM_LEFT, 0, -40);
  lv_obj_set_style_bg_color(wDiv, TC::border(), 0);
  lv_obj_set_style_bg_opa(wDiv, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(wDiv, 0, 0);

  // Tare / Net row at bottom
  lv_obj_t* subRow = lv_obj_create(wInner);
  lv_obj_set_size(subRow, LV_PCT(100), 32);
  lv_obj_align(subRow, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(subRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(subRow, 0, 0);
  lv_obj_set_style_pad_all(subRow, 0, 0);
  lv_obj_clear_flag(subRow, LV_OBJ_FLAG_SCROLLABLE);

  lblTare_ = lv_label_create(subRow);
  lv_label_set_text(lblTare_, "Tare: 0.000 kg");
  lv_obj_set_width(lblTare_, 210);
  lv_label_set_long_mode(lblTare_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(lblTare_, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_text_font(lblTare_, TF::md(), 0);
  lv_obj_set_style_text_color(lblTare_, TC::textSub(), 0);
  lv_obj_align(lblTare_, LV_ALIGN_LEFT_MID, 0, 0);

  lblNet_ = lv_label_create(subRow);
  lv_label_set_text(lblNet_, "Net: 0.000 kg");
  lv_obj_set_width(lblNet_, 210);
  lv_label_set_long_mode(lblNet_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(lblNet_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_font(lblNet_, TF::md(), 0);
  lv_obj_set_style_text_color(lblNet_, TC::active(), 0);
  lv_obj_align(lblNet_, LV_ALIGN_RIGHT_MID, 0, 0);

  // ── Fill Params strip (x=16, y=274, 460×92) ──────────────────────────────
  lv_obj_t* fpCard = lv_obj_create(scr_);
  lv_obj_set_size(fpCard, 460, 92);
  lv_obj_set_pos(fpCard, 16, 274);
  Theme::applyCard(fpCard);
  lv_obj_set_style_pad_all(fpCard, 0, 0);
  lv_obj_clear_flag(fpCard, LV_OBJ_FLAG_SCROLLABLE);

  const char* fpLabels[3] = { "TARGET kg", "RATE PKR/kg", "AMOUNT PKR" };
  lv_obj_t**  fpCells[3]  = { &fpCellTarget_, &fpCellRate_, &fpCellAmount_ };
  lv_obj_t**  fpVals[3]   = { &lblTarget_,    &lblRate_,    &lblAmount_ };

  for (int i = 0; i < 3; i++) {
    *fpCells[i] = lv_obj_create(fpCard);
    lv_obj_set_size(*fpCells[i], 152, 90);
    lv_obj_set_pos(*fpCells[i], i * 154, 0);
    lv_obj_set_style_bg_color(*fpCells[i], i < 2 ? TC::surface2() : TC::bg(), 0);
    lv_obj_set_style_bg_opa(*fpCells[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(*fpCells[i], TC::border(), 0);
    lv_obj_set_style_border_width(*fpCells[i], i > 0 ? 1 : 0, 0);
    lv_obj_set_style_border_side(*fpCells[i], LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_radius(*fpCells[i], 0, 0);
    lv_obj_set_style_pad_hor(*fpCells[i], 10, 0);
    lv_obj_set_style_pad_ver(*fpCells[i], 8, 0);
    lv_obj_clear_flag(*fpCells[i], LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hdr = Theme::label(*fpCells[i], fpLabels[i], TF::sm(), TC::muted());
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    *fpVals[i] = lv_label_create(*fpCells[i]);
    lv_obj_set_width(*fpVals[i], 132);
    lv_label_set_long_mode(*fpVals[i], LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(*fpVals[i], TF::xl(), 0);
    lv_obj_set_style_text_color(*fpVals[i], i < 2 ? TC::active() : TC::ready(), 0);
    lv_label_set_text(*fpVals[i], i == 0 ? "12.0" : i == 1 ? "250" : "0");
    lv_obj_align(*fpVals[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);

    if (i < 2) {
      lv_obj_add_flag(*fpCells[i], LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_user_data(*fpCells[i], (void*)(uintptr_t)i);
      lv_obj_add_event_cb(*fpCells[i], onFpCellTapped, LV_EVENT_CLICKED, this);
    }
  }

  // ── Left column: Today stats card (x=16, y=334) ───────────────────────────
  lv_obj_t* sCard = lv_obj_create(scr_);
  lv_obj_set_size(sCard, 460, 100);
  lv_obj_set_pos(sCard, 16, 374);
  Theme::applyCard(sCard);

  Theme::label(sCard, "TODAY", TF::sm(), TC::textSub());
  lv_obj_t* sLabel = lv_obj_get_child(sCard, 0);
  lv_obj_align(sLabel, LV_ALIGN_TOP_LEFT, 0, 0);

  // 3 stat boxes in a row
  const char* statIcons[] = { LV_SYMBOL_LIST, LV_SYMBOL_DOWNLOAD, LV_SYMBOL_CHARGE };
  lv_color_t  statColors[] = { TC::text(), TC::active(), TC::ready() };
  lv_obj_t**  statLabels[] = { &lblTodayFills_, &lblTodayKg_, &lblTodayAmt_ };

  for (int i = 0; i < 3; i++) {
    lv_obj_t* box = lv_obj_create(sCard);
    lv_obj_set_size(box, 126, 54);
    lv_obj_set_pos(box, i * 134, 28);
    lv_obj_set_style_bg_color(box, TC::surface2(), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_radius(box, 8, 0);
    lv_obj_set_style_pad_all(box, 6, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    Theme::label(box, statIcons[i], TF::sm(), TC::muted());
    lv_obj_t* icon = lv_obj_get_child(box, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0);

    *statLabels[i] = lv_label_create(box);
    lv_label_set_text(*statLabels[i], "—");
    lv_obj_set_style_text_font(*statLabels[i], TF::lg(), 0);
    lv_obj_set_style_text_color(*statLabels[i], statColors[i], 0);
    lv_obj_align(*statLabels[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
  }

  // ── Right column: compact readiness strip (x=492, y=68, 292×68) ───────────
  lv_obj_t* rCard = lv_obj_create(scr_);
  lv_obj_set_size(rCard, 292, 84);
  lv_obj_set_pos(rCard, 492, 68);
  Theme::applyCard(rCard);
  lv_obj_set_style_pad_all(rCard, 4, 0);

  // 4 status cells: E-stop, cylinder, nozzle locked/engaged, stable scale.
  static const char* kReadyText[4] = {
    "E-STOP", "CYL", "NOZZLE", "SCALE"
  };
  lv_obj_t** readyRefs[4] = { &dotEstop_, &dotCylinder_, &dotNozzle_, &dotStable_ };

  for (int i = 0; i < 4; i++) {
    lv_obj_t* cell = lv_obj_create(rCard);
    lv_obj_set_size(cell, 66, 56);
    lv_obj_set_pos(cell, i * 70, 5);
    lv_obj_set_style_bg_color(cell, TC::surface2(), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cell, TC::border(), 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_radius(cell, 8, 0);
    lv_obj_set_style_pad_all(cell, 0, 0);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    if (i == 0)      *readyRefs[i] = makeSymbolIcon(cell, LV_SYMBOL_POWER);
    else if (i == 1) *readyRefs[i] = makeCylinderIcon(cell);
    else if (i == 2) *readyRefs[i] = makeNozzleIcon(cell);
    else             *readyRefs[i] = makeSymbolIcon(cell, LV_SYMBOL_OK);

    lv_obj_t* txt = lv_label_create(cell);
    lv_label_set_text(txt, kReadyText[i]);
    lv_obj_set_width(txt, 62);
    lv_label_set_long_mode(txt, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(txt, TF::sm(), 0);
    lv_obj_set_style_text_color(txt, TC::textSub(), 0);
    lv_obj_align(txt, LV_ALIGN_BOTTOM_MID, 0, -5);
  }

  alertCard_ = lv_obj_create(scr_);
  lv_obj_set_size(alertCard_, 292, 122);
  lv_obj_set_pos(alertCard_, 492, 160);
  Theme::applyCard(alertCard_);
  lv_obj_set_style_pad_all(alertCard_, 12, 0);
  lv_obj_clear_flag(alertCard_, LV_OBJ_FLAG_SCROLLABLE);

  lblAlertTitle_ = lv_label_create(alertCard_);
  lv_label_set_text(lblAlertTitle_, "System ready");
  lv_obj_set_width(lblAlertTitle_, 260);
  lv_label_set_long_mode(lblAlertTitle_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(lblAlertTitle_, TF::md(), 0);
  lv_obj_set_style_text_color(lblAlertTitle_, TC::ready(), 0);
  lv_obj_align(lblAlertTitle_, LV_ALIGN_TOP_LEFT, 0, 0);

  lblAlertBody_ = lv_label_create(alertCard_);
  lv_label_set_text(lblAlertBody_, "All primary readiness checks are healthy.");
  lv_obj_set_width(lblAlertBody_, 260);
  lv_label_set_long_mode(lblAlertBody_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(lblAlertBody_, TF::sm(), 0);
  lv_obj_set_style_text_color(lblAlertBody_, TC::textSub(), 0);
  lv_obj_align(lblAlertBody_, LV_ALIGN_TOP_LEFT, 0, 32);

  // ── Right column: Start Fill button (bottom-right, compact action) ───────
  btnStart_ = lv_obj_create(scr_);
  lv_obj_set_size(btnStart_, 292, 78);
  lv_obj_set_pos(btnStart_, 492, 356);
  lv_obj_set_style_bg_color(btnStart_, TC::muted(), 0);
  lv_obj_set_style_bg_opa(btnStart_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btnStart_, 0, 0);
  lv_obj_set_style_radius(btnStart_, 10, 0);
  lv_obj_set_style_pad_all(btnStart_, 0, 0);
  lv_obj_clear_flag(btnStart_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(btnStart_, onStartPressed, LV_EVENT_CLICKED, this);

  lblActionIcon_ = Theme::label(btnStart_, LV_SYMBOL_PLAY, TF::xl(), TC::white());
  lv_obj_align(lblActionIcon_, LV_ALIGN_CENTER, 0, -16);

  lblActionText_ = Theme::label(btnStart_, "START FILL", TF::lg(), TC::white());
  lv_obj_align(lblActionText_, LV_ALIGN_CENTER, 0, 18);
}

// ── update ────────────────────────────────────────────────────────────────────

void DashboardScreen::update(const ControllerSnapshot& snap) {
  if (!scr_) return;
  if (!firstUpdate_ && !snapChanged(snap, lastSnap_)) return;
  const ControllerSnapshot prev = lastSnap_;
  const bool fullRefresh = firstUpdate_;
  firstUpdate_ = false;
  lastSnap_ = snap;

  // State badge
  if (fullRefresh || snap.state != prev.state) {
    lv_color_t sc = stateColor(snap.state);
    setLabelTextIfChanged(lblState_, stateLabel(snap.state));
    lv_obj_set_style_bg_color(stateBadge_, sc, 0);

    // Weight accent border color mirrors state
    lv_obj_set_style_bg_color(weightAccent_, sc, 0);
  }

  // Time
  if (fullRefresh || snap.rtcHour != prev.rtcHour || snap.rtcMinute != prev.rtcMinute || snap.rtcSecond != prev.rtcSecond) {
    if (snap.rtcHour <= 23 && snap.rtcMinute <= 59 &&
        (snap.rtcHour != 0 || snap.rtcMinute != 0 || snap.rtcSecond != 0)) {
      setLabelFmtIfChanged(lblTime_, "%02u:%02u:%02u", snap.rtcHour, snap.rtcMinute, snap.rtcSecond);
    } else {
      setLabelTextIfChanged(lblTime_, "--:--:--");
    }
  }

  if (fullRefresh || snap.connected != prev.connected) {
    setLabelTextIfChanged(lblMbus_, snap.connected ? LV_SYMBOL_OK " RTU" : LV_SYMBOL_CLOSE " RTU");
    lv_obj_set_style_text_color(lblMbus_, snap.connected ? TC::ready() : TC::danger(), 0);
  }

  // Live weight (no "kg" in hero — separate unit label)
  display_label_setf(lblLive_, "%.3f", snap.liveWeightKg);
  display_label_setf(lblTare_, "Tare: %.3f kg", snap.tareWeightKg);
  display_label_setf(lblNet_,  "Net: %.3f kg",  snap.netWeightKg);

  // Readiness icons — only restyle the icon whose boolean changed.
  if (fullRefresh || snap.eStopOk != prev.eStopOk)
    setReadyIcon(dotEstop_,    snap.eStopOk,         snap.eStopOk ? TC::ready()  : TC::danger());
  if (fullRefresh || snap.cylinderPresent != prev.cylinderPresent)
    setReadyIcon(dotCylinder_, snap.cylinderPresent,  TC::ready());
  if (fullRefresh || snap.nozzleEngaged != prev.nozzleEngaged)
    setReadyIcon(dotNozzle_,   snap.nozzleEngaged,    TC::active());
  if (fullRefresh || snap.weightStable != prev.weightStable)
    setReadyIcon(dotStable_,   snap.weightStable,     snap.weightStable ? TC::ready() : TC::warning());

  // Blink weight-stable dot while controller is actively measuring
  const bool isMeasuring = (snap.state == FillState::Validating ||
                             snap.state == FillState::Fast       ||
                             snap.state == FillState::Slow       ||
                             snap.state == FillState::Settling);
  if (isMeasuring && !snap.weightStable && !stableBlinking_) {
    startStableBlink();
    stableBlinking_ = true;
  } else if ((!isMeasuring || snap.weightStable) && stableBlinking_) {
    stopStableBlink();
    stableBlinking_ = false;
  }

  // Today stats
  setLabelFmtIfChanged(lblTodayFills_, "%u fills",   snap.todayFills);
  display_label_setf(lblTodayKg_,   "%.1f kg", snap.todayKg);
  display_label_setf(lblTodayAmt_,  "%.0f",    snap.todayAmount);
  if (snap.ratePerKg > 0.0f) lastRatePerKg_ = snap.ratePerKg;

  if (lblAmount_) {
    display_label_setf(lblAmount_, "%.0f",
                       snap.currentAmount > 0.0f ? snap.currentAmount : snap.todayAmount);
  }

  updateAlert(snap, fullRefresh);

  // Start button — tappable while idle/ready so offline taps can show guidance.
  const bool needsReset = snap.state == FillState::Fault ||
                          snap.state == FillState::Aborted ||
                          snap.state == FillState::Complete ||
                          (!snap.eStopOk && snap.connected);
  const bool canStart = (snap.state == FillState::Idle || snap.state == FillState::Ready)
                        && (snap.eStopOk || !snap.connected);
  const bool canAct = canStart || needsReset;
  const bool prevCanStart = (prev.state == FillState::Idle || prev.state == FillState::Ready)
                            && (prev.eStopOk || !prev.connected);
  const bool prevNeedsReset = prev.state == FillState::Fault ||
                              prev.state == FillState::Aborted ||
                              prev.state == FillState::Complete ||
                              (!prev.eStopOk && prev.connected);
  if (fullRefresh || canStart != prevCanStart || needsReset != prevNeedsReset) {
    lv_obj_set_style_bg_color(btnStart_, needsReset ? TC::warning() : (canStart ? TC::active() : TC::muted()), 0);
    setLabelTextIfChanged(lblActionIcon_, needsReset ? LV_SYMBOL_REFRESH : LV_SYMBOL_PLAY);
    setLabelTextIfChanged(lblActionText_, needsReset ? "RESET" : "START FILL");
    if (canAct) lv_obj_add_flag(btnStart_, LV_OBJ_FLAG_CLICKABLE);
    else        lv_obj_clear_flag(btnStart_, LV_OBJ_FLAG_CLICKABLE);
  }
}

void DashboardScreen::updateAlert(const ControllerSnapshot& snap, bool fullRefresh) {
  if (!alertCard_) return;
  const lv_color_t c = alertColor(snap);
  if (fullRefresh) lv_obj_set_style_border_width(alertCard_, 1, 0);
  setLabelTextIfChanged(lblAlertTitle_, alertTitle(snap));
  setLabelTextIfChanged(lblAlertBody_, alertBody(snap));
  lv_obj_set_style_border_color(alertCard_, c, 0);
  lv_obj_set_style_text_color(lblAlertTitle_, c, 0);
}

static void tintIconTree(lv_obj_t* obj, lv_color_t color) {
  if (!obj) return;
  lv_obj_set_style_text_color(obj, color, 0);
  lv_obj_set_style_bg_color(obj, color, 0);
  lv_obj_set_style_border_color(obj, color, 0);
  const uint32_t count = lv_obj_get_child_cnt(obj);
  for (uint32_t i = 0; i < count; i++) {
    tintIconTree(lv_obj_get_child(obj, i), color);
  }
}

// iconLbl is the lv_label_create()'d symbol inside a readiness cell.
// activeColor is the colour to use when ok=true.
static void setReadyIcon(lv_obj_t* iconLbl, bool ok, lv_color_t activeColor) {
  lv_color_t fg = ok ? activeColor : TC::muted();
  tintIconTree(iconLbl, fg);
  lv_obj_t* cell = lv_obj_get_parent(iconLbl);
  lv_obj_set_style_border_color(cell, ok ? activeColor : TC::border(), 0);
  // Subtle tinted background when active
  lv_obj_set_style_bg_opa(cell, ok ? 40 : LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(cell, ok ? activeColor : TC::surface2(), 0);
}

void DashboardScreen::updateDot(lv_obj_t* iconLbl, bool ok) {
  // Legacy wrapper — kept for the blink helper which still accesses dotStable_ directly.
  setReadyIcon(iconLbl, ok, TC::ready());
}

// ── Start Fill dialog — stepper-based (no keyboard to avoid crash) ────────────

// Target weight steps: 1 kg per tap, rate steps: 10 PKR per tap.
// Values stored in dialogTargetKg_ / dialogRatePerKg_ (float members).

static lv_obj_t* makeStepper(lv_obj_t* parent, int y,
                              const char* label,
                              lv_event_cb_t onMinus, lv_event_cb_t onPlus,
                              lv_obj_t** outValLabel,
                              void* userData) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, LV_PCT(100), 70);
  lv_obj_set_pos(row, 0, y);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  Theme::label(row, label, TF::sm(), TC::textSub());
  lv_obj_t* lbl = lv_obj_get_child(row, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t* btnM = Theme::button(row, " - ", TC::surface2(), TC::text(), 56, 44);
  lv_obj_align(btnM, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_event_cb(btnM, onMinus, LV_EVENT_CLICKED, userData);

  *outValLabel = lv_label_create(row);
  lv_obj_set_style_text_font(*outValLabel, TF::xxl(), 0);
  lv_obj_set_style_text_color(*outValLabel, TC::text(), 0);
  lv_obj_align(*outValLabel, LV_ALIGN_BOTTOM_MID, 0, 0);

  lv_obj_t* btnP = Theme::button(row, " + ", TC::active(), TC::white(), 56, 44);
  lv_obj_align(btnP, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(btnP, onPlus, LV_EVENT_CLICKED, userData);

  return row;
}

void DashboardScreen::openStartDialog() {
  if (startModal_) return;

  startModal_ = lv_obj_create(scr_);
  lv_obj_set_size(startModal_, 500, 340);
  lv_obj_center(startModal_);
  lv_obj_set_style_bg_color(startModal_, TC::surface(), 0);
  lv_obj_set_style_bg_opa(startModal_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(startModal_, TC::border(), 0);
  lv_obj_set_style_border_width(startModal_, 1, 0);
  lv_obj_set_style_radius(startModal_, 12, 0);
  lv_obj_set_style_pad_all(startModal_, 20, 0);
  lv_obj_clear_flag(startModal_, LV_OBJ_FLAG_SCROLLABLE);

  Theme::label(startModal_, "Configure Fill", TF::xl(), TC::text());
  lv_obj_t* title = lv_obj_get_child(startModal_, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  // Target weight stepper
  makeStepper(startModal_, 44, "Target Weight (kg)",
              onTargetMinus, onTargetPlus, &lblDialogTarget_, this);

  // Rate stepper
  makeStepper(startModal_, 148, "Rate per kg (PKR)",
              onRateMinus, onRatePlus, &lblDialogRate_, this);

  lblStartError_ = Theme::label(startModal_, "", TF::sm(), TC::danger());
  lv_obj_align(lblStartError_, LV_ALIGN_TOP_LEFT, 0, 254);

  lv_obj_t* btnCancel = Theme::button(startModal_, "CANCEL",
                                      TC::surface2(), TC::textSub(), 120, 44);
  lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_event_cb(btnCancel, onStartCancel, LV_EVENT_CLICKED, this);

  lv_obj_t* btnGo = Theme::button(startModal_, LV_SYMBOL_PLAY " START",
                                  TC::active(), TC::white(), 160, 44);
  lv_obj_align(btnGo, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(btnGo, onStartConfirm, LV_EVENT_CLICKED, this);

  updateDialogLabels();
}

void DashboardScreen::updateDialogLabels() {
  if (lblDialogTarget_)
    display_label_setf(lblDialogTarget_, "%.0f kg", dialogTargetKg_);
  if (lblDialogRate_)
    display_label_setf(lblDialogRate_, "%.0f PKR", dialogRatePerKg_);
  if (lblTarget_)
    display_label_setf(lblTarget_, "%.1f", dialogTargetKg_);
  if (lblRate_)
    display_label_setf(lblRate_, "%.0f", dialogRatePerKg_);
}

void DashboardScreen::closeStartDialog() {
  if (!startModal_) return;
  lv_obj_del(startModal_);
  startModal_      = nullptr;
  lblDialogTarget_ = nullptr;
  lblDialogRate_   = nullptr;
  lblStartError_   = nullptr;
}

void DashboardScreen::openNumOverlay(uint8_t field) {
  if (numOverlay_) return;
  numField_ = field;

  numOverlay_ = lv_obj_create(scr_);
  lv_obj_set_size(numOverlay_, 800, 480);
  lv_obj_set_pos(numOverlay_, 0, 0);
  lv_obj_set_style_bg_color(numOverlay_, TC::bg(), 0);
  lv_obj_set_style_bg_opa(numOverlay_, 180, 0);
  lv_obj_set_style_border_width(numOverlay_, 0, 0);
  lv_obj_set_style_radius(numOverlay_, 0, 0);
  lv_obj_clear_flag(numOverlay_, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* card = lv_obj_create(numOverlay_);
  lv_obj_set_size(card, 460, 110);
  lv_obj_set_pos(card, 170, 100);
  lv_obj_set_style_bg_color(card, TC::surface(), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card, TC::border(), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 12, 0);
  lv_obj_set_style_pad_all(card, 16, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  numHint_ = Theme::label(card,
    field == 0 ? "Enter target weight (kg)" : "Enter rate per kg (PKR)",
    TF::sm(), TC::textSub());
  lv_obj_align(numHint_, LV_ALIGN_TOP_LEFT, 0, 0);

  numTa_ = lv_textarea_create(card);
  lv_obj_set_size(numTa_, LV_PCT(100), 52);
  lv_obj_align(numTa_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_textarea_set_one_line(numTa_, true);
  lv_textarea_set_max_length(numTa_, 8);
  lv_textarea_set_accepted_chars(numTa_, "0123456789.");
  lv_obj_set_style_bg_color(numTa_, TC::surface2(), 0);
  lv_obj_set_style_border_color(numTa_, TC::active(), 0);
  lv_obj_set_style_text_color(numTa_, TC::text(), 0);
  lv_obj_set_style_text_font(numTa_, TF::xl(), 0);

  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", field == 0 ? dialogTargetKg_ : dialogRatePerKg_);
  lv_textarea_set_text(numTa_, buf);
  lv_textarea_set_cursor_pos(numTa_, LV_TEXTAREA_CURSOR_LAST);

  numKb_ = lv_keyboard_create(scr_);
  lv_obj_set_size(numKb_, 800, 230);
  lv_obj_align(numKb_, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_mode(numKb_, LV_KEYBOARD_MODE_NUMBER);
  lv_keyboard_set_textarea(numKb_, numTa_);
  lv_obj_set_style_bg_color(numKb_, TC::surface(), 0);
  lv_obj_set_style_text_color(numKb_, TC::text(), 0);
  lv_obj_add_event_cb(numKb_, onNumKbEvent, LV_EVENT_READY, this);
  lv_obj_add_event_cb(numKb_, onNumKbEvent, LV_EVENT_CANCEL, this);
}

void DashboardScreen::closeNumOverlay() {
  if (numKb_)      { lv_obj_del(numKb_);      numKb_      = nullptr; }
  if (numOverlay_) { lv_obj_del(numOverlay_); numOverlay_ = nullptr; }
  numTa_   = nullptr;
  numHint_ = nullptr;
}

void DashboardScreen::openOfflineModal() {
  if (offlineModal_) return;

  offlineModal_ = lv_obj_create(scr_);
  lv_obj_set_size(offlineModal_, 430, 190);
  lv_obj_center(offlineModal_);
  lv_obj_set_style_bg_color(offlineModal_, TC::surface(), 0);
  lv_obj_set_style_bg_opa(offlineModal_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(offlineModal_, TC::danger(), 0);
  lv_obj_set_style_border_width(offlineModal_, 2, 0);
  lv_obj_set_style_radius(offlineModal_, 12, 0);
  lv_obj_set_style_pad_all(offlineModal_, 20, 0);
  lv_obj_clear_flag(offlineModal_, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* icon = Theme::label(offlineModal_, LV_SYMBOL_WARNING, TF::xxl(), TC::danger());
  lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t* msg = Theme::label(offlineModal_, "Controller offline\nCheck RS485", TF::lg(), TC::text());
  lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(msg, LV_ALIGN_CENTER, 0, 8);

  lv_obj_t* ok = Theme::button(offlineModal_, "OK", TC::active(), TC::white(), 120, 42);
  lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(ok, onOfflineOk, LV_EVENT_CLICKED, this);
}

void DashboardScreen::closeOfflineModal() {
  if (!offlineModal_) return;
  lv_obj_del(offlineModal_);
  offlineModal_ = nullptr;
}

// ── Event callbacks ───────────────────────────────────────────────────────────

void DashboardScreen::onStartPressed(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  if (self->dialogTargetKg_ <= 0.0f || self->dialogRatePerKg_ <= 0.0f) return;
  if (!self->mbus_) return;
  const bool needsReset = self->lastSnap_.state == FillState::Fault ||
                          self->lastSnap_.state == FillState::Aborted ||
                          self->lastSnap_.state == FillState::Complete ||
                          (!self->lastSnap_.eStopOk && self->lastSnap_.connected);
  if (needsReset) {
    const bool ok = self->mbus_->cmdReset();
    ESP_LOGI(TAG, "RESET pressed ok=%d", ok ? 1 : 0);
    return;
  }
  if (!self->lastSnap_.connected) {
    ESP_LOGW(TAG, "START blocked: controller offline");
    self->openOfflineModal();
    return;
  }
  self->lastRatePerKg_ = self->dialogRatePerKg_;
  const bool ok = self->mbus_->startFill(self->dialogTargetKg_, self->dialogRatePerKg_);
  ESP_LOGI(TAG, "START pressed target=%.3f rate=%.2f ok=%d",
           self->dialogTargetKg_, self->dialogRatePerKg_, ok ? 1 : 0);
  if (!ok) self->openOfflineModal();
}

void DashboardScreen::onStartConfirm(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  if (!self->mbus_) return;

  if (self->dialogTargetKg_ <= 0.0f) {
    if (self->lblStartError_)
      lv_label_set_text(self->lblStartError_, "Set a target weight > 0 kg");
    return;
  }
  if (self->dialogRatePerKg_ <= 0.0f) {
    if (self->lblStartError_)
      lv_label_set_text(self->lblStartError_, "Set a rate > 0");
    return;
  }
  if (!self->lastSnap_.connected) {
    ESP_LOGW(TAG, "START confirm blocked: controller offline");
    self->closeStartDialog();
    self->openOfflineModal();
    return;
  }
  self->lastRatePerKg_  = self->dialogRatePerKg_;
  const bool ok = self->mbus_->startFill(self->dialogTargetKg_, self->dialogRatePerKg_);
  ESP_LOGI(TAG, "START confirmed target=%.3f rate=%.2f ok=%d",
           self->dialogTargetKg_, self->dialogRatePerKg_, ok ? 1 : 0);
  self->closeStartDialog();
  if (!ok) self->openOfflineModal();
}

void DashboardScreen::onTargetMinus(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  if (self->dialogTargetKg_ > 1.0f) self->dialogTargetKg_ -= 1.0f;
  self->updateDialogLabels();
}
void DashboardScreen::onTargetPlus(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  if (self->dialogTargetKg_ < 500.0f) self->dialogTargetKg_ += 1.0f;
  self->updateDialogLabels();
}
void DashboardScreen::onRateMinus(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  if (self->dialogRatePerKg_ > 10.0f) self->dialogRatePerKg_ -= 10.0f;
  self->updateDialogLabels();
}
void DashboardScreen::onRatePlus(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  if (self->dialogRatePerKg_ < 100000.0f) self->dialogRatePerKg_ += 10.0f;
  self->updateDialogLabels();
}

void DashboardScreen::onStartCancel(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  self->closeStartDialog();
}

void DashboardScreen::onFpCellTapped(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  uint8_t field = (uint8_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(e));
  self->openNumOverlay(field);
}

void DashboardScreen::onNumKbEvent(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY && self->numTa_) {
    const char* txt = lv_textarea_get_text(self->numTa_);
    float val = txt ? atof(txt) : 0.0f;
    if (self->numField_ == 0) {
      if (val > 0.0f) {
        self->dialogTargetKg_ = val;
        if (self->lblTarget_) display_label_setf(self->lblTarget_, "%.1f", val);
      }
    } else {
      if (val > 0.0f) {
        self->dialogRatePerKg_ = val;
        if (self->lblRate_) display_label_setf(self->lblRate_, "%.0f", val);
      }
    }
  }
  self->closeNumOverlay();
}

void DashboardScreen::onOfflineOk(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  self->closeOfflineModal();
}

void DashboardScreen::onWifiPressed(lv_event_t* e) {
  screenManager.navigateTo(Screen::Wifi);
}

// ── Blink animation ───────────────────────────────────────────────────────────

void DashboardScreen::blinkAnimCb(void* obj, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
}

void DashboardScreen::startStableBlink() {
  // dotStable_ is now the icon label itself — animate it directly
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, dotStable_);
  lv_anim_set_exec_cb(&a, blinkAnimCb);
  lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
  lv_anim_set_time(&a, 500);
  lv_anim_set_playback_time(&a, 500);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
}

void DashboardScreen::stopStableBlink() {
  lv_anim_del(dotStable_, blinkAnimCb);
  lv_obj_set_style_opa(dotStable_, LV_OPA_COVER, 0);
}

// ── Role selector ─────────────────────────────────────────────────────────────

static constexpr uint32_t kAdminRolePin        = 1234;
static constexpr uint32_t kManufacturerRolePin = 9999;
static constexpr uint8_t  kRolePinDigits       = 4;

static const char* kRoleNumLabels[] = {
  "1","2","3","4","5","6","7","8","9","","0",LV_SYMBOL_BACKSPACE
};

void DashboardScreen::updateRoleButton() {
  if (!lblRoleBtn_) return;
  switch (currentRole_) {
    case Role::Operator:     lv_label_set_text(lblRoleBtn_, LV_SYMBOL_SETTINGS " OPERATOR");     break;
    case Role::Admin:        lv_label_set_text(lblRoleBtn_, LV_SYMBOL_SETTINGS " ADMIN");        break;
    case Role::Manufacturer: lv_label_set_text(lblRoleBtn_, LV_SYMBOL_SETTINGS " MANUFACTURER"); break;
  }
}

void DashboardScreen::openRoleModal() {
  if (roleModal_) return;
  roleEntered_ = 0;
  roleDigits_  = 0;

  roleModal_ = lv_obj_create(scr_);
  lv_obj_set_size(roleModal_, 420, 420);
  lv_obj_center(roleModal_);
  lv_obj_set_style_bg_color(roleModal_, TC::surface(), 0);
  lv_obj_set_style_bg_opa(roleModal_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(roleModal_, TC::border(), 0);
  lv_obj_set_style_border_width(roleModal_, 1, 0);
  lv_obj_set_style_radius(roleModal_, 12, 0);
  lv_obj_set_style_pad_all(roleModal_, 20, 0);
  lv_obj_clear_flag(roleModal_, LV_OBJ_FLAG_SCROLLABLE);
  pendingRole_ = currentRole_;
  buildRoleChooser();
}

void DashboardScreen::buildRoleChooser() {
  if (!roleModal_) return;
  lv_obj_clean(roleModal_);
  lblRolePinDots_ = nullptr;
  lblRolePinErr_  = nullptr;
  roleEntered_    = 0;
  roleDigits_     = 0;

  Theme::label(roleModal_, "Select Role", TF::xl(), TC::text());
  lv_obj_t* title = lv_obj_get_child(roleModal_, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

  const char* labels[] = {
    LV_SYMBOL_SETTINGS " OPERATOR",
    LV_SYMBOL_SETTINGS " ADMIN",
    LV_SYMBOL_SETTINGS " MANUFACTURER",
  };

  for (int i = 0; i < 3; i++) {
    Role role = (Role)i;
    const bool active = role == currentRole_;
    lv_obj_t* btn = Theme::button(roleModal_, labels[i],
                                  active ? TC::active() : TC::surface2(),
                                  active ? TC::white() : TC::text(), 300, 56);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 70 + i * 70);
    lv_obj_set_user_data(btn, (void*)(uintptr_t)i);
    lv_obj_add_event_cb(btn, onRoleSelect, LV_EVENT_CLICKED, this);
  }

  lv_obj_t* btnCancel = Theme::button(roleModal_, "CANCEL",
                                       TC::danger(), TC::white(), 120, 44);
  lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(btnCancel, [](lv_event_t* e){
    DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
    self->closeRoleModal();
  }, LV_EVENT_CLICKED, this);
}

void DashboardScreen::buildRolePinEntry() {
  if (!roleModal_) return;
  lv_obj_clean(roleModal_);
  roleEntered_ = 0;
  roleDigits_  = 0;

  const char* titleText = pendingRole_ == Role::Admin ? "Admin PIN" : "Manufacturer PIN";
  Theme::label(roleModal_, titleText, TF::xl(), TC::text());
  lv_obj_t* title = lv_obj_get_child(roleModal_, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

  Theme::label(roleModal_, "Enter PIN to switch role", TF::sm(), TC::textSub());
  lv_obj_t* sub = lv_obj_get_child(roleModal_, 1);
  lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 30);

  lblRolePinDots_ = lv_label_create(roleModal_);
  lv_label_set_text(lblRolePinDots_, "- - - -");
  lv_obj_set_style_text_font(lblRolePinDots_, TF::xxl(), 0);
  lv_obj_set_style_text_color(lblRolePinDots_, TC::active(), 0);
  lv_obj_align(lblRolePinDots_, LV_ALIGN_TOP_MID, 0, 60);

  lblRolePinErr_ = lv_label_create(roleModal_);
  lv_label_set_text(lblRolePinErr_, "");
  lv_obj_set_width(lblRolePinErr_, 360);
  lv_label_set_long_mode(lblRolePinErr_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(lblRolePinErr_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(lblRolePinErr_, TF::sm(), 0);
  lv_obj_set_style_text_color(lblRolePinErr_, TC::danger(), 0);
  lv_obj_align(lblRolePinErr_, LV_ALIGN_TOP_MID, 0, 104);

  lv_obj_t* pad = lv_obj_create(roleModal_);
  lv_obj_set_size(pad, 300, 206);
  lv_obj_align(pad, LV_ALIGN_TOP_MID, 0, 126);
  lv_obj_set_style_bg_opa(pad, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(pad, 0, 0);
  lv_obj_set_style_pad_all(pad, 0, 0);
  lv_obj_set_layout(pad, LV_LAYOUT_GRID);
  static lv_coord_t cols[] = {88, 88, 88, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t rows[] = {44, 44, 44, 44, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(pad, cols, rows);

  for (int i = 0; i < 12; i++) {
    lv_obj_t* btn = lv_btn_create(pad);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, i % 3, 1,
                              LV_GRID_ALIGN_STRETCH, i / 3, 1);
    lv_obj_set_style_bg_color(btn, TC::surface2(), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 2, 0);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, kRoleNumLabels[i]);
    lv_obj_set_style_text_font(lbl, TF::lg(), 0);
    lv_obj_set_style_text_color(lbl, TC::text(), 0);
    lv_obj_center(lbl);

    if (i == 9) {
      lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    } else if (i == 11) {
      lv_obj_add_event_cb(btn, onRolePinDel, LV_EVENT_CLICKED, this);
    } else {
      lv_obj_set_user_data(btn, (void*)(uintptr_t)((i == 10) ? 0 : i + 1));
      lv_obj_add_event_cb(btn, onRolePinKey, LV_EVENT_CLICKED, this);
    }
  }

  lv_obj_t* btnCancel = Theme::button(roleModal_, LV_SYMBOL_LEFT " BACK",
                                       TC::surface2(), TC::textSub(), 110, 36);
  lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_event_cb(btnCancel, [](lv_event_t* e){
    DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
    self->buildRoleChooser();
  }, LV_EVENT_CLICKED, this);
}

void DashboardScreen::closeRoleModal() {
  if (!roleModal_) return;
  lv_obj_del(roleModal_);
  roleModal_     = nullptr;
  lblRolePinDots_ = nullptr;
  lblRolePinErr_ = nullptr;
  roleEntered_   = 0;
  roleDigits_    = 0;
}

void DashboardScreen::appendRoleDigit(uint8_t d) {
  if (roleDigits_ >= kRolePinDigits) return;
  roleEntered_ = roleEntered_ * 10 + d;
  roleDigits_++;

  char buf[16] = "";
  for (int i = 0; i < (int)kRolePinDigits; i++) {
    strcat(buf, i < roleDigits_ ? "* " : "- ");
  }
  buf[strlen(buf) - 1] = 0;
  if (lblRolePinDots_) lv_label_set_text(lblRolePinDots_, buf);
  if (lblRolePinErr_)  lv_label_set_text(lblRolePinErr_,  "");

  if (roleDigits_ == kRolePinDigits) submitRolePin();
}

void DashboardScreen::submitRolePin() {
  const uint32_t requiredPin =
      pendingRole_ == Role::Admin ? kAdminRolePin : kManufacturerRolePin;

  if (roleEntered_ == requiredPin) {
    currentRole_ = pendingRole_;
    updateRoleButton();
    closeRoleModal();
  } else {
    if (lblRolePinErr_) lv_label_set_text(lblRolePinErr_, "Incorrect PIN");
    roleEntered_ = 0;
    roleDigits_  = 0;
    if (lblRolePinDots_) lv_label_set_text(lblRolePinDots_, "- - - -");
  }
}

void DashboardScreen::onRolePressed(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  self->openRoleModal();
}

void DashboardScreen::onRolePinKey(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  lv_obj_t* btn = lv_event_get_current_target(e);
  uint8_t d = (uint8_t)(uintptr_t)lv_obj_get_user_data(btn);
  self->appendRoleDigit(d);
}

void DashboardScreen::onRolePinDel(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  if (self->roleDigits_ == 0) return;
  self->roleEntered_ /= 10;
  self->roleDigits_--;
  char buf[16] = "";
  for (int i = 0; i < (int)kRolePinDigits; i++) {
    strcat(buf, i < self->roleDigits_ ? "* " : "- ");
  }
  buf[strlen(buf) - 1] = 0;
  if (self->lblRolePinDots_) lv_label_set_text(self->lblRolePinDots_, buf);
}

void DashboardScreen::onRoleSelect(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  lv_obj_t* btn = lv_event_get_current_target(e);
  uint8_t role = (uint8_t)(uintptr_t)lv_obj_get_user_data(btn);
  if ((Role)role == Role::Operator) {
    self->currentRole_ = Role::Operator;
    self->pendingRole_ = Role::Operator;
    self->updateRoleButton();
    self->closeRoleModal();
    return;
  }
  self->pendingRole_ = (Role)role;
  self->buildRolePinEntry();
}
