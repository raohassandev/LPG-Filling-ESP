import { StatusBar } from "expo-status-bar";
import { useEffect, useMemo, useRef, useState } from "react";
import {
  Animated,
  Easing,
  KeyboardAvoidingView,
  Platform,
  Pressable,
  SafeAreaView,
  ScrollView,
  StyleSheet,
  Text,
  TextInput,
  View,
} from "react-native";
import {
  applyTare,
  calibrateFactor,
  calibrateKnown,
  calibratePoint,
  connectStatusStream,
  createUser,
  deleteUser,
  fetchMqttSettings,
  fetchModbusRtu,
  fetchSettings,
  fetchStats,
  fetchSystem,
  fetchTime,
  fetchSdMonths,
  fetchSdTransactions,
  fetchTransactions,
  fetchTransactionsForUser,
  fetchUsers,
  fetchWeight,
  hwTare,
  login,
  logout,
  resetFill,
  saveRate,
  saveSlowFillThreshold,
  saveStorageMode,
  saveMqttSettings,
  saveModbusRtu,
  saveTime,
  startFill,
  stopFill,
  updateUser,
  zeroNet,
} from "./src/api";
import {
  DEFAULT_DEVICE_URL,
  DEV_AUTO_LOGIN_ROLE,
  DEV_CREDENTIALS,
} from "./src/constants/device";

// ─── config ──────────────────────────────────────────────────────────────────

const INITIAL_URL = DEFAULT_DEVICE_URL;

// ─── helpers ──────────────────────────────────────────────────────────────────

function getPhase(state) {
  if (!state || state === "IDLE") return "setup";
  if (["FILLING_FAST", "FILLING_SLOW", "SETTLING"].includes(state)) return "filling";
  if (state === "COMPLETE") return "complete";
  return "fault";
}
const fmt = {
  kg:    (v) => Number(v || 0).toFixed(3),
  money: (v) => Number(v || 0).toFixed(2),
  pct:   (v) => `${Number(v || 0).toFixed(0)}%`,
};
function showAlert(title, msg) {
  if (Platform.OS === "web") { window.alert(`${title}\n${msg}`); return; }
  require("react-native").Alert.alert(title, msg);
}

// ─── animation hooks ──────────────────────────────────────────────────────────

function useShake() {
  const x = useRef(new Animated.Value(0)).current;
  const shake = () =>
    Animated.sequence([
      Animated.timing(x, { toValue:  9, duration: 55, useNativeDriver: true }),
      Animated.timing(x, { toValue: -9, duration: 55, useNativeDriver: true }),
      Animated.timing(x, { toValue:  6, duration: 55, useNativeDriver: true }),
      Animated.timing(x, { toValue: -6, duration: 55, useNativeDriver: true }),
      Animated.timing(x, { toValue:  0, duration: 55, useNativeDriver: true }),
    ]).start();
  return [x, shake];
}

function useEntrance(delay = 0) {
  const opacity = useRef(new Animated.Value(0)).current;
  const y      = useRef(new Animated.Value(22)).current;
  useEffect(() => {
    Animated.parallel([
      Animated.timing(opacity, { toValue: 1, duration: 420, delay, easing: Easing.out(Easing.quad), useNativeDriver: true }),
      Animated.spring(y,       { toValue: 0, delay,         tension: 70, friction: 11,               useNativeDriver: true }),
    ]).start();
  }, []);
  return { opacity, transform: [{ translateY: y }] };
}

function useSpringIn(delay = 0) {
  const scale   = useRef(new Animated.Value(0.55)).current;
  const opacity = useRef(new Animated.Value(0)).current;
  useEffect(() => {
    Animated.parallel([
      Animated.spring(scale,   { toValue: 1, delay, tension: 90, friction: 9,  useNativeDriver: true }),
      Animated.timing(opacity, { toValue: 1, duration: 250, delay, useNativeDriver: true }),
    ]).start();
  }, []);
  return { opacity, transform: [{ scale }] };
}

function usePulse(active, fast = false) {
  const anim = useRef(new Animated.Value(1)).current;
  useEffect(() => {
    if (!active) { anim.setValue(1); return; }
    const ms = fast ? 600 : 1100;
    const loop = Animated.loop(
      Animated.sequence([
        Animated.timing(anim, { toValue: 0.35, duration: ms, easing: Easing.inOut(Easing.ease), useNativeDriver: true }),
        Animated.timing(anim, { toValue: 1,    duration: ms, easing: Easing.inOut(Easing.ease), useNativeDriver: true }),
      ])
    );
    loop.start();
    return () => loop.stop();
  }, [active, fast]);
  return anim;
}

// ─── cylinder animation pieces ────────────────────────────────────────────────

function Bubble({ left, delay, size }) {
  const anim = useRef(new Animated.Value(0)).current;
  useEffect(() => {
    const loop = Animated.loop(
      Animated.sequence([
        Animated.delay(delay),
        Animated.timing(anim, { toValue: 1, duration: 1900, easing: Easing.out(Easing.ease), useNativeDriver: true }),
        Animated.timing(anim, { toValue: 0, duration: 0,    useNativeDriver: true }),
      ])
    );
    loop.start();
    return () => loop.stop();
  }, []);
  const translateY = anim.interpolate({ inputRange: [0, 1], outputRange: [0, -64] });
  const opacity    = anim.interpolate({ inputRange: [0, 0.15, 0.75, 1], outputRange: [0, 0.55, 0.3, 0] });
  return (
    <Animated.View style={{
      position: "absolute", bottom: 10, left,
      width: size, height: size, borderRadius: size / 2,
      backgroundColor: "rgba(255,255,255,0.55)",
      transform: [{ translateY }], opacity,
    }} />
  );
}

function PulseRing({ isSlow, cylW, cylH }) {
  const anim = useRef(new Animated.Value(0)).current;
  useEffect(() => {
    const ms = isSlow ? 2200 : 1050;
    const loop = Animated.loop(
      Animated.sequence([
        Animated.timing(anim, { toValue: 1, duration: ms, easing: Easing.out(Easing.ease), useNativeDriver: true }),
        Animated.timing(anim, { toValue: 0, duration: 0,  useNativeDriver: true }),
      ])
    );
    loop.start();
    return () => loop.stop();
  }, [isSlow]);
  const scale   = anim.interpolate({ inputRange: [0, 1], outputRange: [1, 1.22] });
  const opacity = anim.interpolate({ inputRange: [0, 0.4, 1], outputRange: [0.55, 0.25, 0] });
  const color   = isSlow ? C.amber : C.teal;
  return (
    <Animated.View style={{
      position: "absolute",
      width: cylW + 18, height: cylH + 18,
      borderRadius: 28, borderWidth: 2.5, borderColor: color,
      transform: [{ scale }], opacity,
      alignSelf: "center",
    }} />
  );
}

function CylinderFill({ pct, isSlow, cylW = 110, cylH = 210 }) {
  const fillAnim = useRef(new Animated.Value(pct)).current;
  const shimmer  = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    Animated.spring(fillAnim, { toValue: pct, tension: 28, friction: 10, useNativeDriver: false }).start();
  }, [pct]);

  useEffect(() => {
    const loop = Animated.loop(
      Animated.sequence([
        Animated.timing(shimmer, { toValue: 1, duration: 2400, easing: Easing.inOut(Easing.ease), useNativeDriver: true }),
        Animated.timing(shimmer, { toValue: 0, duration: 2400, easing: Easing.inOut(Easing.ease), useNativeDriver: true }),
      ])
    );
    loop.start();
    return () => loop.stop();
  }, []);

  const liquidH = fillAnim.interpolate({
    inputRange: [0, 100],
    outputRange: [0, cylH - 16],
    extrapolate: "clamp",
  });
  const shimmerOpacity = shimmer.interpolate({ inputRange: [0, 1], outputRange: [0.04, 0.12] });
  const liquidColor = isSlow ? C.amber : C.teal;

  return (
    <View style={{ alignItems: "center" }}>
      {/* valve cap */}
      <View style={cyl.valveBase} />
      <View style={cyl.valveNeck} />

      {/* ring + body */}
      <View style={{ alignItems: "center" }}>
        <PulseRing isSlow={isSlow} cylW={cylW} cylH={cylH} />

        <View style={[cyl.body, { width: cylW, height: cylH }]}>
          {/* liquid */}
          <Animated.View style={[cyl.liquid, { height: liquidH, backgroundColor: liquidColor }]}>
            {/* surface line */}
            <View style={cyl.surface} />
            {/* bubbles */}
            <Bubble left={18}  delay={0}    size={7} />
            <Bubble left={52}  delay={700}  size={5} />
            <Bubble left={78}  delay={1300} size={8} />
          </Animated.View>

          {/* LPG label in gas space */}
          <View style={cyl.gasLabel}>
            <Text style={cyl.gasLabelTxt}>LPG</Text>
          </View>

          {/* shine stripe */}
          <Animated.View style={[cyl.shine, { opacity: shimmerOpacity }]} />
        </View>
      </View>

      {/* bottom foot */}
      <View style={cyl.foot} />
    </View>
  );
}

// ─── shared atoms ─────────────────────────────────────────────────────────────

function Pill({ label, ok, sm }) {
  return (
    <View style={[A.pill, ok ? A.pillOk : A.pillWarn, sm && A.pillSm]}>
      <Text style={[A.pillTxt, ok ? A.pillTxtOk : A.pillTxtWarn, sm && A.pillTxtSm]}>{label}</Text>
    </View>
  );
}

function Btn({ label, onPress, tone = "primary", full, disabled }) {
  return (
    <Pressable
      onPress={disabled ? undefined : onPress}
      style={({ pressed }) => [A.btn, A[`btn_${tone}`], full && A.btnFull, (pressed || disabled) && A.btnDim]}
    >
      <Text style={[A.btnTxt, tone === "ghost" && A.btnTxtGhost, tone === "danger" && A.btnTxtDanger]}>{label}</Text>
    </Pressable>
  );
}

function Field({ label, hint, children, style }) {
  return (
    <View style={[A.field, style]}>
      <Text style={A.fieldLbl}>{label}</Text>
      {children}
      {hint ? <Text style={A.fieldHint}>{hint}</Text> : null}
    </View>
  );
}

function SegControl({ options, value, onChange }) {
  return (
    <View style={A.seg}>
      {options.map(({ key, label }) => (
        <Pressable key={key} style={[A.segOpt, value === key && A.segOptOn]} onPress={() => onChange(key)}>
          <Text style={[A.segTxt, value === key && A.segTxtOn]}>{label}</Text>
        </Pressable>
      ))}
    </View>
  );
}

// ─── SignInScreen ─────────────────────────────────────────────────────────────

function SignInScreen({ activeUrl, onUrlChange, onLogin, streamMode }) {
  const [urlDraft,  setUrlDraft]  = useState(activeUrl);
  const [username,  setUsername]  = useState(DEV_AUTO_LOGIN_ROLE === "admin" ? "admin" : DEV_AUTO_LOGIN_ROLE === "maintenance" ? "manufacturer" : "operator");
  const [password,  setPassword]  = useState(DEV_CREDENTIALS[DEV_AUTO_LOGIN_ROLE] ?? "");
  const [error,     setError]     = useState("");
  const [busy,      setBusy]      = useState(false);
  const [shakeX,    shake]        = useShake();
  const entrance                  = useEntrance(80);

  async function handle() {
    setError(""); setBusy(true);
    const url = urlDraft.trim() || activeUrl;
    if (url !== activeUrl) onUrlChange(url);
    try {
      const r = await login(url, username, password);
      onLogin(r);
    } catch (err) {
      setError(err.message || "Login failed");
      shake();
    } finally { setBusy(false); }
  }

  return (
    <Animated.View style={[S.authCard, entrance]}>
      <Text style={T.display}>LPG Filling</Text>
      <Text style={[T.body, { color: C.slate, textAlign: "center", marginBottom: 24 }]}>Sign in to continue</Text>

      <Field label="Controller URL">
        <TextInput
          value={urlDraft} onChangeText={setUrlDraft}
          style={A.input} autoCapitalize="none" autoCorrect={false}
          keyboardType="url" returnKeyType="next"
          placeholder="http://192.168.1.x or http://lpg-controller.local"
          placeholderTextColor={C.muted}
        />
      </Field>
      {streamMode === "offline" && (
        <Text style={[T.caption, { color: C.amber, marginTop: 4, marginBottom: 4 }]}>
          Device unreachable. On Android, .local hostnames don't resolve — use the IP address shown on the ESP serial monitor (e.g. http://192.168.1.45).
        </Text>
      )}

      <Field label="Username">
        <TextInput
          value={username} onChangeText={setUsername}
          style={A.input} autoCapitalize="none" autoCorrect={false}
          returnKeyType="next"
          placeholder="admin / operator / manufacturer"
          placeholderTextColor={C.muted}
        />
      </Field>

      <Field label="Password">
        <Animated.View style={{ transform: [{ translateX: shakeX }] }}>
          <TextInput
            value={password} onChangeText={setPassword}
            style={A.input} secureTextEntry placeholder="••••••"
            autoCapitalize="none" returnKeyType="done"
            onSubmitEditing={handle}
            placeholderTextColor={C.muted}
          />
        </Animated.View>
      </Field>

      {!!error && <Text style={[T.caption, { color: C.red, marginTop: 8, textAlign: "center" }]}>{error}</Text>}
      <View style={{ height: 20 }} />
      <Btn label={busy ? "Signing in…" : "Sign In"} onPress={handle} full disabled={busy} />
    </Animated.View>
  );
}

// ─── CalibrationPanel ─────────────────────────────────────────────────────────

