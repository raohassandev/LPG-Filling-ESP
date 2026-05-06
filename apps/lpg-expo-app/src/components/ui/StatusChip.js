import { StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../../theme";

// tone: 'ready' | 'warning' | 'danger' | 'active' | 'settling' | 'complete' | 'offline' | 'info' | 'neutral'
const TONES = {
  ready:    { bg: "rgba(16,185,129,0.18)", fg: C.ready,    border: C.ready },
  complete: { bg: "rgba(16,185,129,0.18)", fg: C.complete, border: C.complete },
  warning:  { bg: "rgba(245,158,11,0.18)", fg: C.warning,  border: C.warning },
  danger:   { bg: "rgba(239,68,68,0.18)",  fg: C.danger,   border: C.danger },
  active:   { bg: "rgba(59,130,246,0.18)", fg: C.active,   border: C.active },
  settling: { bg: "rgba(139,92,246,0.18)", fg: C.settling, border: C.settling },
  offline:  { bg: "rgba(107,114,128,0.18)",fg: C.offline,  border: C.offline },
  info:     { bg: "rgba(59,130,246,0.18)", fg: C.active,   border: C.active },
  neutral:  { bg: C.surface2,              fg: C.textSub,  border: C.border },
};

export default function StatusChip({ label, tone = "neutral", small, style }) {
  const p = TONES[tone] || TONES.neutral;
  return (
    <View style={[styles.box, { backgroundColor: p.bg, borderColor: p.border }, small && styles.boxSm, style]}>
      <Text style={[styles.txt, { color: p.fg }, small && styles.txtSm]}>{label}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  box: {
    borderRadius: R.pill,
    borderWidth: 1,
    paddingHorizontal: S.md,
    paddingVertical: 4,
    alignSelf: "flex-start",
  },
  boxSm: { paddingHorizontal: S.sm, paddingVertical: 2 },
  txt:   { fontSize: T.sm, fontWeight: "800", letterSpacing: 0.3 },
  txtSm: { fontSize: T.xs },
});
