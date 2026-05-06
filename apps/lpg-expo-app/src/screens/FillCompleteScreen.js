import { useEffect, useRef, useState } from "react";
import { Animated, ScrollView, StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../theme";
import { useAppState } from "../state/AppStateProvider";
import { resetFill } from "../services/controllerApi";
import Button from "../components/ui/Button";
import { kg, money } from "../utils/format";

export default function FillCompleteScreen() {
  const { status, transactions, activeUrl, authToken } = useAppState();
  const [busy, setBusy] = useState(false);

  const last = transactions[transactions.length - 1];
  const dispensedKg = last ? Number(last.finalKg || last.netKg || 0) : Number(status.netWeightKg || 0);
  const amountPkr   = last ? Number(last.finalAmount || 0)             : Number(status.currentAmount || 0);
  const ratePerKg   = last ? Number(last.ratePerKg  || 0)              : Number(status.ratePerKg    || 0);

  const checkAnim = useRef(new Animated.Value(0)).current;
  useEffect(() => {
    Animated.spring(checkAnim, { toValue: 1, tension: 80, friction: 8, useNativeDriver: true }).start();
  }, []);

  async function handleNew() {
    setBusy(true);
    try { await resetFill(activeUrl, authToken); }
    catch (err) { showAlert("Error", err?.message || "Reset failed"); }
    finally { setBusy(false); }
  }

  const rows = [
    { label: "Dispensed",   value: `${kg(dispensedKg)} kg`,    big: true  },
    { label: "Amount",      value: `PKR ${money(amountPkr)}`,  big: true, accent: true },
    { label: "Rate",        value: `${money(ratePerKg)} / kg`              },
    last && { label: "Transaction", value: last.transactionId || last.id  },
  ].filter(Boolean);

  return (
    <ScrollView style={styles.shell} contentContainerStyle={styles.content}>
      <View style={styles.hero}>
        <Animated.View style={[styles.checkCircle, { transform: [{ scale: checkAnim }] }]}>
          <Text style={styles.checkMark}>✓</Text>
        </Animated.View>
        <Text style={styles.heroTitle}>Fill Complete</Text>
      </View>

      <View style={styles.receipt}>
        {rows.map((row, i) => (
          <View key={row.label}>
            {i > 0 && <View style={styles.divider} />}
            <View style={styles.row}>
              <Text style={styles.rowLabel}>{row.label}</Text>
              <Text style={[
                styles.rowValue,
                row.big    && styles.rowBig,
                row.accent && { color: C.active },
              ]}>
                {row.value}
              </Text>
            </View>
          </View>
        ))}
      </View>

      <Button label="New Fill →" variant="primary" size="full" onPress={handleNew} loading={busy} />
    </ScrollView>
  );
}

function showAlert(title, msg) {
  try { require("react-native").Alert.alert(title, msg); } catch {}
}

const styles = StyleSheet.create({
  shell:       { flex: 1, backgroundColor: C.bg },
  content:     { padding: S.lg, paddingBottom: 40 },
  hero:        { backgroundColor: C.ready, borderRadius: R.xl, padding: S.xxl, alignItems: "center", marginBottom: S.lg },
  checkCircle: { width: 80, height: 80, borderRadius: 40, backgroundColor: "rgba(255,255,255,0.25)", alignItems: "center", justifyContent: "center", marginBottom: S.md },
  checkMark:   { fontSize: 44, color: C.white },
  heroTitle:   { color: C.white, fontSize: T.xl, fontWeight: "900" },
  receipt:     { backgroundColor: C.surface, borderRadius: R.lg, borderWidth: 1, borderColor: C.border, paddingHorizontal: S.lg, paddingVertical: S.sm, marginBottom: S.lg },
  row:         { flexDirection: "row", justifyContent: "space-between", alignItems: "center", paddingVertical: S.md },
  rowLabel:    { color: C.textSub, fontSize: T.md },
  rowValue:    { color: C.text, fontSize: T.md, fontWeight: "700", fontVariant: ["tabular-nums"] },
  rowBig:      { fontSize: T.xl, fontWeight: "900" },
  divider:     { height: 1, backgroundColor: C.border },
});
