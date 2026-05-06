import { StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../../theme";

// tone: 'info' | 'warning' | 'danger' | 'success'
export default function Banner({ tone = "info", title, message, children, style }) {
  const palette = TONES[tone] || TONES.info;
  return (
    <View style={[styles.box, { backgroundColor: palette.bg, borderColor: palette.border }, style]}>
      {!!title && <Text style={[styles.title, { color: palette.fg }]}>{title}</Text>}
      {!!message && <Text style={[styles.msg, { color: palette.fg }]}>{message}</Text>}
      {children}
    </View>
  );
}

const TONES = {
  info:    { bg: "rgba(59,130,246,0.12)",  border: C.primary,  fg: C.text },
  warning: { bg: "rgba(245,158,11,0.15)",  border: C.warning,  fg: C.text },
  danger:  { bg: C.sim,                    border: C.sim,      fg: C.white },
  success: { bg: "rgba(16,185,129,0.15)",  border: C.ready,    fg: C.text },
};

const styles = StyleSheet.create({
  box: {
    borderRadius: R.md,
    borderWidth: 1,
    paddingVertical: S.sm + 2,
    paddingHorizontal: S.md,
    marginBottom: S.sm,
  },
  title: { fontSize: T.md, fontWeight: "900", letterSpacing: 0.3 },
  msg:   { fontSize: T.sm, marginTop: 2, opacity: 0.95 },
});
