import { useEffect, useState } from "react";
import { Pressable, ScrollView, StyleSheet, Text, TextInput, View } from "react-native";
import { C, R, S, T } from "../theme";
import { useAppState } from "../state/AppStateProvider";
import {
  fetchSettings, saveRate, saveSlowFillThreshold, saveStorageMode,
  fetchMqttSettings, saveMqttSettings,
  fetchModbusRtu, saveModbusRtu,
  fetchTime, saveTime,
  getJson, postCommand,
} from "../services/controllerApi";
import { Field, Input } from "../components/ui/Field";
import SegmentedControl from "../components/ui/SegmentedControl";
import Button from "../components/ui/Button";
import BottomNav from "../components/BottomNav";
import { money } from "../utils/format";

const TABS = [
  { key: "wifi",    label: "WiFi"    },
  { key: "mqtt",    label: "MQTT"    },
  { key: "storage", label: "Storage" },
  { key: "rtc",     label: "Clock"   },
  { key: "modbus",  label: "Modbus"  },
];

const MANUFACTURER_TABS = ["rtc", "modbus"];

export default function NetworkSettingsScreen() {
  const { navigate, activeUrl, authToken, authRole, status } = useAppState();
  const isMfg = authRole === "manufacturer";

  const visibleTabs = TABS.filter((t) => isMfg || !MANUFACTURER_TABS.includes(t.key));
  const [tab, setTab] = useState(visibleTabs[0]?.key || "wifi");

  return (
    <View style={styles.shell}>
      <View style={styles.header}>
        <Pressable onPress={() => navigate("dashboard")} style={styles.backBtn}>
          <Text style={styles.backTxt}>← Back</Text>
        </Pressable>
        <Text style={styles.title}>Settings</Text>
      </View>

      <ScrollView horizontal showsHorizontalScrollIndicator={false} style={styles.tabBar}>
        <View style={{ flexDirection: "row", gap: S.xs, paddingHorizontal: S.md, paddingVertical: S.sm }}>
          {visibleTabs.map((t) => (
            <Pressable key={t.key} style={[styles.tabBtn, tab === t.key && styles.tabBtnOn]} onPress={() => setTab(t.key)}>
              <Text style={[styles.tabTxt, tab === t.key && styles.tabTxtOn]}>{t.label}</Text>
            </Pressable>
          ))}
        </View>
      </ScrollView>

      <ScrollView contentContainerStyle={styles.content} keyboardShouldPersistTaps="handled">
        {tab === "wifi"    && <WifiPanel    activeUrl={activeUrl} authToken={authToken} />}
        {tab === "mqtt"    && <MqttPanel    activeUrl={activeUrl} authToken={authToken} />}
        {tab === "storage" && <StoragePanel activeUrl={activeUrl} authToken={authToken} status={status} authRole={authRole} />}
        {tab === "rtc"     && <ClockPanel   activeUrl={activeUrl} authToken={authToken} />}
        {tab === "modbus"  && <ModbusPanel  activeUrl={activeUrl} authToken={authToken} />}
      </ScrollView>
      <BottomNav active="network" />
    </View>
  );
}

// ── WiFi Panel ────────────────────────────────────────────────────────────────

function WifiPanel({ activeUrl, authToken }) {
  const [cfg, setCfg]     = useState(null);
  const [ssid, setSsid]   = useState("");
  const [pass, setPass]   = useState("");
  const [busy, setBusy]   = useState(false);
  const [result, setResult] = useState(null);

  useEffect(() => {
    getJson(activeUrl, "/api/wifi", authToken)
      .then((d) => { setCfg(d); setSsid(d.ssid || ""); })
      .catch(() => {});
  }, [activeUrl, authToken]);

  async function save() {
    setBusy(true); setResult(null);
    try {
      await postCommand(activeUrl, "/api/wifi", { ssid, password: pass }, authToken);
      setResult({ ok: true, msg: "WiFi settings saved — reconnect may occur" });
    } catch (err) {
      setResult({ ok: false, msg: err?.message || "Failed" });
    } finally { setBusy(false); }
  }

  return (
    <View>
      <SectionTitle>WiFi STA (Station)</SectionTitle>
      {cfg && (
        <InfoRow label="Status" value={cfg.connected ? `Connected — ${cfg.staIP}` : "Disconnected"} ok={cfg.connected} />
      )}
      <Field label="SSID">
        <Input value={ssid} onChangeText={setSsid} autoCapitalize="none" autoCorrect={false} placeholder="Network name" />
      </Field>
      <Field label="Password">
        <Input value={pass} onChangeText={setPass} secureTextEntry autoCapitalize="none" placeholder="(leave blank to keep)" />
      </Field>
      <ResultMsg result={result} />
      <Button label={busy ? "Saving…" : "Save WiFi"} variant="primary" size="full" onPress={save} loading={busy} style={{ marginTop: S.md }} />
    </View>
  );
}

