import { StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../theme";
import { getFaultMessage } from "../utils/faultMessages";
import Button from "./ui/Button";

export default function FaultRecoveryCard({ status, onReset, busy, showReset = true }) {
  const code = status?.reasonCode || "fault";
  const f = getFaultMessage(code) || {};
  const sev = f.severity || "critical";
  const headerColor = sev === "critical" ? C.danger : sev === "warning" ? C.warning : C.active;

  // Firmware should expose canReset / readyToReset; fall back to allowing manual reset.
  // The Reset button itself defers to firmware (which may still reject).
  const canReset = status?.readyToReset !== undefined
    ? !!status.readyToReset || sev !== "critical"
    : status?.canReset !== undefined
      ? !!status.canReset
      : true;

  return (
    <View style={styles.card}>
      <View style={[styles.header, { backgroundColor: headerColor }]}>
        <Text style={styles.headerCode}>{String(code).replace(/_/g, " ").toUpperCase()}</Text>
        <Text style={styles.headerTitle}>{f.title || "Fault"}</Text>
      </View>

      <View style={styles.body}>
        <Section label="What happened" text={f.meaning} />
        <Section label="Do now" text={f.action} accent />
        <Section label="Reset when" text={f.resetCondition} />

        {showReset && (
          <View style={{ marginTop: S.md }}>
            <Button
              label={busy ? "Resetting..." : "Reset to Idle"}
              variant="danger"
              size="full"
              disabled={!canReset || busy}
              disabledReason={!canReset ? "Firmware will allow reset once conditions are met." : undefined}
              confirmLabel="Reset the controller to idle?"
              confirmMessage="Only do this after the underlying condition is cleared."
              onPress={onReset}
              loading={busy}
            />
          </View>
        )}
      </View>
    </View>
  );
}

function Section({ label, text, accent }) {
  if (!text) return null;
  return (
    <View style={styles.section}>
      <Text style={styles.sectionLabel}>{label}</Text>
      <Text style={[styles.sectionText, accent && { color: C.warning, fontWeight: "700" }]}>{text}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: C.surface,
    borderRadius: R.lg,
    borderWidth: 2,
    borderColor: C.danger,
    overflow: "hidden",
    marginBottom: S.md,
  },
  header: { paddingVertical: S.md, paddingHorizontal: S.lg },
  headerCode: { color: C.white, fontSize: T.xs, fontWeight: "800", letterSpacing: 1 },
  headerTitle: { color: C.white, fontSize: T.xl, fontWeight: "900", marginTop: 2 },
  body: { padding: S.lg },
  section: { marginBottom: S.sm },
  sectionLabel: {
    color: C.textSub, fontSize: T.xs, fontWeight: "800",
    letterSpacing: 0.7, textTransform: "uppercase", marginBottom: 2,
  },
  sectionText: { color: C.text, fontSize: T.md, lineHeight: 22 },
});