function CalibrationPanel({ activeUrl, authToken }) {
  const [weightData,   setWeightData]   = useState(null);
  const [lowKg,        setLowKg]        = useState("2.000");
  const [highKg,       setHighKg]       = useState("10.000");
  const [manualFactor, setManualFactor] = useState("");
  const [busy,         setBusy]         = useState(false);
  const [result,       setResult]       = useState(null); // { ok, msg }

  useEffect(() => {
    let t;
    const poll = async () => {
      try { setWeightData(await fetchWeight(activeUrl, authToken)); } catch {}
      t = setTimeout(poll, 1000);
    };
    poll();
    return () => clearTimeout(t);
  }, [activeUrl]);

  async function doAction(fn) {
    setBusy(true); setResult(null);
    try {
      const r = await fn();
      setResult({ ok: true, msg: JSON.stringify(r) });
    } catch (err) {
      setResult({ ok: false, msg: err.message || "Failed" });
    } finally { setBusy(false); }
  }

  const isTwoPoint = weightData?.calMode === "two-point";
  const liveKg = Number(weightData?.weightKg || 0).toFixed(3);

  return (
    <View style={{ marginTop: 20 }}>
      <View style={S.panelDivider} />
      <Text style={[T.cardTitle, { marginBottom: 4 }]}>Calibration</Text>

      {/* mode badge + live */}
      <View style={{ flexDirection: "row", gap: 8, marginBottom: 12, flexWrap: "wrap", alignItems: "center" }}>
        {weightData && <>
          <Pill label={isTwoPoint ? "Two-point active" : "Single-point"} ok={isTwoPoint} sm />
          <Pill label={weightData.initialized ? "HX711 OK" : "HX711 FAIL"} ok={!!weightData.initialized} sm />
          <Pill label={weightData.readError   ? "READ ERR" : "No Errors"}  ok={!weightData.readError}    sm />
        </>}
      </View>

      {/* live readings */}
      {weightData && (
        <View style={S.kpiRow}>
          {[
            { l: "LIVE", v: `${liveKg} kg` },
            { l: "RAW",  v: String(weightData.rawValue || 0) },
            isTwoPoint
              ? { l: "LOW PT",  v: `${weightData.calLowKg} kg` }
              : { l: "FACTOR",  v: Math.round(weightData.calFactor || 0).toLocaleString() },
            isTwoPoint
              ? { l: "HIGH PT", v: `${weightData.calHighKg} kg` }
              : { l: "TARE RAW",v: String(weightData.tareRawValue || 0) },
          ].map(({ l, v }) => (
            <View key={l} style={[S.kpiCell, { minWidth: "44%" }]}>
              <Text style={T.label}>{l}</Text>
              <Text style={[T.body, { fontWeight: "800", marginTop: 4, fontVariant: ["tabular-nums"] }]}>{v}</Text>
            </View>
          ))}
        </View>
      )}

      {/* ── Two-point (recommended) ────────────────────────────── */}
      <Text style={[T.label, { marginTop: 20, marginBottom: 4, color: C.teal }]}>
        Two-point calibration (recommended for 100 kg cell)
      </Text>
      <Text style={[T.caption, { color: C.slate, marginBottom: 12 }]}>
        Captures load cell non-linearity. Set point 1 near empty, point 2 near operating weight.
      </Text>

      <Text style={[T.label, { marginBottom: 8 }]}>Point 1 — low weight (e.g., 2 kg test weight)</Text>
      <View style={A.inputRow}>
        <TextInput value={lowKg} onChangeText={setLowKg}
          keyboardType="decimal-pad" style={[A.input, A.inputFlex]}
          placeholder="2.000" placeholderTextColor={C.muted} returnKeyType="done" />
        <Text style={[T.body, { color: C.slate, fontWeight: "700" }]}>kg</Text>
        <Btn label={busy ? "…" : "Set Pt 1"} tone="ghost" disabled={busy}
          onPress={() => doAction(() => calibratePoint(activeUrl, 1, Number(lowKg), authToken))} />
      </View>

      <Text style={[T.label, { marginTop: 14, marginBottom: 8 }]}>Point 2 — high weight (e.g., empty cylinder or 10 kg)</Text>
      <View style={A.inputRow}>
        <TextInput value={highKg} onChangeText={setHighKg}
          keyboardType="decimal-pad" style={[A.input, A.inputFlex]}
          placeholder="10.000" placeholderTextColor={C.muted} returnKeyType="done" />
        <Text style={[T.body, { color: C.slate, fontWeight: "700" }]}>kg</Text>
        <Btn label={busy ? "…" : "Set Pt 2"} tone="ghost" disabled={busy}
          onPress={() => doAction(() => calibratePoint(activeUrl, 2, Number(highKg), authToken))} />
      </View>

      {/* ── Single-point fallback ─────────────────────────────── */}
      <View style={[S.panelDivider, { marginTop: 20, marginBottom: 14 }]} />
      <Text style={[T.label, { marginBottom: 8 }]}>Single-point (legacy) — HW tare first, then place known weight</Text>
      <Btn label={busy ? "Working…" : "HW Tare  (zero empty scale)"} tone="ghost" full disabled={busy}
        onPress={() => doAction(() => hwTare(activeUrl, authToken))} />
      <View style={[A.inputRow, { marginTop: 10 }]}>
        <TextInput value={lowKg} onChangeText={setLowKg}
          keyboardType="decimal-pad" style={[A.input, A.inputFlex]}
          placeholder="2.000" placeholderTextColor={C.muted} returnKeyType="done" />
        <Text style={[T.body, { color: C.slate, fontWeight: "700" }]}>kg</Text>
        <Btn label={busy ? "…" : "Calculate"} tone="ghost" disabled={busy}
          onPress={() => doAction(() => calibrateKnown(activeUrl, Number(lowKg), authToken))} />
      </View>

      <Text style={[T.label, { marginTop: 14, marginBottom: 8 }]}>Manual factor override</Text>
      <View style={A.inputRow}>
        <TextInput value={manualFactor} onChangeText={setManualFactor}
          keyboardType="numbers-and-punctuation" style={[A.input, A.inputFlex]}
          placeholder="-45429" placeholderTextColor={C.muted} returnKeyType="done" />
        <Btn label="Set" tone="ghost" disabled={busy || !manualFactor}
          onPress={() => doAction(() => calibrateFactor(activeUrl, Number(manualFactor), authToken))} />
      </View>

      {result && (
        <View style={[S.resultMsg, result.ok ? S.resultOk : S.resultErr]}>
          <Text style={[T.caption, { color: result.ok ? C.green : C.red, fontWeight: "700" }]}>
            {result.ok ? "✓  " : "✗  "}{result.msg}
          </Text>
        </View>
      )}
    </View>
  );
}

// ─── SettingsWifiPanel ────────────────────────────────────────────────────────

function SettingsWifiPanel({ activeUrl, authToken }) {
  const [ssid,   setSsid]   = useState("");
  const [pass,   setPass]   = useState("");
  const [result, setResult] = useState(null);
  const [busy,   setBusy]   = useState(false);

  async function save() {
    setBusy(true); setResult(null);
    try {
      await fetch(`${activeUrl}/api/wifi?token=${encodeURIComponent(authToken)}&staSsid=${encodeURIComponent(ssid)}&staPassword=${encodeURIComponent(pass)}`, { method: "POST" });
      setResult({ ok: true, msg: "WiFi saved. Board will reconnect." });
    } catch (err) {
      setResult({ ok: false, msg: err.message || "Failed" });
    } finally { setBusy(false); }
  }

  return (
    <View style={{ marginTop: 16 }}>
      <View style={S.panelDivider} />
      <Text style={[T.cardTitle, { marginBottom: 4 }]}>WiFi (STA)</Text>
      <Field label="Network SSID">
        <TextInput value={ssid} onChangeText={setSsid} style={A.input}
          autoCapitalize="none" autoCorrect={false} placeholder="Your WiFi name"
          placeholderTextColor={C.muted} returnKeyType="next" />
      </Field>
      <Field label="Password">
        <TextInput value={pass} onChangeText={setPass} style={A.input}
          secureTextEntry autoCapitalize="none" placeholder="8+ characters"
          placeholderTextColor={C.muted} returnKeyType="done" />
      </Field>
      <View style={{ height: 12 }} />
      <Btn label={busy ? "Saving…" : "Save WiFi"} tone="ghost" full disabled={busy || !ssid}
        onPress={save} />
      {result && (
        <Text style={[T.caption, { color: result.ok ? C.green : C.red, marginTop: 8, textAlign: "center" }]}>
          {result.msg}
        </Text>
      )}
    </View>
  );
}

// ─── UsersPanel ───────────────────────────────────────────────────────────────

const ROLE_LABELS = { 1: "Operator", 2: "Manufacturer", 3: "Admin" };
const ROLE_VALUES = [{ key: "1", label: "Operator" }, { key: "2", label: "Manufacturer" }, { key: "3", label: "Admin" }];

function UsersPanel({ activeUrl, authToken }) {
  const [users,    setUsers]    = useState([]);
  const [busy,     setBusy]     = useState(false);
  const [newUser,  setNewUser]  = useState({ username: "", password: "", role: "1", canSetRate: false });
  const [result,   setResult]   = useState(null);

  async function load() {
    try { setUsers(await fetchUsers(activeUrl, authToken)); } catch {}
  }

  useEffect(() => { load(); }, [activeUrl, authToken]);

  async function doCreate() {
    if (!newUser.username || !newUser.password) return;
    setBusy(true); setResult(null);
    try {
      await createUser(activeUrl, newUser.username, newUser.password, newUser.role, newUser.canSetRate, authToken);
      setNewUser({ username: "", password: "", role: "1", canSetRate: false });
      setResult({ ok: true, msg: "User created" });
      load();
    } catch (err) {
      setResult({ ok: false, msg: err.message || "Failed" });
    } finally { setBusy(false); }
  }

  async function toggle(username, field, currentVal) {
    setBusy(true);
    try {
      await updateUser(activeUrl, username, { [field]: currentVal ? "0" : "1" }, authToken);
      load();
    } catch {} finally { setBusy(false); }
  }

  async function doDelete(username) {
    if (Platform.OS !== "web") {
      const { Alert } = require("react-native");
      Alert.alert("Delete user", `Delete "${username}"?`, [
        { text: "Cancel", style: "cancel" },
        { text: "Delete", style: "destructive", onPress: async () => {
          try { await deleteUser(activeUrl, username, authToken); load(); } catch {}
        }},
      ]);
    } else {
      if (!window.confirm(`Delete user "${username}"?`)) return;
      try { await deleteUser(activeUrl, username, authToken); load(); } catch {}
    }
  }

  return (
    <View style={S.card}>
      <Text style={T.cardTitle}>User Management</Text>

      {/* user list */}
      {users.length === 0
        ? <Text style={[T.body, { color: C.muted, textAlign: "center", paddingVertical: 12 }]}>No users</Text>
        : users.map((u) => (
          <View key={u.username} style={[S.txnRow, { alignItems: "center" }]}>
            <View style={{ flex: 1 }}>
              <View style={{ flexDirection: "row", alignItems: "center", gap: 8, marginBottom: 4 }}>
                <Text style={[T.body, { fontWeight: "800" }]}>{u.username}</Text>
                <Pill label={u.role} ok={u.role !== "blocked"} sm />
                {u.blocked && <Pill label="Blocked" ok={false} sm />}
                {u.canSetRate && <Pill label="Set Rate" ok={true} sm />}
              </View>
            </View>
            <View style={{ flexDirection: "row", gap: 6 }}>
              <Pressable
                style={[S.tareBtn, u.blocked && { backgroundColor: C.tealLight }]}
                onPress={() => toggle(u.username, "blocked", u.blocked)}
                disabled={busy}
              >
                <Text style={[S.tareBtnTxt, { color: u.blocked ? C.teal : C.red }]}>{u.blocked ? "Unblock" : "Block"}</Text>
              </Pressable>
              <Pressable
                style={[S.tareBtn, u.canSetRate && { backgroundColor: C.tealLight }]}
                onPress={() => toggle(u.username, "canSetRate", u.canSetRate)}
                disabled={busy}
              >
                <Text style={[S.tareBtnTxt, { color: u.canSetRate ? C.teal : C.slate }]}>Rate</Text>
              </Pressable>
              {u.role !== "admin" && (
                <Pressable style={[S.tareBtn, { backgroundColor: C.redLight }]}
                  onPress={() => doDelete(u.username)} disabled={busy}>
                  <Text style={[S.tareBtnTxt, { color: C.red }]}>Del</Text>
                </Pressable>
              )}
            </View>
          </View>
        ))
      }

      {/* create user form */}
      <View style={S.panelDivider} />
      <Text style={[T.label, { marginBottom: 8, marginTop: 4 }]}>Create User</Text>
      <Field label="Username">
        <TextInput value={newUser.username} onChangeText={(v) => setNewUser((p) => ({ ...p, username: v }))}
          style={A.input} autoCapitalize="none" placeholder="username" placeholderTextColor={C.muted} />
      </Field>
      <Field label="Password">
        <TextInput value={newUser.password} onChangeText={(v) => setNewUser((p) => ({ ...p, password: v }))}
          style={A.input} secureTextEntry placeholder="password" placeholderTextColor={C.muted} />
      </Field>
      <Field label="Role">
        <SegControl options={ROLE_VALUES} value={newUser.role}
          onChange={(r) => setNewUser((p) => ({ ...p, role: r }))} />
      </Field>
      <View style={{ flexDirection: "row", alignItems: "center", marginTop: 12, gap: 12 }}>
        <Pressable onPress={() => setNewUser((p) => ({ ...p, canSetRate: !p.canSetRate }))} style={{ flexDirection: "row", alignItems: "center", gap: 8 }}>
          <View style={{ width: 22, height: 22, borderRadius: 6, borderWidth: 2,
            borderColor: newUser.canSetRate ? C.teal : C.border,
            backgroundColor: newUser.canSetRate ? C.teal : "transparent",
            alignItems: "center", justifyContent: "center" }}>
            {newUser.canSetRate && <Text style={{ color: C.white, fontSize: 13, fontWeight: "900" }}>✓</Text>}
          </View>
          <Text style={[T.body, { color: C.secondary }]}>Allow rate setting</Text>
        </Pressable>
      </View>
      <View style={{ height: 12 }} />
      <Btn label={busy ? "Creating…" : "Create User"} tone="primary" full
        disabled={busy || !newUser.username || !newUser.password} onPress={doCreate} />
      {result && (
        <Text style={[T.caption, { color: result.ok ? C.green : C.red, marginTop: 8, textAlign: "center" }]}>
          {result.msg}
        </Text>
      )}
    </View>
  );
}

// ─── BoardInfoBar ─────────────────────────────────────────────────────────────
// Shown at the top of SetupScreen — board time, IP, Wi-Fi status

const STREAM_MODE_LABELS = {
  websocket:  { label: "WS",   color: "#34d399" },  // green — local WebSocket
  mqtt:       { label: "MQTT", color: "#60a5fa" },  // blue  — remote MQTT
  polling:    { label: "REST", color: "#fbbf24" },  // amber — REST polling
  offline:    { label: "OFF",  color: "#f87171" },  // red   — offline
  connecting: { label: "…",    color: "#94a3b8" },  // slate — connecting
};

