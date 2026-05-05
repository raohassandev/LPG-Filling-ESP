import { DEFAULT_DEVICE_URL } from "../constants/device";

// ── MQTT WebSocket URLs per broker preset ──────────────────────────────────
// Firmware uses plain TCP MQTT (port 1883). The app subscribes via WebSocket.
const MQTT_WS_URLS = {
  1: "wss://broker.hivemq.com:8884/mqtt",
  2: "ws://test.mosquitto.org:8080",
  3: "wss://broker.emqx.io:8084/mqtt",
};

export function mqttWsUrl(mqttSettings) {
  if (!mqttSettings) return null;
  const preset = Number(mqttSettings.preset ?? 1);
  if (preset >= 1 && preset <= 3) return MQTT_WS_URLS[preset];
  // Custom preset: derive from brokerHost + wsPort (default 8083)
  const host = mqttSettings.brokerHost;
  if (!host) return null;
  const port = mqttSettings.brokerWsPort || 8083;
  return `ws://${host}:${port}/mqtt`;
}

// ── URL helpers ────────────────────────────────────────────────────────────

export function normalizeDeviceUrl(value) {
  const trimmed = String(value || "").trim();
  if (!trimmed) return DEFAULT_DEVICE_URL;
  if (trimmed.startsWith("http://") || trimmed.startsWith("https://")) return trimmed.replace(/\/$/, "");
  return `http://${trimmed}`.replace(/\/$/, "");
}

export function websocketUrl(deviceUrl) {
  const base = normalizeDeviceUrl(deviceUrl).replace(/^http/, "ws");
  return base.replace(/(\/\/[^/:]+)(:\d+)?/, "$1:81") + "/ws";
}

// ── Core HTTP helpers ──────────────────────────────────────────────────────

function authHeaders(token) {
  if (!token) return {};
  return { Authorization: `Bearer ${token}` };
}

async function requestJson(deviceUrl, path, options = {}, token = "") {
  const headers = { ...(options.headers || {}), ...authHeaders(token) };
  const response = await fetch(`${normalizeDeviceUrl(deviceUrl)}${path}`, { ...options, headers });
  const text = await response.text();
  if (!response.ok) {
    let message = text;
    try { message = JSON.parse(text).message || message; } catch {}
    throw new Error(message || `HTTP ${response.status}`);
  }
  return text ? JSON.parse(text) : {};
}

export function getJson(deviceUrl, path, token = "") {
  return requestJson(deviceUrl, path, {}, token);
}

export function postCommand(deviceUrl, path, params = {}, token = "") {
  const query = new URLSearchParams(params).toString();
  return requestJson(deviceUrl, `${path}${query ? `?${query}` : ""}`, { method: "POST" }, token);
}

// ── Status stream — WebSocket → MQTT → REST polling ───────────────────────
//
// Connectivity priority:
//   1. WebSocket (port 81)  — low latency, local. Delivers minimal public payload
//      (state, readyToFill, emergencyStopOk) without auth; merge into full status.
//   2. MQTT over WebSocket  — works remotely via public broker (if mqttConfig given)
//   3. REST /api/status poll — full authenticated status, 1-second interval
//
// onMode values: "websocket" | "mqtt" | "polling" | "offline"
// mqttConfig: object from fetchMqttSettings() — optional, pass null to skip MQTT
// token: auth token for REST polling fallback

