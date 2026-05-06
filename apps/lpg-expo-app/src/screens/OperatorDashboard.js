import { useEffect, useRef, useState } from "react";
import {
  Animated, KeyboardAvoidingView, Platform, Pressable,
  ScrollView, StyleSheet, Text, TextInput, View,
} from "react-native";
import { C, R, S, T } from "../theme";
import { useAppState } from "../state/AppStateProvider";
import { useControllerActions } from "../hooks/useControllerActions";
import { saveSlowFillThreshold, saveRate } from "../services/controllerApi";
import SafetyBanner from "../components/SafetyBanner";
import StatusHeader from "../components/StatusHeader";
import ReadinessCard from "../components/ReadinessCard";
import { Field, Input } from "../components/ui/Field";
import SegmentedControl from "../components/ui/SegmentedControl";
import Button from "../components/ui/Button";
import { kg, money, shortDateTime } from "../utils/format";

export default function OperatorDashboard() {
  const {
    status, streamMode, transactions, navigate,
    activeUrl, authToken, authRole, authUsername, authCanSetRate,
    logout,
  } = useAppState();
  const { startFill, applyTare, zeroNet, busy } = useControllerActions();

  const [inputMode,    setInputMode]    = useState("weight");
  const [targetWeight, setTargetWeight] = useState("11.800");
  const [targetAmount, setTargetAmount] = useState("");
  const [rate,         setRate]         = useState("");
  const [tareWeight,   setTareWeight]   = useState("");
  const [editTare,     setEditTare]     = useState(false);
  const [slowFillPct,  setSlowFillPct]  = useState("95");
  const [result,       setResult]       = useState(null);

  const rateInit     = useRef(false);
  const slowFillInit = useRef(false);
  const shakeAnim    = useRef(new Animated.Value(0)).current;

  // Sync rate from status once
  useEffect(() => {
    if (!rateInit.current && status.ratePerKg) {
      const r = money(status.ratePerKg);
      setRate(r);
      rateInit.current = true;
    }
  }, [status.ratePerKg]);

  // Sync slowFillThreshold from status once
  useEffect(() => {
    if (!slowFillInit.current && status.slowFillThreshold) {
      setSlowFillPct(String(Math.round(status.slowFillThreshold * 100)));
      slowFillInit.current = true;
    }
  }, [status.slowFillThreshold]);

  // Sync tare from status when not being edited
  useEffect(() => {
    if (!editTare) setTareWeight(money(status.tareWeightKg));
  }, [status.tareWeightKg, editTare]);

  function syncTargets(mode, changed, value) {
    const r = Number(rate || 0);
    if (r <= 0) return;
    const m = mode || inputMode;
    if (m === "amount" || changed === "amount") {
      const a = Number(changed === "amount" ? value : targetAmount);
      setTargetWeight(kg(a / r));
      if (changed === "amount") setTargetAmount(value);
    } else {
      const w = Number(changed === "weight" ? value : targetWeight);
      setTargetAmount(money(w * r));
      if (changed === "weight") setTargetWeight(value);
    }
  }

  function shake() {
    Animated.sequence([
      Animated.timing(shakeAnim, { toValue: 8,  duration: 50, useNativeDriver: true }),
      Animated.timing(shakeAnim, { toValue: -8, duration: 50, useNativeDriver: true }),
      Animated.timing(shakeAnim, { toValue: 4,  duration: 50, useNativeDriver: true }),
      Animated.timing(shakeAnim, { toValue: 0,  duration: 50, useNativeDriver: true }),
    ]).start();
  }

  async function handleStart() {
    setResult(null);
    try {
      await startFill(targetWeight, rate, targetAmount);
    } catch (err) {
      shake();
      setResult({ ok: false, msg: err?.message || "Start failed" });
    }
  }

  async function handleSaveRate() {
    try {
      await saveRate(activeUrl, rate, authToken);
      setResult({ ok: true, msg: "Rate saved" });
    } catch (err) {
      setResult({ ok: false, msg: err?.message || "Failed" });
    }
  }

  async function handleSaveSlowFill() {
    const v = Number(slowFillPct) / 100;
    try {
      await saveSlowFillThreshold(activeUrl, v, authToken);
      setResult({ ok: true, msg: "Threshold saved" });
    } catch (err) {
      setResult({ ok: false, msg: err?.message || "Failed" });
    }
  }

  const ready = !!status.readyToFill;
  const last  = transactions[transactions.length - 1];
  const isAdmin = authRole === "admin" || authRole === "manufacturer";

  return (
    <KeyboardAvoidingView behavior={Platform.OS === "ios" ? "padding" : undefined} style={styles.shell}>
      <SafetyBanner status={status} streamMode={streamMode} />
      <StatusHeader status={status} streamMode={streamMode} />

      <ScrollView contentContainerStyle={styles.content} keyboardShouldPersistTaps="handled">

        {/* Weight strip */}
        <View style={styles.weightStrip}>
          <WeightCell label="LIVE" value={kg(status.liveWeightKg ?? status.weightKg)} />
          <WeightCell label="TARE" value={kg(status.tareWeightKg)} border />
          <WeightCell label="NET" value={kg(status.netWeightKg)} accent stable={status.weightStable} border />
        </View>

        {/* Readiness */}
        <ReadinessCard status={status} />

        {/* Fill setup card */}
        <Animated.View style={[styles.card, { transform: [{ translateX: shakeAnim }] }]}>
          <Text style={styles.cardTitle}>Fill Setup</Text>

          {/* Tare row */}
          <View style={styles.tareRow}>
            <View style={{ flex: 1 }}>
              <Text style={styles.fieldLabel}>Empty cylinder tare (kg)</Text>
              <TextInput
                value={tareWeight}
                onChangeText={setTareWeight}
                onFocus={() => setEditTare(true)}
                onBlur={() => setEditTare(false)}
                keyboardType="decimal-pad"
                style={styles.input}
                placeholder="0.000"
                placeholderTextColor={C.muted}
              />
            </View>
            <View style={styles.tareBtns}>
              <Pressable style={styles.tBtn} onPress={() => applyTare(tareWeight)}>
                <Text style={styles.tBtnTxt}>Apply</Text>
              </Pressable>
              <Pressable style={styles.tBtn} onPress={() => zeroNet()}>
                <Text style={styles.tBtnTxt}>Zero</Text>
              </Pressable>
            </View>
          </View>

          <View style={styles.divider} />

          {/* Input mode toggle */}
          <SegmentedControl
            options={[
              { key: "weight", label: "Fill by Weight (kg)" },
              { key: "amount", label: "Fill by Amount (PKR)" },
            ]}
            value={inputMode}
            onChange={(m) => { setInputMode(m); syncTargets(m); }}
          />

          {/* Primary input */}
          <View style={styles.primaryWrap}>
            <Text style={styles.fieldLabel}>{inputMode === "weight" ? "Target weight (kg)" : "Target amount (PKR)"}</Text>
            <View style={styles.primaryRow}>
              <Text style={styles.primaryUnit}>{inputMode === "weight" ? "kg" : "PKR"}</Text>
              <TextInput
                value={inputMode === "weight" ? targetWeight : targetAmount}
                onChangeText={(v) => syncTargets(inputMode, inputMode === "weight" ? "weight" : "amount", v)}
                keyboardType="decimal-pad"
                style={styles.primaryInput}
                placeholder="0.000"
                placeholderTextColor={C.muted}
              />
            </View>
          </View>

          {/* Rate + result row */}
          <View style={styles.rateRow}>
            <View style={{ flex: 1 }}>
              <Text style={styles.fieldLabel}>Rate / kg</Text>
              <View style={styles.rateInputWrap}>
                <Text style={styles.rateUnit}>PKR</Text>
                <TextInput
                  value={rate}
                  onChangeText={authCanSetRate ? (v) => { setRate(v); syncTargets(inputMode); } : undefined}
                  editable={authCanSetRate}
                  keyboardType="decimal-pad"
                  style={[styles.rateInput, !authCanSetRate && { color: C.muted }]}
                  placeholder="0.00"
                  placeholderTextColor={C.muted}
                />
              </View>
            </View>
            <View style={{ flex: 1 }}>
              <Text style={styles.fieldLabel}>{inputMode === "weight" ? "Total amount" : "Equiv. weight"}</Text>
              <View style={styles.resultBox}>
                <Text style={styles.resultUnit}>{inputMode === "weight" ? "PKR" : "kg"}</Text>
                <Text style={styles.resultVal}>{inputMode === "weight" ? targetAmount : targetWeight}</Text>
              </View>
            </View>
          </View>

          {/* Slow fill threshold (admin+) */}
          {authCanSetRate && (
            <>
              <View style={styles.divider} />
              <Field label={`Fast→Slow at ${slowFillPct || "—"}% of target`}>
                <View style={{ flexDirection: "row", gap: S.sm, alignItems: "center", marginTop: S.xs }}>
                  <TextInput
                    value={slowFillPct}
                    onChangeText={setSlowFillPct}
                    keyboardType="numeric"
                    style={[styles.input, { flex: 1 }]}
                    placeholder="95"
                    placeholderTextColor={C.muted}
                  />
                  <Text style={{ color: C.muted, fontSize: T.sm }}>%</Text>
                  <Button label="Set" variant="ghost" size="sm" onPress={handleSaveSlowFill} />
                  <Button label="Save rate" variant="ghost" size="sm" onPress={handleSaveRate} />
                </View>
              </Field>
            </>
          )}

          {result && (
            <Text style={[styles.resultMsg, { color: result.ok ? C.ready : C.danger }]}>{result.msg}</Text>
          )}

          <View style={styles.divider} />

          <Button
            label={busy ? "Starting…" : "Start Fill"}
            variant="primary"
            size="full"
            disabled={!ready || busy || !!status.simActive}
            disabledReason={!ready ? "Resolve interlocks before starting." : status.simActive ? "Simulation active — real fill blocked." : undefined}
            onPress={handleStart}
            loading={busy}
          />
        </Animated.View>

        {/* Last transaction */}
        {last && <LastTxnCard txn={last} />}

        {/* Navigation row */}
        <View style={styles.navRow}>
          {isAdmin && (
            <NavBtn label="History"     onPress={() => navigate("transactions")} />
          )}
          {authRole === "operator" && (
            <NavBtn label="My History"  onPress={() => navigate("transactions")} />
          )}
          {isAdmin && (
            <NavBtn label="Settings"    onPress={() => navigate("network")} />
          )}
          {isAdmin && (
            <NavBtn label="Users"       onPress={() => navigate("admin-users")} />
          )}
          {authRole === "manufacturer" && (
            <NavBtn label="Diagnostics" onPress={() => navigate("diagnostics")} />
          )}
          {authRole === "manufacturer" && (
            <NavBtn label="Calibration" onPress={() => navigate("calibration")} />
          )}
          <NavBtn label={`Sign Out (${authUsername})`} onPress={logout} />
        </View>

      </ScrollView>
    </KeyboardAvoidingView>
  );
}

