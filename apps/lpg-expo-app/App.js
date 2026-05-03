import { StatusBar } from "expo-status-bar";
import { useEffect, useMemo, useState } from "react";
import {
  Platform,
  Pressable,
  SafeAreaView,
  ScrollView,
  StyleSheet,
  Text,
  TextInput,
  View,
} from "react-native";

function showAlert(title, message) {
  if (Platform.OS === "web") {
    window.alert(`${title}\n${message}`);
  } else {
    const { Alert } = require("react-native");
    Alert.alert(title, message);
  }
}
import {
  applyTare,
  connectStatusStream,
  fetchSettings,
  fetchTransactions,
  login,
  logout,
  normalizeDeviceUrl,
  resetFill,
  saveRate,
  startFill,
  stopFill,
  zeroNet,
} from "./src/api";
import { DEFAULT_DEVICE_URL, DEV_AUTO_LOGIN_ROLE, DEV_CREDENTIALS } from "./src/constants/device";

// On web the app talks to the dev proxy (localhost:4000) which forwards /api/* to the device.
// On native the app connects directly to the device IP.
const INITIAL_URL = Platform.OS === "web" ? "http://localhost:4000" : DEFAULT_DEVICE_URL;
import { NO_AUTH_TOKEN } from "./src/services/controllerApi";

const relayPurposes = [
  "Fast fill valve",
  "Slow fill valve",
  "Main supply valve",
  "Pump / compressor",
  "Alarm horn / beacon",
  "Status indicator",
];

function kg(value) {
  return Number(value || 0).toFixed(3);
}

function money(value) {
  return Number(value || 0).toFixed(2);
}

function inPeriod(item, period) {
  if (period === "all") return true;
  const seconds = Number(item.endTime || item.startTime || 0);
  if (!seconds) return true;
  const date = new Date(seconds * 1000);
  const now = new Date();
  if (period === "today") return date.toDateString() === now.toDateString();
  if (period === "week") {
    const start = new Date(now);
    start.setHours(0, 0, 0, 0);
    start.setDate(now.getDate() - now.getDay());
    return date >= start;
  }
  if (period === "month") {
    return date.getFullYear() === now.getFullYear() && date.getMonth() === now.getMonth();
  }
  return date.getFullYear() === now.getFullYear();
}

function Kpi({ label, value, tone }) {
  return (
    <View style={[styles.kpi, tone === "danger" && styles.kpiDanger, tone === "ok" && styles.kpiOk]}>
      <Text style={styles.kpiLabel}>{label}</Text>
      <Text style={styles.kpiValue}>{value}</Text>
    </View>
  );
}

function Button({ label, onPress, tone = "primary" }) {
  return (
    <Pressable style={[styles.button, styles[`button_${tone}`]]} onPress={onPress}>
      <Text style={[styles.buttonText, tone === "light" && styles.buttonTextLight]}>{label}</Text>
    </Pressable>
  );
}

