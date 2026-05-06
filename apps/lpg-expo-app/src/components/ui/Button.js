import { useState } from "react";
import { ActivityIndicator, Platform, Pressable, StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../../theme";
import ConfirmModal from "./ConfirmModal";

// variants: primary | danger | secondary | ghost
// size: sm | md | lg | full
export default function Button({
  label,
  onPress,
  variant = "primary",
  size = "md",
  disabled = false,
  disabledReason,
  loading = false,
  confirmLabel,
  confirmMessage,
  style,
  testID,
}) {
  const [showConfirm, setShowConfirm] = useState(false);

  const handlePress = () => {
    if (disabled || loading) return;
    if (confirmLabel) {
      setShowConfirm(true);
      return;
    }
    onPress && onPress();
  };

  const palette = VARIANTS[variant] || VARIANTS.primary;
  const sizing  = SIZES[size]       || SIZES.md;

  return (
    <>
      <Pressable
        testID={testID}
        onPress={handlePress}
        disabled={disabled || loading}
        style={({ pressed }) => [
          styles.base,
          { backgroundColor: palette.bg, borderColor: palette.border },
          sizing.box,
          (pressed && !disabled) && { opacity: 0.85 },
          (disabled || loading) && styles.disabled,
          style,
        ]}
      >
        {loading
          ? <ActivityIndicator color={palette.fg} />
          : <Text style={[styles.label, sizing.text, { color: palette.fg }]}>{label}</Text>}
      </Pressable>

      {disabled && disabledReason ? (
        <Text style={styles.disabledReason}>{disabledReason}</Text>
      ) : null}

      {confirmLabel && (
        <ConfirmModal
          visible={showConfirm}
          title={confirmLabel}
          message={confirmMessage}
          confirmLabel="Confirm"
          variant={variant === "danger" ? "danger" : "primary"}
          onConfirm={() => { setShowConfirm(false); onPress && onPress(); }}
          onCancel={() => setShowConfirm(false)}
        />
      )}
    </>
  );
}

const VARIANTS = {
  primary:   { bg: C.primary, fg: C.white,   border: C.primary },
  danger:    { bg: C.danger,  fg: C.white,   border: C.danger },
  secondary: { bg: C.surface2,fg: C.text,    border: C.border },
  ghost:     { bg: "transparent", fg: C.text, border: C.border },
};

const SIZES = {
  sm:   { box: { paddingVertical: S.xs + 2, paddingHorizontal: S.md, borderRadius: R.md }, text: { fontSize: T.sm } },
  md:   { box: { paddingVertical: S.sm + 2, paddingHorizontal: S.lg, borderRadius: R.md }, text: { fontSize: T.md } },
  lg:   { box: { paddingVertical: S.md + 2, paddingHorizontal: S.lg, borderRadius: R.lg }, text: { fontSize: T.lg, fontWeight: "800" } },
  full: { box: { paddingVertical: S.md + 2, paddingHorizontal: S.lg, borderRadius: R.lg, alignSelf: "stretch", width: "100%" }, text: { fontSize: T.lg, fontWeight: "800" } },
};

const styles = StyleSheet.create({
  base: {
    alignItems: "center",
    justifyContent: "center",
    borderWidth: 1,
    ...(Platform.OS === "web" ? { cursor: "pointer" } : {}),
  },
  label: { fontWeight: "700", letterSpacing: 0.2 },
  disabled: { opacity: 0.45 },
  disabledReason: {
    fontSize: T.xs,
    color: C.textSub,
    marginTop: S.xs,
    textAlign: "center",
  },
});
