import { StyleSheet, View } from "react-native";
import { C, S } from "../theme";
import Button from "./ui/Button";

// Sticky action bar at the bottom of operator screens.
// Buttons are passed via slots so each screen controls its own behavior.
export default function ActionBar({
  primaryLabel,
  onPrimary,
  primaryDisabled,
  primaryDisabledReason,
  primaryVariant = "primary",
  primaryConfirm,
  primaryConfirmMessage,

  secondaryLabel,
  onSecondary,
  secondaryDisabled,
  secondaryVariant = "secondary",

  loadingPrimary,
}) {
  return (
    <View style={styles.bar}>
      {!!secondaryLabel && (
        <Button
          label={secondaryLabel}
          variant={secondaryVariant}
          size="full"
          onPress={onSecondary}
          disabled={secondaryDisabled}
          style={{ flex: 1 }}
        />
      )}
      {!!primaryLabel && (
        <Button
          label={primaryLabel}
          variant={primaryVariant}
          size="full"
          onPress={onPrimary}
          disabled={primaryDisabled}
          disabledReason={primaryDisabledReason}
          confirmLabel={primaryConfirm}
          confirmMessage={primaryConfirmMessage}
          loading={loadingPrimary}
          style={{ flex: 2 }}
        />
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  bar: {
    flexDirection: "row",
    gap: S.sm,
    paddingHorizontal: S.md,
    paddingVertical: S.md,
    backgroundColor: C.surface,
    borderTopWidth: 1,
    borderTopColor: C.border,
  },
});