// ── MQTT Panel ────────────────────────────────────────────────────────────────

const MQTT_PRESETS = [
  { key: "1", label: "HiveMQ"   },
  { key: "2", label: "Mosquitto"},
  { key: "3", label: "EMQX"    },
  { key: "0", label: "Custom"  },
];

function MqttPanel({ activeUrl, authToken }) {
  const [cfg, setCfg]     = useState(null);
  const [busy, setBusy]   = useState(false);
  const [result, setResult] = useState(null);

  useEffect(() => {
    fetchMqttSettings(activeUrl, authToken).then(setCfg).catch(() => {});
  }, [activeUrl, authToken]);

  async function save() {
    if (!cfg) return;
    setBusy(true); setResult(null);
    try {
      await saveMqttSettings(activeUrl, {
        enabled:     cfg.enabled     ? "1" : "0",
        preset:      cfg.preset,
        brokerHost:  cfg.brokerHost  || "",
        brokerPort:  cfg.brokerPort  || 1883,
        topicPrefix: cfg.topicPrefix || "lpg/controller",
        clientId:    cfg.clientId    || "",
        username:    cfg.username    || "",
        ...(cfg.newPassword ? { password: cfg.newPassword } : {}),
      }, authToken);
      setResult({ ok: true, msg: "MQTT settings saved" });
    } catch (err) {
      setResult({ ok: false, msg: err?.message || "Failed" });
    } finally { setBusy(false); }
  }

  if (!cfg) return <Text style={styles.loading}>Loading MQTT settings…</Text>;

  return (
    <View>
      <SectionTitle>MQTT Broker</SectionTitle>
      <Toggle label="MQTT Enabled" value={cfg.enabled} onChange={(v) => setCfg((p) => ({ ...p, enabled: v }))} />
      <Field label="Broker">
        <SegmentedControl options={MQTT_PRESETS} value={String(cfg.preset)} onChange={(v) => setCfg((p) => ({ ...p, preset: Number(v) }))} style={{ marginTop: S.xs }} />
      </Field>
      {cfg.preset === 0 && (
        <>
          <Field label="Broker Host">
            <Input value={cfg.brokerHost || ""} onChangeText={(v) => setCfg((p) => ({ ...p, brokerHost: v }))} autoCapitalize="none" placeholder="192.168.1.100" />
          </Field>
          <Field label="Port">
            <Input value={String(cfg.brokerPort || 1883)} keyboardType="number-pad" onChangeText={(v) => setCfg((p) => ({ ...p, brokerPort: Number(v) || 1883 }))} />
          </Field>
        </>
      )}
      <Field label="Topic Prefix">
        <Input value={cfg.topicPrefix || "lpg/controller"} onChangeText={(v) => setCfg((p) => ({ ...p, topicPrefix: v }))} autoCapitalize="none" />
      </Field>
      <Field label="Client ID (blank = auto)">
        <Input value={cfg.clientId || ""} onChangeText={(v) => setCfg((p) => ({ ...p, clientId: v }))} autoCapitalize="none" placeholder="lpg-controller-001" />
      </Field>
      <Field label="Username (optional)">
        <Input value={cfg.username || ""} onChangeText={(v) => setCfg((p) => ({ ...p, username: v }))} autoCapitalize="none" />
      </Field>
      <Field label={cfg.hasPassword ? "Password (set — blank to keep)" : "Password (optional)"}>
        <Input value={cfg.newPassword || ""} onChangeText={(v) => setCfg((p) => ({ ...p, newPassword: v }))} secureTextEntry autoCapitalize="none" />
      </Field>
      <ResultMsg result={result} />
      <Button label={busy ? "Saving…" : "Save MQTT"} variant="primary" size="full" onPress={save} loading={busy} style={{ marginTop: S.md }} />
    </View>
  );
}

// ── Storage Panel ─────────────────────────────────────────────────────────────

