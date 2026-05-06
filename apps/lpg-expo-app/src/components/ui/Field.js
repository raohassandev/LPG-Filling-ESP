import { StyleSheet, Text, TextInput, View } from "react-native";
import { C, R, S, T } from "../../theme";

export function Field({ label, hint, error, children, style }) {
  return (
    <View style={[styles.field, style]}>
      {!!label && <Text style={styles.label}>{label}</Text>}
      {children}
      {!!error && <Text style={styles.error}>{error}</Text>}
      {!!hint && !error && <Text style={styles.hint}>{hint}</Text>}
    </View>
  );
}

export function Input(props) {
  return (
    <TextInput
      placeholderTextColor={C.muted}
      {...props}
      style={[styles.input, props.style]}
    />
  );
}

export default Field;

const styles = StyleSheet.create({
  field: { marginTop: S.md },
  label: {
    fontSize: T.xs, fontWeight: "800", color: C.textSub,
    textTransform: "uppercase", letterSpacing: 0.7, marginBottom: S.xs + 2,
  },
  hint:  { fontSize: T.xs, color: C.muted, marginTop: 4 },
  error: { fontSize: T.xs, color: C.danger, marginTop: 4, fontWeight: "700" },
  input: {
    backgroundColor: C.surface2,
    borderWidth: 1,
    borderColor: C.border,
    borderRadius: R.md,
    paddingHorizontal: S.md,
    paddingVertical: S.sm + 2,
    fontSize: T.md,
    color: C.text,
    fontVariant: ["tabular-nums"],
  },
});
