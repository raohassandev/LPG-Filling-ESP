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
import BottomNav from "../components/BottomNav";
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
  const [showAdmin,    setShowAdmin]    = useState(false);

  const rateInit     = useRef(false);
  const slowFillInit = useRef(false);
  const shakeAnim    = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    if (!rateInit.current && status.ratePerKg) {
      setRate(money(status.ratePerKg));
      rateInit.current = true;
    }
  }, [status.ratePerKg]);

  useEffect(() => {
    if (!slowFillInit.current && status.slowFillThreshold) {
      setSlowFillPct(String(Math.round(status.slowFillThreshold * 100)));
      slowFillInit.current = true;
    }
  }, [status.slowFillThreshold]);

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
    try {
      await saveSlowFillThreshold(activeUrl, Number(slowFillPct) / 100, authToken);
      setResult({ ok: true, msg: "Threshold saved" });
    } catch (err) {
      setResult({ ok: false, msg: err?.message || "Failed" });
    }
  }

  const ready   = !!status.readyToFill;
  const isAdmin = authRole === "admin" || authRole === "manufacturer";

  // Today's transactions (last 5, most recent first)
  const todayTxns = [...transactions].reverse().slice(0, 5);

  return (
    <KeyboardAvoidingView
      behavior={Platform.OS === "ios" ? "padding" : undefined}
      style={styles.shell}
    >
      <SafetyBanner status={status} streamMode={streamMode} />
      <StatusHeader
        status={status}
        streamMode={streamMode}
        authRole={authRole}
        authUsername={authUsername}
        onSwitchAccount={logout}
      />

      <ScrollView contentContainerStyle={styles.content} keyboardShouldPersistTaps="handled">

        {/* ── Weight strip ───────────────────────────────────────── */}
        <View style={styles.weightStrip}>
          <WeightCell label="LIVE"  value={kg(status.liveWeightKg ?? status.weightKg)} />
          <WeightCell label="TARE"  value={kg(status.tareWeightKg)} border />
          <WeightCell label="NET"   value={kg(status.netWeightKg)}  accent stable={status.weightStable} border />
        </View>

        {/* ── Readiness icons + START FILL ──────────────────────── */}
        <Animated.View style={[styles.actionRow, { transform: [{ translateX: shakeAnim }] }]}>
          <ReadinessCard status={status} />
          <Pressable
            style={[styles.startBtn, (!ready || busy || !!status.simActive) && styles.startBtnDisabled]}
            onPress={handleStart}
            disabled={!ready || busy || !!status.simActive}
          >
            <Text style={styles.startBtnIcon}>▶</Text>
            <Text style={styles.startBtnLabel}>{busy ? "Starting…" : "START\nFILL"}</Text>
          </Pressable>
        </Animated.View>

        {!ready && (
          <Text style={styles.blockHint}>
            {status.simActive ? "Simulation active — real fill blocked." : "Resolve interlocks before starting."}
          </Text>
        )}
        {result && (
          <Text style={[styles.resultMsg, { color: result.ok ? C.ready : C.danger }]}>
            {result.msg}
          </Text>
        )}

        {/* ── Fill setup card ─────────────────────────────────────── */}
        <View style={styles.card}>
          {/* Mode toggle */}
          <SegmentedControl
            options={[
              { key: "weight", label: "Fill by Weight (kg)" },
              { key: "amount", label: "Fill by Amount (PKR)" },
            ]}
            value={inputMode}
            onChange={(m) => { setInputMode(m); syncTargets(m); }}
          />

          {/* Target input (large) */}
          <View style={styles.targetRow}>
            <View style={styles.targetWrap}>
              <Text style={styles.fieldLabel}>{inputMode === "weight" ? "TARGET kg" : "TARGET PKR"}</Text>
              <View style={styles.targetInputRow}>
                <Text style={styles.targetUnit}>{inputMode === "weight" ? "kg" : "PKR"}</Text>
                <TextInput
                  value={inputMode === "weight" ? targetWeight : targetAmount}
                  onChangeText={(v) => syncTargets(inputMode, inputMode === "weight" ? "weight" : "amount", v)}
                  keyboardType="decimal-pad"
                  style={styles.targetInput}
                  placeholder="0.000"
                  placeholderTextColor={C.muted}
                />
              </View>
            </View>
            <View style={styles.resultWrap}>
              <Text style={styles.fieldLabel}>{inputMode === "weight" ? "TOTAL PKR" : "EQUIV. kg"}</Text>
              <View style={styles.resultBox}>
                <Text style={styles.resultUnit}>{inputMode === "weight" ? "PKR" : "kg"}</Text>
                <Text style={styles.resultVal}>{inputMode === "weight" ? targetAmount : targetWeight}</Text>
              </View>
            </View>
          </View>

          {/* Rate + Tare row */}
          <View style={styles.secondaryRow}>
            <View style={{ flex: 1 }}>
              <Text style={styles.fieldLabel}>RATE / kg</Text>
              <View style={styles.miniInputRow}>
                <Text style={styles.miniUnit}>PKR</Text>
                <TextInput
                  value={rate}
                  onChangeText={authCanSetRate ? (v) => { setRate(v); syncTargets(inputMode); } : undefined}
                  editable={authCanSetRate}
                  keyboardType="decimal-pad"
                  style={[styles.miniInput, !authCanSetRate && { color: C.muted }]}
                  placeholder="—"
                  placeholderTextColor={C.muted}
                />
              </View>
            </View>

            <View style={{ flex: 1 }}>
              <Text style={styles.fieldLabel}>TARE (EMPTY)</Text>
              <View style={styles.miniInputRow}>
                <Text style={styles.miniUnit}>kg</Text>
                <TextInput
                  value={tareWeight}
                  onChangeText={setTareWeight}
                  onFocus={() => setEditTare(true)}
                  onBlur={() => setEditTare(false)}
                  keyboardType="decimal-pad"
                  style={styles.miniInput}
                  placeholder="0.000"
                  placeholderTextColor={C.muted}
                />
              </View>
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

          {/* Admin settings (collapsible) */}
          {isAdmin && (
            <>
              <Pressable style={styles.adminToggle} onPress={() => setShowAdmin(v => !v)}>
                <Text style={styles.adminToggleTxt}>{showAdmin ? "▲ Hide settings" : "▼ Admin settings"}</Text>
              </Pressable>
              {showAdmin && (
                <View style={styles.adminSection}>
                  <Text style={styles.fieldLabel}>FAST→SLOW AT {slowFillPct || "—"}% OF TARGET</Text>
                  <View style={styles.adminRow}>
                    <TextInput
                      value={slowFillPct}
                      onChangeText={setSlowFillPct}
                      keyboardType="numeric"
                      style={[styles.miniInput, { flex: 1, borderWidth: 1, borderColor: C.border, borderRadius: R.md, paddingHorizontal: S.sm, paddingVertical: S.xs + 2 }]}
                      placeholder="95"
                      placeholderTextColor={C.muted}
                    />
                    <Text style={{ color: C.muted, fontSize: T.sm, marginHorizontal: S.xs }}>%</Text>
                    <Button label="Set" variant="ghost" size="sm" onPress={handleSaveSlowFill} />
                    <Button label="Save rate" variant="ghost" size="sm" onPress={handleSaveRate} />
                  </View>
                </View>
              )}
            </>
          )}
        </View>

        {/* ── Today's transactions ─────────────────────────────────── */}
        {todayTxns.length > 0 && (
          <View style={styles.card}>
            <View style={styles.historyHeader}>
              <Text style={styles.cardTitle}>TODAY</Text>
              {isAdmin && (
                <Pressable onPress={() => navigate("transactions")}>
                  <Text style={styles.historyLink}>All history →</Text>
                </Pressable>
              )}
            </View>
            {todayTxns.map((txn, i) => <TxnRow key={txn.id || i} txn={txn} />)}
          </View>
        )}

        {/* ── Navigation row ──────────────────────────────────────── */}
      </ScrollView>
      <BottomNav active="dashboard" />
    </KeyboardAvoidingView>
  );
}

