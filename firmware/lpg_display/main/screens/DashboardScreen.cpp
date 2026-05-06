#include "screens/DashboardScreen.h"
#include "Theme.h"
#include "ScreenManager.h"

extern ScreenManager screenManager;
extern ModbusClient  modbusClient;

// ── helpers ───────────────────────────────────────────────────────────────────

static const char* stateLabel(FillState s) {
  switch (s) {
    case FillState::Idle:        return "IDLE";
    case FillState::Ready:       return "READY";
    case FillState::Validating:  return "VALIDATING";
    case FillState::Fast:        return "FILLING — FAST";
    case FillState::Slow:        return "FILLING — SLOW";
    case FillState::Settling:    return "SETTLING";
    case FillState::Complete:    return "COMPLETE";
    case FillState::Aborted:     return "ABORTED";
    case FillState::Fault:       return "FAULT";
    case FillState::Maintenance: return "MAINTENANCE";
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

// ── build ─────────────────────────────────────────────────────────────────────

void DashboardScreen::build(ModbusClient& mbus) {
  mbus_ = &mbus;

  scr_ = lv_obj_create(nullptr);
  Theme::applyScreenBg(scr_);

  // ── Header bar ──────────────────────────────────────────────────────────────
  lv_obj_t* hdr = Theme::headerBar(scr_);

  Theme::label(hdr, "LPG FILLING STATION", TF::lg(), TC::text());
  lv_obj_t* hdrTitle = lv_obj_get_child(hdr, 0);
  lv_obj_align(hdrTitle, LV_ALIGN_LEFT_MID, 0, 0);

  lblState_ = lv_label_create(hdr);
  lv_obj_set_style_text_font(lblState_, TF::md(), 0);
  lv_obj_align(lblState_, LV_ALIGN_CENTER, 0, 0);

  lblConnStatus_ = lv_label_create(hdr);
  lv_obj_set_style_text_font(lblConnStatus_, TF::sm(), 0);
  lv_obj_set_style_text_color(lblConnStatus_, TC::muted(), 0);
  lv_obj_align(lblConnStatus_, LV_ALIGN_RIGHT_MID, -90, 0);

  lblTime_ = lv_label_create(hdr);
  lv_label_set_text(lblTime_, "00:00");
  lv_obj_set_style_text_font(lblTime_, TF::md(), 0);
  lv_obj_set_style_text_color(lblTime_, TC::textSub(), 0);
  lv_obj_align(lblTime_, LV_ALIGN_RIGHT_MID, -10, 0);

  // Admin button
  lv_obj_t* btnAdmin = Theme::button(hdr, "ADMIN", TC::surface2(), TC::textSub(), 80, 34);
  lv_obj_align(btnAdmin, LV_ALIGN_RIGHT_MID, -72, 0);
  lv_obj_add_event_cb(btnAdmin, onAdminPressed, LV_EVENT_CLICKED, this);

  // ── Content area (y=60) ──────────────────────────────────────────────────────
  // Weight card (left, 440×200)
  lv_obj_t* wCard = lv_obj_create(scr_);
  lv_obj_set_size(wCard, 440, 200);
  lv_obj_set_pos(wCard, TS::xl, 68);
  Theme::applyCard(wCard);

  Theme::label(wCard, "LIVE WEIGHT", TF::sm(), TC::textSub());
  lv_obj_t* lWLabel = lv_obj_get_child(wCard, 0);
  lv_obj_align(lWLabel, LV_ALIGN_TOP_LEFT, 0, 0);

  lblLive_ = lv_label_create(wCard);
  lv_label_set_text(lblLive_, "0.000 kg");
  lv_obj_set_style_text_font(lblLive_, TF::hero(), 0);
  lv_obj_set_style_text_color(lblLive_, TC::text(), 0);
  lv_obj_align(lblLive_, LV_ALIGN_CENTER, 0, -10);

  // Tare / Net row
  lv_obj_t* subRow = lv_obj_create(wCard);
  lv_obj_set_size(subRow, LV_PCT(100), 30);
  lv_obj_align(subRow, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(subRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(subRow, 0, 0);
  lv_obj_set_style_pad_all(subRow, 0, 0);
  lv_obj_clear_flag(subRow, LV_OBJ_FLAG_SCROLLABLE);

  lblTare_ = lv_label_create(subRow);
  lv_label_set_text(lblTare_, "Tare 0.000 kg");
  lv_obj_set_style_text_font(lblTare_, TF::md(), 0);
  lv_obj_set_style_text_color(lblTare_, TC::textSub(), 0);
  lv_obj_align(lblTare_, LV_ALIGN_LEFT_MID, 0, 0);

  lblNet_ = lv_label_create(subRow);
  lv_label_set_text(lblNet_, "Net 0.000 kg");
  lv_obj_set_style_text_font(lblNet_, TF::md(), 0);
  lv_obj_set_style_text_color(lblNet_, TC::text(), 0);
  lv_obj_align(lblNet_, LV_ALIGN_RIGHT_MID, 0, 0);

  // ── Readiness card (right, 290×200) ──────────────────────────────────────────
  lv_obj_t* rCard = lv_obj_create(scr_);
  lv_obj_set_size(rCard, 290, 200);
  lv_obj_set_pos(rCard, 474, 68);
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
  lv_obj_set_style_pad_row(dotCol, TS::lg, 0);

  dotEstop_    = Theme::statusDot(dotCol, "E-STOP OK",       false);
  dotCylinder_ = Theme::statusDot(dotCol, "CYLINDER PRESENT",false);
  dotNozzle_   = Theme::statusDot(dotCol, "NOZZLE ENGAGED",  false);
  dotStable_   = Theme::statusDot(dotCol, "WEIGHT STABLE",   false);

  // ── Today stats card (bottom-left, 440×110) ───────────────────────────────
  lv_obj_t* sCard = lv_obj_create(scr_);
  lv_obj_set_size(sCard, 440, 110);
  lv_obj_set_pos(sCard, TS::xl, 282);
  Theme::applyCard(sCard);

  Theme::label(sCard, "TODAY", TF::sm(), TC::textSub());
  lv_obj_t* sLabel = lv_obj_get_child(sCard, 0);
  lv_obj_align(sLabel, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t* sRow = lv_obj_create(sCard);
  lv_obj_set_size(sRow, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_align(sRow, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(sRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(sRow, 0, 0);
  lv_obj_set_style_pad_all(sRow, 0, 0);
  lv_obj_set_layout(sRow, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(sRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(sRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

  lblTodayFills_ = lv_label_create(sRow);
  lv_obj_set_style_text_font(lblTodayFills_, TF::xl(), 0);
  lv_obj_set_style_text_color(lblTodayFills_, TC::text(), 0);

  lblTodayKg_ = lv_label_create(sRow);
  lv_obj_set_style_text_font(lblTodayKg_, TF::xl(), 0);
  lv_obj_set_style_text_color(lblTodayKg_, TC::active(), 0);

  lblTodayAmt_ = lv_label_create(sRow);
  lv_obj_set_style_text_font(lblTodayAmt_, TF::xl(), 0);
  lv_obj_set_style_text_color(lblTodayAmt_, TC::ready(), 0);

  // ── Start Fill button (bottom-right, 290×110) ─────────────────────────────
  btnStart_ = Theme::button(scr_, "START FILL", TC::active(), TC::white(), 290, 110);
  lv_obj_set_pos(btnStart_, 474, 282);
  lv_obj_set_style_radius(btnStart_, 10, 0);
  lv_obj_set_style_text_font(lv_obj_get_child(btnStart_, 0), TF::xxl(), 0);
  lv_obj_add_event_cb(btnStart_, onStartPressed, LV_EVENT_CLICKED, this);
}

// ── update ────────────────────────────────────────────────────────────────────

void DashboardScreen::update(const ControllerSnapshot& snap) {
  if (!scr_) return;

  // State chip
  lv_label_set_text(lblState_, stateLabel(snap.state));
  lv_obj_set_style_text_color(lblState_, stateColor(snap.state), 0);

  // Time
  lv_label_set_text_fmt(lblTime_, "%02u:%02u", snap.rtcHour, snap.rtcMinute);

  // Connectivity
  if (snap.connected) {
    lv_label_set_text(lblConnStatus_, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(lblConnStatus_, TC::ready(), 0);
  } else {
    lv_label_set_text(lblConnStatus_, LV_SYMBOL_WIFI " OFFLINE");
    lv_obj_set_style_text_color(lblConnStatus_, TC::danger(), 0);
  }

  // Weights
  lv_label_set_text_fmt(lblLive_, "%.3f kg", snap.liveWeightKg);
  lv_label_set_text_fmt(lblTare_, "Tare %.3f kg", snap.tareWeightKg);
  lv_label_set_text_fmt(lblNet_,  "Net %.3f kg",  snap.netWeightKg);

  // Readiness dots
  updateDot(dotEstop_,    snap.eStopOk);
  updateDot(dotCylinder_, snap.cylinderPresent);
  updateDot(dotNozzle_,   snap.nozzleEngaged);
  updateDot(dotStable_,   snap.weightStable);

  // Today stats
  lv_label_set_text_fmt(lblTodayFills_, "%u fills", snap.todayFills);
  lv_label_set_text_fmt(lblTodayKg_,   "%.1f kg",  snap.todayKg);
  lv_label_set_text_fmt(lblTodayAmt_,  "%.0f " LV_SYMBOL_CHARGE, snap.todayAmount);

  // Start button enabled only when idle/ready
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

// ── event handlers ────────────────────────────────────────────────────────────

void DashboardScreen::onStartPressed(lv_event_t* e) {
  DashboardScreen* self = static_cast<DashboardScreen*>(lv_event_get_user_data(e));
  if (self->mbus_) self->mbus_->cmdStart();
}

void DashboardScreen::onAdminPressed(lv_event_t* e) {
  screenManager.navigateTo(Screen::Pin);
}
