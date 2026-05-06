import { StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../../theme";

export default function MetricTile({ label, value, unit, accent, hero, warn, style }) {
  const valueColor = warn ? C.danger : accent ? C.primary : C.text;
  return (
    <View style={[styles.tile, warn && styles.tileWarn, style]}>
      <Text style={styles.label}>{label}</Text>
      <View style={styles.row}>
        <Text style={[
          styles.value,
          hero && styles.valueHero,
          { color: valueColor },
        ]}>{value}</Text>
        {!!unit && <Text style={[styles.unit, hero && styles.unitHero]}>{unit}</Text>}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  tile: {
    backgroundColor: C.surface2,
    borderRadius: R.md,
    borderWidth: 1,
    borderColor: C.border,
    paddingVertical: S.md,
    paddingHorizontal: S.md,
    flex: 1,
    minWidth: 110,
  },
  tileWarn: { borderColor: C.danger, backgroundColor: "rgba(239,68,68,0.10)" },
  label: { fontSize: T.xs, fontWeight: "800", color: C.textSub, letterSpacing: 0.6, textTransform: "uppercase" },
  row:   { flexDirection: "row", alignItems: "baseline", gap: 4, marginTop: 4 },
  value: { fontSize: T.xl, fontWeight: "900", color: C.text, fontVariant: ["tabular-nums"] },
  valueHero: { fontSize: T.hero, lineHeight: T.hero + 4 },
  unit:  { fontSize: T.sm, color: C.textSub, fontWeight: "700" },
  unitHero: { fontSize: T.lg },
});