// ── Sub-components ─────────────────────────────────────────────────────────

function WeightCell({ label, value, border, accent, stable }) {
  return (
    <View style={[styles.wCell, border && styles.wCellBorder, accent && styles.wCellAccent]}>
      <View style={{ flexDirection: "row", alignItems: "center", gap: 4 }}>
        <Text style={[styles.wLabel, accent && { color: C.white + "8c" }]}>{label}</Text>
        {accent && (
          <View style={{ width: 7, height: 7, borderRadius: 4, backgroundColor: stable ? C.ready : C.warning }} />
        )}
      </View>
      <Text style={[styles.wVal, accent && { color: C.white }]}>{value}</Text>
      <Text style={[styles.wUnit, accent && { color: C.white + "73" }]}>kg</Text>
    </View>
  );
}

function TxnRow({ txn }) {
  const isOk    = Number(txn.status) === 1;
  const dateStr = shortDateTime(txn.endTime || txn.startTime);
  const netKg   = kg(txn.netKg ?? txn.finalKg ?? 0);
  const amount  = money(txn.finalAmount ?? 0);
  const txnId   = txn.transactionId || `#${txn.id}`;

  return (
    <View style={styles.txnRow}>
      <View style={[styles.txnDot, { backgroundColor: isOk ? C.ready : C.warning }]} />
      <Text style={styles.txnId} numberOfLines={1}>{txnId}</Text>
      <Text style={styles.txnKg}  numberOfLines={1}>{netKg} kg</Text>
      <Text style={styles.txnAmt} numberOfLines={1}>PKR {amount}</Text>
      <Text style={styles.txnTime} numberOfLines={1}>{dateStr}</Text>
    </View>
  );
}

