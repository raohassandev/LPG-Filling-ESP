import { useEffect, useRef } from "react";
import { Animated, StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../theme";
import { useAppState } from "../state/AppStateProvider";
import { useControllerActions } from "../hooks/useControllerActions";
import Button from "../components/ui/Button";
import StatusHeader from "../components/StatusHeader";

export default function FillProgressScreen() {
  const { status, streamMode, authRole, authUsername, logout } = useAppState();
  const { stopFill, busy } = useControllerActions();

  const state      = status?.state || "IDLE";
  const target     = Number(status?.targetWeightKg || 0);
  const net        = Number(status?.netWeightKg    || 0);
  const pct        = Math.min(Math.max(net / (target || 1), 0), 1);

  const progressAnim = useRef(new Animated.Value(0)).current;
  useEffect(() => {
    Animated.spring(progressAnim, { toValue: pct, useNativeDriver: false }).start();
  }, [pct, progressAnim]);
  const progressWidth = progressAnim.interpolate({ inputRange: [0, 1], outputRange: ["0%", "100%"], extrapolate: "clamp" });

  const barColor = state === "FILLING_FAST" ? C.active : state === "FILLING_SLOW" ? C.warning : C.settling;
  const stateLabel  = state === "SETTLING" ? "SETTLING" : state === "FILLING_SLOW" ? "SLOW FILL" : "FAST FILL";

  return (
    <View style={styles.shell}>
      <StatusHeader
        status={status}
        streamMode={streamMode}
        authRole={authRole}
        authUsername={authUsername}
        onSwitchAccount={logout}
      />

      <View style={styles.content}>
        <View style={[styles.stateBadge, { borderColor: barColor }]}>
          <Text style={[styles.stateText, { color: barColor }]}>{stateLabel}</Text>
        </View>

        <View style={styles.progressRow}>
          <View style={styles.track}>
            <Animated.View style={[styles.fill, { width: progressWidth, backgroundColor: barColor }]} />
          </View>
          <Text style={styles.pctNum}>{Math.round(pct * 100)}%</Text>
        </View>

        <View style={styles.keyNumbers}>
          {[
            { label: "NET kg", value: (status?.netWeightKg ?? 0).toFixed(3) },
            { label: "TARGET", value: (status?.targetWeightKg ?? 0).toFixed(3) },
            { label: "PKR", value: Math.round(status?.currentAmount ?? 0).toLocaleString() },
          ].map(({ label, value }) => (
            <View key={label} style={styles.keyCell}>
              <Text style={styles.keyLabel}>{label}</Text>
              <Text style={styles.keyValue}>{value}</Text>
            </View>
          ))}
        </View>

        <Button
          label={busy ? "Stopping..." : "STOP FILL"}
          variant="danger"
          size="full"
          style={styles.stopBtn}
          onPress={() => stopFill()}
          loading={busy}
          confirmLabel="Stop the fill?"
          confirmMessage="This will close the valve and abort the current fill."
        />
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  shell: { flex: 1, backgroundColor: C.bg },
  content: { flex: 1, paddingHorizontal: S.lg, paddingTop: S.lg, paddingBottom: S.xl },

  stateBadge:   { alignItems: "center", borderWidth: 1, borderRadius: R.md, paddingHorizontal: S.md, paddingVertical: S.sm },
  stateText:    { fontSize: T.lg, fontWeight: "900", letterSpacing: 1 },

  progressRow: { flexDirection: "row", alignItems: "center", gap: S.sm, marginVertical: S.md },
  track:       { flex: 1, height: 14, backgroundColor: C.surface2, borderRadius: R.pill, overflow: "hidden" },
  fill:        { height: 14, borderRadius: R.pill },
  pctNum:      { fontSize: T.lg, fontWeight: "900", color: C.text, minWidth: 48, textAlign: "right", fontVariant: ["tabular-nums"] },

  keyNumbers: { flexDirection: "row", marginBottom: S.md },
  keyCell:    { flex: 1, alignItems: "center" },
  keyLabel:   { fontSize: T.xs, fontWeight: "700", color: C.muted, letterSpacing: 0.5 },
  keyValue:   { fontSize: T.xl, fontWeight: "900", color: C.text, fontVariant: ["tabular-nums"] },
  stopBtn:    { marginTop: "auto" },
});
