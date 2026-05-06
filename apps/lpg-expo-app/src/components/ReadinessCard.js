import { StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../theme";
import { getBlockerUI } from "../utils/faultMessages";
import StatusChip from "./ui/StatusChip";

export default function ReadinessCard({ status }) {
  // Firmware is authority. readyToFill === true is the sole source of truth.
  // Local flags only used for legacy firmware that doesn't expose blockers[].
  const ready = !!status?.readyToFill;
  const blockers = Array.isArray(status?.blockers) ? status.blockers : deriveLegacyBlockers(status);

  if (ready && blockers.length === 0) {
    return (
      <View style={[styles.card, styles.cardReady]}>
        <View style={styles.headerRow}>
          <StatusChip label="READY" tone="ready" />
          <Text style={styles.title}>System ready to fill</Text>
        </View>
        <Text style={styles.sub}>All interlocks satisfied. You can start a fill.</Text>
      </View>
    );
  }

  return (
    <View style={[styles.card, styles.cardBlocked]}>
      <View style={styles.headerRow}>
        <StatusChip label="NOT READY" tone="warning" />
        <Text style={styles.title}>Resolve before starting</Text>
      </View>
      {blockers.map((code) => {
        const ui = getBlockerUI(code);
        return (
          <View key={code} style={styles.blockerRow}>
            <View style={styles.dot} />
            <View style={{ flex: 1 }}>
              <Text style={styles.blockerLabel}>{ui.label}</Text>
              {!!ui.guidance && <Text style={styles.blockerGuide}>{ui.guidance}</Text>}
            </View>
          </View>
        );
      })}
    </View>
  );
}

function deriveLegacyBlockers(status) {
  const out = [];
  if (!status) return out;
  if (status.simActive) out.push("simulation_active");
  if (!status.emergencyStopOk) out.push("estop_active");
  if (!status.cylinderPresent) out.push("cylinder_missing");
  if (!status.nozzleEngaged) out.push("nozzle_not_engaged");
  if (!status.weightInitialized) out.push("scale_not_initialized");
  if (status.weightReadError) out.push("scale_read_error");
  if (!status.weightStable) out.push("scale_unstable");
  if (!status.calValid) out.push("scale_not_calibrated");
  return out;
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: C.surface,
    borderRadius: R.lg,
    borderWidth: 1,
    borderColor: C.border,
    padding: S.md,
    marginBottom: S.md,
  },
  cardReady: { borderColor: C.ready, backgroundColor: "rgba(16,185,129,0.08)" },
  cardBlocked: { borderColor: C.warning },
  headerRow: { flexDirection: "row", alignItems: "center", gap: S.sm, marginBottom: S.sm },
  title: { color: C.text, fontSize: T.md, fontWeight: "800" },
  sub:   { color: C.textSub, fontSize: T.sm },
  blockerRow: { flexDirection: "row", gap: S.sm, alignItems: "flex-start", paddingVertical: 4 },
  dot: { width: 8, height: 8, borderRadius: 4, backgroundColor: C.warning, marginTop: 6 },
  blockerLabel: { color: C.text, fontSize: T.sm, fontWeight: "700" },
  blockerGuide: { color: C.textSub, fontSize: T.xs, marginTop: 2 },
});