// ── Styles ─────────────────────────────────────────────────────────────────

const styles = StyleSheet.create({
  shell:   { flex: 1, backgroundColor: C.bg },
  content: { padding: S.md, paddingBottom: 52, gap: S.md },

  // Weight strip
  weightStrip: {
    flexDirection: "row",
    backgroundColor: C.surface,
    borderRadius: R.lg,
    borderWidth: 1,
    borderColor: C.border,
    overflow: "hidden",
  },
  wCell:       { flex: 1, alignItems: "center", paddingVertical: S.lg },
  wCellBorder: { borderLeftWidth: 1, borderLeftColor: C.border },
  wCellAccent: { backgroundColor: C.active + "1a", flex: 1.3 },
  wLabel:      { fontSize: T.xs, fontWeight: "800", color: C.textSub, textTransform: "uppercase", letterSpacing: 0.8 },
  wVal:        { fontSize: 26, fontWeight: "900", color: C.text, fontVariant: ["tabular-nums"], marginTop: 2 },
  wUnit:       { fontSize: T.xs, color: C.textSub, marginTop: 1 },

  // Readiness + start button row
  actionRow: {
    flexDirection: "row",
    gap: S.md,
    alignItems: "stretch",
  },
  startBtn: {
    width: 88,
    backgroundColor: C.active,
    borderRadius: R.lg,
    alignItems: "center",
    justifyContent: "center",
    paddingVertical: S.sm,
    gap: S.xs,
  },
  startBtnDisabled: {
    backgroundColor: C.muted,
  },
  startBtnIcon: {
    fontSize: 22,
    color: C.white,
    fontWeight: "900",
  },
  startBtnLabel: {
    fontSize: T.sm,
    fontWeight: "900",
    color: C.white,
    textAlign: "center",
    lineHeight: 16,
  },
  blockHint: {
    fontSize: T.xs,
    color: C.warning,
    textAlign: "center",
    marginTop: -S.xs,
  },
  resultMsg: {
    fontSize: T.sm,
    textAlign: "center",
    marginTop: -S.xs,
  },

  // Fill setup card
  card: {
    backgroundColor: C.surface,
    borderRadius: R.lg,
    borderWidth: 1,
    borderColor: C.border,
    padding: S.md,
    gap: S.md,
  },
  cardTitle: {
    fontSize: T.sm,
    fontWeight: "900",
    color: C.textSub,
    textTransform: "uppercase",
    letterSpacing: 0.8,
  },

  // Target row
  targetRow:      { flexDirection: "row", gap: S.sm, alignItems: "stretch" },
  targetWrap:     { flex: 1.4 },
  resultWrap:     { flex: 1 },
  fieldLabel:     { fontSize: T.xs - 1, fontWeight: "800", color: C.muted, textTransform: "uppercase", letterSpacing: 0.7, marginBottom: S.xs },
  targetInputRow: { flexDirection: "row", alignItems: "center", backgroundColor: C.surface2, borderWidth: 2, borderColor: C.primary, borderRadius: R.md, overflow: "hidden" },
  targetUnit:     { fontSize: T.sm, fontWeight: "900", color: C.primary, paddingHorizontal: S.sm },
  targetInput:    { flex: 1, fontSize: 26, fontWeight: "900", color: C.text, paddingVertical: S.sm, paddingRight: S.sm, fontVariant: ["tabular-nums"] },
  resultBox:      { flexDirection: "row", alignItems: "center", backgroundColor: C.active + "18", borderRadius: R.md, borderWidth: 1, borderColor: C.active + "55", paddingHorizontal: S.sm, paddingVertical: S.sm, gap: S.xs, flex: 1 },
  resultUnit:     { fontSize: T.xs, fontWeight: "900", color: C.active },
  resultVal:      { fontSize: T.md, fontWeight: "900", color: C.active, fontVariant: ["tabular-nums"], flex: 1 },

  // Secondary row (rate + tare)
  secondaryRow:  { flexDirection: "row", gap: S.sm, alignItems: "flex-end" },
  miniInputRow:  { flexDirection: "row", alignItems: "center", backgroundColor: C.surface2, borderWidth: 1, borderColor: C.border, borderRadius: R.md, overflow: "hidden" },
  miniUnit:      { fontSize: T.xs, fontWeight: "800", color: C.muted, paddingHorizontal: S.sm, paddingVertical: S.sm },
  miniInput:     { flex: 1, fontSize: T.md, fontWeight: "700", color: C.text, paddingVertical: S.sm, paddingRight: S.sm, fontVariant: ["tabular-nums"] },

  // Tare buttons
  tareBtns: { flexDirection: "column", gap: S.xs },
  tBtn:     { paddingHorizontal: S.sm, paddingVertical: S.xs + 2, borderRadius: R.sm, borderWidth: 1, borderColor: C.border, backgroundColor: C.surface2, alignItems: "center" },
  tBtnTxt:  { fontSize: T.xs, fontWeight: "700", color: C.textSub },

  // Admin collapsible
  adminToggle:    { alignSelf: "flex-start", paddingVertical: 2 },
  adminToggleTxt: { fontSize: T.xs, fontWeight: "700", color: C.active },
  adminSection:   { gap: S.xs },
  adminRow:       { flexDirection: "row", alignItems: "center", gap: S.xs },

  // Today's transactions
  historyHeader: { flexDirection: "row", justifyContent: "space-between", alignItems: "center" },
  historyLink:   { fontSize: T.xs, fontWeight: "700", color: C.active },
  txnRow: {
    flexDirection: "row",
    alignItems: "center",
    gap: S.sm,
    paddingVertical: S.xs + 2,
    borderTopWidth: 1,
    borderTopColor: C.border,
  },
  txnDot:  { width: 8, height: 8, borderRadius: 4, flexShrink: 0 },
  txnId:   { fontSize: T.xs, fontWeight: "700", color: C.textSub, width: 90 },
  txnKg:   { fontSize: T.xs, fontWeight: "700", color: C.text, fontVariant: ["tabular-nums"], width: 62 },
  txnAmt:  { fontSize: T.xs, fontWeight: "700", color: C.ready, fontVariant: ["tabular-nums"], flex: 1 },
  txnTime: { fontSize: T.xs, color: C.muted, fontVariant: ["tabular-nums"] },

});