function StoragePanel({ activeUrl, authToken, status, authRole }) {
  const [rate,         setRate]         = useState(money(status?.ratePerKg || 0));
  const [slowFillPct,  setSlowFillPct]  = useState(String(Math.round((status?.slowFillThreshold ?? 0.95) * 100)));
  const [result,       setResult]       = useState(null);

  async function saveRateVal() {
    try { await saveRate(activeUrl, rate, authToken); setResult({ ok: true, msg: "Rate saved" }); }
    catch (err) { setResult({ ok: false, msg: err?.message || "Failed" }); }
  }

  async function saveThreshold() {
    const v = Number(slowFillPct) / 100;
    try { await saveSlowFillThreshold(activeUrl, v, authToken); setResult({ ok: true, msg: "Threshold saved" }); }
    catch (err) { setResult({ ok: false, msg: err?.message || "Failed" }); }
  }

  async function saveMode(v) {
    try { await saveStorageMode(activeUrl, Number(v), authToken); setResult({ ok: true, msg: "Storage mode saved" }); }
    catch (err) { setResult({ ok: false, msg: err?.message || "Failed" }); }
  }

  return (
    <View>
      <SectionTitle>Fill Settings</SectionTitle>
      <Field label="Rate per kg (PKR)">
        <View style={{ flexDirection: "row", gap: S.sm, alignItems: "center" }}>
          <Input value={rate} onChangeText={setRate} keyboardType="decimal-pad" style={{ flex: 1 }} />
          <Button label="Save" variant="ghost" size="sm" onPress={saveRateVal} />
        </View>
      </Field>
      <Field label={`Fast→Slow threshold (${slowFillPct || "—"}%)`}>
        <View style={{ flexDirection: "row", gap: S.sm, alignItems: "center" }}>
          <Input value={slowFillPct} onChangeText={setSlowFillPct} keyboardType="numeric" style={{ flex: 1 }} placeholder="95" />
          <Text style={{ color: C.muted, fontSize: T.sm }}>%</Text>
          <Button label="Save" variant="ghost" size="sm" onPress={saveThreshold} />
        </View>
      </Field>

      {authRole === "manufacturer" && (
        <>
          <SectionTitle style={{ marginTop: S.lg }}>SD Storage</SectionTitle>
          <Text style={styles.sub}>
            {status?.sdReady
              ? `SD ready — ${Math.round((status.sdFreeKb || 0) / 1024)} MB free of ${Math.round((status.sdTotalKb || 0) / 1024)} MB`
              : "SD not mounted"}
          </Text>
          <Field label="Storage mode">
            <SegmentedControl
              options={[{ key: "0", label: "SPIFFS" }, { key: "1", label: "SD Only" }, { key: "2", label: "Both" }]}
              value={String(status?.storageMode ?? 0)}
              onChange={saveMode}
              style={{ marginTop: S.xs }}
            />
          </Field>
        </>
      )}
      <ResultMsg result={result} />
    </View>
  );
}

// ── Clock Panel ───────────────────────────────────────────────────────────────

