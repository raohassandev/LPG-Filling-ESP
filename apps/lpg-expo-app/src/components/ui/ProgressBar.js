import { StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../../theme";

// netKg, targetKg numeric. slowFillThreshold 0..1. overfillKg numeric (max safe).
export default function ProgressBar({
  netKg = 0,
  targetKg = 0,
  slowFillThreshold = 0.95,
  overfillRatio = 1.1,
  isSlow = false,
  showLabels = true,
  height = 14,
}) {
  const target = Math.max(0.0001, Number(targetKg) || 0);
  const net    = Math.max(0, Number(netKg) || 0);
  const ratio  = net / target; // 0..>1

  // The bar is scaled so that overfillRatio (e.g. 1.1) hits 100%.
  const trackMaxRatio = overfillRatio;
  const fillPct       = Math.min(100, (ratio / trackMaxRatio) * 100);
  const slowMarkerPct = (slowFillThreshold / trackMaxRatio) * 100;
  const targetPct     = (1 / trackMaxRatio) * 100;

  const fillColor = ratio >= 1 ? C.danger : isSlow ? C.warning : C.active;

  return (
    <View>
      <View style={[styles.track, { height }]}>
        {/* Overfill danger zone (from 100% to end) */}
        <View style={[styles.dangerZone, { left: `${targetPct}%` }]} />
        {/* Filled portion */}
        <View style={[styles.fill, { width: `${fillPct}%`, backgroundColor: fillColor, height }]} />
        {/* slow-fill marker */}
        <View style={[styles.marker, { left: `${slowMarkerPct}%`, backgroundColor: C.warning, height }]} />
        {/* target marker */}
        <View style={[styles.markerThick, { left: `${targetPct}%`, height }]} />
      </View>
      {showLabels && (
        <View style={styles.labelRow}>
          <Text style={styles.label}>0</Text>
          <Text style={[styles.label, { color: C.warning }]}>slow {Math.round(slowFillThreshold * 100)}%</Text>
          <Text style={[styles.label, { color: C.text, fontWeight: "800" }]}>target {Number(target).toFixed(2)}kg</Text>
          <Text style={[styles.label, { color: C.danger }]}>overfill</Text>
        </View>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  track: {
    backgroundColor: C.surface2,
    borderRadius: R.pill,
    overflow: "hidden",
    position: "relative",
    width: "100%",
  },
  fill: {
    position: "absolute",
    left: 0, top: 0,
    borderRadius: R.pill,
  },
  dangerZone: {
    position: "absolute",
    top: 0, bottom: 0, right: 0,
    backgroundColor: "rgba(239,68,68,0.18)",
  },
  marker: {
    position: "absolute",
    top: 0, bottom: 0,
    width: 2,
    opacity: 0.85,
  },
  markerThick: {
    position: "absolute",
    top: 0, bottom: 0,
    width: 3,
    backgroundColor: C.text,
  },
  labelRow: {
    flexDirection: "row",
    justifyContent: "space-between",
    marginTop: 4,
  },
  label: { fontSize: T.xs, color: C.textSub },
});