function WeightCell({ label, value, border, accent, stable }) {
  return (
    <View style={[styles.wCell, border && styles.wCellBorder, accent && styles.wCellAccent]}>
      <View style={{ flexDirection: "row", alignItems: "center", gap: 4 }}>
        <Text style={[styles.wLabel, accent && { color: "rgba(255,255,255,0.6)" }]}>{label}</Text>
        {accent && (
          <View style={{ width: 6, height: 6, borderRadius: 3, backgroundColor: stable ? C.ready : C.warning }} />
        )}
      </View>
      <Text style={[styles.wVal, accent && { color: C.white }]}>{value}</Text>
      <Text style={[styles.wUnit, accent && { color: "rgba(255,255,255,0.5)" }]}>kg</Text>
    </View>
  );
}

function LastTxnCard({ txn }) {
  const isOk    = Number(txn.status) === 1;
  const dateStr = shortDateTime(txn.endTime || txn.startTime);
  return (
    <View style={styles.lastTxn}>
      <View style={{ flexDirection: "row", justifyContent: "space-between", marginBottom: S.xs }}>
        <Text style={styles.ltLabel}>LAST TRANSACTION</Text>
        <Text style={[styles.ltStatus, { color: isOk ? C.ready : C.warning }]}>
          {isOk ? "● Complete" : "✗ Fault"}
        </Text>
      </View>
      <View style={{ flexDirection: "row", justifyContent: "space-between", alignItems: "center" }}>
        <View>
          <Text style={styles.ltAmount}>PKR {money(txn.finalAmount)}</Text>
          <Text style={styles.ltSub}>{money(txn.netKg || txn.finalKg)} kg · {dateStr || "—"}</Text>
        </View>
        <Text style={styles.ltId}>{txn.transactionId || txn.id}</Text>
      </View>
    </View>
  );
}