export function connectStatusStream(deviceUrl, onStatus, onMode, mqttConfig = null, token = "") {
  let stopped    = false;
  let socket     = null;
  let pollTimer  = null;
  let mqttClient = null;
  let wsLive     = false;

  // ── REST polling (final fallback) ──────────────────────────────────────
  const pollStatus = async () => {
    if (stopped) return;
    try {
      const status = await getJson(deviceUrl, "/api/status", token);
      onMode("polling");
      onStatus(status);
    } catch {
      onMode("offline");
    } finally {
      if (!stopped) pollTimer = setTimeout(pollStatus, 1000);
    }
  };

  // ── MQTT via WebSocket (remote fallback) ───────────────────────────────
  const tryMqtt = (wsUrl, topicPrefix) => {
    if (stopped || mqttClient) return;
    try {
      // Dynamic import so the mqtt package is only loaded when needed.
      import("mqtt").then(({ default: mqtt }) => {
        if (stopped) return;
        const topic = `${topicPrefix || "lpg/controller"}/status`;
        mqttClient = mqtt.connect(wsUrl, {
          reconnectPeriod: 5000,
          connectTimeout: 8000,
        });
        mqttClient.on("connect", () => {
          if (stopped) { mqttClient.end(true); return; }
          mqttClient.subscribe(topic);
          onMode("mqtt");
          // Cancel REST polling — MQTT is now pushing
          if (pollTimer) { clearTimeout(pollTimer); pollTimer = null; }
        });
        mqttClient.on("message", (_topic, message) => {
          try { onStatus(JSON.parse(message.toString())); } catch {}
        });
        mqttClient.on("error",  () => { if (!pollTimer && !stopped && !wsLive) pollStatus(); });
        mqttClient.on("close",  () => { if (!pollTimer && !stopped && !wsLive) pollStatus(); });
      }).catch(() => {
        if (!pollTimer && !stopped) pollStatus();
      });
    } catch {
      if (!pollTimer && !stopped) pollStatus();
    }
  };

  // ── WebSocket (local, primary) ─────────────────────────────────────────
  try {
    socket = new WebSocket(websocketUrl(deviceUrl));
    socket.onopen = () => { wsLive = true; onMode("websocket"); };
    socket.onmessage = (event) => {
      try { onStatus(JSON.parse(event.data)); } catch {}
    };
    socket.onerror = () => {
      wsLive = false;
      // Try MQTT if configured, then REST
      if (mqttConfig && mqttConfig.enabled) {
        const wsUrl = mqttWsUrl(mqttConfig);
        if (wsUrl) { tryMqtt(wsUrl, mqttConfig.topicPrefix); return; }
      }
      onMode("polling");
    };
    socket.onclose = () => {
      wsLive = false;
      if (stopped) return;
      // If MQTT already running, leave it; otherwise start REST polling
      if (!mqttClient && !pollTimer) {
        if (mqttConfig && mqttConfig.enabled) {
          const wsUrl = mqttWsUrl(mqttConfig);
          if (wsUrl) { tryMqtt(wsUrl, mqttConfig.topicPrefix); return; }
        }
        pollStatus();
      }
    };
  } catch {
    pollStatus();
  }

  // Safety: if WS hasn't opened within 2 s, start MQTT / polling
  const fallbackTimer = setTimeout(() => {
    if (!stopped && !wsLive && !pollTimer && !mqttClient) {
      if (mqttConfig && mqttConfig.enabled) {
        const wsUrl = mqttWsUrl(mqttConfig);
        if (wsUrl) { tryMqtt(wsUrl, mqttConfig.topicPrefix); return; }
      }
      pollStatus();
    }
  }, 2000);

  return () => {
    stopped = true;
    clearTimeout(fallbackTimer);
    if (pollTimer)  clearTimeout(pollTimer);
    if (socket)     socket.close();
    if (mqttClient) mqttClient.end(true);
  };
}

// ── Transaction / history ─────────────────────────────────────────────────

export async function fetchTransactions(deviceUrl, token = "") {
  const data = await requestJson(deviceUrl, "/api/transactions", {}, token);
  return Array.isArray(data.transactions) ? data.transactions : [];
}

export async function fetchTransactionsForUser(deviceUrl, username, token) {
  const path = username ? `/api/transactions?username=${encodeURIComponent(username)}` : "/api/transactions";
  const data = await requestJson(deviceUrl, path, {}, token);
  return Array.isArray(data.transactions) ? data.transactions : [];
}

// ── SD Card history ──────────────────────────────────────────────────────

export function fetchSdMonths(deviceUrl, token) {
  return requestJson(deviceUrl, "/api/sd/months", {}, token);
}

export async function fetchSdTransactions(deviceUrl, month, token) {
  const data = await requestJson(
    deviceUrl,
    `/api/sd/transactions?month=${encodeURIComponent(month)}`,
    {},
    token
  );
  return Array.isArray(data.transactions) ? data.transactions : [];
}

// ── Settings ──────────────────────────────────────────────────────────────

export function fetchSettings(deviceUrl, token = "") {
  return getJson(deviceUrl, "/api/settings", token);
}

