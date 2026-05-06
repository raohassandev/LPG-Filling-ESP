import { StatusBar } from "expo-status-bar";
import { SafeAreaView, StyleSheet, Text, View } from "react-native";
import { AppStateProvider, useAppState } from "./src/state/AppStateProvider";
import { C } from "./src/theme";

// Screens
import LoginScreen           from "./src/screens/LoginScreen";
import OperatorDashboard     from "./src/screens/OperatorDashboard";
import FillProgressScreen    from "./src/screens/FillProgressScreen";
import FillCompleteScreen    from "./src/screens/FillCompleteScreen";
import FaultScreen           from "./src/screens/FaultScreen";
import TransactionsScreen    from "./src/screens/TransactionsScreen";
import AdminUsersScreen      from "./src/screens/AdminUsersScreen";
import NetworkSettingsScreen from "./src/screens/NetworkSettingsScreen";
import CalibrationScreen     from "./src/screens/CalibrationScreen";
import DiagnosticsScreen     from "./src/screens/DiagnosticsScreen";

function getPhase(state) {
  if (!state || state === "IDLE") return "idle";
  if (["FILLING_FAST", "FILLING_SLOW", "SETTLING"].includes(state)) return "filling";
  if (state === "COMPLETE") return "complete";
  if (state === "FAULT")    return "fault";
  return "idle";
}

function AppRouter() {
  const { authToken, authLoading, status, screen } = useAppState();
  const phase = getPhase(status.state);

  if (authLoading) {
    return (
      <View style={styles.center}>
        <Text style={styles.loadingTxt}>Connecting…</Text>
      </View>
    );
  }

  if (!authToken) return <LoginScreen />;

  // Firmware-driven phases always override manual navigation
  if (phase === "filling")  return <FillProgressScreen />;
  if (phase === "complete") return <FillCompleteScreen />;
  if (phase === "fault")    return <FaultScreen />;

  // Manual navigation (idle phase only)
  switch (screen) {
    case "transactions":  return <TransactionsScreen />;
    case "admin-users":   return <AdminUsersScreen />;
    case "network":       return <NetworkSettingsScreen />;
    case "calibration":   return <CalibrationScreen />;
    case "diagnostics":   return <DiagnosticsScreen />;
    default:              return <OperatorDashboard />;
  }
}

export default function App() {
  return (
    <AppStateProvider>
      <SafeAreaView style={styles.safe}>
        <StatusBar style="light" />
        <AppRouter />
      </SafeAreaView>
    </AppStateProvider>
  );
}

const styles = StyleSheet.create({
  safe:       { flex: 1, backgroundColor: C.bg },
  center:     { flex: 1, alignItems: "center", justifyContent: "center", backgroundColor: C.bg },
  loadingTxt: { color: C.textSub, fontSize: 15 },
});
