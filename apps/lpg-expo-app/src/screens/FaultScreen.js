import { useState } from "react";
import { ScrollView, StyleSheet, View } from "react-native";
import { C, S } from "../theme";
import { useAppState } from "../state/AppStateProvider";
import { resetFill } from "../services/controllerApi";
import FaultRecoveryCard from "../components/FaultRecoveryCard";
import SafetyBanner from "../components/SafetyBanner";
import StatusHeader from "../components/StatusHeader";
import { getFaultMessage } from "../utils/faultMessages";

export default function FaultScreen() {
  const { status, streamMode, activeUrl, authToken, authRole, authUsername, logout } = useAppState();
  const [busy, setBusy] = useState(false);
  const fault = getFaultMessage(status?.reasonCode || status?.faultCode);
  const hasReadyToReset = status?.readyToReset !== undefined;
  const showReset = !hasReadyToReset || !!status.readyToReset || fault?.severity !== "critical";

  async function handleReset() {
    setBusy(true);
    try { await resetFill(activeUrl, authToken); }
    catch (err) { showAlert("Error", err?.message || "Reset failed"); }
    finally { setBusy(false); }
  }

  return (
    <View style={styles.shell}>
      <SafetyBanner status={status} streamMode={streamMode} />
      <StatusHeader
        status={status}
        streamMode={streamMode}
        authRole={authRole}
        authUsername={authUsername}
        onSwitchAccount={logout}
      />
      <ScrollView contentContainerStyle={styles.content}>
        <FaultRecoveryCard status={status} onReset={handleReset} busy={busy} showReset={showReset} />
      </ScrollView>
    </View>
  );
}

function showAlert(title, msg) {
  try { require("react-native").Alert.alert(title, msg); } catch {}
}

const styles = StyleSheet.create({
  shell:   { flex: 1, backgroundColor: C.bg },
  content: { padding: S.md, paddingBottom: 40 },
});
