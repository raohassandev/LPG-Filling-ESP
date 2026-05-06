import { StyleSheet, Text, View } from "react-native";
import { C, S, T } from "../theme";

// Full-width red banner — visible on EVERY screen when simulation is active.
// This is intentionally loud and cannot be dismissed.
export default function SafetyBanner({ status, streamMode }) {
  const sim = !!status?.simActive;
  const offline = streamMode === "offline";

  if (sim) {
    return (
      <View style={[styles.bar, { backgroundColor: C.sim }]}>
        <Text style={styles.title}>SIMULATION MODE ACTIVE</Text>
        <Text style={styles.sub}>Outputs are NOT driving real valves. Disable simulation in firmware before any live fill.</Text>
      </View>
    );
  }

  if (offline) {
    return (
      <View style={[styles.bar, { backgroundColor: C.warning }]}>
        <Text style={styles.title}>CONTROLLER OFFLINE</Text>
        <Text style={styles.sub}>App cannot reach the controller. Safety actions are disabled.</Text>
      </View>
    );
  }

  return null;
}

const styles = StyleSheet.create({
  bar: {
    paddingVertical: S.sm + 2,
    paddingHorizontal: S.md,
    alignItems: "center",
  },
  title: { color: C.white, fontSize: T.md, fontWeight: "900", letterSpacing: 0.6 },
  sub:   { color: C.white, fontSize: T.xs, marginTop: 2, opacity: 0.95, textAlign: "center" },
});