function BoardInfoBar({ status, streamMode }) {
  const connected = !!status.wifiConnected;
  const rssi      = Number(status.wifiRssi || 0);
  const ip        = status.staIP || "—";
  const time      = status.boardTime || "";

  const rssiLabel = !connected ? "No WiFi"
                  : rssi >= -50 ? "▂▄▆█"
                  : rssi >= -65 ? "▂▄▆░"
                  : rssi >= -75 ? "▂▄░░"
                  :               "▂░░░";

  const sm = STREAM_MODE_LABELS[streamMode] || STREAM_MODE_LABELS.connecting;

  return (
    <View style={BI.bar}>
      <View style={BI.col}>
        <Text style={BI.label}>BOARD TIME</Text>
        <Text style={BI.val} numberOfLines={1}>{time || "—"}</Text>
      </View>
      <View style={[BI.col, BI.colCenter]}>
        <Text style={BI.label}>IP</Text>
        <Text style={BI.val}>{ip}</Text>
      </View>
      <View style={[BI.col, BI.colRight]}>
        <View style={{ flexDirection: "row", alignItems: "center", gap: 5, justifyContent: "flex-end" }}>
          <Text style={[BI.label, { color: connected ? C.green : C.amber }]}>
            {connected ? "WIFI" : "NO WIFI"}
          </Text>
          <View style={{ backgroundColor: sm.color + "28", borderRadius: 4, paddingHorizontal: 4, paddingVertical: 1 }}>
            <Text style={{ fontSize: 8, fontWeight: "800", color: sm.color, letterSpacing: 0.5 }}>{sm.label}</Text>
          </View>
        </View>
        <Text style={[BI.rssi, { color: connected ? C.green : C.amber }]}>{rssiLabel}</Text>
      </View>
    </View>
  );
}

// ─── LastTransactionCard ──────────────────────────────────────────────────────

function LastTransactionCard({ transactions }) {
  const last = transactions.length > 0 ? transactions[transactions.length - 1] : null;
  if (!last) return null;

  const isOk = Number(last.status) === 1;
  const ts   = Number(last.endTime || last.startTime || 0);
  const dateStr = ts > 1000000000
    ? new Date(ts * 1000).toLocaleString(undefined, { month: "short", day: "numeric", hour: "2-digit", minute: "2-digit" })
    : null;

  return (
    <View style={LT.card}>
      <View style={LT.headerRow}>
        <Text style={T.label}>LAST FILL</Text>
        <Pill label={isOk ? "COMPLETE" : "EXCEPTION"} ok={isOk} sm />
      </View>
      <View style={LT.bodyRow}>
        <View style={LT.amtBlock}>
          <Text style={LT.amtVal}>PKR {fmt.money(last.finalAmount)}</Text>
          <Text style={LT.amtSub}>{fmt.kg(last.finalKg ?? last.netKg)} kg  ·  {fmt.money(last.ratePerKg)}/kg</Text>
        </View>
        <View style={LT.metaBlock}>
          <Text style={LT.metaId} numberOfLines={1}>{last.transactionId || `#${last.id}`}</Text>
          {dateStr && <Text style={LT.metaTime}>{dateStr}</Text>}
          {!!last.operator && <Text style={LT.metaOp}>{last.operator}</Text>}
        </View>
      </View>
    </View>
  );
}

// ─── SystemResourcesPanel ────────────────────────────────────────────────────

function SystemResourcesPanel({ activeUrl, authToken }) {
  const [info, setInfo] = useState(null);

  useEffect(() => {
    let t;
    const poll = async () => {
      try { setInfo(await fetchSystem(activeUrl, authToken)); } catch {}
      t = setTimeout(poll, 5000);
    };
    poll();
    return () => clearTimeout(t);
  }, [activeUrl, authToken]);

  if (!info) return (
    <Text style={[T.body, { color: C.muted, textAlign: "center", paddingVertical: 12 }]}>Loading…</Text>
  );

  const heapPct = info.heapTotal > 0
    ? Math.round((1 - info.freeHeap / info.heapTotal) * 100) : 0;
  const spiffsPct = info.spiffsTotal > 0
    ? Math.round((info.spiffsUsed / info.spiffsTotal) * 100) : 0;

  const rows = [
    { l: "FREE HEAP",     v: `${(info.freeHeap / 1024).toFixed(0)} KB`,        warn: heapPct > 80 },
    { l: "MIN HEAP",      v: `${(info.minFreeHeap / 1024).toFixed(0)} KB`,      warn: false },
    { l: "HEAP USED",     v: `${heapPct}%`,                                     warn: heapPct > 80 },
    { l: "SPIFFS USED",   v: `${spiffsPct}%  (${(info.spiffsUsed / 1024).toFixed(0)} KB)`, warn: spiffsPct > 85 },
    { l: "SPIFFS FREE",   v: `${((info.spiffsTotal - info.spiffsUsed) / 1024).toFixed(0)} KB`, warn: false },
    { l: "UPTIME",        v: formatUptime(info.uptimeSec),                       warn: false },
    { l: "CPU",           v: `${info.cpuFreqMhz} MHz`,                          warn: false },
    { l: "WIFI RSSI",     v: info.wifiRssi ? `${info.wifiRssi} dBm` : "—",     warn: info.wifiRssi < -80 },
    { l: "BOARD IP",      v: info.staIP || "—",                                 warn: false },
    { l: "BOARD TIME",    v: info.boardTime || "—",                             warn: false },
    { l: "FIRMWARE",      v: info.firmware || "—",                              warn: false },
  ];

  return (
    <View style={{ marginTop: 16 }}>
      <View style={S.panelDivider} />
      <Text style={[T.cardTitle, { marginBottom: 10 }]}>System Resources</Text>
      {/* Heap gauge */}
      <View style={SR.gaugeWrap}>
        <View style={{ flexDirection: "row", justifyContent: "space-between", marginBottom: 4 }}>
          <Text style={T.label}>HEAP PRESSURE</Text>
          <Text style={[T.label, { color: heapPct > 80 ? C.red : C.green }]}>{heapPct}%</Text>
        </View>
        <View style={SR.gaugeTrack}>
          <View style={[SR.gaugeFill, { width: `${heapPct}%`, backgroundColor: heapPct > 80 ? C.red : heapPct > 60 ? C.amber : C.green }]} />
        </View>
      </View>
      {/* SPIFFS gauge */}
      <View style={[SR.gaugeWrap, { marginTop: 10 }]}>
        <View style={{ flexDirection: "row", justifyContent: "space-between", marginBottom: 4 }}>
          <Text style={T.label}>SPIFFS USAGE</Text>
          <Text style={[T.label, { color: spiffsPct > 85 ? C.red : C.green }]}>{spiffsPct}%</Text>
        </View>
        <View style={SR.gaugeTrack}>
          <View style={[SR.gaugeFill, { width: `${spiffsPct}%`, backgroundColor: spiffsPct > 85 ? C.red : spiffsPct > 70 ? C.amber : C.teal }]} />
        </View>
      </View>
      {/* KPI grid */}
      <View style={[S.kpiRow, { marginTop: 14 }]}>
        {rows.map(({ l, v, warn }) => (
          <View key={l} style={[S.kpiCell, warn && S.kpiCellWarn, { minWidth: "45%" }]}>
            <Text style={T.label}>{l}</Text>
            <Text style={[T.body, { fontWeight: "700", marginTop: 3, fontSize: 13 }, warn && { color: C.red }]}>{v}</Text>
          </View>
        ))}
      </View>
    </View>
  );
}

