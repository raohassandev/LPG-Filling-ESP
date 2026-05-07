#include "screens/WifiScreen.h"
#include "Theme.h"
#include "ScreenManager.h"
#include "esp_wifi.h"
#include <cstdio>
#include <cstring>

extern ScreenManager screenManager;

// ── build ─────────────────────────────────────────────────────────────────────

void WifiScreen::build(WifiManager& mgr) {
    mgr_  = &mgr;
    scr_  = lv_obj_create(nullptr);
    Theme::applyScreenBg(scr_);

    // ── Header ───────────────────────────────────────────────────────────────
    lv_obj_t* hdr = Theme::headerBar(scr_);

    lv_obj_t* btnBack = Theme::button(hdr, LV_SYMBOL_LEFT " BACK",
                                      TC::surface2(), TC::textSub(), 100, 36);
    lv_obj_align(btnBack, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(btnBack, onBack, LV_EVENT_CLICKED, this);

    Theme::label(hdr, LV_SYMBOL_WIFI "  WiFi Management", TF::lg(), TC::text());
    lv_obj_t* title = lv_obj_get_child(hdr, 1);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // ── Status card ───────────────────────────────────────────────────────────
    lv_obj_t* statusCard = lv_obj_create(scr_);
    lv_obj_set_size(statusCard, 760, 72);
    lv_obj_set_pos(statusCard, 20, 66);
    Theme::applyCard(statusCard);
    lv_obj_set_style_pad_all(statusCard, 12, 0);

    lblStatus_ = lv_label_create(statusCard);
    lv_obj_set_style_text_font(lblStatus_, TF::lg(), 0);
    lv_obj_set_style_text_color(lblStatus_, TC::text(), 0);
    lv_obj_align(lblStatus_, LV_ALIGN_LEFT_MID, 8, -8);

    lblIp_ = lv_label_create(statusCard);
    lv_obj_set_style_text_font(lblIp_, TF::md(), 0);
    lv_obj_set_style_text_color(lblIp_, TC::textSub(), 0);
    lv_obj_align(lblIp_, LV_ALIGN_LEFT_MID, 8, 12);

    // ── Left panel — Saved networks (x=20, y=150) ─────────────────────────────
    lv_obj_t* netCard = lv_obj_create(scr_);
    lv_obj_set_size(netCard, 460, 294);
    lv_obj_set_pos(netCard, 20, 150);
    Theme::applyCard(netCard);
    lv_obj_set_style_pad_all(netCard, 12, 0);

    Theme::label(netCard, "SAVED NETWORKS", TF::sm(), TC::textSub());
    lv_obj_t* netHdr = lv_obj_get_child(netCard, 0);
    lv_obj_align(netHdr, LV_ALIGN_TOP_LEFT, 0, 0);

    // Add button
    lv_obj_t* btnAdd = Theme::button(netCard, LV_SYMBOL_PLUS " ADD",
                                     TC::active(), TC::white(), 100, 32);
    lv_obj_align(btnAdd, LV_ALIGN_TOP_RIGHT, 0, -4);
    lv_obj_add_event_cb(btnAdd, onAddNetwork, LV_EVENT_CLICKED, this);

    // Scrollable network list container
    networkList_ = lv_obj_create(netCard);
    lv_obj_set_size(networkList_, LV_PCT(100), 230);
    lv_obj_align(networkList_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(networkList_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(networkList_, 0, 0);
    lv_obj_set_style_pad_all(networkList_, 0, 0);
    lv_obj_set_layout(networkList_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(networkList_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(networkList_, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(networkList_, 6, 0);
    lv_obj_add_flag(networkList_, LV_OBJ_FLAG_SCROLLABLE);

    rebuildNetworkList();

    // ── Right panel — Settings ────────────────────────────────────────────────
    lv_obj_t* settCard = lv_obj_create(scr_);
    lv_obj_set_size(settCard, 280, 294);
    lv_obj_set_pos(settCard, 500, 150);
    Theme::applyCard(settCard);

    Theme::label(settCard, "SETTINGS", TF::sm(), TC::textSub());
    lv_obj_t* settHdr = lv_obj_get_child(settCard, 0);
    lv_obj_align(settHdr, LV_ALIGN_TOP_LEFT, 0, 0);

    // Hotspot toggle row
    lv_obj_t* hotRow = lv_obj_create(settCard);
    lv_obj_set_size(hotRow, LV_PCT(100), 50);
    lv_obj_align(hotRow, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_set_style_bg_opa(hotRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hotRow, 0, 0);
    lv_obj_set_style_pad_all(hotRow, 0, 0);
    lv_obj_clear_flag(hotRow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hotLbl = Theme::label(hotRow, LV_SYMBOL_WIFI "  Hotspot", TF::md(), TC::text());
    lv_obj_align(hotLbl, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t* hotSub = Theme::label(hotRow, kApSsid, TF::sm(), TC::textSub());
    lv_obj_align(hotSub, LV_ALIGN_LEFT_MID, 0, 14);

    swHotspot_ = lv_switch_create(hotRow);
    lv_obj_set_size(swHotspot_, 52, 26);
    lv_obj_align(swHotspot_, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(swHotspot_, TC::ready(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (mgr.hotspotEnabled()) lv_obj_add_state(swHotspot_, LV_STATE_CHECKED);
    lv_obj_add_event_cb(swHotspot_, onHotspotToggle, LV_EVENT_VALUE_CHANGED, this);

    // Divider
    lv_obj_t* div = lv_obj_create(settCard);
    lv_obj_set_size(div, LV_PCT(100), 1);
    lv_obj_align(div, LV_ALIGN_TOP_LEFT, 0, 100);
    lv_obj_set_style_bg_color(div, TC::border(), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);

    // Auto-switch toggle row
    lv_obj_t* swRow = lv_obj_create(settCard);
    lv_obj_set_size(swRow, LV_PCT(100), 60);
    lv_obj_align(swRow, LV_ALIGN_TOP_LEFT, 0, 112);
    lv_obj_set_style_bg_opa(swRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(swRow, 0, 0);
    lv_obj_set_style_pad_all(swRow, 0, 0);
    lv_obj_clear_flag(swRow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* swLbl = Theme::label(swRow, LV_SYMBOL_SHUFFLE "  Auto Switch", TF::md(), TC::text());
    lv_obj_align(swLbl, LV_ALIGN_LEFT_MID, 0, -10);
    lv_obj_t* swSub = Theme::label(swRow, "Try next network on drop", TF::sm(), TC::textSub());
    lv_obj_align(swSub, LV_ALIGN_LEFT_MID, 0, 12);

    swAutoSwitch_ = lv_switch_create(swRow);
    lv_obj_set_size(swAutoSwitch_, 52, 26);
    lv_obj_align(swAutoSwitch_, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(swAutoSwitch_, TC::active(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (mgr.autoSwitch()) lv_obj_add_state(swAutoSwitch_, LV_STATE_CHECKED);
    lv_obj_add_event_cb(swAutoSwitch_, onAutoSwitchToggle, LV_EVENT_VALUE_CHANGED, this);

    // AP credentials info
    lv_obj_t* credCard = lv_obj_create(settCard);
    lv_obj_set_size(credCard, LV_PCT(100), 80);
    lv_obj_align(credCard, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(credCard, TC::surface2(), 0);
    lv_obj_set_style_bg_opa(credCard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(credCard, 0, 0);
    lv_obj_set_style_radius(credCard, 8, 0);
    lv_obj_set_style_pad_all(credCard, 10, 0);
    lv_obj_clear_flag(credCard, LV_OBJ_FLAG_SCROLLABLE);

    char apInfo[80];
    snprintf(apInfo, sizeof(apInfo), "SSID: %s\nPass: %s", kApSsid, kApPass);
    lv_obj_t* credLbl = Theme::label(credCard, apInfo, TF::sm(), TC::textSub());
    lv_obj_align(credLbl, LV_ALIGN_LEFT_MID, 0, 0);

    update(mgr);
}

// ── rebuildNetworkList ────────────────────────────────────────────────────────

void WifiScreen::rebuildNetworkList() {
    if (!networkList_ || !mgr_) return;
    lv_obj_clean(networkList_);

    if (mgr_->count() == 0) {
        lv_obj_t* empty = Theme::label(networkList_,
                                       "No saved networks.\nTap ADD to add one.",
                                       TF::md(), TC::muted());
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }

    for (int i = 0; i < mgr_->count(); i++) {
        lv_obj_t* row = lv_obj_create(networkList_);
        lv_obj_set_size(row, LV_PCT(100), 44);
        lv_obj_set_style_bg_color(row, TC::surface2(), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, TC::border(), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_hor(row, 10, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        // Active indicator
        bool isActive = (i == mgr_->currentIndex() && mgr_->connected());
        lv_obj_t* dot = lv_obj_create(row);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, isActive ? TC::ready() : TC::muted(), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* ssidLbl = Theme::label(row, mgr_->network(i).ssid,
                                         TF::md(), isActive ? TC::ready() : TC::text());
        lv_obj_align(ssidLbl, LV_ALIGN_LEFT_MID, 18, 0);

        // Delete button
        lv_obj_t* btnDel = Theme::button(row, LV_SYMBOL_TRASH,
                                         TC::danger(), TC::white(), 34, 30);
        lv_obj_align(btnDel, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_user_data(btnDel, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btnDel, onRemoveNetwork, LV_EVENT_CLICKED, this);
    }
}

// ── update ────────────────────────────────────────────────────────────────────

void WifiScreen::update(WifiManager& mgr) {
    if (!scr_) return;

    bool changed = (mgr.connected()   != lastConnected_)
                || (mgr.count()       != lastCount_)
                || (mgr.hotspotEnabled() != lastAp_)
                || (mgr.autoSwitch()  != lastAutoSwitch_);
    if (!changed) return;

    lastConnected_  = mgr.connected();
    lastCount_      = mgr.count();
    lastAp_         = mgr.hotspotEnabled();
    lastAutoSwitch_ = mgr.autoSwitch();

    // Status label
    if (mgr.connected()) {
        char buf[64];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI "  Connected: %s  (RSSI %d dBm)",
                 mgr.currentSsid(), mgr.rssi());
        lv_label_set_text(lblStatus_, buf);
        lv_obj_set_style_text_color(lblStatus_, TC::ready(), 0);
    } else {
        lv_label_set_text(lblStatus_, LV_SYMBOL_WIFI "  Not connected");
        lv_obj_set_style_text_color(lblStatus_, TC::danger(), 0);
    }

    char ipBuf[32];
    snprintf(ipBuf, sizeof(ipBuf), "IP: %s", mgr.ipAddress());
    lv_label_set_text(lblIp_, ipBuf);

    rebuildNetworkList();

    // Sync toggles
    if (mgr.hotspotEnabled()) lv_obj_add_state(swHotspot_, LV_STATE_CHECKED);
    else                      lv_obj_clear_state(swHotspot_, LV_STATE_CHECKED);

    if (mgr.autoSwitch()) lv_obj_add_state(swAutoSwitch_, LV_STATE_CHECKED);
    else                  lv_obj_clear_state(swAutoSwitch_, LV_STATE_CHECKED);
}

// ── Add modal ─────────────────────────────────────────────────────────────────

void WifiScreen::openAddModal() {
    if (addModal_) return;

    addModal_ = lv_obj_create(scr_);
    lv_obj_set_size(addModal_, 560, 380);
    lv_obj_center(addModal_);
    lv_obj_set_style_bg_color(addModal_, TC::surface(), 0);
    lv_obj_set_style_bg_opa(addModal_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(addModal_, TC::border(), 0);
    lv_obj_set_style_border_width(addModal_, 1, 0);
    lv_obj_set_style_radius(addModal_, 12, 0);
    lv_obj_set_style_pad_all(addModal_, 20, 0);
    lv_obj_clear_flag(addModal_, LV_OBJ_FLAG_SCROLLABLE);

    Theme::label(addModal_, "Add Network", TF::xl(), TC::text());
    lv_obj_t* title = lv_obj_get_child(addModal_, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    // SSID field
    Theme::label(addModal_, "Network Name (SSID)", TF::sm(), TC::textSub());
    lv_obj_t* ssidLbl = lv_obj_get_child(addModal_, 1);
    lv_obj_align(ssidLbl, LV_ALIGN_TOP_LEFT, 0, 40);

    taSsid_ = lv_textarea_create(addModal_);
    lv_obj_set_size(taSsid_, LV_PCT(100), 44);
    lv_obj_align(taSsid_, LV_ALIGN_TOP_LEFT, 0, 58);
    lv_textarea_set_one_line(taSsid_, true);
    lv_textarea_set_max_length(taSsid_, 32);
    lv_obj_set_style_bg_color(taSsid_, TC::surface2(), 0);
    lv_obj_set_style_border_color(taSsid_, TC::border(), 0);
    lv_obj_set_style_text_color(taSsid_, TC::text(), 0);
    lv_obj_set_style_text_font(taSsid_, TF::md(), 0);
    lv_obj_add_event_cb(taSsid_, onKeyboard, LV_EVENT_FOCUSED, this);

    // Password field
    lv_obj_t* passLbl = Theme::label(addModal_, "Password", TF::sm(), TC::textSub());
    lv_obj_align(passLbl, LV_ALIGN_TOP_LEFT, 0, 116);

    taPass_ = lv_textarea_create(addModal_);
    lv_obj_set_size(taPass_, LV_PCT(100), 44);
    lv_obj_align(taPass_, LV_ALIGN_TOP_LEFT, 0, 134);
    lv_textarea_set_one_line(taPass_, true);
    lv_textarea_set_max_length(taPass_, 64);
    lv_textarea_set_password_mode(taPass_, true);
    lv_obj_set_style_bg_color(taPass_, TC::surface2(), 0);
    lv_obj_set_style_border_color(taPass_, TC::border(), 0);
    lv_obj_set_style_text_color(taPass_, TC::text(), 0);
    lv_obj_set_style_text_font(taPass_, TF::md(), 0);
    lv_obj_add_event_cb(taPass_, onKeyboard, LV_EVENT_FOCUSED, this);

    lblAddErr_ = Theme::label(addModal_, "", TF::sm(), TC::danger());
    lv_obj_align(lblAddErr_, LV_ALIGN_TOP_LEFT, 0, 190);

    // Buttons row
    lv_obj_t* btnCancel = Theme::button(addModal_, "CANCEL",
                                        TC::surface2(), TC::textSub(), 120, 40);
    lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(btnCancel, onAddCancel, LV_EVENT_CLICKED, this);

    lv_obj_t* btnSave = Theme::button(addModal_, "SAVE",
                                      TC::active(), TC::white(), 120, 40);
    lv_obj_align(btnSave, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(btnSave, onAddConfirm, LV_EVENT_CLICKED, this);

    // Keyboard attached to SSID by default
    kb_ = lv_keyboard_create(addModal_);
    lv_obj_set_size(kb_, LV_PCT(100), 180);
    lv_obj_align(kb_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb_, taSsid_);
    lv_obj_set_style_bg_color(kb_, TC::surface2(), 0);
    lv_obj_set_style_text_color(kb_, TC::text(), 0);
    lv_obj_add_event_cb(kb_, onKeyboard, LV_EVENT_READY, this);
}

void WifiScreen::closeAddModal() {
    if (!addModal_) return;
    lv_obj_del(addModal_);
    addModal_ = nullptr;
    taSsid_   = nullptr;
    taPass_   = nullptr;
    kb_       = nullptr;
    lblAddErr_= nullptr;
}

void WifiScreen::openScanModal() {
    if (scanModal_) return;

    scanModal_ = lv_obj_create(scr_);
    lv_obj_set_size(scanModal_, 560, 390);
    lv_obj_center(scanModal_);
    lv_obj_set_style_bg_color(scanModal_, TC::surface(), 0);
    lv_obj_set_style_bg_opa(scanModal_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(scanModal_, TC::border(), 0);
    lv_obj_set_style_border_width(scanModal_, 1, 0);
    lv_obj_set_style_radius(scanModal_, 12, 0);
    lv_obj_set_style_pad_all(scanModal_, 18, 0);
    lv_obj_clear_flag(scanModal_, LV_OBJ_FLAG_SCROLLABLE);

    Theme::label(scanModal_, "Select Network", TF::xl(), TC::text());
    lv_obj_t* title = lv_obj_get_child(scanModal_, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    scanList_ = lv_obj_create(scanModal_);
    lv_obj_set_size(scanList_, LV_PCT(100), 250);
    lv_obj_align(scanList_, LV_ALIGN_TOP_LEFT, 0, 48);
    lv_obj_set_style_bg_color(scanList_, TC::surface2(), 0);
    lv_obj_set_style_bg_opa(scanList_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(scanList_, TC::border(), 0);
    lv_obj_set_style_border_width(scanList_, 1, 0);
    lv_obj_set_style_radius(scanList_, 8, 0);
    lv_obj_set_style_pad_all(scanList_, 8, 0);
    lv_obj_set_layout(scanList_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(scanList_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(scanList_, 6, 0);
    lv_obj_add_flag(scanList_, LV_OBJ_FLAG_SCROLLABLE);

    Theme::label(scanList_, "Scanning...", TF::lg(), TC::textSub());

    lv_obj_t* manual = Theme::button(scanModal_, "MANUAL ENTRY",
                                     TC::surface2(), TC::text(), 160, 40);
    lv_obj_align(manual, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(manual, onManualEntry, LV_EVENT_CLICKED, this);

    lv_obj_t* cancel = Theme::button(scanModal_, "CANCEL",
                                     TC::danger(), TC::white(), 120, 40);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(cancel, [](lv_event_t* e){
        WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
        self->closeScanModal();
    }, LV_EVENT_CLICKED, this);

    scanCount_ = 0;
    memset(scanResults_, 0, sizeof(scanResults_));
    wifi_scan_config_t cfg = {};
    esp_wifi_scan_start(&cfg, false);
    scanTimer_ = lv_timer_create(onScanTimer, 2500, this);
    lv_timer_set_repeat_count(scanTimer_, 1);
}

void WifiScreen::closeScanModal() {
    if (scanTimer_) {
        lv_timer_del(scanTimer_);
        scanTimer_ = nullptr;
    }
    if (scanModal_) {
        lv_obj_del(scanModal_);
        scanModal_ = nullptr;
    }
    scanList_ = nullptr;
}

void WifiScreen::populateScanList() {
    if (!scanList_) return;
    lv_obj_clean(scanList_);

    scanCount_ = 12;
    if (esp_wifi_scan_get_ap_records(&scanCount_, scanResults_) != ESP_OK) scanCount_ = 0;

    if (scanCount_ == 0) {
        Theme::label(scanList_, "No networks found.\nUse MANUAL ENTRY.", TF::md(), TC::muted());
        return;
    }

    for (uint16_t i = 0; i < scanCount_; i++) {
        char rowText[80];
        snprintf(rowText, sizeof(rowText), "%s  (%d dBm)", scanResults_[i].ssid, scanResults_[i].rssi);
        lv_obj_t* row = Theme::button(scanList_, rowText,
                                      TC::surface(), TC::text(), 500, 42);
        lv_obj_set_user_data(row, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(row, onScanNetwork, LV_EVENT_CLICKED, this);
    }
}

// ── Event callbacks ───────────────────────────────────────────────────────────

void WifiScreen::onBack(lv_event_t* e) {
    screenManager.navigateTo(Screen::Dashboard);
}

void WifiScreen::onAddNetwork(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    self->openScanModal();
}

void WifiScreen::onManualEntry(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    self->closeScanModal();
    self->openAddModal();
}

void WifiScreen::onScanNetwork(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    uint16_t idx = (uint16_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
    if (idx >= self->scanCount_) return;
    char ssid[33];
    strncpy(ssid, (const char*)self->scanResults_[idx].ssid, sizeof(ssid));
    ssid[32] = 0;
    self->closeScanModal();
    self->openAddModal();
    if (self->taSsid_) lv_textarea_set_text(self->taSsid_, ssid);
    if (self->kb_ && self->taPass_) lv_keyboard_set_textarea(self->kb_, self->taPass_);
}

void WifiScreen::onScanTimer(lv_timer_t* timer) {
    WifiScreen* self = (WifiScreen*)timer->user_data;
    if (!self) return;
    self->scanTimer_ = nullptr;
    self->populateScanList();
}

void WifiScreen::onAddConfirm(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    if (!self->taSsid_ || !self->mgr_) return;

    const char* ssid = lv_textarea_get_text(self->taSsid_);
    const char* pass = lv_textarea_get_text(self->taPass_);

    if (!ssid || strlen(ssid) == 0) {
        if (self->lblAddErr_) lv_label_set_text(self->lblAddErr_, "SSID cannot be empty");
        return;
    }
    if (!self->mgr_->addNetwork(ssid, pass)) {
        if (self->lblAddErr_) lv_label_set_text(self->lblAddErr_, "Network list is full (max 5)");
        return;
    }
    self->closeAddModal();
    self->lastCount_ = -1; // force rebuild
}

void WifiScreen::onAddCancel(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    self->closeAddModal();
}

void WifiScreen::onRemoveNetwork(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (self->mgr_) {
        self->mgr_->removeNetwork(idx);
        self->lastCount_ = -1; // force rebuild on next update
        self->rebuildNetworkList();
    }
}

void WifiScreen::onHotspotToggle(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    if (self->mgr_ && self->swHotspot_) {
        bool en = lv_obj_has_state(self->swHotspot_, LV_STATE_CHECKED);
        self->mgr_->setHotspot(en);
    }
}

void WifiScreen::onAutoSwitchToggle(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    if (self->mgr_ && self->swAutoSwitch_) {
        bool en = lv_obj_has_state(self->swAutoSwitch_, LV_STATE_CHECKED);
        self->mgr_->setAutoSwitch(en);
    }
}

void WifiScreen::onKeyboard(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    if (!self->kb_) return;
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* target = lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(self->kb_, target);
    } else if (code == LV_EVENT_READY) {
        // ENTER pressed — move SSID → password, or confirm
        if (self->taSsid_ && lv_keyboard_get_textarea(self->kb_) == self->taSsid_) {
            lv_keyboard_set_textarea(self->kb_, self->taPass_);
        }
    }
}
