import { Modal, Pressable, StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../../theme";

export default function ConfirmModal({
  visible,
  title,
  message,
  confirmLabel = "Confirm",
  cancelLabel = "Cancel",
  variant = "primary",
  onConfirm,
  onCancel,
}) {
  const accent = variant === "danger" ? C.danger : C.primary;
  return (
    <Modal visible={visible} transparent animationType="fade" onRequestClose={onCancel}>
      <View style={styles.scrim}>
        <View style={styles.card}>
          <Text style={styles.title}>{title}</Text>
          {!!message && <Text style={styles.message}>{message}</Text>}
          <View style={styles.btnRow}>
            <Pressable style={[styles.btn, styles.btnGhost]} onPress={onCancel}>
              <Text style={styles.btnGhostTxt}>{cancelLabel}</Text>
            </Pressable>
            <Pressable style={[styles.btn, { backgroundColor: accent }]} onPress={onConfirm}>
              <Text style={styles.btnTxt}>{confirmLabel}</Text>
            </Pressable>
          </View>
        </View>
      </View>
    </Modal>
  );
}

const styles = StyleSheet.create({
  scrim: {
    flex: 1,
    backgroundColor: "rgba(0,0,0,0.6)",
    alignItems: "center",
    justifyContent: "center",
    padding: S.lg,
  },
  card: {
    backgroundColor: C.surface,
    borderRadius: R.lg,
    borderWidth: 1,
    borderColor: C.border,
    padding: S.lg,
    width: "100%",
    maxWidth: 420,
  },
  title:   { fontSize: T.lg, fontWeight: "800", color: C.text, marginBottom: S.sm },
  message: { fontSize: T.md, color: C.textSub, marginBottom: S.lg },
  btnRow:  { flexDirection: "row", gap: S.sm, justifyContent: "flex-end" },
  btn:     { paddingVertical: S.sm + 2, paddingHorizontal: S.lg, borderRadius: R.md, minWidth: 96, alignItems: "center" },
  btnGhost: { backgroundColor: "transparent", borderWidth: 1, borderColor: C.border },
  btnTxt:   { color: C.white, fontWeight: "800" },
  btnGhostTxt: { color: C.text, fontWeight: "700" },
});
