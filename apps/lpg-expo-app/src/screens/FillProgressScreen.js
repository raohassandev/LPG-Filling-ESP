import { useEffect, useRef } from "react";
import { Animated, StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../theme";
import { useAppState } from "../state/AppStateProvider";
import { useControllerActions } from "../hooks/useControllerActions";
import Button from "../components/ui/Button";
import StatusHeader from "../components/StatusHeader";
import { kg, money } from "../utils/format";

export default function FillProgressScreen() {
  const { status, streamMode, authRole, authUsername, logout } = useAppState();
  const { stopFill, busy } = useControllerActions();

  const isSlow     = status.state === "FILLING_SLOW";
  const isSettling = status.state === "SETTLING";
  const target     = Number(status.targetWeightKg || 0);
  const net        = Number(status.netWeightKg    || 0);
  const pct        = target > 0 ? Math.min(100, Math.max(0, (net / target) * 100)) : 0;

  const progressAnim = useRef(new Animated.Value(0)).current;
  useEffect(() => {
    Animated.spring(progressAnim, { toValue: pct, tension: 30, friction: 10, useNativeDriver: false }).start();
  }, [pct, progressAnim]);
  const progressWidth = progressAnim.interpolate({ inputRange: [0, 100], outputRange: ["0%", "100%"], extrapolate: "clamp" });

  const accentColor = isSettling ? C.settling : isSlow ? C.warning : C.active;
  const stateLabel  = isSettling ? "SETTLING" : isSlow ? "SLOW FILL" : "FAST FILL";

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
        <View style={[styles.stateBadge, { borderColor: accentColor }]}>
          <Text style={[styles.stateText, { color: accentColor }]}>{stateLabel}</Text>
          <Text style={styles.stateAmt}>PKR {money(status.currentAmount)} running</Text>
        </View>

        <View style={styles.weightRow}>
          <Text style={styles.netNum}>{kg(net)}</Text>
          <Text style={styles.weightSep}>/</Text>
          <Text style={styles.targetNum}>{kg(target)}</Text>
          <Text style={styles.weightUnit}>kg</Text>
        </View>

        <Text style={[styles.pctNum, { color: accentColor }]}>{pct.toFixed(0)}%</Text>

        <View style={styles.track}>
          <Animated.View style={[styles.fill, { width: progressWidth, backgroundColor: accentColor }]} />
          {!!status.slowFillThreshold && (
            <View style={[styles.marker, { left: `${Math.round(status.slowFillThreshold * 100)}%` }]} />
          )}
        </View>

        <View style={styles.statsRow}>
          {[
            { l: "RATE",   v: `${money(status.ratePerKg)}/kg` },
            { l: "TARGET", v: `${kg(target)} kg` },
            { l: "AMOUNT", v: `PKR ${money(status.currentAmount)}` },
          ].map(({ l, v }, i, arr) => (
            <View key={l} style={[styles.statCell, i < arr.length - 1 && styles.statDivider]}>
              <Text style={styles.statLabel}>{l}</Text>
              <Text style={styles.statValue}>{v}</Text>
            </View>
          ))}
        </View>

        <View style={styles.spacer} />

        <Button
          label={busy ? "Stopping..." : "STOP FILL"}
          variant="danger"
          size="full"
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

  stateBadge:   { flexDirection: "row", justifyContent: "space-between", alignItems: "center", borderWidth: 1, borderRadius: R.md, paddingHorizontal: S.md, paddingVertical: S.sm, marginBottom: S.xl },
  stateText:    { fontSize: T.lg, fontWeight: "900", letterSpacing: 1 },
  stateAmt:     { color: C.textSub, fontSize: T.sm },

  weightRow:   { flexDirection: "row", alignItems: "flex-end", justifyContent: "center", gap: S.sm, marginBottom: S.xs },
  netNum:      { fontSize: 52, fontWeight: "900", color: C.text, lineHeight: 56, fontVariant: ["tabular-nums"] },
  weightSep:   { fontSize: 30, fontWeight: "300", color: C.textSub, paddingBottom: 4 },
  targetNum:   { fontSize: 30, fontWeight: "700", color: C.textSub, lineHeight: 40, fontVariant: ["tabular-nums"] },
  weightUnit:  { fontSize: T.lg, fontWeight: "600", color: C.textSub, paddingBottom: 8 },

  pctNum: { fontSize: 56, fontWeight: "900", textAlign: "center", marginBottom: S.md, fontVariant: ["tabular-nums"] },

  track:  { height: 12, backgroundColor: C.surface2, borderRadius: R.pill, overflow: "hidden", marginBottom: S.xl, position: "relative" },
  fill:   { height: "100%", borderRadius: R.pill },
  marker: { position: "absolute", top: 0, bottom: 0, width: 2, backgroundColor: C.white, opacity: 0.4 },

  statsRow:    { flexDirection: "row", backgroundColor: C.surface, borderRadius: R.lg, paddingVertical: S.md, marginBottom: S.lg },
  statCell:    { flex: 1, alignItems: "center", paddingHorizontal: S.sm },
  statDivider: { borderRightWidth: 1, borderRightColor: C.border },
  statLabel:   { color: C.textSub, fontSize: T.xs, fontWeight: "800", letterSpacing: 0.8, textTransform: "uppercase" },
  statValue:   { color: C.text, fontSize: T.md, fontWeight: "700", marginTop: S.xs, fontVariant: ["tabular-nums"] },
  spacer:      { flex: 1 },
});