function NavBtn({ label, onPress }) {
  return (
    <Pressable style={styles.navBtn} onPress={onPress}>
      <Text style={styles.navBtnTxt}>{label}</Text>
    </Pressable>
  );
}

const styles = StyleSheet.create({
  shell:   { flex: 1, backgroundColor: C.bg },
  content: { padding: S.md, paddingBottom: 52 },

  weightStrip:  { flexDirection: "row", backgroundColor: C.surface, borderRadius: R.lg, borderWidth: 1, borderColor: C.border, overflow: "hidden", marginBottom: S.md },
  wCell:        { flex: 1, alignItems: "center", paddingVertical: S.lg },
  wCellBorder:  { borderLeftWidth: 1, borderLeftColor: C.border },
  wCellAccent:  { backgroundColor: C.active + "22", flex: 1.2 },
  wLabel:       { fontSize: T.xs, fontWeight: "800", color: C.textSub, textTransform: "uppercase", letterSpacing: 0.8 },
  wVal:         { fontSize: 24, fontWeight: "900", color: C.text, fontVariant: ["tabular-nums"], marginTop: 2 },
  wUnit:        { fontSize: T.xs, color: C.textSub, marginTop: 1 },

  card:      { backgroundColor: C.surface, borderRadius: R.lg, borderWidth: 1, borderColor: C.border, padding: S.md, marginBottom: S.md },
  cardTitle: { fontSize: T.lg, fontWeight: "900", color: C.text, marginBottom: S.md },

  tareRow:  { flexDirection: "row", alignItems: "flex-end", gap: S.sm, marginBottom: S.md },
  tareBtns: { flexDirection: "row", gap: S.xs },
  tBtn:     { paddingHorizontal: S.md, paddingVertical: S.sm + 2, borderRadius: R.md, borderWidth: 1, borderColor: C.border, backgroundColor: C.surface2 },
  tBtnTxt:  { fontSize: T.sm, fontWeight: "700", color: C.textSub },

  fieldLabel: { fontSize: T.xs, fontWeight: "800", color: C.textSub, textTransform: "uppercase", letterSpacing: 0.7, marginBottom: S.xs + 2 },
  input:      { backgroundColor: C.surface2, borderWidth: 1, borderColor: C.border, borderRadius: R.md, paddingHorizontal: S.md, paddingVertical: S.sm + 2, fontSize: T.md, color: C.text, fontVariant: ["tabular-nums"] },

  divider: { height: 1, backgroundColor: C.border, marginVertical: S.md },

  primaryWrap:  { marginTop: S.md },
  primaryRow:   { flexDirection: "row", alignItems: "center", backgroundColor: C.surface2, borderWidth: 2, borderColor: C.primary, borderRadius: R.md, marginTop: S.xs, overflow: "hidden" },
  primaryUnit:  { fontSize: T.md, fontWeight: "900", color: C.primary, paddingHorizontal: S.md },
  primaryInput: { flex: 1, fontSize: 28, fontWeight: "900", color: C.text, paddingVertical: S.md, paddingRight: S.md, fontVariant: ["tabular-nums"] },

  rateRow:      { flexDirection: "row", gap: S.sm, marginTop: S.md },
  rateInputWrap:{ flexDirection: "row", alignItems: "center", backgroundColor: C.surface2, borderWidth: 1, borderColor: C.border, borderRadius: R.md, marginTop: S.xs, overflow: "hidden" },
  rateUnit:     { fontSize: T.xs, fontWeight: "800", color: C.muted, paddingHorizontal: S.sm },
  rateInput:    { flex: 1, fontSize: T.md, fontWeight: "700", color: C.text, paddingVertical: S.sm + 2, paddingRight: S.sm, fontVariant: ["tabular-nums"] },
  resultBox:    { flexDirection: "row", alignItems: "center", backgroundColor: C.active + "22", borderRadius: R.md, borderWidth: 1, borderColor: C.active, marginTop: S.xs, paddingHorizontal: S.sm, paddingVertical: S.sm + 2, gap: S.xs },
  resultUnit:   { fontSize: T.xs, fontWeight: "900", color: C.active },
  resultVal:    { fontSize: T.md, fontWeight: "900", color: C.active, fontVariant: ["tabular-nums"] },

  resultMsg: { fontSize: T.sm, textAlign: "center", marginTop: S.sm, marginBottom: S.xs },

  lastTxn:  { backgroundColor: C.surface, borderRadius: R.md, borderWidth: 1, borderColor: C.border, padding: S.md, marginBottom: S.md },
  ltLabel:  { fontSize: T.xs, fontWeight: "800", color: C.textSub, textTransform: "uppercase", letterSpacing: 0.7 },
  ltStatus: { fontSize: T.xs, fontWeight: "800" },
  ltAmount: { fontSize: T.xl, fontWeight: "900", color: C.text, fontVariant: ["tabular-nums"] },
  ltSub:    { fontSize: T.xs, color: C.textSub, marginTop: 2 },
  ltId:     { fontSize: T.xs, fontWeight: "700", color: C.textSub },

  navRow:    { flexDirection: "row", flexWrap: "wrap", gap: S.xs, marginTop: S.md },
  navBtn:    { paddingVertical: S.sm - 1, paddingHorizontal: S.md, borderRadius: R.sm, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border },
  navBtnTxt: { fontSize: T.xs + 1, fontWeight: "700", color: C.textSub },
});