function formatUptime(sec) {
  if (!sec) return "—";
  const d = Math.floor(sec / 86400);
  const h = Math.floor((sec % 86400) / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = sec % 60;
  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m ${s}s`;
  return `${m}m ${s}s`;
}

// ─── MqttSettingsPanel ────────────────────────────────────────────────────────

const MQTT_PRESETS = [
  { key: "0", label: "Custom" },
  { key: "1", label: "HiveMQ  (broker.hivemq.com)" },
  { key: "2", label: "Mosquitto  (test.mosquitto.org)" },
  { key: "3", label: "EMQX  (broker.emqx.io)" },
];

function MqttSettingsPanel({ activeUrl, authToken }) {
  const [cfg,     setCfg]    = useState(null);
  const [busy,    setBusy]   = useState(false);
  const [result,  setResult] = useState(null);

  useEffect(() => {
    fetchMqttSettings(activeUrl, authToken)
      .then((d) => setCfg(d))
      .catch(() => {});
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
      setResult({ ok: false, msg: err.message || "Failed" });
    } finally { setBusy(false); }
  }

  if (!cfg) return (
    <Text style={[T.body, { color: C.muted, marginTop: 8 }]}>Loading MQTT settings…</Text>
  );

  return (
    <View style={{ marginTop: 16 }}>
      <View style={S.panelDivider} />
      <Text style={[T.cardTitle, { marginBottom: 4 }]}>MQTT</Text>
      <Text style={[T.caption, { color: C.slate, marginBottom: 12 }]}>
        Publish fill status and transactions to a MQTT broker for cloud dashboards, alerts, and billing integration.
      </Text>

      {/* Enable toggle */}
      <Pressable onPress={() => setCfg((p) => ({ ...p, enabled: !p.enabled }))}
        style={{ flexDirection: "row", alignItems: "center", gap: 12, marginBottom: 16 }}>
        <View style={{
          width: 48, height: 28, borderRadius: 14,
          backgroundColor: cfg.enabled ? C.teal : C.border,
          justifyContent: "center", paddingHorizontal: 3,
        }}>
          <View style={{
            width: 22, height: 22, borderRadius: 11, backgroundColor: C.white,
            alignSelf: cfg.enabled ? "flex-end" : "flex-start",
          }} />
        </View>
        <Text style={[T.body, { fontWeight: "700" }]}>
          {cfg.enabled ? "MQTT Enabled" : "MQTT Disabled"}
        </Text>
      </Pressable>

      {/* Broker preset */}
      <Field label="Broker">
        <SegControl
          options={[
            { key: "1", label: "HiveMQ" },
            { key: "2", label: "Mosquitto" },
            { key: "3", label: "EMQX" },
            { key: "0", label: "Custom" },
          ]}
          value={String(cfg.preset)}
          onChange={(v) => setCfg((p) => ({ ...p, preset: Number(v) }))}
        />
      </Field>

      {/* Public broker note */}
      {cfg.preset !== 0 && (
        <Text style={[T.caption, { color: C.slate, marginTop: 6 }]}>
          {MQTT_PRESETS.find((b) => b.key === String(cfg.preset))?.label}  ·  port 1883  ·  no auth required
        </Text>
      )}

      {/* Custom host/port */}
      {cfg.preset === 0 && (
        <>
          <Field label="Broker Host">
            <TextInput value={cfg.brokerHost || ""} onChangeText={(v) => setCfg((p) => ({ ...p, brokerHost: v }))}
              style={A.input} autoCapitalize="none" autoCorrect={false}
              placeholder="192.168.1.100 or mqtt.example.com" placeholderTextColor={C.muted} />
          </Field>
          <Field label="Port">
            <TextInput value={String(cfg.brokerPort || 1883)} keyboardType="number-pad"
              onChangeText={(v) => setCfg((p) => ({ ...p, brokerPort: Number(v) || 1883 }))}
              style={A.input} placeholderTextColor={C.muted} />
          </Field>
        </>
      )}

      <Field label="Topic Prefix">
        <TextInput value={cfg.topicPrefix || "lpg/controller"} autoCapitalize="none"
          onChangeText={(v) => setCfg((p) => ({ ...p, topicPrefix: v }))}
          style={A.input} placeholderTextColor={C.muted} />
      </Field>

      <Text style={[T.caption, { color: C.muted, marginTop: 4, marginBottom: 12 }]}>
        Topics: {cfg.topicPrefix || "lpg/controller"}/status  ·  /transaction  ·  /alert  ·  /lwt
      </Text>

      <Field label="Client ID  (leave blank = auto)">
        <TextInput value={cfg.clientId || ""} autoCapitalize="none"
          onChangeText={(v) => setCfg((p) => ({ ...p, clientId: v }))}
          style={A.input} placeholder="lpg-controller-001" placeholderTextColor={C.muted} />
      </Field>

      <Field label="Username  (optional)">
        <TextInput value={cfg.username || ""} autoCapitalize="none"
          onChangeText={(v) => setCfg((p) => ({ ...p, username: v }))}
          style={A.input} placeholder="(blank = no auth)" placeholderTextColor={C.muted} />
      </Field>

      <Field label={cfg.hasPassword ? "Password  (set — leave blank to keep)" : "Password  (optional)"}>
        <TextInput value={cfg.newPassword || ""} secureTextEntry autoCapitalize="none"
          onChangeText={(v) => setCfg((p) => ({ ...p, newPassword: v }))}
          style={A.input} placeholder="••••••" placeholderTextColor={C.muted} />
      </Field>

      <View style={{ height: 14 }} />
      <Btn label={busy ? "Saving…" : "Save MQTT Settings"} tone="primary" full disabled={busy} onPress={save} />
      {result && (
        <Text style={[T.caption, { color: result.ok ? C.green : C.red, marginTop: 8, textAlign: "center" }]}>
          {result.msg}
        </Text>
      )}
    </View>
  );
}

// ─── StatsPanel ───────────────────────────────────────────────────────────────
// Manufacturer / admin: aggregated transaction KPIs by period

const STAT_PERIODS = [
  { key: "today", label: "Today" },
  { key: "week",  label: "This Week" },
  { key: "month", label: "This Month" },
  { key: "year",  label: "This Year" },
  { key: "all",   label: "All Time" },
];

function StatsPanel({ activeUrl, authToken }) {
  const [stats,  setStats]  = useState(null);
  const [period, setPeriod] = useState("today");

  useEffect(() => {
    let t;
    const poll = async () => {
      try { setStats(await fetchStats(activeUrl, authToken)); } catch {}
      t = setTimeout(poll, 30000);
    };
    poll();
    return () => clearTimeout(t);
  }, [activeUrl, authToken]);

  const p = period;
  const completed = stats ? (p === "all" ? stats.allCompleted : stats[`${p}Completed`]) : "—";
  const failed    = stats ? (p === "all" ? stats.allFailed    : stats[`${p}Failed`])    : "—";
  const kg        = stats ? Number(p === "all" ? stats.allKg     : stats[`${p}Kg`])     : 0;
  const amount    = stats ? Number(p === "all" ? stats.allAmount : stats[`${p}Amount`]) : 0;

  return (
    <View style={{ marginTop: 16 }}>
      <View style={S.panelDivider} />
      <Text style={[T.cardTitle, { marginBottom: 8 }]}>Transaction Statistics</Text>
      <View style={S.periodRow}>
        {STAT_PERIODS.map(({ key, label }) => (
          <Pressable key={key} style={[S.chip, period === key && S.chipOn]} onPress={() => setPeriod(key)}>
            <Text style={[T.caption, { fontWeight: "700", color: period === key ? C.white : C.secondary }]}>{label}</Text>
          </Pressable>
        ))}
      </View>
      {!stats
        ? <Text style={[T.body, { color: C.muted, textAlign: "center", paddingVertical: 12 }]}>Loading…</Text>
        : (
          <View style={S.kpiRow}>
            {[
              { l: "COMPLETED",   v: String(completed),               ok: true },
              { l: "FAILED",      v: String(failed),                   ok: Number(failed) === 0 },
              { l: "KG SOLD",     v: `${Number(kg).toFixed(1)} kg`,   ok: null },
              { l: "REVENUE PKR", v: `${Number(amount).toFixed(0)}`,  ok: null },
            ].map(({ l, v, ok }) => (
              <View key={l} style={[S.kpiCell, ok === false && S.kpiCellWarn]}>
                <Text style={T.label}>{l}</Text>
                <Text style={[T.numSm, { marginTop: 4 }, ok === false && { color: C.red }]}>{v}</Text>
              </View>
            ))}
          </View>
        )
      }
    </View>
  );
}

// ─── ClockPanel ───────────────────────────────────────────────────────────────

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
  }, [activeUrl]);

  async function save() {
    setBusy(true); setResult(null);
    try {
      await saveTime(activeUrl, { year, month, day, hour, minute, second }, authToken);
      const d = await fetchTime(activeUrl, authToken);
      setTime(d);
      setResult({ ok: true, msg: "RTC updated" });
    } catch (err) {
      setResult({ ok: false, msg: err.message || "Failed" });
    } finally { setBusy(false); }
  }

  return (
    <View style={{ marginTop: 16 }}>
      <View style={S.panelDivider} />
      <Text style={[T.cardTitle, { marginBottom: 4 }]}>Board Clock (RTC)</Text>
      {time && (
        <View style={{ flexDirection: "row", alignItems: "center", gap: 8, marginBottom: 10 }}>
          <Pill label={time.initialized ? "RTC OK" : "NO RTC"} ok={!!time.initialized} sm />
          {time.lostPower && <Pill label="Lost Power" ok={false} sm />}
          <Text style={[T.body, { fontWeight: "700", fontVariant: ["tabular-nums"] }]}>{time.iso8601}</Text>
        </View>
      )}
      <View style={A.inputRow}>
        {[
          { label: "Year",   val: year,   set: setYear,   kbType: "numeric", width: 70 },
          { label: "Month",  val: month,  set: setMonth,  kbType: "numeric", width: 50 },
          { label: "Day",    val: day,    set: setDay,    kbType: "numeric", width: 50 },
        ].map(({ label, val, set, width }) => (
          <View key={label} style={{ width }}>
            <Text style={[A.fieldLbl, { marginBottom: 2 }]}>{label}</Text>
            <TextInput value={val} onChangeText={set} keyboardType="numeric"
              style={[A.input, { textAlign: "center" }]} placeholderTextColor={C.muted} />
          </View>
        ))}
      </View>
      <View style={[A.inputRow, { marginTop: 6 }]}>
        {[
          { label: "Hour",   val: hour,   set: setHour,   width: 50 },
          { label: "Min",    val: minute, set: setMinute, width: 50 },
          { label: "Sec",    val: second, set: setSecond, width: 50 },
        ].map(({ label, val, set, width }) => (
          <View key={label} style={{ width }}>
            <Text style={[A.fieldLbl, { marginBottom: 2 }]}>{label}</Text>
            <TextInput value={val} onChangeText={set} keyboardType="numeric"
              style={[A.input, { textAlign: "center" }]} placeholderTextColor={C.muted} />
          </View>
        ))}
        <View style={{ flex: 1, justifyContent: "flex-end" }}>
          <Btn label={busy ? "…" : "Set RTC"} onPress={save} disabled={busy} tone="ghost" />
        </View>
      </View>
      {result && (
        <Text style={[T.caption, { color: result.ok ? C.green : C.red, marginTop: 6 }]}>{result.msg}</Text>
      )}
    </View>
  );
}

// ─── ModbusRtuPanel ───────────────────────────────────────────────────────────

const BAUD_OPTIONS = [
  { key: "9600",   label: "9600" },
  { key: "19200",  label: "19.2k" },
  { key: "38400",  label: "38.4k" },
  { key: "57600",  label: "57.6k" },
  { key: "115200", label: "115.2k" },
];
const PARITY_OPTIONS = [
  { key: "0", label: "None" },
  { key: "1", label: "Even" },
  { key: "2", label: "Odd" },
];

function ModbusRtuPanel({ activeUrl, authToken }) {
  const [cfg,    setCfg]    = useState(null);
  const [busy,   setBusy]   = useState(false);
  const [result, setResult] = useState(null);

  useEffect(() => {
    fetchModbusRtu(activeUrl, authToken)
      .then((d) => setCfg(d))
      .catch(() => {});
  }, [activeUrl, authToken]);

  async function save() {
    if (!cfg) return;
    setBusy(true); setResult(null);
    try {
      const r = await saveModbusRtu(activeUrl, {
        enabled:      cfg.enabled ? "1" : "0",
        slaveAddress: cfg.slaveAddress,
        baudRate:     cfg.baudRate,
        parity:       cfg.parity,
        stopBits:     cfg.stopBits,
      }, authToken);
      setResult({ ok: true, msg: r.message || "RTU settings saved" });
    } catch (err) {
      setResult({ ok: false, msg: err.message || "Failed" });
    } finally { setBusy(false); }
  }

  if (!cfg) return <Text style={[T.body, { color: C.muted, marginTop: 8 }]}>Loading RTU config…</Text>;

  return (
    <View style={{ marginTop: 16 }}>
      <View style={S.panelDivider} />
      <Text style={[T.cardTitle, { marginBottom: 4 }]}>Modbus RTU (RS-485)</Text>
      <Text style={[T.caption, { color: C.slate, marginBottom: 8 }]}>
        RX GPIO {cfg.rxPin} · TX GPIO {cfg.txPin} · DE GPIO {cfg.dePin}
      </Text>

      {/* Enable toggle */}
      <Pressable onPress={() => setCfg((p) => ({ ...p, enabled: !p.enabled }))}
        style={{ flexDirection: "row", alignItems: "center", gap: 12, marginBottom: 14 }}>
        <View style={{ width: 48, height: 28, borderRadius: 14,
          backgroundColor: cfg.enabled ? C.teal : C.border, justifyContent: "center", paddingHorizontal: 3 }}>
          <View style={{ width: 22, height: 22, borderRadius: 11, backgroundColor: C.white,
            alignSelf: cfg.enabled ? "flex-end" : "flex-start" }} />
        </View>
        <Text style={[T.body, { fontWeight: "700" }]}>{cfg.enabled ? "RTU Enabled" : "RTU Disabled"}</Text>
      </Pressable>

      {/* Slave Address */}
      <Field label="Slave Address (1–247)">
        <TextInput value={String(cfg.slaveAddress)}
          onChangeText={(v) => setCfg((p) => ({ ...p, slaveAddress: Number(v) || 1 }))}
          keyboardType="numeric" style={[A.input, { width: 80 }]}
          returnKeyType="done" placeholderTextColor={C.muted} />
      </Field>

      {/* Baud Rate */}
      <Field label="Baud Rate">
        <SegControl
          options={BAUD_OPTIONS}
          value={String(cfg.baudRate)}
          onChange={(v) => setCfg((p) => ({ ...p, baudRate: Number(v) }))}
        />
      </Field>

      {/* Parity */}
      <Field label="Parity">
        <SegControl
          options={PARITY_OPTIONS}
          value={String(cfg.parity)}
          onChange={(v) => setCfg((p) => ({ ...p, parity: Number(v) }))}
        />
      </Field>

      {/* Stop Bits */}
      <Field label="Stop Bits">
        <SegControl
          options={[{ key: "1", label: "1" }, { key: "2", label: "2" }]}
          value={String(cfg.stopBits)}
          onChange={(v) => setCfg((p) => ({ ...p, stopBits: Number(v) }))}
        />
      </Field>

      <Btn label={busy ? "Saving…" : "Save RTU Settings"} onPress={save} disabled={busy} full />
      <Text style={[T.caption, { color: C.muted, marginTop: 4, textAlign: "center" }]}>
        Changes take effect after restart
      </Text>
      {result && (
        <Text style={[T.caption, { color: result.ok ? C.green : C.red, marginTop: 6, textAlign: "center" }]}>
          {result.msg}
        </Text>
      )}
    </View>
  );
}

// ─── StatusDetailPanel ────────────────────────────────────────────────────────
// Structured status view replacing raw JSON.stringify in MFG diagnostics

function StatusDetailPanel({ status }) {
  const [expanded, setExpanded] = useState(false);

  const sections = [
    {
      title: "Fill Process",
      rows: [
        { l: "State",       v: status.stateLabel || status.state || "—" },
        { l: "Boot Reason", v: status.bootReason || "—" },
        { l: "Uptime",      v: formatUptime(Math.round((status.uptimeMs || 0) / 1000)) },
      ],
    },
    {
      title: "Weight",
      rows: [
        { l: "Live",    v: `${fmt.kg(status.liveWeightKg ?? status.weightKg)} kg` },
        { l: "Tare",    v: `${fmt.kg(status.tareWeightKg)} kg` },
        { l: "Net",     v: `${fmt.kg(status.netWeightKg)} kg` },
        { l: "Target",  v: `${fmt.kg(status.targetWeightKg)} kg` },
        { l: "Stable",  v: status.weightStable ? "Yes" : "No",   warn: !status.weightStable },
        { l: "HX711 DOUT", v: `GPIO ${status.hx711DoutPin || "?"} = ${status.hx711DoutLevel}` },
      ],
    },
    {
      title: "Safety Interlocks",
      rows: [
        { l: "E-Stop",    v: status.emergencyStopOk  ? "OK" : "TRIPPED", warn: !status.emergencyStopOk },
        { l: "Cylinder",  v: status.cylinderPresent  ? "Present" : "Absent" },
        { l: "Nozzle",    v: status.nozzleEngaged    ? "Engaged" : "Disengaged" },
      ],
    },
    {
      title: "Network",
      rows: [
        { l: "WiFi",      v: status.wifiConnected ? "Connected" : "Disconnected", warn: !status.wifiConnected },
        { l: "IP",        v: status.staIP || "—" },
        { l: "RSSI",      v: status.wifiRssi ? `${status.wifiRssi} dBm` : "—",  warn: status.wifiRssi < -80 },
        { l: "Board Time",v: status.boardTime || "—" },
      ],
    },
    {
      title: "Settings",
      rows: [
        { l: "Rate / kg",        v: `PKR ${fmt.money(status.ratePerKg)}` },
        { l: "Slow Fill at",     v: status.slowFillThreshold ? `${Math.round(status.slowFillThreshold * 100)}%` : "—" },
        { l: "Transactions",     v: String(status.transactionCount || 0) },
        { l: "Reason Code",      v: status.reasonCode || "—" },
      ],
    },
  ];

  return (
    <View style={{ marginTop: 16 }}>
      <View style={S.panelDivider} />
      <Pressable style={{ flexDirection: "row", justifyContent: "space-between", alignItems: "center", marginBottom: 8 }}
        onPress={() => setExpanded((p) => !p)}>
        <Text style={T.cardTitle}>Status Detail</Text>
        <Text style={[T.caption, { color: C.teal, fontWeight: "700" }]}>{expanded ? "▲ Collapse" : "▼ Expand"}</Text>
      </Pressable>
      {expanded && sections.map(({ title, rows }) => (
        <View key={title} style={SD.section}>
          <Text style={SD.sectionTitle}>{title}</Text>
          {rows.map(({ l, v, warn }) => (
            <View key={l} style={SD.row}>
              <Text style={SD.rowLabel}>{l}</Text>
              <Text style={[SD.rowVal, warn && { color: C.amber }]}>{v}</Text>
            </View>
          ))}
        </View>
      ))}
      {expanded && (
        <Pressable onPress={() => setExpanded(false)} style={{ marginTop: 4 }}>
          <Text style={[T.caption, { color: C.muted, textAlign: "center" }]}>▲ Collapse</Text>
        </Pressable>
      )}
    </View>
  );
}

// ─── SetupScreen ──────────────────────────────────────────────────────────────

function SafetyBadge({ label, ok }) {
  const pulse = usePulse(!ok, true);
  return (
    <Animated.View style={[A.pill, ok ? A.pillOk : A.pillWarn, A.pillSm, { opacity: ok ? 1 : pulse }]}>
      <Text style={[A.pillTxt, ok ? A.pillTxtOk : A.pillTxtWarn, A.pillTxtSm]}>
        {ok ? `${label} ✓` : `${label} ✗`}
      </Text>
    </Animated.View>
  );
}

// ─── SdArchivePanel ──────────────────────────────────────────────────────────
// Shown in History tab when SD card is ready — month picker + SD transaction list

function SdArchivePanel({ activeUrl, authToken }) {
  const [months,      setMonths]      = useState([]);
  const [sdReady,     setSdReady]     = useState(false);
  const [selMonth,    setSelMonth]    = useState(null);
  const [records,     setRecords]     = useState([]);
  const [loading,     setLoading]     = useState(false);

  useEffect(() => {
    fetchSdMonths(activeUrl, authToken)
      .then((d) => {
        setSdReady(!!d.ready);
        setMonths(Array.isArray(d.months) ? d.months : []);
      })
      .catch(() => {});
  }, [activeUrl, authToken]);

  async function loadMonth(month) {
    setSelMonth(month); setLoading(true); setRecords([]);
    try {
      const rows = await fetchSdTransactions(activeUrl, month, authToken);
      setRecords(rows);
    } catch {}
    setLoading(false);
  }

  if (!sdReady) return (
    <View style={{ marginTop: 12, padding: 12, backgroundColor: C.surface, borderRadius: 10 }}>
      <Text style={[T.label, { color: C.muted }]}>SD Card not mounted</Text>
    </View>
  );

  return (
    <View style={{ marginTop: 12 }}>
      <Text style={T.cardTitle}>SD Archive</Text>
      <ScrollView horizontal showsHorizontalScrollIndicator={false} style={{ marginBottom: 8 }}>
        {months.map((m) => (
          <Pressable key={m}
            style={[{ paddingHorizontal: 12, paddingVertical: 6, borderRadius: 8, marginRight: 6,
                      backgroundColor: selMonth === m ? C.teal : C.surface,
                      borderWidth: 1, borderColor: selMonth === m ? C.teal : C.border }]}
            onPress={() => loadMonth(m)}>
            <Text style={[T.label, { color: selMonth === m ? C.white : C.textPrimary }]}>{m}</Text>
          </Pressable>
        ))}
      </ScrollView>
      {loading && <Text style={[T.caption, { color: C.muted }]}>Loading…</Text>}
      {records.map((rec, i) => {
        const isOk = Number(rec.status) === 1;
        const ts   = Number(rec.endTime || rec.startTime || 0);
        const dateStr = ts > 1e9
          ? new Date(ts * 1000).toLocaleString(undefined, { month:"short", day:"numeric", hour:"2-digit", minute:"2-digit" })
          : "—";
        return (
          <View key={rec.id ?? i} style={[S.txnRow, { marginBottom: 6 }]}>
            <View style={{ flex: 1 }}>
              <Text style={[T.label, { color: isOk ? C.green : C.amber }]}>
                {isOk ? "● Complete" : "✗ " + (rec.faultCode || "Aborted")}
              </Text>
              <Text style={T.caption}>{dateStr} · {rec.operator || "—"}</Text>
            </View>
            <View style={{ alignItems: "flex-end" }}>
              <Text style={T.numSm}>{fmt.kg(rec.netKg)} kg</Text>
              <Text style={[T.caption, { color: C.muted }]}>PKR {fmt.money(rec.finalAmount)}</Text>
            </View>
          </View>
        );
      })}
      {!loading && selMonth && records.length === 0 && (
        <Text style={[T.caption, { color: C.muted }]}>No records for {selMonth}</Text>
      )}
    </View>
  );
}

// ─── SetupScreen ─────────────────────────────────────────────────────────────

function SetupScreen({ status, activeUrl, authToken, authRole, authUsername, authCanSetRate, onLogout, transactions, streamMode }) {
  const [tareWeight,  setTareWeight]  = useState(fmt.money(status.tareWeightKg));
  const [targetWeight,setTargetWeight]= useState("11.800");
  const [targetAmount,setTargetAmount]= useState("2950.00");
  const [rate,        setRate]        = useState(fmt.money(status.ratePerKg || 250));
  const [inputMode,   setInputMode]   = useState("weight");
  const [adminRate,   setAdminRate]   = useState(fmt.money(status.ratePerKg || 250));
  const [period,      setPeriod]      = useState("all");
  const [secTab,      setSecTab]      = useState(null);
  const [editTare,      setEditTare]    = useState(false);
  const [slowFillPct,   setSlowFillPct] = useState(String(Math.round((status.slowFillThreshold ?? 0.95) * 100)));
  const slowFillInit = useRef(false);
  const rateInitialized = useRef(false);
  const [formShakeX,  formShake]      = useShake();

  useEffect(() => { if (!editTare) setTareWeight(fmt.money(status.tareWeightKg)); }, [status.tareWeightKg]);
  useEffect(() => {
    if (!rateInitialized.current && status.ratePerKg) {
      setRate(fmt.money(status.ratePerKg));
      setAdminRate(fmt.money(status.ratePerKg));
      rateInitialized.current = true;
    }
  }, [status.ratePerKg]);

  useEffect(() => {
    if (!slowFillInit.current && status.slowFillThreshold) {
      setSlowFillPct(String(Math.round(status.slowFillThreshold * 100)));
      slowFillInit.current = true;
    }
  }, [status.slowFillThreshold]);

  function syncTargets(mode, changed, value) {
    const r = Number(rate || 0);
    if (r <= 0) return;
    const m = mode || inputMode;
    if (m === "amount" || changed === "amount") {
      const a = Number(changed === "amount" ? value : targetAmount);
      setTargetWeight(fmt.kg(a / r));
      if (changed === "amount") setTargetAmount(value);
    } else {
      const w = Number(changed === "weight" ? value : targetWeight);
      setTargetAmount(fmt.money(w * r));
      if (changed === "weight") setTargetWeight(value);
    }
  }

  async function run(action, success) {
    try { await action(); if (success) showAlert("Done", success); }
    catch (err) { formShake(); showAlert("Error", err.message || "Command failed"); }
  }

  // If firmware exposes readyToFill/blockers, use them as authoritative source.
  // Fall back to individual status flags for older firmware.
  const BLOCKER_LABELS = {
    active_fault:          "Active fault — reset required",
    estop_active:          "E-Stop active",
    cylinder_missing:      "No cylinder detected",
    nozzle_not_engaged:    "Nozzle not engaged",
    scale_not_initialized: "Scale not initialized",
    scale_read_error:      "Scale read error",
    scale_unstable:        "Scale unstable",
    scale_not_calibrated:  "Scale not calibrated",
    simulation_active:     "SIMULATION MODE ACTIVE",
  };
  const safety = status.blockers
    ? status.blockers.map((b) => ({ key: b, label: BLOCKER_LABELS[b] || b, ok: false }))
        .concat(status.readyToFill ? [{ key: "ready", label: "Ready", ok: true }] : [])
        .filter((s) => !s.ok || s.key === "ready")
    : [
        { key: "nozzle",   label: "Nozzle",    ok: !!status.nozzleEngaged },
        { key: "cylinder", label: "Cylinder",  ok: !!status.cylinderPresent },
        { key: "estop",    label: "E-Stop",    ok: !!status.emergencyStopOk },
        { key: "scale",    label: "Scale",     ok: !!status.weightInitialized && !status.weightReadError },
        { key: "stable",   label: "Stable",    ok: !!status.weightStable },
        { key: "cal",      label: "Calibrated",ok: !!status.calValid },
      ];

  const txnSummary = useMemo(() => {
    const f = transactions.filter((item) => {
      if (period === "all") return true;
      const sec = Number(item.endTime || item.startTime || 0);
      if (!sec) return true;
      const d = new Date(sec * 1000), now = new Date();
      if (period === "today") return d.toDateString() === now.toDateString();
      if (period === "week")  { const s = new Date(now); s.setHours(0,0,0,0); s.setDate(now.getDate()-now.getDay()); return d>=s; }
      if (period === "month") return d.getFullYear()===now.getFullYear() && d.getMonth()===now.getMonth();
      return d.getFullYear() === now.getFullYear();
    });
    const done = f.filter((i) => Number(i.status) === 1);
    const excp = f.filter((i) => Number(i.status) >= 2);
    return { filtered: f, done: done.length, excp: excp.length,
      sales: done.reduce((s,i)=>s+Number(i.finalAmount||0),0),
      kgSold: done.reduce((s,i)=>s+Number(i.netKg||0),0) };
  }, [transactions, period]);

  return (
    <KeyboardAvoidingView behavior={Platform.OS === "ios" ? "padding" : undefined} style={{ flex: 1 }}>
      <ScrollView contentContainerStyle={S.setupShell} keyboardShouldPersistTaps="handled">

        {/* board info bar — time, IP, Wi-Fi */}
        <BoardInfoBar status={status} streamMode={streamMode} />

        {/* weight strip */}
        <View style={S.weightStrip}>
          {[
            { lbl: "LIVE", val: fmt.kg(status.liveWeightKg ?? status.weightKg), accent: false },
            { lbl: "TARE", val: fmt.kg(status.tareWeightKg),                    accent: false },
          ].map(({ lbl, val }, i) => (
            <View key={lbl} style={[S.wCell, i > 0 && S.wCellBorder]}>
              <Text style={[T.label, { color: C.muted }]}>{lbl}</Text>
              <Text style={T.numMd}>{val}</Text>
              <Text style={[T.caption, { color: C.muted, marginTop: 1 }]}>kg</Text>
            </View>
          ))}
          <View style={[S.wCell, S.wCellBorder, S.wCellAccent]}>
            <View style={{ flexDirection: "row", alignItems: "center", gap: 5 }}>
              <Text style={[T.label, { color: "rgba(255,255,255,0.6)" }]}>NET</Text>
              <View style={{ width: 6, height: 6, borderRadius: 3, backgroundColor: status.weightStable ? "rgba(255,255,255,0.9)" : C.amber }} />
            </View>
            <Text style={[T.numLg, { color: C.white, marginTop: 2 }]}>{fmt.kg(status.netWeightKg)}</Text>
            <Text style={[T.caption, { color: "rgba(255,255,255,0.5)", marginTop: 1 }]}>kg</Text>
          </View>
        </View>

        {/* last transaction summary */}
        <LastTransactionCard transactions={transactions} />

        {/* fill form */}
        <Animated.View style={[S.fillCard, { transform: [{ translateX: formShakeX }] }]}>

          {/* card header: title + safety row */}
          <View style={S.fillCardHeader}>
            <Text style={S.fillCardTitle}>Fill Setup</Text>
            <View style={S.safetyRow}>
              {safety.every((s) => s.ok)
                ? <View style={S.readyBadge}><Text style={S.readyBadgeTxt}>● Ready</Text></View>
                : safety.map((s) => <SafetyBadge key={s.key} label={s.label} ok={s.ok} />)
              }
            </View>
          </View>

          {/* tare row */}
          <View style={S.tareRow}>
            <View style={{ flex: 1 }}>
              <Text style={A.fieldLbl}>Empty cylinder tare</Text>
              <TextInput value={tareWeight} onChangeText={setTareWeight}
                onFocus={() => setEditTare(true)} onBlur={() => setEditTare(false)}
                keyboardType="decimal-pad" style={S.tareInput}
                returnKeyType="done" placeholderTextColor={C.muted} placeholder="0.000" />
            </View>
            <View style={S.tareBtns}>
              <Pressable style={S.tareBtn} onPress={() => run(() => applyTare(activeUrl, tareWeight, authToken))}>
                <Text style={S.tareBtnTxt}>Apply</Text>
              </Pressable>
              <Pressable style={S.tareBtn} onPress={() => run(() => zeroNet(activeUrl, authToken))}>
                <Text style={S.tareBtnTxt}>Zero</Text>
              </Pressable>
            </View>
          </View>

          <View style={S.fillDivider} />

          {/* mode toggle */}
          <SegControl
            options={[{ key: "weight", label: "Fill by Weight (kg)" }, { key: "amount", label: "Fill by Amount (PKR)" }]}
            value={inputMode}
            onChange={(m) => { setInputMode(m); syncTargets(m); }}
          />

          {/* primary input */}
          <View style={S.primaryInputWrap}>
            <Text style={A.fieldLbl}>{inputMode === "weight" ? "Target weight" : "Target amount"}</Text>
            <View style={S.primaryInputRow}>
              <Text style={S.primaryUnit}>{inputMode === "weight" ? "kg" : "PKR"}</Text>
              <TextInput
                value={inputMode === "weight" ? targetWeight : targetAmount}
                onChangeText={(v) => syncTargets(inputMode, inputMode === "weight" ? "weight" : "amount", v)}
                keyboardType="decimal-pad" style={S.primaryInput}
                returnKeyType="next" placeholderTextColor={C.muted}
                placeholder={inputMode === "weight" ? "0.000" : "0.00"}
              />
            </View>
          </View>

          {/* rate + result row */}
          <View style={S.rateResultRow}>
            <View style={{ flex: 1 }}>
              <Text style={A.fieldLbl}>Rate / kg</Text>
              <View style={S.rateInputWrap}>
                <Text style={S.rateUnit}>PKR</Text>
                <TextInput value={rate} onChangeText={authCanSetRate ? (v) => { setRate(v); syncTargets(inputMode); } : undefined}
                  editable={authCanSetRate}
                  keyboardType="decimal-pad" style={[S.rateInput, !authCanSetRate && { color: C.muted }]}
                  returnKeyType="done" placeholderTextColor={C.muted} placeholder="0.00" />
              </View>
            </View>
            <View style={{ flex: 1 }}>
              <Text style={A.fieldLbl}>{inputMode === "weight" ? "Total amount" : "Equiv. weight"}</Text>
              <View style={S.resultBox}>
                <Text style={S.resultUnit}>{inputMode === "weight" ? "PKR" : "kg"}</Text>
                <Text style={S.resultVal}>
                  {inputMode === "weight" ? targetAmount : targetWeight}
                </Text>
              </View>
            </View>
          </View>

          {/* slow fill threshold */}
          <View style={S.fillDivider} />
          <Field label={`Fast→Slow switch at ${slowFillPct || "—"}% of target`}>
            <View style={A.inputRow}>
              <TextInput
                value={slowFillPct}
                onChangeText={authCanSetRate ? setSlowFillPct : undefined}
                editable={!!authCanSetRate}
                keyboardType="numeric"
                style={[A.input, A.inputFlex, !authCanSetRate && { color: C.muted }]}
                placeholder="95" placeholderTextColor={C.muted} returnKeyType="done"
              />
              <Text style={[T.caption, { color: C.muted, alignSelf: "center" }]}>%</Text>
              {authCanSetRate && (
                <Btn label="Set" tone="ghost"
                  onPress={() => {
                    const v = Number(slowFillPct) / 100;
                    run(() => saveSlowFillThreshold(activeUrl, v, authToken), "Threshold saved");
                  }}
                />
              )}
            </View>
          </Field>

          {status.simActive && (
            <View style={{ backgroundColor: "#dc2626", borderRadius: 6, padding: 8, marginBottom: 8 }}>
              <Text style={{ color: "#fff", fontWeight: "bold", textAlign: "center" }}>
                ⚠ SIMULATION MODE ACTIVE — DO NOT CONNECT REAL LPG OUTPUTS
              </Text>
            </View>
          )}

          {!safety.every((s) => s.ok) && (
            <Text style={S.interlockWarn}>⚠  Check interlocks before starting</Text>
          )}

          <Pressable
            style={[S.startBtn, (!safety.every((s) => s.ok) || status.simActive) && S.startBtnDim]}
            disabled={!!status.simActive}
            onPress={() => run(() => startFill(activeUrl, targetWeight, rate, targetAmount, authToken))}
          >
            <Text style={S.startBtnTxt}>Start Fill</Text>
          </Pressable>
        </Animated.View>

        {/* secondary nav — role-gated */}
        <View style={S.secNav}>
          {(authRole === "admin" || authRole === "manufacturer") && (
            <Pressable style={[S.secBtn, secTab === "history" && S.secBtnOn]} onPress={() => setSecTab(secTab === "history" ? null : "history")}>
              <Text style={[T.caption, { fontWeight: "700", color: secTab === "history" ? C.white : C.slate }]}>History</Text>
            </Pressable>
          )}
          {(authRole === "admin" || authRole === "manufacturer") && (
            <Pressable style={[S.secBtn, secTab === "users" && S.secBtnOn]} onPress={() => setSecTab(secTab === "users" ? null : "users")}>
              <Text style={[T.caption, { fontWeight: "700", color: secTab === "users" ? C.white : C.slate }]}>Users</Text>
            </Pressable>
          )}
          {(authRole === "admin" || authRole === "manufacturer") && (
            <Pressable style={[S.secBtn, secTab === "settings" && S.secBtnOn]} onPress={() => setSecTab(secTab === "settings" ? null : "settings")}>
              <Text style={[T.caption, { fontWeight: "700", color: secTab === "settings" ? C.white : C.slate }]}>Settings</Text>
            </Pressable>
          )}
          {authRole === "manufacturer" && (
            <Pressable style={[S.secBtn, secTab === "diag" && S.secBtnOn]} onPress={() => setSecTab(secTab === "diag" ? null : "diag")}>
              <Text style={[T.caption, { fontWeight: "700", color: secTab === "diag" ? C.white : C.slate }]}>Diagnostics</Text>
            </Pressable>
          )}
          {authRole === "operator" && (
            <Pressable style={[S.secBtn, secTab === "history" && S.secBtnOn]} onPress={() => setSecTab(secTab === "history" ? null : "history")}>
              <Text style={[T.caption, { fontWeight: "700", color: secTab === "history" ? C.white : C.slate }]}>My History</Text>
            </Pressable>
          )}
          <Pressable style={S.secBtn} onPress={onLogout}>
            <Text style={[T.caption, { fontWeight: "700", color: C.slate }]}>Sign Out ({authUsername})</Text>
          </Pressable>
        </View>

        {/* history panel */}
        {secTab === "history" && (
          <View style={S.card}>
            <Text style={T.cardTitle}>
              {authRole === "operator" ? "My Transactions" : "All Transactions"}
            </Text>
            <View style={S.periodRow}>
              {["today","week","month","year","all"].map((p) => (
                <Pressable key={p} style={[S.chip, period === p && S.chipOn]} onPress={() => setPeriod(p)}>
                  <Text style={[T.caption, { fontWeight: "700", textTransform: "capitalize", color: period === p ? C.white : C.secondary }]}>{p}</Text>
                </Pressable>
              ))}
            </View>
            <View style={S.kpiRow}>
              {[
                { l: "Sales (PKR)", v: fmt.money(txnSummary.sales) },
                { l: "kg Sold",     v: fmt.kg(txnSummary.kgSold) },
                { l: "Complete",    v: String(txnSummary.done) },
                { l: "Exceptions",  v: String(txnSummary.excp), warn: txnSummary.excp > 0 },
              ].map(({ l, v, warn }) => (
                <View key={l} style={[S.kpiCell, warn && S.kpiCellWarn]}>
                  <Text style={T.label}>{l}</Text>
                  <Text style={[T.numSm, { marginTop: 4 }, warn && { color: C.red }]}>{v}</Text>
                </View>
              ))}
            </View>
            <Text style={[T.label, { marginTop: 18, marginBottom: 6 }]}>Recent Transactions</Text>
            {txnSummary.filtered.length === 0
              ? <Text style={[T.body, { color: C.muted, textAlign: "center", paddingVertical: 12 }]}>No transactions</Text>
              : txnSummary.filtered.slice(-20).reverse().map((item) => {
                  const ts = Number(item.endTime || item.startTime || 0);
                  const dateStr = (ts > 1000000000)
                    ? new Date(ts * 1000).toLocaleString(undefined, { month: "short", day: "numeric", hour: "2-digit", minute: "2-digit" })
                    : null;
                  const isOk = Number(item.status) === 1;
                  return (
                    <View key={item.transactionId || item.id} style={S.txnRow}>
                      <View style={{ flex: 1 }}>
                        <View style={{ flexDirection: "row", alignItems: "center", gap: 8, marginBottom: 3 }}>
                          <Text style={[T.body, { fontWeight: "800" }]}>{item.transactionId || item.id}</Text>
                          <Pill label={isOk ? "OK" : "ERR"} ok={isOk} sm />
                          {!!item.operator && <Text style={[T.caption, { color: C.slate }]}>{item.operator}</Text>}
                        </View>
                        <View style={S.txnDetail}>
                          <Text style={[T.caption, { color: C.teal, fontWeight: "800", minWidth: 72 }]}>
                            PKR {fmt.money(item.finalAmount)}
                          </Text>
                          <Text style={[T.caption, { color: C.secondary }]}>
                            {fmt.kg(item.finalKg ?? item.netKg)} kg
                          </Text>
                          <Text style={[T.caption, { color: C.muted }]}>
                            @ {fmt.money(item.ratePerKg)}/kg
                          </Text>
                        </View>
                        {dateStr && (
                          <Text style={[T.caption, { color: C.muted, marginTop: 2 }]}>{dateStr}</Text>
                        )}
                        {!isOk && !!item.faultCode && (
                          <Text style={[T.caption, { color: C.red, marginTop: 2 }]}>{item.faultCode}</Text>
                        )}
                      </View>
                    </View>
                  );
                })}
            {/* SD archive — only when card is mounted */}
            {(authRole === "admin" || authRole === "manufacturer") && status.sdReady && (
              <SdArchivePanel activeUrl={activeUrl} authToken={authToken} />
            )}
          </View>
        )}

        {/* users panel (admin+) */}
        {secTab === "users" && (
          <UsersPanel activeUrl={activeUrl} authToken={authToken} />
        )}

        {/* settings panel (admin+) */}
        {secTab === "settings" && (
          <View style={S.card}>
            <Text style={T.cardTitle}>Settings</Text>
            <Field label="Rate per kg">
              <View style={A.inputRow}>
                <TextInput value={adminRate} onChangeText={setAdminRate}
                  keyboardType="decimal-pad" style={[A.input, A.inputFlex]}
                  returnKeyType="done" placeholderTextColor={C.muted} />
                <Btn label="Save" onPress={() => run(() => saveRate(activeUrl, adminRate, authToken), "Rate saved")} tone="ghost" />
              </View>
            </Field>
            <Field label={`Fast→Slow switch (${slowFillPct || "—"}% of target)`}>
              <View style={A.inputRow}>
                <TextInput value={slowFillPct} onChangeText={setSlowFillPct}
                  keyboardType="numeric" style={[A.input, A.inputFlex]}
                  returnKeyType="done" placeholderTextColor={C.muted} placeholder="95" />
                <Text style={[T.caption, { color: C.muted, alignSelf: "center" }]}>%</Text>
                <Btn label="Save" tone="ghost"
                  onPress={() => {
                    const v = Number(slowFillPct) / 100;
                    run(() => saveSlowFillThreshold(activeUrl, v, authToken), "Threshold saved");
                  }}
                />
              </View>
            </Field>
            <SettingsWifiPanel activeUrl={activeUrl} authToken={authToken} />
            <MqttSettingsPanel activeUrl={activeUrl} authToken={authToken} />
            {authRole === "manufacturer" && (
              <>
                <ClockPanel    activeUrl={activeUrl} authToken={authToken} />
                <ModbusRtuPanel activeUrl={activeUrl} authToken={authToken} />
                <View style={S.panelDivider} />
                <Text style={T.cardTitle}>Storage Mode</Text>
                <Text style={[T.caption, { color: C.muted, marginBottom: 8 }]}>
                  {status.sdReady
                    ? `SD ready — ${Math.round((status.sdFreeKb||0)/1024)} MB free of ${Math.round((status.sdTotalKb||0)/1024)} MB`
                    : "SD not mounted (wire microSD to SPI2: MOSI=13 MISO=12 CLK=14 CS=27)"}
                </Text>
                <SegControl
                  options={[
                    { key: "0", label: "SPIFFS" },
                    { key: "1", label: "SD Only" },
                    { key: "2", label: "Both" },
                  ]}
                  value={String(status.storageMode ?? 0)}
                  onChange={(v) => run(() => saveStorageMode(activeUrl, Number(v), authToken), "Storage mode saved")}
                />
              </>
            )}
          </View>
        )}

        {/* diagnostics panel (manufacturer only) */}
        {secTab === "diag" && (
          <View style={S.card}>
            <Text style={T.cardTitle}>Diagnostics</Text>
            <Text style={[T.label, { marginBottom: 8 }]}>Relays</Text>
            <View style={S.diagGrid}>
              {(status.relays || []).map((on, i) => (
                <View key={i} style={[S.diagCell, on && S.diagCellOn]}>
                  <Text style={[T.label, on && { color: C.green }]}>R{i+1}</Text>
                  <Text style={[T.body, { fontWeight: "800", marginTop: 2, color: on ? C.green : C.muted }]}>{on ? "ON" : "—"}</Text>
                </View>
              ))}
            </View>
            <Text style={[T.label, { marginTop: 14, marginBottom: 8 }]}>Inputs</Text>
            <View style={S.diagGrid}>
              {(status.inputs || []).map((on, i) => (
                <View key={i} style={[S.diagCell, on && S.diagCellOn]}>
                  <Text style={[T.label, on && { color: C.green }]}>IN{i+1}</Text>
                  <Text style={[T.body, { fontWeight: "800", marginTop: 2, color: on ? C.green : C.muted }]}>{on ? "HI" : "LO"}</Text>
                </View>
              ))}
            </View>
            <StatsPanel activeUrl={activeUrl} authToken={authToken} />
            <SystemResourcesPanel activeUrl={activeUrl} authToken={authToken} />
            <StatusDetailPanel status={status} />
            <CalibrationPanel activeUrl={activeUrl} authToken={authToken} />
          </View>
        )}
      </ScrollView>
    </KeyboardAvoidingView>
  );
}

// ─── FillScreen ───────────────────────────────────────────────────────────────

function FillScreen({ status, activeUrl, authToken }) {
  const isSlow = status.state === "FILLING_SLOW";
  const target = Number(status.targetWeightKg || 0);
  const net    = Number(status.netWeightKg    || 0);
  const pct    = target > 0 ? Math.min(100, Math.max(0, (net / target) * 100)) : 0;

  const progressAnim = useRef(new Animated.Value(pct)).current;
  useEffect(() => {
    Animated.spring(progressAnim, { toValue: pct, tension: 30, friction: 10, useNativeDriver: false }).start();
  }, [pct]);
  const progressWidth = progressAnim.interpolate({ inputRange: [0, 100], outputRange: ["0%", "100%"], extrapolate: "clamp" });

  const [stopShakeX, stopShake] = useShake();

  async function handleStop() {
    try { await stopFill(activeUrl, authToken); }
    catch (err) { stopShake(); showAlert("Error", err.message || "Stop failed"); }
  }

  const accentColor = isSlow ? C.amber : C.teal;

  return (
    <View style={F.shell}>
      {/* state badge row */}
      <View style={F.topRow}>
        <View style={[F.stateBadge, { backgroundColor: isSlow ? C.amberDark : C.tealDark }]}>
          <Text style={[T.label, { color: C.white, letterSpacing: 1 }]}>
            {isSlow ? "⬇  SLOW FILL" : "⚡  FAST FILL"}
          </Text>
        </View>
        <Text style={[T.caption, { color: C.navyMuted }]}>
          PKR {fmt.money(status.currentAmount)} running
        </Text>
      </View>

      {/* cylinder + percentage */}
      <View style={F.cylinderRow}>
        <CylinderFill pct={pct} isSlow={isSlow} cylW={100} cylH={200} />
        <View style={F.pctBlock}>
          <Text style={F.pctNum}>{pct.toFixed(0)}</Text>
          <Text style={F.pctSymbol}>%</Text>
        </View>
      </View>

      {/* net / target */}
      <View style={F.weightRow}>
        <Text style={F.netNum}>{fmt.kg(net)}</Text>
        <Text style={F.weightSep}>/</Text>
        <Text style={F.targetNum}>{fmt.kg(target)}</Text>
        <Text style={F.weightUnit}>kg</Text>
      </View>

      {/* progress bar */}
      <View style={F.progressTrack}>
        <Animated.View style={[F.progressFill, { width: progressWidth, backgroundColor: accentColor }]} />
      </View>

      {/* stats row */}
      <View style={F.statsRow}>
        {[
          { l: "RATE",   v: `${fmt.money(status.ratePerKg)}/kg` },
          { l: "TARGET", v: `${fmt.kg(target)} kg`              },
          { l: "AMOUNT", v: `PKR ${fmt.money(status.currentAmount)}` },
        ].map(({ l, v }, i, arr) => (
          <View key={l} style={[F.statCell, i < arr.length - 1 && F.statDivider]}>
            <Text style={[T.label, { color: C.navyMuted, letterSpacing: 0.8 }]}>{l}</Text>
            <Text style={[T.body,  { color: C.white, fontWeight: "700", marginTop: 3 }]}>{v}</Text>
          </View>
        ))}
      </View>

      <View style={{ flex: 1 }} />

      {/* stop button */}
      <Animated.View style={{ transform: [{ translateX: stopShakeX }] }}>
        <Pressable style={({ pressed }) => [F.stopBtn, pressed && F.stopBtnPressed]} onPress={handleStop}>
          <Text style={F.stopTxt}>■  STOP FILL</Text>
        </Pressable>
      </Animated.View>
    </View>
  );
}

// ─── CompleteScreen ───────────────────────────────────────────────────────────

function ReceiptRow({ label, value, big, accent, delay, first }) {
  const entrance = useEntrance(delay);
  return (
    <Animated.View style={entrance}>
      {!first && <View style={CP.line} />}
      <View style={CP.receiptRow}>
        <Text style={[T.body, { color: C.secondary }]}>{label}</Text>
        <Text style={[big ? T.numSm : T.body, { fontWeight: "800" }, accent && { color: C.teal }]}>
          {value}
        </Text>
      </View>
    </Animated.View>
  );
}

function CompleteScreen({ status, activeUrl, authToken, transactions }) {
  const last        = transactions[transactions.length - 1];
  const checkSpring = useSpringIn(0);

  // Use transaction record (stable) rather than live status (zeroes when cylinder removed)
  const dispensedKg  = last ? Number(last.finalKg  || last.netKg || 0) : Number(status.netWeightKg  || 0);
  const amountPkr    = last ? Number(last.finalAmount || 0)             : Number(status.currentAmount || 0);
  const ratePerKg    = last ? Number(last.ratePerKg  || 0)              : Number(status.ratePerKg    || 0);

  const rows = [
    { label: "Dispensed",   value: `${fmt.kg(dispensedKg)} kg`,      accent: false, big: true  },
    { label: "Amount",      value: `PKR ${fmt.money(amountPkr)}`,     accent: true,  big: true  },
    { label: "Rate",        value: `${fmt.money(ratePerKg)} / kg`,    accent: false, big: false },
    last && { label: "Transaction", value: last.transactionId,        accent: false, big: false },
  ].filter(Boolean);

  async function handleNew() {
    try { await resetFill(activeUrl, authToken); }
    catch (err) { showAlert("Error", err.message); }
  }

  return (
    <View style={CP.shell}>
      <View style={CP.hero}>
        <Animated.View style={[CP.checkCircle, checkSpring]}>
          <Text style={CP.checkMark}>✓</Text>
        </Animated.View>
        <Text style={[T.title, { color: C.white, marginTop: 14 }]}>Fill Complete</Text>
      </View>

      <View style={CP.receipt}>
        {rows.map((row, i) => (
          <ReceiptRow key={row.label} {...row} delay={120 + i * 80} first={i === 0} />
        ))}
      </View>

      <Btn label="New Fill  →" onPress={handleNew} tone="primary" full />
    </View>
  );
}

// ─── FaultScreen ──────────────────────────────────────────────────────────────

function FaultScreen({ status, activeUrl, authToken }) {
  const [shakeX, shake] = useShake();
  useEffect(() => { setTimeout(shake, 250); }, []);

  async function handleReset() {
    try { await resetFill(activeUrl, authToken); }
    catch (err) { showAlert("Error", err.message); }
  }

  return (
    <View style={FT.shell}>
      <Animated.View style={[FT.hero, { transform: [{ translateX: shakeX }] }]}>
        <Text style={FT.icon}>⚠</Text>
        <Text style={[T.title, { color: C.white, marginTop: 10 }]}>{status.state || "FAULT"}</Text>
        {!!status.reasonCode && (() => {
          const FAULT_GUIDANCE = {
            emergency_stop:       { action: "Check and reset the emergency stop switch.", reset: "Clear E-stop, then reset." },
            nozzle_disengaged:    { action: "Nozzle was removed during fill.", reset: "Reconnect nozzle, reset to idle." },
            scale_read_error:     { action: "HX711 scale sensor failure.", reset: "Check HX711 wiring, power cycle, reset." },
            overfill:             { action: "Weight exceeded target + 500 g limit.", reset: "Check scale calibration, reset." },
            no_flow:              { action: "No weight increase detected for 10 s.", reset: "Check valve/pump, reset and retry." },
            fill_timeout:         { action: "Fill exceeded 5-minute time limit.", reset: "Check system, reset and retry." },
            transaction_log_failed:{ action: "Storage full — cannot log transaction.", reset: "Free SPIFFS storage, reset." },
          };
          const g = FAULT_GUIDANCE[status.reasonCode];
          return (
            <>
              <Text style={[T.body, { color: "rgba(255,255,255,0.75)", marginTop: 6, textAlign: "center" }]}>
                {status.reasonCode}
              </Text>
              {g && (
                <>
                  <Text style={[T.caption, { color: "rgba(255,255,255,0.85)", marginTop: 8, textAlign: "center" }]}>{g.action}</Text>
                  <Text style={[T.caption, { color: "rgba(255,200,100,0.9)", marginTop: 4, textAlign: "center" }]}>{g.reset}</Text>
                </>
              )}
            </>
          );
        })()}
      </Animated.View>
      <Btn label="Reset to Idle" onPress={handleReset} tone="danger" full />
    </View>
  );
}

// ─── Root App ─────────────────────────────────────────────────────────────────

export default function App() {
  const [activeUrl,      setActiveUrl]      = useState(INITIAL_URL);
  const [connectionKey,  setConnectionKey]  = useState(0);
  const [streamMode,     setStreamMode]     = useState("connecting");
  const [status,         setStatus]         = useState({});
  const [transactions,   setTransactions]   = useState([]);
  const [authToken,      setAuthToken]      = useState("");
  const [authRole,       setAuthRole]       = useState("operator");
  const [authUsername,   setAuthUsername]   = useState("");
  const [authCanSetRate, setAuthCanSetRate] = useState(false);
  const [authLoading,    setAuthLoading]    = useState(true);
  const [mqttConfig,     setMqttConfig]     = useState(null);

  // Fetch MQTT config once after login so connectStatusStream can use it
  useEffect(() => {
    if (!authToken) return;
    fetchMqttSettings(activeUrl, authToken)
      .then((d) => setMqttConfig(d))
      .catch(() => {});
  }, [activeUrl, authToken]);

  useEffect(() => {
    const stop = connectStatusStream(activeUrl, setStatus, setStreamMode, mqttConfig, authToken);
    return () => stop();
  }, [activeUrl, connectionKey, mqttConfig, authToken]);

  useEffect(() => {
    if (!authToken) return;
    const refresh = async () => {
      try {
        const txns = await fetchTransactionsForUser(activeUrl, "", authToken);
        setTransactions(txns);
        const s = await fetchSettings(activeUrl, authToken);
        setStatus((p) => ({
          ...p,
          ...(s.ratePerKg         != null && { ratePerKg: s.ratePerKg }),
          ...(s.slowFillThreshold != null && { slowFillThreshold: s.slowFillThreshold }),
          ...(s.storageMode       != null && { storageMode: s.storageMode }),
          ...(s.sdReady           != null && { sdReady: s.sdReady }),
          ...(s.sdTotalKb         != null && { sdTotalKb: s.sdTotalKb }),
          ...(s.sdFreeKb          != null && { sdFreeKb: s.sdFreeKb }),
        }));
      } catch {}
    };
    refresh();
    const t = setInterval(refresh, 5000);
    return () => clearInterval(t);
  }, [activeUrl, connectionKey, authToken]);

  function applySession(r) {
    setAuthToken(r.token || "");
    setAuthRole(r.role || "operator");
    setAuthUsername(r.username || "");
    setAuthCanSetRate(!!r.canSetRate || r.role === "admin" || r.role === "manufacturer");
  }

  useEffect(() => {
    if (!DEV_AUTO_LOGIN_ROLE) { setAuthLoading(false); return; }
    setAuthToken(""); setAuthLoading(true);
    login(activeUrl, DEV_AUTO_LOGIN_ROLE, DEV_CREDENTIALS[DEV_AUTO_LOGIN_ROLE] ?? "")
      .then((r) => { applySession(r); setAuthLoading(false); })
      .catch(()  => setAuthLoading(false));
  }, [activeUrl, connectionKey]);

  async function handleLogout() {
    if (authToken) { try { await logout(activeUrl, authToken); } catch {} }
    setAuthToken(""); setAuthRole("operator"); setAuthUsername(""); setAuthCanSetRate(false);
  }

  const phase    = getPhase(status.state);
  const filling  = phase === "filling";
  const connected = streamMode === "polling" || streamMode === "websocket" || streamMode === "mqtt";

  return (
    <SafeAreaView style={[S.safe, filling && S.safeFilling]}>
      <StatusBar style={filling ? "light" : "dark"} />

      {/* top bar */}
      <View style={[S.topBar, filling && S.topBarFilling]}>
        <Text style={[T.topBarTitle, filling && { color: C.white }]}>LPG Filling</Text>
        <View style={S.topRight}>
          <Text style={{ fontSize: 11, color: connected ? C.green : C.amber }}>●</Text>
          <View style={[S.statePill, phase === "complete" && S.statePillOk, phase === "fault" && S.statePillFault, filling && S.statePillFilling]}>
            <Text style={[T.label, { color: filling ? C.white : C.textPrimary }]}>{status.state || "…"}</Text>
          </View>
          <Pressable onPress={() => { setConnectionKey((k) => k + 1); setAuthToken(""); }} style={{ padding: 6 }}>
            <Text style={{ fontSize: 18, color: filling ? C.navyMuted : C.slate }}>⟳</Text>
          </Pressable>
        </View>
      </View>

      {/* content */}
      {authLoading ? (
        <View style={S.center}>
          <Text style={[T.body, { color: C.slate }]}>Connecting to device…</Text>
        </View>
      ) : !authToken ? (
        <ScrollView contentContainerStyle={S.centerScroll}>
          <SignInScreen activeUrl={activeUrl} onUrlChange={setActiveUrl} onLogin={applySession} streamMode={streamMode} />
        </ScrollView>
      ) : filling ? (
        <FillScreen status={status} activeUrl={activeUrl} authToken={authToken} />
      ) : phase === "complete" ? (
        <ScrollView contentContainerStyle={S.centerScroll}>
          <CompleteScreen status={status} activeUrl={activeUrl} authToken={authToken} transactions={transactions} />
        </ScrollView>
      ) : phase === "fault" ? (
        <ScrollView contentContainerStyle={S.centerScroll}>
          <FaultScreen status={status} activeUrl={activeUrl} authToken={authToken} />
        </ScrollView>
      ) : (
        <SetupScreen status={status} activeUrl={activeUrl} authToken={authToken}
          authRole={authRole} authUsername={authUsername} authCanSetRate={authCanSetRate}
          onLogout={handleLogout} transactions={transactions} streamMode={streamMode} />
      )}
    </SafeAreaView>
  );
}

// ─── Design tokens ────────────────────────────────────────────────────────────

const C = {
  bg:         "#f8fafc",
  card:       "#ffffff",
  border:     "#e2e8f0",
  teal:       "#0d9488",
  tealLight:  "#ccfbf1",
  tealDark:   "#0f4c45",
  amber:      "#d97706",
  amberLight: "#fef3c7",
  amberDark:  "#78350f",
  red:        "#dc2626",
  redLight:   "#fef2f2",
  green:      "#16a34a",
  greenLight: "#f0fdf4",
  navy:       "#080f1e",
  navyMid:    "#0f1b2d",
  navyCard:   "#141f33",
  navyMuted:  "#8898aa",
  slate:      "#64748b",
  secondary:  "#475569",
  muted:      "#94a3b8",
  textPrimary:"#0f172a",
  white:      "#ffffff",
};

// typography system
const T = StyleSheet.create({
  display:     { fontSize: 30, fontWeight: "900", color: C.textPrimary, textAlign: "center", letterSpacing: -0.5 },
  title:       { fontSize: 22, fontWeight: "900", color: C.textPrimary, letterSpacing: -0.3 },
  cardTitle:   { fontSize: 16, fontWeight: "800", color: C.textPrimary, letterSpacing: -0.2, marginBottom: 4 },
  topBarTitle: { fontSize: 17, fontWeight: "900", color: C.textPrimary, letterSpacing: -0.3 },
  body:        { fontSize: 14, fontWeight: "400", color: C.textPrimary, lineHeight: 21 },
  label:       { fontSize: 11, fontWeight: "800", color: C.muted, textTransform: "uppercase", letterSpacing: 0.8 },
  caption:     { fontSize: 12, fontWeight: "400", color: C.secondary, lineHeight: 17 },
  numLg:       { fontSize: 28, fontWeight: "900", color: C.textPrimary, fontVariant: ["tabular-nums"] },
  numMd:       { fontSize: 24, fontWeight: "900", color: C.textPrimary, fontVariant: ["tabular-nums"] },
  numSm:       { fontSize: 18, fontWeight: "900", color: C.textPrimary, fontVariant: ["tabular-nums"] },
});

// shared atoms styles
const A = StyleSheet.create({
  pill:       { borderRadius: 999, paddingHorizontal: 10, paddingVertical: 5, borderWidth: 1 },
  pillOk:     { backgroundColor: C.greenLight, borderColor: C.green },
  pillWarn:   { backgroundColor: C.redLight,   borderColor: C.red   },
  pillSm:     { paddingHorizontal: 8, paddingVertical: 3 },
  pillTxt:    { fontSize: 13, fontWeight: "800" },
  pillTxtOk:  { color: C.green },
  pillTxtWarn:{ color: C.red   },
  pillTxtSm:  { fontSize: 11 },

  btn:         { borderRadius: 12, paddingVertical: 15, paddingHorizontal: 20, alignItems: "center", justifyContent: "center" },
  btnFull:     { width: "100%" },
  btnDim:      { opacity: 0.65 },
  btn_primary: { backgroundColor: C.teal },
  btn_danger:  { backgroundColor: C.red  },
  btn_ghost:   { backgroundColor: "transparent", borderWidth: 1.5, borderColor: C.border, paddingVertical: 13 },
  btn_dark:    { backgroundColor: C.secondary },
  btnTxt:      { color: C.white,     fontSize: 15, fontWeight: "800", letterSpacing: 0.2 },
  btnTxtGhost: { color: C.secondary, fontSize: 15, fontWeight: "700" },
  btnTxtDanger:{ color: C.white },

  field:       { marginTop: 16 },
  fieldLbl:    { fontSize: 11, fontWeight: "800", color: C.slate, textTransform: "uppercase", letterSpacing: 0.7, marginBottom: 8 },
  fieldHint:   { fontSize: 11, color: C.muted, marginTop: 5 },

  seg:         { flexDirection: "row", borderRadius: 10, borderWidth: 1.5, borderColor: C.border, overflow: "hidden", backgroundColor: C.bg },
  segOpt:      { flex: 1, paddingVertical: 11, alignItems: "center" },
  segOptOn:    { backgroundColor: C.teal },
  segTxt:      { fontSize: 13, fontWeight: "700", color: C.secondary },
  segTxtOn:    { color: C.white },

  input:       { backgroundColor: C.white, borderWidth: 1.5, borderColor: C.border, borderRadius: 12, paddingHorizontal: 14, paddingVertical: 13, fontSize: 16, color: C.textPrimary, fontVariant: ["tabular-nums"] },
  inputFlex:   { flex: 1 },
  inputMuted:  { backgroundColor: C.bg, color: C.muted, borderColor: C.border },
  inputRow:    { flexDirection: "row", gap: 8, alignItems: "center" },
});

// screen / layout styles
const S = StyleSheet.create({
  safe:          { flex: 1, backgroundColor: C.bg },
  safeFilling:   { backgroundColor: C.navy },
  center:        { flex: 1, alignItems: "center", justifyContent: "center" },
  centerScroll:  { flexGrow: 1, justifyContent: "center", padding: 20 },

  topBar:        { flexDirection: "row", alignItems: "center", justifyContent: "space-between", paddingHorizontal: 16, paddingVertical: 12, backgroundColor: C.white, borderBottomWidth: 1, borderBottomColor: C.border },
  topBarFilling: { backgroundColor: C.navyMid, borderBottomColor: C.navyCard },
  topRight:      { flexDirection: "row", alignItems: "center", gap: 8 },
  statePill:     { borderRadius: 999, paddingHorizontal: 10, paddingVertical: 4, backgroundColor: C.amberLight },
  statePillOk:   { backgroundColor: C.greenLight },
  statePillFault:{ backgroundColor: C.redLight },
  statePillFilling: { backgroundColor: C.tealDark },

  setupShell:    { padding: 14, paddingBottom: 52 },

  formRow:       { flexDirection: "row", gap: 10, marginTop: 16 },
  amountResult:  { flexDirection: "row", alignItems: "center", gap: 8, backgroundColor: C.tealLight, borderRadius: 12, borderWidth: 1.5, borderColor: C.teal, paddingHorizontal: 16, paddingVertical: 14 },
  amountCurrency:{ fontSize: 14, fontWeight: "800", color: C.teal },
  amountFigure:  { fontSize: 26, fontWeight: "900", color: C.teal, fontVariant: ["tabular-nums"], lineHeight: 30 },

  weightStrip:   { flexDirection: "row", backgroundColor: C.white, borderRadius: 16, borderWidth: 1, borderColor: C.border, overflow: "hidden", marginBottom: 12 },
  wCell:         { flex: 1, alignItems: "center", paddingVertical: 18 },
  wCellBorder:   { borderLeftWidth: 1, borderLeftColor: C.border },
  wCellAccent:   { backgroundColor: C.teal, flex: 1.2 },

  safetyStrip:   { flexDirection: "row", flexWrap: "wrap", gap: 7, marginBottom: 4 },

  card:          { backgroundColor: C.white, borderRadius: 18, borderWidth: 1, borderColor: C.border, padding: 20, marginTop: 14 },

  // Fill card
  fillCard:        { backgroundColor: C.white, borderRadius: 20, borderWidth: 1, borderColor: C.border, marginTop: 12, overflow: "hidden" },
  fillCardHeader:  { flexDirection: "row", alignItems: "center", justifyContent: "space-between", paddingHorizontal: 20, paddingTop: 18, paddingBottom: 14 },
  fillCardTitle:   { fontSize: 17, fontWeight: "900", color: C.textPrimary, letterSpacing: -0.3 },
  safetyRow:       { flexDirection: "row", gap: 6, flexWrap: "wrap", justifyContent: "flex-end", flex: 1, marginLeft: 12 },
  readyBadge:      { backgroundColor: C.greenLight, borderRadius: 999, paddingHorizontal: 10, paddingVertical: 4, borderWidth: 1, borderColor: C.green },
  readyBadgeTxt:   { fontSize: 12, fontWeight: "800", color: C.green },

  fillDivider:     { height: 1, backgroundColor: C.border, marginHorizontal: 20, marginBottom: 16 },

  // Tare row
  tareRow:         { flexDirection: "row", alignItems: "flex-end", gap: 10, paddingHorizontal: 20, paddingBottom: 14 },
  tareInput:       { backgroundColor: C.bg, borderWidth: 1.5, borderColor: C.border, borderRadius: 10, paddingHorizontal: 12, paddingVertical: 10, fontSize: 15, color: C.textPrimary, fontVariant: ["tabular-nums"], flex: 1 },
  tareBtns:        { flexDirection: "row", gap: 6 },
  tareBtn:         { paddingHorizontal: 14, paddingVertical: 10, borderRadius: 10, borderWidth: 1.5, borderColor: C.border, backgroundColor: C.bg },
  tareBtnTxt:      { fontSize: 13, fontWeight: "700", color: C.secondary },

  // Primary input
  primaryInputWrap:{ paddingHorizontal: 20, marginTop: 16 },
  primaryInputRow: { flexDirection: "row", alignItems: "center", backgroundColor: C.bg, borderWidth: 2, borderColor: C.teal, borderRadius: 14, marginTop: 8, overflow: "hidden" },
  primaryUnit:     { fontSize: 15, fontWeight: "900", color: C.teal, paddingHorizontal: 14, paddingVertical: 14 },
  primaryInput:    { flex: 1, fontSize: 28, fontWeight: "900", color: C.textPrimary, paddingVertical: 14, paddingRight: 14, fontVariant: ["tabular-nums"] },

  // Rate + result row
  rateResultRow:   { flexDirection: "row", gap: 10, paddingHorizontal: 20, marginTop: 14 },
  rateInputWrap:   { flexDirection: "row", alignItems: "center", backgroundColor: C.bg, borderWidth: 1.5, borderColor: C.border, borderRadius: 12, marginTop: 8, overflow: "hidden" },
  rateUnit:        { fontSize: 12, fontWeight: "800", color: C.muted, paddingHorizontal: 10 },
  rateInput:       { flex: 1, fontSize: 16, fontWeight: "700", color: C.textPrimary, paddingVertical: 12, paddingRight: 12, fontVariant: ["tabular-nums"] },
  resultBox:       { flexDirection: "row", alignItems: "center", backgroundColor: C.tealLight, borderRadius: 12, borderWidth: 1.5, borderColor: C.teal, marginTop: 8, paddingHorizontal: 12, paddingVertical: 12, gap: 6 },
  resultUnit:      { fontSize: 12, fontWeight: "900", color: C.teal },
  resultVal:       { fontSize: 16, fontWeight: "900", color: C.teal, fontVariant: ["tabular-nums"] },

  interlockWarn:   { fontSize: 12, fontWeight: "700", color: C.red, textAlign: "center", paddingTop: 14, paddingHorizontal: 20 },

  // Start button
  startBtn:        { margin: 20, marginTop: 16, backgroundColor: C.teal, borderRadius: 16, paddingVertical: 18, alignItems: "center" },
  startBtnDim:     { opacity: 0.55 },
  startBtnTxt:     { fontSize: 17, fontWeight: "900", color: C.white, letterSpacing: 0.3 },

  secNav:        { flexDirection: "row", gap: 8, flexWrap: "wrap", marginTop: 18 },
  secBtn:        { paddingVertical: 8, paddingHorizontal: 14, borderRadius: 8, backgroundColor: C.white, borderWidth: 1, borderColor: C.border },
  secBtnOn:      { backgroundColor: C.secondary, borderColor: C.secondary },

  periodRow:     { flexDirection: "row", flexWrap: "wrap", gap: 6, marginTop: 14, marginBottom: 6 },
  chip:          { paddingVertical: 6, paddingHorizontal: 12, borderRadius: 999, backgroundColor: C.bg, borderWidth: 1, borderColor: C.border },
  chipOn:        { backgroundColor: C.teal, borderColor: C.teal },

  kpiRow:        { flexDirection: "row", flexWrap: "wrap", gap: 8, marginTop: 10 },
  kpiCell:       { flex: 1, minWidth: "45%", backgroundColor: C.bg, borderRadius: 10, borderWidth: 1, borderColor: C.border, padding: 12 },
  kpiCellWarn:   { backgroundColor: C.redLight, borderColor: C.red },

  txnRow:        { paddingVertical: 10, borderTopWidth: 1, borderTopColor: C.border },
  txnDetail:     { flexDirection: "row", alignItems: "center", gap: 10, flexWrap: "wrap" },

  diagGrid:      { flexDirection: "row", flexWrap: "wrap", gap: 8 },
  diagCell:      { width: 56, height: 56, borderRadius: 10, alignItems: "center", justifyContent: "center", backgroundColor: C.bg, borderWidth: 1, borderColor: C.border },
  diagCellOn:    { backgroundColor: C.greenLight, borderColor: C.green },

  rawTxt:        { fontFamily: Platform.OS === "ios" ? "Menlo" : "monospace", fontSize: 10, color: C.secondary, backgroundColor: C.bg, padding: 10, borderRadius: 8, marginTop: 6, lineHeight: 16 },

  panelDivider:  { height: 1, backgroundColor: C.border, marginBottom: 20 },
  resultMsg:     { borderRadius: 10, padding: 12, marginTop: 12 },
  resultOk:      { backgroundColor: C.greenLight, borderWidth: 1, borderColor: C.green },
  resultErr:     { backgroundColor: C.redLight,   borderWidth: 1, borderColor: C.red   },

  authCard:      { backgroundColor: C.white, borderRadius: 20, borderWidth: 1, borderColor: C.border, padding: 28, width: "100%", maxWidth: 440, alignSelf: "center" },
});

// fill screen styles
const F = StyleSheet.create({
  shell:         { flex: 1, paddingHorizontal: 22, paddingTop: 18, paddingBottom: 28, backgroundColor: C.navy },
  topRow:        { flexDirection: "row", alignItems: "center", justifyContent: "space-between", marginBottom: 20 },
  stateBadge:    { borderRadius: 999, paddingHorizontal: 16, paddingVertical: 8 },

  cylinderRow:   { flexDirection: "row", alignItems: "center", justifyContent: "center", gap: 32, marginBottom: 18 },
  pctBlock:      { alignItems: "flex-start" },
  pctNum:        { fontSize: 64, fontWeight: "900", color: C.white, lineHeight: 68, fontVariant: ["tabular-nums"] },
  pctSymbol:     { fontSize: 22, fontWeight: "700", color: C.navyMuted, marginTop: 4 },

  weightRow:     { flexDirection: "row", alignItems: "flex-end", justifyContent: "center", gap: 8, marginBottom: 14 },
  netNum:        { fontSize: 42, fontWeight: "900", color: C.white,     lineHeight: 46, fontVariant: ["tabular-nums"] },
  weightSep:     { fontSize: 28, fontWeight: "300", color: C.navyMuted, paddingBottom: 4 },
  targetNum:     { fontSize: 26, fontWeight: "700", color: C.navyMuted, lineHeight: 38, fontVariant: ["tabular-nums"] },
  weightUnit:    { fontSize: 16, fontWeight: "600", color: C.navyMuted, paddingBottom: 6 },

  progressTrack: { height: 10, backgroundColor: C.navyCard, borderRadius: 999, overflow: "hidden", marginBottom: 20 },
  progressFill:  { height: "100%", borderRadius: 999 },

  statsRow:      { flexDirection: "row", backgroundColor: C.navyCard, borderRadius: 14, paddingVertical: 14 },
  statCell:      { flex: 1, alignItems: "center", paddingHorizontal: 8 },
  statDivider:   { borderRightWidth: 1, borderRightColor: "#1e2a3a" },

  stopBtn:       { backgroundColor: C.red, borderRadius: 16, paddingVertical: 22, alignItems: "center", marginTop: 10 },
  stopBtnPressed:{ backgroundColor: "#991b1b" },
  stopTxt:       { color: C.white, fontSize: 17, fontWeight: "900", letterSpacing: 1.5 },
});

// complete screen styles
const CP = StyleSheet.create({
  shell:       { width: "100%", maxWidth: 440, alignSelf: "center" },
  hero:        { backgroundColor: C.green, borderRadius: 20, padding: 36, alignItems: "center", marginBottom: 16 },
  checkCircle: { width: 80, height: 80, borderRadius: 40, backgroundColor: "rgba(255,255,255,0.25)", alignItems: "center", justifyContent: "center" },
  checkMark:   { fontSize: 44, color: C.white },
  receipt:     { backgroundColor: C.white, borderRadius: 16, borderWidth: 1, borderColor: C.border, paddingHorizontal: 20, paddingVertical: 8, marginBottom: 16 },
  receiptRow:  { flexDirection: "row", justifyContent: "space-between", alignItems: "center", paddingVertical: 14 },
  line:        { height: 1, backgroundColor: C.border },
});

// fault screen styles
const FT = StyleSheet.create({
  shell: { width: "100%", maxWidth: 440, alignSelf: "center" },
  hero:  { backgroundColor: C.red, borderRadius: 20, padding: 36, alignItems: "center", marginBottom: 16 },
  icon:  { fontSize: 52, color: C.white },
});

// cylinder widget styles
const cyl = StyleSheet.create({
  valveBase: { width: 28, height: 10, backgroundColor: "#2d3f55", borderRadius: 4, marginBottom: 2 },
  valveNeck: { width: 18, height: 12, backgroundColor: "#384f68", borderRadius: 3 },
  body:      { borderRadius: 22, borderWidth: 2.5, borderColor: "rgba(255,255,255,0.13)", backgroundColor: "rgba(255,255,255,0.04)", overflow: "hidden" },
  liquid:    { position: "absolute", bottom: 0, left: 0, right: 0, borderTopLeftRadius: 10, borderTopRightRadius: 10 },
  surface:   { position: "absolute", top: 0, left: 0, right: 0, height: 3, backgroundColor: "rgba(255,255,255,0.45)", borderRadius: 2 },
  gasLabel:  { position: "absolute", top: 14, alignSelf: "center" },
  gasLabelTxt: { color: "rgba(255,255,255,0.2)", fontSize: 10, fontWeight: "800", letterSpacing: 2 },
  shine:     { position: "absolute", top: 0, bottom: 0, left: 8, width: 10, backgroundColor: "rgba(255,255,255,1)", borderRadius: 5 },
  foot:      { width: 80, height: 10, backgroundColor: "#2d3f55", borderRadius: 5, marginTop: 2 },
});

// board info bar styles
const BI = StyleSheet.create({
  bar:        { flexDirection: "row", alignItems: "center", paddingHorizontal: 14, paddingVertical: 7,
                backgroundColor: C.surface, borderRadius: 10, marginBottom: 8, gap: 0 },
  col:        { flex: 1 },
  colCenter:  { flex: 1, alignItems: "center" },
  colRight:   { flex: 1, alignItems: "flex-end" },
  label:      { fontSize: 9, fontWeight: "700", color: C.muted, letterSpacing: 0.8, textTransform: "uppercase" },
  val:        { fontSize: 12, fontWeight: "700", color: C.textPrimary, marginTop: 1, fontVariant: ["tabular-nums"] },
  rssi:       { fontSize: 14, lineHeight: 16 },
});

// last transaction card styles
const LT = StyleSheet.create({
  card:       { backgroundColor: C.surface, borderRadius: 12, padding: 12, marginBottom: 10,
                borderWidth: 1, borderColor: C.border },
  headerRow:  { flexDirection: "row", justifyContent: "space-between", alignItems: "center", marginBottom: 6 },
  bodyRow:    { flexDirection: "row", alignItems: "center", gap: 10 },
  amtBlock:   { flex: 1 },
  amtVal:     { fontSize: 20, fontWeight: "900", color: C.teal, fontVariant: ["tabular-nums"] },
  amtSub:     { fontSize: 11, color: C.slate, marginTop: 1 },
  metaBlock:  { alignItems: "flex-end" },
  metaId:     { fontSize: 11, fontWeight: "700", color: C.secondary },
  metaTime:   { fontSize: 10, color: C.muted, marginTop: 2 },
  metaOp:     { fontSize: 10, color: C.muted, marginTop: 1 },
});

// system resources panel styles
const SR = StyleSheet.create({
  gaugeWrap:  { marginBottom: 12 },
  gaugeTrack: { height: 8, backgroundColor: C.border, borderRadius: 4, marginTop: 4, overflow: "hidden" },
  gaugeFill:  { height: 8, borderRadius: 4 },
});

// status detail panel styles
const SD = StyleSheet.create({
  section:      { marginBottom: 10 },
  sectionTitle: { fontSize: 10, fontWeight: "800", color: C.teal, letterSpacing: 0.8,
                  textTransform: "uppercase", marginBottom: 4, marginTop: 6 },
  row:          { flexDirection: "row", justifyContent: "space-between", alignItems: "center",
                  paddingVertical: 3, borderBottomWidth: 1, borderBottomColor: C.border },
  rowLabel:     { fontSize: 11, color: C.slate, flex: 1 },
  rowVal:       { fontSize: 11, fontWeight: "700", color: C.textPrimary, textAlign: "right",
                  flex: 1, fontVariant: ["tabular-nums"] },
});