function ClockPanel({ activeUrl, authToken }) {
  const [time,   setTime]   = useState(null);
  const [year,   setYear]   = useState("");
  const [month,  setMonth]  = useState("");
  const [day,    setDay]    = useState("");
  const [hour,   setHour]   = useState("");
  const [minute, setMinute] = useState("");
  const [second, setSecond] = useState("");
  const [busy,   setBusy]   = useState(false);
  const [result, setResult] = useState(null);

  useEffect(() => {
    fetchTime(activeUrl, authToken)
      .then((d) => {
        setTime(d);
        setYear(String(d.year)); setMonth(String(d.month)); setDay(String(d.day));
        setHour(String(d.hour)); setMinute(String(d.minute)); setSecond(String(d.second));
      })
      .catch(() => {});
  }, [activeUrl, authToken]);

  async function save() {
    setBusy(true); setResult(null);
    try {
      await saveTime(activeUrl, { year, month, day, hour, minute, second }, authToken);
      const d = await fetchTime(activeUrl, authToken);
      setTime(d);
      setResult({ ok: true, msg: "RTC updated" });
    } catch (err) {
      setResult({ ok: false, msg: err?.message || "Failed" });
    } finally { setBusy(false); }
  }

  return (
    <View>
      <SectionTitle>Board Clock (RTC)</SectionTitle>
      {time && (
        <View style={{ flexDirection: "row", gap: S.sm, alignItems: "center", marginBottom: S.md }}>
          <Text style={[styles.chipTxt, { color: time.initialized ? C.ready : C.danger }]}>
            {time.initialized ? "RTC OK" : "NO RTC"}
          </Text>
          {time.lostPower && <Text style={[styles.chipTxt, { color: C.warning }]}>Lost Power</Text>}
          <Text style={{ color: C.text, fontWeight: "700", fontVariant: ["tabular-nums"] }}>{time.iso8601}</Text>
        </View>
      )}
      <View style={{ flexDirection: "row", gap: S.sm, marginBottom: S.sm }}>
        {[["Year", year, setYear, 70], ["Month", month, setMonth, 50], ["Day", day, setDay, 50]].map(([l, v, set, w]) => (
          <View key={l} style={{ width: w }}>
            <Text style={styles.fieldLbl}>{l}</Text>
            <TextInput value={v} onChangeText={set} keyboardType="numeric" style={[styles.smallInput, { textAlign: "center" }]} />
          </View>
        ))}
      </View>
      <View style={{ flexDirection: "row", gap: S.sm, alignItems: "flex-end" }}>
        {[["Hour", hour, setHour, 50], ["Min", minute, setMinute, 50], ["Sec", second, setSecond, 50]].map(([l, v, set, w]) => (
          <View key={l} style={{ width: w }}>
            <Text style={styles.fieldLbl}>{l}</Text>
            <TextInput value={v} onChangeText={set} keyboardType="numeric" style={[styles.smallInput, { textAlign: "center" }]} />
          </View>
        ))}
        <Button label={busy ? "…" : "Set RTC"} variant="ghost" size="sm" onPress={save} style={{ flex: 1 }} />
      </View>
      <ResultMsg result={result} />
    </View>
  );
}

// ── Modbus RTU Panel ──────────────────────────────────────────────────────────

const BAUD_OPTIONS   = [{ key: "9600", label: "9600" }, { key: "19200", label: "19.2k" }, { key: "38400", label: "38.4k" }, { key: "57600", label: "57.6k" }, { key: "115200", label: "115.2k" }];
const PARITY_OPTIONS = [{ key: "0", label: "None" }, { key: "1", label: "Even" }, { key: "2", label: "Odd" }];

function ModbusPanel({ activeUrl, authToken }) {
  const [cfg, setCfg]     = useState(null);
  const [busy, setBusy]   = useState(false);
  const [result, setResult] = useState(null);

  useEffect(() => {
    fetchModbusRtu(activeUrl, authToken).then(setCfg).catch(() => {});
  }, [activeUrl, authToken]);

  async function save() {
    if (!cfg) return;
    setBusy(true); setResult(null);
    try {
      const r = await saveModbusRtu(activeUrl, { enabled: cfg.enabled ? "1" : "0", slaveAddress: cfg.slaveAddress, baudRate: cfg.baudRate, parity: cfg.parity, stopBits: cfg.stopBits }, authToken);
      setResult({ ok: true, msg: r?.message || "RTU settings saved. Restart to apply." });
    } catch (err) {
      setResult({ ok: false, msg: err?.message || "Failed" });
    } finally { setBusy(false); }
  }

  if (!cfg) return <Text style={styles.loading}>Loading Modbus config…</Text>;

  return (
    <View>
      <SectionTitle>Modbus RTU (RS-485)</SectionTitle>
      <Text style={styles.sub}>RX GPIO {cfg.rxPin} · TX GPIO {cfg.txPin} · DE GPIO {cfg.dePin}</Text>
      <Toggle label="RTU Enabled" value={cfg.enabled} onChange={(v) => setCfg((p) => ({ ...p, enabled: v }))} />
      <Field label="Slave Address (1–247)">
        <Input value={String(cfg.slaveAddress)} onChangeText={(v) => setCfg((p) => ({ ...p, slaveAddress: Number(v) || 1 }))} keyboardType="numeric" style={{ width: 80 }} />
      </Field>
      <Field label="Baud Rate">
        <SegmentedControl options={BAUD_OPTIONS} value={String(cfg.baudRate)} onChange={(v) => setCfg((p) => ({ ...p, baudRate: Number(v) }))} style={{ marginTop: S.xs }} />
      </Field>
      <Field label="Parity">
        <SegmentedControl options={PARITY_OPTIONS} value={String(cfg.parity)} onChange={(v) => setCfg((p) => ({ ...p, parity: Number(v) }))} style={{ marginTop: S.xs }} />
      </Field>
      <Field label="Stop Bits">
        <SegmentedControl options={[{ key: "1", label: "1" }, { key: "2", label: "2" }]} value={String(cfg.stopBits)} onChange={(v) => setCfg((p) => ({ ...p, stopBits: Number(v) }))} style={{ marginTop: S.xs }} />
      </Field>
      <ResultMsg result={result} />
      <Button label={busy ? "Saving…" : "Save RTU Settings"} variant="primary" size="full" onPress={save} loading={busy} style={{ marginTop: S.md }} />
    </View>
  );
}

