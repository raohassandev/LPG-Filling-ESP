import { Pressable, StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../../theme";

export default function SegmentedControl({ options, value, onChange, disabled, style }) {
  return (
    <View style={[styles.bar, disabled && { opacity: 0.5 }, style]}>
      {options.map((opt) => {
        const active = String(value) === String(opt.key);
        return (
          <Pressable
            key={String(opt.key)}
            disabled={disabled}
            style={[styles.opt, active && styles.optOn]}
            onPress={() => !disabled && onChange(opt.key)}
          >
            <Text style={[styles.txt, active && styles.txtOn]}>{opt.label}</Text>
          </Pressable>
        );
      })}
    </View>
  );
}

const styles = StyleSheet.create({
  bar: {
    flexDirection: "row",
    borderWidth: 1,
    borderColor: C.border,
    borderRadius: R.md,
    overflow: "hidden",
    backgroundColor: C.surface,
  },
  opt:   { flex: 1, paddingVertical: S.sm + 2, alignItems: "center" },
  optOn: { backgroundColor: C.primary },
  txt:   { fontSize: T.sm, fontWeight: "700", color: C.textSub },
  txtOn: { color: C.white },
});