export function saveRate(deviceUrl, ratePerKg, token) {
  return postCommand(deviceUrl, "/api/settings", { ratePerKg }, token);
}

export function saveSlowFillThreshold(deviceUrl, slowFillThreshold, token) {
  return postCommand(deviceUrl, "/api/settings", { slowFillThreshold }, token);
}

export function saveStorageMode(deviceUrl, storageMode, token) {
  return postCommand(deviceUrl, "/api/settings", { storageMode }, token);
}

export function fetchMqttSettings(deviceUrl, token) {
  return requestJson(deviceUrl, "/api/mqtt", {}, token);
}

export function saveMqttSettings(deviceUrl, fields, token) {
  return postCommand(deviceUrl, "/api/mqtt", fields, token);
}

export function fetchModbusRtu(deviceUrl, token) {
  return requestJson(deviceUrl, "/api/modbus-rtu", {}, token);
}

export function saveModbusRtu(deviceUrl, fields, token) {
  return postCommand(deviceUrl, "/api/modbus-rtu", fields, token);
}

// ── Auth ───────────────────────────────────────────────────────────────────

export function login(deviceUrl, username, password) {
  return postCommand(deviceUrl, "/api/login", { username, password });
}

export function logout(deviceUrl, token) {
  return postCommand(deviceUrl, "/api/logout", {}, token);
}

// ── Fill controls ──────────────────────────────────────────────────────────

export function startFill(deviceUrl, targetWeightKg, ratePerKg, targetAmount, token) {
  return postCommand(deviceUrl, "/api/start", { targetWeightKg, ratePerKg, targetAmount }, token);
}

export function stopFill(deviceUrl, token) {
  return postCommand(deviceUrl, "/api/stop", {}, token);
}

export function resetFill(deviceUrl, token) {
  return postCommand(deviceUrl, "/api/reset", {}, token);
}

// ── Weight / tare / calibration ───────────────────────────────────────────

export function fetchWeight(deviceUrl, token = "") {
  return getJson(deviceUrl, "/api/weight", token);
}

export function applyTare(deviceUrl, tareWeightKg, token) {
  return postCommand(deviceUrl, "/api/tare", { tareWeightKg }, token);
}

export function zeroNet(deviceUrl, token) {
  return postCommand(deviceUrl, "/api/tare-zero", {}, token);
}

export function hwTare(deviceUrl, token) {
  return postCommand(deviceUrl, "/api/tare-hw", {}, token);
}

export function calibrateKnown(deviceUrl, knownKg, token) {
  return postCommand(deviceUrl, "/api/calibrate", { knownKg }, token);
}

export function calibratePoint(deviceUrl, point, knownKg, token) {
  return postCommand(deviceUrl, "/api/calibrate", { knownKg, point }, token);
}

export function calibrateFactor(deviceUrl, factor, token) {
  return postCommand(deviceUrl, "/api/calibrate", { factor }, token);
}

// ── Users ──────────────────────────────────────────────────────────────────

export async function fetchUsers(deviceUrl, token) {
  const data = await requestJson(deviceUrl, "/api/users", {}, token);
  return Array.isArray(data.users) ? data.users : [];
}

export function createUser(deviceUrl, username, password, role, canSetRate, token) {
  return postCommand(deviceUrl, "/api/users", { username, password, role, canSetRate: canSetRate ? "1" : "0" }, token);
}

export function updateUser(deviceUrl, username, fields, token) {
  return postCommand(deviceUrl, "/api/users/update", { username, ...fields }, token);
}

export function deleteUser(deviceUrl, username, token) {
  return postCommand(deviceUrl, "/api/users/delete", { username }, token);
}

// ── System / diagnostics ───────────────────────────────────────────────────

export function fetchSystem(deviceUrl, token) {
  return requestJson(deviceUrl, "/api/system", {}, token);
}

export function fetchStats(deviceUrl, token) {
  return requestJson(deviceUrl, "/api/stats", {}, token);
}

// ── Time ───────────────────────────────────────────────────────────────────

export function fetchTime(deviceUrl, token = "") {
  return getJson(deviceUrl, "/api/time", token);
}

export function saveTime(deviceUrl, fields, token) {
  return postCommand(deviceUrl, "/api/time", fields, token);
}