export default function App() {
  const [activeUrl, setActiveUrl] = useState(INITIAL_URL);
  const [connectionKey, setConnectionKey] = useState(0);
  const [role, setRole] = useState("operator");
  const [mode, setMode] = useState("weight");
  const [streamMode, setStreamMode] = useState("connecting");
  const [status, setStatus] = useState({});
  const [transactions, setTransactions] = useState([]);
  const [tareWeight, setTareWeight] = useState("0.00");
  const [targetWeight, setTargetWeight] = useState("11.80");
  const [targetAmount, setTargetAmount] = useState("2950.00");
  const [rate, setRate] = useState("250.00");
  const [adminRate, setAdminRate] = useState("250.00");
  const [period, setPeriod] = useState("all");
  const [authToken, setAuthToken] = useState("");
  const [authLoading, setAuthLoading] = useState(true);
  const [loginRole, setLoginRole] = useState(DEV_AUTO_LOGIN_ROLE);
  const [loginPassword, setLoginPassword] = useState(DEV_CREDENTIALS[DEV_AUTO_LOGIN_ROLE]);
  const [loginError, setLoginError] = useState("");

  const effectiveRate = Number(rate || status.ratePerKg || 0);

  useEffect(() => {
    const stop = connectStatusStream(
      activeUrl,
      (nextStatus) => {
        setStatus(nextStatus);
        setTareWeight((current) => (current === "" ? current : money(nextStatus.tareWeightKg)));
        setRate((current) => (current === "" ? current : money(nextStatus.ratePerKg)));
        setAdminRate((current) => (current === "" ? current : money(nextStatus.ratePerKg)));
      },
      setStreamMode
    );

    const refreshSlowData = async () => {
      try {
        setTransactions(await fetchTransactions(activeUrl));
        const settings = await fetchSettings(activeUrl);
        if (settings.ratePerKg) {
          setRate(money(settings.ratePerKg));
          setAdminRate(money(settings.ratePerKg));
        }
      } catch {}
    };

    refreshSlowData();
    const timer = setInterval(refreshSlowData, 5000);
    return () => {
      stop();
      clearInterval(timer);
    };
  }, [activeUrl, connectionKey]);

  useEffect(() => {
    setAuthToken("");
    setAuthLoading(true);
    login(activeUrl, DEV_AUTO_LOGIN_ROLE, DEV_CREDENTIALS[DEV_AUTO_LOGIN_ROLE])
      .then((result) => { setAuthToken(result.token); setAuthLoading(false); })
      .catch(() => setAuthLoading(false));
  }, [activeUrl, connectionKey]);

  const summary = useMemo(() => {
    const filtered = transactions.filter((item) => inPeriod(item, period));
    const completed = filtered.filter((item) => Number(item.status) === 1);
    const exceptions = filtered.filter((item) => Number(item.status) === 2 || Number(item.status) === 3);
    const totalSales = completed.reduce((sum, item) => sum + Number(item.finalAmount || 0), 0);
    const totalKg = completed.reduce((sum, item) => sum + Number(item.netKg || 0), 0);
    return {
      filtered,
      completed: completed.length,
      exceptions: exceptions.length,
      totalSales,
      totalKg,
      averageSale: completed.length ? totalSales / completed.length : 0,
    };
  }, [transactions, period]);

  function syncTargets(nextMode, changed, value) {
    const rateValue = Number(rate || 0);
    if (rateValue <= 0) return;
    const activeMode = nextMode || mode;
    if (activeMode === "amount" || changed === "amount") {
      const amount = Number(changed === "amount" ? value : targetAmount);
      setTargetWeight(kg(amount / rateValue));
      if (changed === "amount") setTargetAmount(value);
    } else {
      const weight = Number(changed === "weight" ? value : targetWeight);
      setTargetAmount(money(weight * rateValue));
      if (changed === "weight") setTargetWeight(value);
    }
  }

  async function handleLogin() {
    setLoginError("");
    try {
      const result = await login(activeUrl, loginRole, loginPassword);
      setAuthToken(result.token);
    } catch (err) {
      setLoginError(err.message || "Login failed");
    }
  }

  async function handleLogout() {
    if (authToken && authToken !== NO_AUTH_TOKEN) {
      try { await logout(activeUrl, authToken); } catch {}
    }
    setAuthToken("");
  }

  async function runCommand(action, success) {
    try {
      await action();
      showAlert("Done", success);
    } catch (error) {
      const msg = error.message || "";
      if (authToken !== NO_AUTH_TOKEN &&
          (msg.includes("token") || msg.includes("session") || msg.includes("credentials"))) {
        setAuthToken("");
      }
      showAlert("Rejected", msg);
    }
  }

  const ready = status.nozzleEngaged && status.cylinderPresent && status.emergencyStopOk;

  return (
    <SafeAreaView style={styles.safe}>
      <StatusBar style="dark" />
      <ScrollView contentContainerStyle={styles.shell}>
        <View style={styles.header}>
          <View>
            <Text style={styles.title}>LPG Filling</Text>
            <Text style={styles.subtitle}>{normalizeDeviceUrl(activeUrl)} · {streamMode}</Text>
          </View>
          <Text style={[styles.state, ready ? styles.stateOk : styles.stateWarn]}>{status.state || "BOOT"}</Text>
        </View>

        <View style={styles.deviceRow}>
          <View style={styles.deviceStatus}>
            <Text style={streamMode === "connected" ? styles.deviceDotOk : styles.deviceDotOff}>●</Text>
            <Text style={styles.deviceLabel} numberOfLines={1}>{normalizeDeviceUrl(activeUrl)}</Text>
          </View>
          <Pressable style={styles.reconnectBtn} onPress={() => { setConnectionKey(k => k + 1); setAuthToken(""); }}>
            <Text style={styles.buttonText}>Reconnect</Text>
          </Pressable>
        </View>

        {authLoading ? (
          <View style={styles.card}>
            <Text style={styles.label}>Connecting to device...</Text>
          </View>
        ) : !authToken ? (
          <View style={styles.card}>
            <Text style={styles.sectionTitle}>Sign In</Text>
            <Text style={styles.label}>Role</Text>
            <View style={styles.roleRow}>
              {["operator", "maintenance", "admin"].map((r) => (
                <Pressable
                  key={r}
                  style={[styles.roleOption, loginRole === r && styles.roleOptionActive]}
                  onPress={() => { setLoginRole(r); setLoginPassword(DEV_CREDENTIALS[r] ?? ""); }}
                >
                  <Text style={[styles.roleOptionText, loginRole === r && styles.roleOptionTextActive]}>{r}</Text>
                </Pressable>
              ))}
            </View>
            <Text style={styles.label}>Password</Text>
            <TextInput
              value={loginPassword}
              onChangeText={setLoginPassword}
              style={styles.input}
              secureTextEntry
              placeholder="Password"
              autoCapitalize="none"
            />
            {loginError ? <Text style={styles.errorText}>{loginError}</Text> : null}
            <Button label="Sign In" onPress={handleLogin} />
          </View>
        ) : (
          <>

        <View style={styles.tabs}>
          {["operator", "admin", "manufacturer"].map((item) => (
            <Pressable key={item} style={[styles.tab, role === item && styles.tabActive]} onPress={() => setRole(item)}>
              <Text style={[styles.tabText, role === item && styles.tabTextActive]}>{item}</Text>
            </Pressable>
          ))}
        </View>

        <View style={styles.kpiGrid}>
          <Kpi label="Live" value={`${kg(status.liveWeightKg ?? status.weightKg)} kg`} />
          <Kpi label="Tare" value={`${kg(status.tareWeightKg)} kg`} />
          <Kpi label="Net" value={`${kg(status.netWeightKg)} kg`} tone="ok" />
          <Kpi label="Amount" value={money(status.currentAmount)} />
        </View>

        {role === "operator" && (
          <View style={styles.card}>
            <Text style={styles.sectionTitle}>Operator</Text>
            <View style={styles.safetyRow}>
              <Kpi label="Nozzle" value={status.nozzleEngaged ? "OK" : "OPEN"} tone={status.nozzleEngaged ? "ok" : "danger"} />
              <Kpi label="Cylinder" value={status.cylinderPresent ? "OK" : "MISS"} tone={status.cylinderPresent ? "ok" : "danger"} />
              <Kpi label="E-Stop" value={status.emergencyStopOk ? "OK" : "TRIP"} tone={status.emergencyStopOk ? "ok" : "danger"} />
            </View>

            <Text style={styles.label}>Tare Weight / Empty Cylinder</Text>
            <TextInput value={tareWeight} onChangeText={setTareWeight} keyboardType="decimal-pad" style={styles.input} />
            <View style={styles.actionRow}>
              <Button label="Apply Tare" onPress={() => runCommand(() => applyTare(activeUrl, tareWeight, authToken), "Tare saved")} />
              <Button label="Zero Net" onPress={() => runCommand(() => zeroNet(activeUrl, authToken), "Net weight zeroed")} tone="dark" />
            </View>

            <Text style={styles.label}>Input Mode</Text>
            <View style={styles.modeRow}>
              <Button label="By Weight" tone={mode === "weight" ? "primary" : "light"} onPress={() => { setMode("weight"); syncTargets("weight"); }} />
              <Button label="By Amount" tone={mode === "amount" ? "primary" : "light"} onPress={() => { setMode("amount"); syncTargets("amount"); }} />
            </View>

            <Text style={styles.label}>Target Weight</Text>
            <TextInput value={targetWeight} onChangeText={(value) => syncTargets(mode, "weight", value)} editable={mode === "weight"} keyboardType="decimal-pad" style={styles.input} />
            <Text style={styles.label}>Rate per Kg</Text>
            <TextInput value={rate} onChangeText={(value) => { setRate(value); syncTargets(mode); }} keyboardType="decimal-pad" style={styles.input} />
            <Text style={styles.label}>Target Amount</Text>
            <TextInput value={targetAmount} onChangeText={(value) => syncTargets(mode, "amount", value)} editable={mode === "amount"} keyboardType="decimal-pad" style={styles.input} />

            <View style={styles.actionRow}>
              <Button label="Start Fill" onPress={() => runCommand(() => startFill(activeUrl, targetWeight, rate, targetAmount, authToken), "Fill started")} />
              <Button label="Stop" onPress={() => runCommand(() => stopFill(activeUrl, authToken), "Fill stopped")} tone="danger" />
            </View>
            <Button label="Reset To Idle" onPress={() => runCommand(() => resetFill(activeUrl, authToken), "Controller reset")} tone="dark" />
          </View>
        )}

        {role === "admin" && (
          <View style={styles.card}>
            <Text style={styles.sectionTitle}>Admin</Text>
            <Text style={styles.label}>Rate per Kg</Text>
            <TextInput value={adminRate} onChangeText={setAdminRate} keyboardType="decimal-pad" style={styles.input} />
            <Button label="Save Rate" onPress={() => runCommand(() => saveRate(activeUrl, adminRate, authToken), "Rate saved")} />
            <Button label="Sign Out" onPress={handleLogout} tone="light" />

            <View style={styles.periodRow}>
              {["today", "week", "month", "year", "all"].map((item) => (
                <Pressable key={item} style={[styles.period, period === item && styles.periodActive]} onPress={() => setPeriod(item)}>
                  <Text style={[styles.periodText, period === item && styles.periodTextActive]}>{item}</Text>
                </Pressable>
              ))}
            </View>
            <View style={styles.kpiGrid}>
              <Kpi label="Sales" value={money(summary.totalSales)} />
              <Kpi label="KG Sold" value={kg(summary.totalKg)} />
              <Kpi label="Complete" value={String(summary.completed)} />
              <Kpi label="Abort/Fault" value={String(summary.exceptions)} tone={summary.exceptions ? "danger" : "ok"} />
            </View>
            <Text style={styles.label}>Recent Transactions</Text>
            {summary.filtered.slice(-8).reverse().map((item) => (
              <View key={item.transactionId || item.id} style={styles.historyRow}>
                <Text style={styles.historyTitle}>{item.transactionId || item.id}</Text>
                <Text style={styles.historyMeta}>{kg(item.netKg)} kg · {money(item.finalAmount)} · status {item.status}</Text>
              </View>
            ))}
          </View>
        )}

        {role === "manufacturer" && (
          <View style={styles.card}>
            <Text style={styles.sectionTitle}>Manufacturer</Text>
            {relayPurposes.map((purpose, index) => (
              <View key={purpose} style={styles.relayRow}>
                <Text style={styles.historyTitle}>Relay {index + 1}</Text>
                <Text style={styles.historyMeta}>{purpose}</Text>
                <Text style={[styles.relayState, status.relays?.[index] && styles.relayOn]}>
                  {status.relays?.[index] ? "ON" : "OFF"}
                </Text>
              </View>
            ))}
            <Text style={styles.label}>Raw Status</Text>
            <Text style={styles.raw}>{JSON.stringify(status, null, 2)}</Text>
          </View>
        )}

          </>
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safe: { flex: 1, backgroundColor: "#edf1f5" },
  shell: { padding: 14, paddingBottom: 40 },
  header: { flexDirection: "row", alignItems: "center", justifyContent: "space-between", gap: 10, marginBottom: 12 },
  title: { fontSize: 28, fontWeight: "900", color: "#17212b" },
  subtitle: { color: "#647282", marginTop: 2 },
  state: { overflow: "hidden", borderRadius: 999, paddingHorizontal: 12, paddingVertical: 8, fontWeight: "900" },
  stateOk: { backgroundColor: "#e8f6ee", color: "#14823f" },
  stateWarn: { backgroundColor: "#fff3dc", color: "#b54708" },
  deviceRow: { flexDirection: "row", alignItems: "center", gap: 8, marginBottom: 10 },
  deviceStatus: { flex: 1, flexDirection: "row", alignItems: "center", gap: 6 },
  deviceDotOk: { color: "#14823f", fontSize: 14 },
  deviceDotOff: { color: "#b54708", fontSize: 14 },
  deviceLabel: { flex: 1, color: "#647282", fontSize: 13 },
  reconnectBtn: { backgroundColor: "#334155", borderRadius: 8, paddingVertical: 10, paddingHorizontal: 16 },
  tabs: { flexDirection: "row", backgroundColor: "#f7f9fc", borderRadius: 8, padding: 4, borderWidth: 1, borderColor: "#d7dee8", marginBottom: 12 },
  tab: { flex: 1, padding: 10, borderRadius: 6, alignItems: "center" },
  tabActive: { backgroundColor: "#155e75" },
  tabText: { color: "#647282", fontWeight: "800", textTransform: "capitalize" },
  tabTextActive: { color: "#fff" },
  card: { backgroundColor: "#fff", borderRadius: 8, borderWidth: 1, borderColor: "#d7dee8", padding: 14, marginTop: 12 },
  sectionTitle: { fontSize: 18, fontWeight: "900", color: "#17212b", marginBottom: 10 },
  kpiGrid: { flexDirection: "row", flexWrap: "wrap", gap: 10 },
  kpi: { flexGrow: 1, minWidth: "47%", backgroundColor: "#fff", borderRadius: 8, borderWidth: 1, borderColor: "#d7dee8", padding: 12 },
  kpiOk: { backgroundColor: "#e8f6ee", borderColor: "#39a265" },
  kpiDanger: { backgroundColor: "#fdebea", borderColor: "#b42318" },
  kpiLabel: { color: "#647282", fontSize: 12, fontWeight: "900", textTransform: "uppercase" },
  kpiValue: { marginTop: 6, fontSize: 22, fontWeight: "900", color: "#17212b" },
  safetyRow: { flexDirection: "row", flexWrap: "wrap", gap: 10, marginBottom: 10 },
  label: { marginTop: 12, marginBottom: 6, color: "#647282", fontWeight: "800" },
  input: { backgroundColor: "#fff", borderWidth: 1, borderColor: "#d7dee8", borderRadius: 8, padding: 12, fontSize: 16, color: "#17212b" },
  actionRow: { flexDirection: "row", gap: 10, marginTop: 10 },
  modeRow: { flexDirection: "row", gap: 10 },
  button: { flex: 1, borderRadius: 8, paddingVertical: 13, paddingHorizontal: 12, alignItems: "center", marginTop: 10 },
  button_primary: { backgroundColor: "#0f766e" },
  button_danger: { backgroundColor: "#b42318" },
  button_dark: { backgroundColor: "#334155" },
  button_light: { backgroundColor: "#e8eef5" },
  buttonText: { color: "#fff", fontWeight: "900" },
  buttonTextLight: { color: "#334155" },
  periodRow: { flexDirection: "row", flexWrap: "wrap", gap: 8, marginVertical: 14 },
  period: { paddingVertical: 8, paddingHorizontal: 10, borderRadius: 8, backgroundColor: "#e8eef5" },
  periodActive: { backgroundColor: "#155e75" },
  periodText: { color: "#334155", fontWeight: "800", textTransform: "capitalize" },
  periodTextActive: { color: "#fff" },
  historyRow: { borderWidth: 1, borderColor: "#d7dee8", borderRadius: 8, padding: 10, marginTop: 8, backgroundColor: "#f7f9fc" },
  historyTitle: { fontWeight: "900", color: "#17212b" },
  historyMeta: { color: "#647282", marginTop: 3 },
  relayRow: { borderWidth: 1, borderColor: "#d7dee8", borderRadius: 8, padding: 10, marginTop: 8, backgroundColor: "#f7f9fc" },
  relayState: { marginTop: 6, color: "#b42318", fontWeight: "900" },
  relayOn: { color: "#14823f" },
  raw: { fontFamily: "monospace", color: "#263340", backgroundColor: "#f7f9fc", padding: 10, borderRadius: 8 },
  errorText: { color: "#b42318", marginTop: 8, fontWeight: "700" },
  roleRow: { flexDirection: "row", gap: 8, marginBottom: 4 },
  roleOption: { flex: 1, paddingVertical: 10, borderRadius: 8, alignItems: "center", backgroundColor: "#e8eef5", borderWidth: 1, borderColor: "#d7dee8" },
  roleOptionActive: { backgroundColor: "#155e75", borderColor: "#155e75" },
  roleOptionText: { fontWeight: "800", textTransform: "capitalize", color: "#334155" },
  roleOptionTextActive: { color: "#fff" },
});
