import { useState } from "react";
import { KeyboardAvoidingView, Platform, ScrollView, StyleSheet, Text, View } from "react-native";
import { DEV_AUTO_LOGIN_ROLE, DEV_CREDENTIALS } from "../constants/device";
import { useAppState } from "../state/AppStateProvider";
import { C, R, S, T } from "../theme";
import Banner from "../components/ui/Banner";
import Button from "../components/ui/Button";
import { Field, Input } from "../components/ui/Field";

export default function LoginScreen() {
  const { activeUrl, setActiveUrl, login, streamMode, authLoading } = useAppState();
  const [urlDraft, setUrlDraft] = useState(activeUrl);
  const [username, setUsername] = useState(
    DEV_AUTO_LOGIN_ROLE === "admin"        ? "admin"
    : DEV_AUTO_LOGIN_ROLE === "manufacturer" ? "manufacturer"
    : DEV_AUTO_LOGIN_ROLE === "operator"     ? "operator"
    : ""
  );
  const [password, setPassword] = useState(DEV_CREDENTIALS[DEV_AUTO_LOGIN_ROLE] ?? "");
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);

  async function handleLogin() {
    setError("");
    setBusy(true);
    const url = urlDraft.trim() || activeUrl;
    if (url !== activeUrl) setActiveUrl(url);
    try {
      await login(url, username, password);
    } catch (err) {
      setError(err?.message || "Login failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <KeyboardAvoidingView behavior={Platform.OS === "ios" ? "padding" : undefined} style={{ flex: 1, backgroundColor: C.bg }}>
      <ScrollView contentContainerStyle={styles.scroll} keyboardShouldPersistTaps="handled">
        <View style={styles.card}>
          <Text style={styles.title}>LPG Filling Controller</Text>
          <Text style={styles.subtitle}>Sign in to continue</Text>

          {streamMode === "offline" && (
            <Banner
              tone="warning"
              title="Controller unreachable"
              message="On Android, .local hostnames may not resolve. Use the IP address shown on the controller serial monitor (e.g. http://192.168.1.45)."
            />
          )}

          <Field label="Controller URL">
            <Input
              value={urlDraft}
              onChangeText={setUrlDraft}
              autoCapitalize="none"
              autoCorrect={false}
              keyboardType="url"
              placeholder="http://lpg-controller.local"
              placeholderTextColor={C.muted}
            />
          </Field>

          {(() => {
            const dot = streamMode === "offline" ? C.danger : streamMode === "connecting" ? C.warning : C.ready;
            const lbl = streamMode === "offline" ? "Offline" : streamMode === "connecting" ? "Connecting..." : "Connected";
            return (
              <View style={styles.connectionRow}>
                <View style={[styles.connectionDot, { backgroundColor: dot }]} />
                <Text style={[styles.connectionText, { color: dot }]}>{lbl}</Text>
              </View>
            );
          })()}

          <Field label="Username">
            <Input
              value={username}
              onChangeText={setUsername}
              autoCapitalize="none"
              autoCorrect={false}
              placeholder="operator / admin / manufacturer"
            />
          </Field>

          <Field label="Password" error={error || undefined}>
            <Input
              value={password}
              onChangeText={setPassword}
              secureTextEntry
              autoCapitalize="none"
              placeholder="password"
              onSubmitEditing={handleLogin}
            />
          </Field>

          <View style={{ height: S.md }} />

          <Button
            label={authLoading || busy ? "Signing in..." : "Sign In"}
            variant="primary"
            size="full"
            onPress={handleLogin}
            disabled={!!authLoading || busy || !username || !password}
            loading={busy || !!authLoading}
          />
        </View>
      </ScrollView>
    </KeyboardAvoidingView>
  );
}

const styles = StyleSheet.create({
  scroll: { flexGrow: 1, justifyContent: "center", padding: S.lg },
  card: {
    backgroundColor: C.surface,
    borderRadius: R.xl,
    borderWidth: 1,
    borderColor: C.border,
    padding: S.xl,
    width: "100%",
    maxWidth: 440,
    alignSelf: "center",
  },
  title:    { fontSize: T.xl, fontWeight: "900", color: C.text, textAlign: "center" },
  subtitle: { fontSize: T.sm, color: C.textSub, textAlign: "center", marginTop: 4, marginBottom: S.lg },
  connectionRow:  { flexDirection: "row", alignItems: "center", gap: S.xs, marginTop: -S.sm, marginBottom: S.md },
  connectionDot:  { width: 7, height: 7, borderRadius: 999 },
  connectionText: { fontSize: T.xs },
});