// ── Shared helpers ────────────────────────────────────────────────────────────

function SectionTitle({ children, style }) {
  return <Text style={[styles.sectionTitle, style]}>{children}</Text>;
}

function InfoRow({ label, value, ok }) {
  return (
    <View style={{ flexDirection: "row", justifyContent: "space-between", paddingVertical: S.xs }}>
      <Text style={styles.infoLabel}>{label}</Text>
      <Text style={[styles.infoVal, ok !== undefined && { color: ok ? C.ready : C.danger }]}>{value}</Text>
    </View>
  );
}

function Toggle({ label, value, onChange }) {
  return (
    <Pressable onPress={() => onChange(!value)} style={{ flexDirection: "row", alignItems: "center", gap: S.md, marginVertical: S.sm }}>
      <View style={{ width: 44, height: 26, borderRadius: 13, backgroundColor: value ? C.ready : C.surface2, borderWidth: 1, borderColor: value ? C.ready : C.border, justifyContent: "center", paddingHorizontal: 2 }}>
        <View style={{ width: 20, height: 20, borderRadius: 10, backgroundColor: C.white, alignSelf: value ? "flex-end" : "flex-start" }} />
      </View>
      <Text style={{ color: C.text, fontSize: T.md, fontWeight: "700" }}>{label}</Text>
    </Pressable>
  );
}

function ResultMsg({ result }) {
  if (!result) return null;
  return <Text style={[styles.resultMsg, { color: result.ok ? C.ready : C.danger }]}>{result.msg}</Text>;
}

const styles = StyleSheet.create({
  shell:   { flex: 1, backgroundColor: C.bg },
  header:  { flexDirection: "row", alignItems: "center", paddingHorizontal: S.md, paddingVertical: S.sm + 2, backgroundColor: C.surface, borderBottomWidth: 1, borderBottomColor: C.border, gap: S.md },
  backBtn: { padding: S.xs },
  backTxt: { color: C.primary, fontSize: T.md, fontWeight: "700" },
  title:   { flex: 1, color: C.text, fontSize: T.md, fontWeight: "900" },
  tabBar:  { backgroundColor: C.surface, borderBottomWidth: 1, borderBottomColor: C.border, flexGrow: 0 },
  tabBtn:  { paddingVertical: S.xs + 1, paddingHorizontal: S.md, borderRadius: R.md, borderWidth: 1, borderColor: "transparent" },
  tabBtnOn:{ backgroundColor: C.primary + "22", borderColor: C.primary },
  tabTxt:  { fontSize: T.sm, fontWeight: "700", color: C.textSub },
  tabTxtOn:{ color: C.primary },
  content: { padding: S.md, paddingBottom: S.xl },

  sectionTitle: { fontSize: T.xs, fontWeight: "800", color: C.textSub, textTransform: "uppercase", letterSpacing: 0.7, marginBottom: S.sm, marginTop: S.sm },
  sub:          { fontSize: T.xs, color: C.muted, marginBottom: S.sm },
  loading:      { color: C.muted, fontSize: T.sm, marginTop: S.md },
  infoLabel:    { fontSize: T.sm, color: C.textSub },
  infoVal:      { fontSize: T.sm, fontWeight: "700", color: C.text },
  resultMsg:    { fontSize: T.sm, textAlign: "center", marginTop: S.sm },
  chipTxt:      { fontSize: T.xs, fontWeight: "800", paddingHorizontal: S.sm, paddingVertical: S.xs, borderRadius: R.sm, backgroundColor: C.surface2 },
  fieldLbl:     { fontSize: T.xs, fontWeight: "800", color: C.textSub, textTransform: "uppercase", letterSpacing: 0.7, marginBottom: S.xs },
  smallInput:   { backgroundColor: C.surface2, borderWidth: 1, borderColor: C.border, borderRadius: R.sm, paddingHorizontal: S.sm, paddingVertical: S.xs + 2, fontSize: T.md, color: C.text },
});
