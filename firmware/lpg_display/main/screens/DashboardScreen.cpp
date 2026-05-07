#include "screens/DashboardScreen.h"
#include "Theme.h"
#include "ScreenManager.h"
#include <cstdio>
#include <stdlib.h>
#include <string.h>

extern ScreenManager screenManager;
extern ModbusClient  modbusClient;

// ── helpers ───────────────────────────────────────────────────────────────────

static const char* stateLabel(FillState s) {
  switch (s) {
    case FillState::Idle:        return "IDLE";
    case FillState::Ready:       return LV_SYMBOL_OK "  READY";
    case FillState::Validating:  return LV_SYMBOL_REFRESH "  VALIDATING";
    case FillState::Fast:        return LV_SYMBOL_PLAY "  FAST FILL";
    case FillState::Slow:        return LV_SYMBOL_PLAY "  SLOW FILL";
    case FillState::Settling:    return LV_SYMBOL_REFRESH "  SETTLING";
    case FillState::Complete:    return LV_SYMBOL_OK "  COMPLETE";
    case FillState::Aborted:     return LV_SYMBOL_STOP "  ABORTED";
    case FillState::Fault:       return LV_SYMBOL_WARNING "  FAULT";
    case FillState::Maintenance: return LV_SYMBOL_SETTINGS "  MAINTENANCE";
    default:                     return "UNKNOWN";
  }
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
      || a.rtcHour         != b.rtcHour
      || a.rtcMinute       != b.rtcMinute
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
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

  // State badge (center of status bar)
  stateBadge_ = lv_obj_create(bar);
  lv_obj_set_size(stateBadge_, LV_SIZE_CONTENT, 32);
  lv_obj_set_style_pad_hor(stateBadge_, 14, 0);
  lv_obj_set_style_pad_ver(stateBadge_, 0, 0);
  lv_obj_set_style_radius(stateBadge_, 16, 0);
  lv_obj_set_style_border_width(stateBadge_, 0, 0);
  lv_obj_set_style_bg_color(stateBadge_, TC::muted(), 0);
  lv_obj_set_style_bg_opa(stateBadge_, LV_OPA_COVER, 0);
  lv_obj_align(stateBadge_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(stateBadge_, LV_OBJ_FLAG_SCROLLABLE);

  lblState_ = lv_label_create(stateBadge_);
  lv_label_set_text(lblState_, "IDLE");
  lv_obj_set_style_text_font(lblState_, TF::md(), 0);
  lv_obj_set_style_text_color(lblState_, TC::white(), 0);
  lv_obj_center(lblState_);

  // Right side of status bar: WiFi | time | admin
  lblConnStatus_ = lv_label_create(bar);
  lv_label_set_text(lblConnStatus_, LV_SYMBOL_WIFI " OFFLINE");
  lv_obj_set_style_text_font(lblConnStatus_, TF::sm(), 0);
  lv_obj_set_style_text_color(lblConnStatus_, TC::danger(), 0);
  lv_obj_align(lblConnStatus_, LV_ALIGN_RIGHT_MID, -220, 0);

  // WiFi nav button
  lv_obj_t* btnWifi = Theme::button(bar, LV_SYMBOL_WIFI,
                                    TC::surface2(), TC::textSub(), 42, 34);
  lv_obj_align(btnWifi, LV_ALIGN_RIGHT_MID, -164, 0);
  lv_obj_add_event_cb(btnWifi, onWifiPressed, LV_EVENT_CLICKED, this);

  lblTime_ = lv_label_create(bar);
  lv_label_set_text(lblTime_, "00:00");
  lv_obj_set_style_text_font(lblTime_, TF::lg(), 0);
  lv_obj_set_style_text_color(lblTime_, TC::textSub(), 0);
  lv_obj_align(lblTime_, LV_ALIGN_RIGHT_MID, -90, 0);

  lv_obj_t* btnAdmin = Theme::button(bar, LV_SYMBOL_SETTINGS " ADMIN",
                                     TC::surface2(), TC::textSub(), 100, 34);
  lv_obj_align(btnAdmin, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(btnAdmin, onAdminPressed, LV_EVENT_CLICKED, this);

  // ── Left column: Weight card (x=16, y=68, 460×258) ──────────────────────────
  lv_obj_t* wCard = lv_obj_create(scr_);
  lv_obj_set_size(wCard, 460, 258);
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
  lv_obj_set_style_text_font(lblTare_, TF::md(), 0);
  lv_obj_set_style_text_color(lblTare_, TC::textSub(), 0);
  lv_obj_align(lblTare_, LV_ALIGN_LEFT_MID, 0, 0);

  lblNet_ = lv_label_create(subRow);
  lv_label_set_text(lblNet_, "Net: 0.000 kg");
  lv_obj_set_style_text_font(lblNet_, TF::md(), 0);
  lv_obj_set_style_text_color(lblNet_, TC::active(), 0);
  lv_obj_align(lblNet_, LV_ALIGN_RIGHT_MID, 0, 0);

  // ── Left column: Today stats card (x=16, y=334) ───────────────────────────
  lv_obj_t* sCard = lv_obj_create(scr_);
  lv_obj_set_size(sCard, 460, 100);
  lv_obj_set_pos(sCard, 16, 334);
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

  // ── Right column: Readiness card (x=492, y=68, 292×200) ───────────────────
  lv_obj_t* rCard = lv_obj_create(scr_);
  lv_obj_set_size(rCard, 292, 200);
  lv_obj_set_pos(rCard, 492, 68);
  Theme::applyCard(rCard);

  Theme::label(rCard, "READINESS", TF::sm(), TC::textSub());
  lv_obj_t* rLabel = lv_obj_get_child(rCard, 0);
  lv_obj_align(rLabel, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t* dotCol = lv_obj_create(rCard);
  lv_obj_set_size(dotCol, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_align(dotCol, LV_ALIGN_CENTER, 0, 8);
  lv_obj_set_style_bg_opa(dotCol, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(dotCol, 0, 0);
  lv_obj_set_style_pad_all(dotCol, 0, 0);
  lv_obj_set_layout(dotCol, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(dotCol, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(dotCol, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(dotCol, 14, 0);

  dotEstop_    = Theme::statusDot(dotCol, "E-STOP OK",        false);
  dotCylinder_ = Theme::statusDot(dotCol, "CYLINDER PRESENT", false);
  dotNozzle_   = Theme::statusDot(dotCol, "NOZZLE ENGAGED",   false);
  dotStable_   = Theme::statusDot(dotCol, "WEIGHT STABLE",    false);

  // ── Right column: Start Fill button (x=492, y=276, 292×158) ──────────────
  btnStart_ = lv_obj_create(scr_);
  lv_obj_set_size(btnStart_, 292, 158);
  lv_obj_set_pos(btnStart_, 492, 276);
  lv_obj_set_style_bg_color(btnStart_, TC::muted(), 0);
  lv_obj_set_style_bg_opa(btnStart_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btnStart_, 0, 0);
  lv_obj_set_style_radius(btnStart_, 10, 0);
  lv_obj_set_style_pad_all(btnStart_, 0, 0);
  lv_obj_clear_flag(btnStart_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(btnStart_, onStartPressed, LV_EVENT_CLICKED, this);

  lv_obj_t* startIcon = Theme::label(btnStart_, LV_SYMBOL_PLAY, TF::xxl(), TC::white());
  lv_obj_align(startIcon, LV_ALIGN_CENTER, 0, -16);

  lv_obj_t* startLbl = Theme::label(btnStart_, "START FILL", TF::lg(), TC::white());
  lv_obj_align(startLbl, LV_ALIGN_CENTER, 0, 22);
}

// ── update ────────────────────────────────────────────────────────────────────

void DashboardScreen::update(const ControllerSnapshot& snap) {
  if (!scr_) return;
  if (!firstUpdate_ && !snapChanged(snap, lastSnap_)) return;
  firstUpdate_ = false;
  lastSnap_ = snap;

  // State badge
  lv_color_t sc = stateColor(snap.state);
  lv_label_set_text(lblState_, stateLabel(snap.state));
  lv_obj_set_style_bg_color(stateBadge_, sc, 0);

  // Weight accent border color mirrors state
  lv_obj_set_style_bg_color(weightAccent_, sc, 0);

  // Connectivity
  if (snap.connected) {
    lv_label_set_text(lblConnStatus_, LV_SYMBOL_WIFI "  ONLINE");
    lv_obj_set_style_text_color(lblConnStatus_, TC::ready(), 0);
  } else {
    lv_label_set_text(lblConnStatus_, LV_SYMBOL_WIFI "  OFFLINE");
    lv_obj_set_style_text_color(lblConnStatus_, TC::danger(), 0);
  }

  // Time
  lv_label_set_text_fmt(lblTime_, "%02u:%02u", snap.rtcHour, snap.rtcMinute);

  // Live weight (no "kg" in hero — separate unit label)
  lv_label_set_text_fmt(lblLive_, "%.3f", snap.liveWeightKg);
  lv_label_set_text_fmt(lblTare_, "Tare: %.3f kg", snap.tareWeightKg);
  lv_label_set_text_fmt(lblNet_,  "Net: %.3f kg",  snap.netWeightKg);

  // Readiness dots
  updateDot(dotEstop_,    snap.eStopOk);
  updateDot(dotCylinder_, snap.cylinderPresent);
  updateDot(dotNozzle_,   snap.nozzleEngaged);
  updateDot(dotStable_,   snap.weightStable);

  // Today stats
  lv_label_set_text_fmt(lblTodayFills_, "%u fills",   snap.todayFills);
  lv_label_set_text_fmt(lblTodayKg_,   "%.1f kg",    snap.todayKg);
  lv_label_set_text_fmt(lblTodayAmt_,  "%.0f",       snap.todayAmount);
  if (snap.ratePerKg > 0.0f) lastRatePerKg_ = snap.ratePerKg;

  // Start button — enabled only when idle/ready and safe
  const bool canStart = (snap.state == FillState::Idle || snap.state == FillState::Ready)
                        && snap.eStopOk && snap.connected;
  lv_obj_set_style_bg_color(btnStart_, canStart ? TC::active() : TC::muted(), 0);
  if (canStart) lv_obj_add_flag(btnStart_, LV_OBJ_FLAG_CLICKABLE);
  else          lv_obj_clear_flag(btnStart_, LV_OBJ_FLAG_CLICKABLE);
}

void DashboardScreen::updateDot(lv_obj_t* row, bool ok) {
  lv_obj_t* dot = lv_obj_get_child(row, 0);
  lv_obj_t* lbl = lv_obj_get_child(row, 1);
  lv_obj_set_style_bg_color(dot, ok ? TC::ready() : TC::muted(), 0);
  lv_obj_set_style_text_color(lbl, ok ? TC::text() : TC::muted(), 0);
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
    lv_label_set_text_fmt(lblDialogTarget_, "%.0f kg", dialogTargetKg_);
  if (lblDialogRate_)
    lv_label_set_text_fmt(lblDialogRate_, "%.0f PKR", dialogRatePerKg_);
}

void DashboardScreen::closeStartDialog() {
  if (!startModal_) return;
  lv_obj_del(startModal_);
  startModal_      = nullptr;
  lblDialogTarget_ = nullptr;
  lblDialogRate_   = nullptr;
  lblStartError_   = nullptr;
}

// ── Event callbacks ───────────────────────────────────────────────────────────

void DashboardScreen::onStartPressed(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  self->openStartDialog();
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
  self->lastRatePerKg_  = self->dialogRatePerKg_;
  self->mbus_->startFill(self->dialogTargetKg_, self->dialogRatePerKg_);
  self->closeStartDialog();
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


void DashboardScreen::onAdminPressed(lv_event_t* e) {
  screenManager.navigateTo(Screen::Pin);
}

void DashboardScreen::onWifiPressed(lv_event_t* e) {
  screenManager.navigateTo(Screen::Wifi);
}
