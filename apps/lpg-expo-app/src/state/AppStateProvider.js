import { createContext, useCallback, useContext, useEffect, useMemo, useState } from "react";
import {
  fetchMqttSettings,
  fetchSettings,
  fetchTransactionsForUser,
  login as apiLogin,
  logout as apiLogout,
} from "../services/controllerApi";
import { DEFAULT_DEVICE_URL, DEV_AUTO_LOGIN_ROLE, DEV_CREDENTIALS } from "../constants/device";
import { useStatusStream } from "../hooks/useStatusStream";

const AppStateContext = createContext(null);

export function AppStateProvider({ children }) {
  const [screen, setScreen]           = useState("dashboard");
  const [activeUrl, setActiveUrl]     = useState(DEFAULT_DEVICE_URL);
  const [connectionKey, setConnKey]   = useState(0);
  const [authToken, setAuthToken]     = useState("");
  const [authRole, setAuthRole]       = useState("operator");
  const [authUsername, setAuthUser]   = useState("");
  const [authCanSetRate, setCanRate]  = useState(false);
  const [authLoading, setAuthLoading] = useState(true);
  const [mqttConfig, setMqttConfig]   = useState(null);
  const [transactions, setTransactions] = useState([]);

  // Live status stream — encapsulated in hook
  const { status, streamMode, mergeStatus } = useStatusStream({
    url: activeUrl,
    token: authToken,
    mqttConfig,
    enabled: !!authToken || streamModeEnabledWithoutAuth(),
    connectionKey,
  });

  // Apply session result from /api/login
  const applySession = useCallback((r) => {
    setAuthToken(r?.token || "");
    setAuthRole(r?.role || "operator");
    setAuthUser(r?.username || "");
    setCanRate(!!r?.canSetRate || r?.role === "admin" || r?.role === "manufacturer");
  }, []);

  const login = useCallback(async (url, username, password) => {
    if (url && url !== activeUrl) setActiveUrl(url);
    const r = await apiLogin(url || activeUrl, username, password);
    applySession(r);
    return r;
  }, [activeUrl, applySession]);

  const logout = useCallback(async () => {
    if (authToken) { try { await apiLogout(activeUrl, authToken); } catch {} }
    setAuthToken("");
    setAuthRole("operator");
    setAuthUser("");
    setCanRate(false);
  }, [activeUrl, authToken]);

  const reconnect = useCallback(() => {
    setConnKey(k => k + 1);
    setAuthToken("");
  }, []);

  const navigate = useCallback((dest) => setScreen(dest || "dashboard"), []);

  // Auto-login for dev builds
  useEffect(() => {
    if (!DEV_AUTO_LOGIN_ROLE) { setAuthLoading(false); return; }
    setAuthLoading(true);
    apiLogin(activeUrl, DEV_AUTO_LOGIN_ROLE, DEV_CREDENTIALS[DEV_AUTO_LOGIN_ROLE] ?? "")
      .then((r) => { applySession(r); })
      .catch(() => {})
      .finally(() => setAuthLoading(false));
  }, [activeUrl, connectionKey, applySession]);

  // Fetch MQTT config once after login (used by status stream for remote fallback)
  useEffect(() => {
    if (!authToken) return;
    fetchMqttSettings(activeUrl, authToken)
      .then(setMqttConfig)
      .catch(() => {});
  }, [activeUrl, authToken]);

  // Periodic settings + transactions refresh
  useEffect(() => {
    if (!authToken) return;
    let alive = true;
    const refresh = async () => {
      try {
        const txns = await fetchTransactionsForUser(activeUrl, "", authToken);
        if (alive) setTransactions(txns);
        const s = await fetchSettings(activeUrl, authToken);
        if (!alive) return;
        mergeStatus({
          ...(s.ratePerKg         != null && { ratePerKg: s.ratePerKg }),
          ...(s.slowFillThreshold != null && { slowFillThreshold: s.slowFillThreshold }),
          ...(s.storageMode       != null && { storageMode: s.storageMode }),
          ...(s.sdReady           != null && { sdReady: s.sdReady }),
          ...(s.sdTotalKb         != null && { sdTotalKb: s.sdTotalKb }),
          ...(s.sdFreeKb          != null && { sdFreeKb: s.sdFreeKb }),
        });
      } catch {}
    };
    refresh();
    const t = setInterval(refresh, 5000);
    return () => { alive = false; clearInterval(t); };
  }, [activeUrl, authToken, connectionKey, mergeStatus]);

  const value = useMemo(() => ({
    screen,
    navigate,
    activeUrl,
    setActiveUrl,
    authToken,
    authRole,
    authUsername,
    authCanSetRate,
    authLoading,
    status,
    streamMode,
    mqttConfig,
    transactions,
    login,
    logout,
    reconnect,
    applySession,
  }), [screen, navigate, activeUrl, authToken, authRole, authUsername, authCanSetRate, authLoading,
       status, streamMode, mqttConfig, transactions, login, logout, reconnect, applySession]);

  return <AppStateContext.Provider value={value}>{children}</AppStateContext.Provider>;
}

function streamModeEnabledWithoutAuth() {
  // Allow public WS/REST status before login so the LoginScreen can show offline state.
  return true;
}

export function useAppState() {
  const ctx = useContext(AppStateContext);
  if (!ctx) throw new Error("useAppState must be used inside AppStateProvider");
  return ctx;
}
