import { useEffect, useRef, useState } from "react";
import { Pressable, ScrollView, StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../theme";
import { useAppState } from "../state/AppStateProvider";
import { calibrateKnown, calibratePoint, calibrateFactor, fetchWeight } from "../services/controllerApi";
import { Field, Input } from "../components/ui/Field";
import SegmentedControl from "../components/ui/SegmentedControl";
import Button from "../components/ui/Button";
import { kg } from "../utils/format";
import BottomNav from "../components/BottomNav";

const METHODS = [
  { key: "twopoint", label: "Two-Point (recommended)" },
  { key: "known",    label: "Known Weight"             },
  { key: "factor",   label: "Manual Factor"            },
];

export default function CalibrationScreen() {
  const { navigate, activeUrl, authToken, status } = useAppState();
  const [method,    setMethod]    = useState("twopoint");
  const [liveKg,    setLiveKg]    = useState(null);
  const [pollError, setPollError] = useState(false);
  const pollRef = useRef(null);

  // Poll live weight during calibration
  useEffect(() => {
    let alive = true;
    const poll = async () => {
      try {
        const w = await fetchWeight(activeUrl, authToken);
        if (alive) { setLiveKg(Number(w.liveWeightKg ?? w.weightKg ?? 0)); setPollError(false); }
      } catch { if (alive) setPollError(true); }
      if (alive) pollRef.current = setTimeout(poll, 800);
    };
    poll();
    return () => { alive = false; clearTimeout(pollRef.current); };
  }, [activeUrl, authToken]);

  return (
    <View style={styles.shell}>
      <View style={styles.header}>
        <Pressable onPress={() => navigate("diagnostics")} style={styles.backBtn}>
          <Text style={styles.backTxt}>← Diagnostics</Text>
        </Pressable>
        <Text style={styles.title}>Calibration</Text>
      </View>

      <ScrollView contentContainerStyle={styles.content} keyboardShouldPersistTaps="handled">
        {/* Live weight readout */}
        <View style={styles.liveCard}>
          <Text style={styles.liveLabel}>LIVE WEIGHT</Text>
          <Text style={[styles.liveVal, pollError && { color: C.danger }]}>
            {pollError ? "READ ERROR" : liveKg !== null ? `${kg(liveKg)} kg` : "…"}
          </Text>
          <Text style={styles.liveSub}>
            Cal valid: {status?.calValid ? "Yes ✓" : "No ✗"}
          </Text>
        </View>

        {/* Method selector */}
        <Field label="Calibration method">
          <SegmentedControl options={METHODS} value={method} onChange={setMethod} style={{ marginTop: S.xs }} />
        </Field>

        {method === "twopoint" && <TwoPointForm activeUrl={activeUrl} authToken={authToken} />}
        {method === "known"    && <KnownWeightForm activeUrl={activeUrl} authToken={authToken} />}
        {method === "factor"   && <FactorForm activeUrl={activeUrl} authToken={authToken} />}

        <View style={styles.warningBox}>
          <Text style={styles.warningTitle}>⚠ Calibration Safety</Text>
          <Text style={styles.warningText}>
            Use certified reference weights only. Inaccurate calibration can cause overfill or short-fill. Always verify against a known weight after calibration.
          </Text>
        </View>
      </ScrollView>
      <BottomNav active="calibration" />
    </View>
  );
}

// ── Two-Point Calibration ─────────────────────────────────────────────────────

function TwoPointForm({ activeUrl, authToken }) {
  const [step,    setStep]    = useState(0); // 0=point1, 1=point2
  const [known1,  setKnown1]  = useState("");
  const [known2,  setKnown2]  = useState("");
  const [busy,    setBusy]    = useState(false);
  const [result,  setResult]  = useState(null);

  async function setPoint(point, known) {
    setBusy(true); setResult(null);
    try {
      const r = await calibratePoint(activeUrl, point, Number(known), authToken);
      setResult({ ok: true, msg: r?.message || `Point ${point} set` });
      if (point === 1) setStep(1);
    } catch (err) {
      setResult({ ok: false, msg: err?.message || "Failed" });
    } finally { setBusy(false); }
  }

  return (
    <View style={styles.methodCard}>
      <Text style={styles.methodTitle}>Two-Point Calibration</Text>
      <Text style={styles.methodSub}>
        Place two different reference weights on the scale. Record each with its known value. The firmware computes the gain and offset.
      </Text>

      {/* Step indicator */}
      <View style={{ flexDirection: "row", gap: S.sm, marginVertical: S.md }}>
        {["Point 1", "Point 2"].map((l, i) => (
          <View key={l} style={[styles.stepDot, step >= i && styles.stepDotOn]}>
            <Text style={[styles.stepTxt, step >= i && styles.stepTxtOn]}>{i + 1}</Text>
          </View>
        ))}
      </View>

      {step === 0 && (
        <>
          <Text style={styles.instruction}>Place first reference weight on scale and enter its known value.</Text>
          <Field label="Known weight 1 (kg)">
            <Input value={known1} onChangeText={setKnown1} keyboardType="decimal-pad" placeholder="e.g. 5.000" />
          </Field>
          <ResultMsg result={result} />
          <Button label={busy ? "Setting…" : "Set Point 1"} variant="primary" size="full" onPress={() => setPoint(1, known1)} loading={busy} style={{ marginTop: S.md }} />
        </>
      )}

      {step === 1 && (
        <>
          <Text style={styles.instruction}>Replace with second reference weight and enter its known value.</Text>
          <Field label="Known weight 2 (kg)">
            <Input value={known2} onChangeText={setKnown2} keyboardType="decimal-pad" placeholder="e.g. 10.000" />
          </Field>
          <ResultMsg result={result} />
          <View style={{ flexDirection: "row", gap: S.sm, marginTop: S.md }}>
            <Button label="← Back" variant="secondary" size="md" onPress={() => { setStep(0); setResult(null); }} style={{ flex: 1 }} />
            <Button label={busy ? "Setting…" : "Set Point 2"} variant="primary" size="md" onPress={() => setPoint(2, known2)} loading={busy} style={{ flex: 2 }} />
          </View>
        </>
      )}
    </View>
  );
}

// ── Known Weight Calibration ──────────────────────────────────────────────────

function KnownWeightForm({ activeUrl, authToken }) {
  const [known,  setKnown]  = useState("");
  const [busy,   setBusy]   = useState(false);
  const [result, setResult] = useState(null);

  async function submit() {
    setBusy(true); setResult(null);
    try {
      const r = await calibrateKnown(activeUrl, Number(known), authToken);
      setResult({ ok: true, msg: r?.message || "Calibration applied" });
    } catch (err) {
      setResult({ ok: false, msg: err?.message || "Failed" });
    } finally { setBusy(false); }
  }

  return (
    <View style={styles.methodCard}>
      <Text style={styles.methodTitle}>Known Weight</Text>
      <Text style={styles.methodSub}>Place a single reference weight on the scale and enter its value. The firmware auto-tares then sets the scale factor.</Text>
      <Field label="Known weight (kg)">
        <Input value={known} onChangeText={setKnown} keyboardType="decimal-pad" placeholder="e.g. 10.000" />
      </Field>
      <ResultMsg result={result} />
      <Button label={busy ? "Calibrating…" : "Calibrate"} variant="primary" size="full" onPress={submit} loading={busy} style={{ marginTop: S.md }} />
    </View>
  );
}

// ── Manual Factor ─────────────────────────────────────────────────────────────

function FactorForm({ activeUrl, authToken }) {
  const [factor, setFactor] = useState("");
  const [busy,   setBusy]   = useState(false);
  const [result, setResult] = useState(null);

  async function submit() {
    setBusy(true); setResult(null);
    try {
      const r = await calibrateFactor(activeUrl, Number(factor), authToken);
      setResult({ ok: true, msg: r?.message || "Factor applied" });
    } catch (err) {
      setResult({ ok: false, msg: err?.message || "Failed" });
    } finally { setBusy(false); }
  }

  return (
    <View style={styles.methodCard}>
      <Text style={styles.methodTitle}>Manual Scale Factor</Text>
      <Text style={styles.methodSub}>Enter the raw scale factor directly. Use only if you have the value from a previous calibration or datasheet.</Text>
      <Field label="Scale factor (raw)">
        <Input value={factor} onChangeText={setFactor} keyboardType="decimal-pad" placeholder="e.g. 419700" />
      </Field>
      <ResultMsg result={result} />
      <Button label={busy ? "Applying…" : "Apply Factor"} variant="primary" size="full" onPress={submit} loading={busy} style={{ marginTop: S.md }} />
    </View>
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
  title:   { color: C.text, fontSize: T.md, fontWeight: "900" },
  content: { padding: S.md, paddingBottom: S.xl },

  liveCard:  { backgroundColor: C.surface, borderRadius: R.lg, borderWidth: 1, borderColor: C.border, padding: S.lg, alignItems: "center", marginBottom: S.md },
  liveLabel: { fontSize: T.xs, fontWeight: "800", color: C.textSub, textTransform: "uppercase", letterSpacing: 0.7 },
  liveVal:   { fontSize: 40, fontWeight: "900", color: C.text, fontVariant: ["tabular-nums"], marginTop: S.xs },
  liveSub:   { fontSize: T.xs, color: C.textSub, marginTop: S.xs },

  methodCard:  { backgroundColor: C.surface, borderRadius: R.lg, borderWidth: 1, borderColor: C.border, padding: S.md, marginTop: S.md },
  methodTitle: { fontSize: T.md, fontWeight: "900", color: C.text, marginBottom: S.xs },
  methodSub:   { fontSize: T.sm, color: C.textSub, lineHeight: 20, marginBottom: S.sm },
  instruction: { fontSize: T.sm, color: C.text, marginBottom: S.sm },

  stepDot:    { width: 32, height: 32, borderRadius: 16, backgroundColor: C.surface2, borderWidth: 1, borderColor: C.border, alignItems: "center", justifyContent: "center" },
  stepDotOn:  { backgroundColor: C.primary, borderColor: C.primary },
  stepTxt:    { color: C.textSub, fontSize: T.sm, fontWeight: "800" },
  stepTxtOn:  { color: C.white },

  resultMsg: { fontSize: T.sm, textAlign: "center", marginTop: S.sm },

  warningBox:   { backgroundColor: C.warning + "1a", borderRadius: R.md, borderWidth: 1, borderColor: C.warning, padding: S.md, marginTop: S.xl },
  warningTitle: { color: C.warning, fontSize: T.sm, fontWeight: "900", marginBottom: S.xs },
  warningText:  { color: C.textSub, fontSize: T.xs, lineHeight: 18 },
});
