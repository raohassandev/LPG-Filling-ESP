import { StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../theme";
import { kg } from "../utils/format";
import Card from "./ui/Card";
import MetricTile from "./ui/MetricTile";
import StatusChip from "./ui/StatusChip";

export default function ScaleHealthCard({ status }) {
  const initialized = !!status?.weightInitialized;
  const readError   = !!status?.weightReadError;
  const stable      = !!status?.weightStable;
  const calValid    = !!status?.calValid;

  const okChip = (label, ok) => (
    <StatusChip label={label} tone={ok ? "ready" : "warning"} small />
  );

  return (
    <Card
      title="Scale"
      chip={
        <View style={styles.chips}>
          {okChip(initialized ? "HX711 OK" : "HX711 FAIL", initialized && !readError)}
          {okChip(stable ? "STABLE" : "UNSTABLE", stable)}
          {okChip(calValid ? "CAL OK" : "NEEDS CAL", calValid)}
        </View>
      }
    >
      <View style={styles.row}>
        <MetricTile label="Live" value={kg(status?.liveWeightKg ?? status?.weightKg)} unit="kg" />
        <MetricTile label="Tare" value={kg(status?.tareWeightKg)} unit="kg" />
        <MetricTile label="Net" value={kg(status?.netWeightKg)} unit="kg" accent />
      </View>
      <View style={styles.metaRow}>
        <Text style={styles.meta}>Raw: {String(status?.rawValue ?? "—")}</Text>
        <Text style={styles.meta}>Tare raw: {String(status?.tareRawValue ?? "—")}</Text>
        <Text style={styles.meta}>Mode: {status?.calMode || "—"}</Text>
      </View>
    </Card>
  );
}

const styles = StyleSheet.create({
  chips:   { flexDirection: "row", gap: 4, flexWrap: "wrap", justifyContent: "flex-end" },
  row:     { flexDirection: "row", gap: S.sm, marginTop: 4 },
  metaRow: { flexDirection: "row", gap: S.md, marginTop: S.sm, flexWrap: "wrap" },
  meta:    { color: C.textSub, fontSize: T.xs, fontVariant: ["tabular-nums"] },
});
