import { StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../../theme";

export default function Card({ title, chip, action, children, style, padded = true }) {
  return (
    <View style={[styles.card, style]}>
      {(title || chip || action) && (
        <View style={styles.header}>
          <View style={{ flexDirection: "row", alignItems: "center", gap: S.sm, flex: 1 }}>
            {!!title && <Text style={styles.title}>{title}</Text>}
            {chip}
          </View>
          {action}
        </View>
      )}
      <View style={padded ? styles.body : null}>{children}</View>
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: C.surface,
    borderRadius: R.lg,
    borderWidth: 1,
    borderColor: C.border,
    marginBottom: S.md,
    overflow: "hidden",
  },
  header: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
    paddingHorizontal: S.lg,
    paddingTop: S.md,
    paddingBottom: S.sm,
  },
  title:  { fontSize: T.lg, fontWeight: "800", color: C.text, letterSpacing: -0.2 },
  body:   { paddingHorizontal: S.lg, paddingBottom: S.lg, paddingTop: S.xs },
});
