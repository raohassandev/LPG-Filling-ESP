import { DEFAULT_DEVICE_URL } from "../constants/device";

// Sentinel returned when the device firmware has no /api/login (old firmware).
// postCommand skips appending the token when this value is passed.
export const NO_AUTH_TOKEN = "__open__";

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

async function requestJson(deviceUrl, path, options) {
  const response = await fetch(`${normalizeDeviceUrl(deviceUrl)}${path}`, options);
  const text = await response.text();
  if (!response.ok) {
    let message = text;
    try {
      message = JSON.parse(text).message || message;
    } catch {}
    throw new Error(message || `HTTP ${response.status}`);
  }
  return text ? JSON.parse(text) : {};
}

export function getJson(deviceUrl, path) {
  return requestJson(deviceUrl, path);
}

export function postCommand(deviceUrl, path, params = {}, token = "") {
  const allParams = (token && token !== NO_AUTH_TOKEN) ? { ...params, token } : params;
  const query = new URLSearchParams(allParams).toString();
  return requestJson(deviceUrl, `${path}${query ? `?${query}` : ""}`, { method: "POST" });
}

export function connectStatusStream(deviceUrl, onStatus, onMode) {
  let stopped = false;
  let socket = null;
  let pollTimer = null;

  const pollStatus = async () => {
    if (stopped) return;
    try {
      const status = await getJson(deviceUrl, "/api/status");
      onMode("polling");
      onStatus(status);
    } catch {
      onMode("offline");
    } finally {
      if (!stopped) pollTimer = setTimeout(pollStatus, 1000);
    }
  };

  try {
    socket = new WebSocket(websocketUrl(deviceUrl));
    socket.onopen = () => onMode("websocket");
    socket.onmessage = (event) => {
      try {
        onStatus(JSON.parse(event.data));
      } catch {}
    };
    socket.onerror = () => onMode("polling");
    socket.onclose = () => {
      if (!stopped && !pollTimer) pollStatus();
    };
  } catch {
    pollStatus();
  }

  const fallbackTimer = setTimeout(() => {
    if (!stopped && !pollTimer) pollStatus();
  }, 1200);

  return () => {
    stopped = true;
    clearTimeout(fallbackTimer);
    if (pollTimer) clearTimeout(pollTimer);
    if (socket) socket.close();
  };
}

export async function fetchTransactions(deviceUrl) {
  const data = await getJson(deviceUrl, "/api/transactions");
  return Array.isArray(data.transactions) ? data.transactions : [];
}

export function fetchSettings(deviceUrl) {
  return getJson(deviceUrl, "/api/settings");
}

export async function login(deviceUrl, username, password) {
  try {
    return await postCommand(deviceUrl, "/api/login", { username, password });
  } catch (err) {
    // Firmware without /api/login — treat device as open-access
    if (/not found|404/i.test(err.message)) {
      return { token: NO_AUTH_TOKEN, role: "operator" };
    }
    throw err;
  }
}

export function logout(deviceUrl, token) {
  return postCommand(deviceUrl, "/api/logout", {}, token);
}

export function startFill(deviceUrl, targetWeightKg, ratePerKg, targetAmount, token) {
  return postCommand(deviceUrl, "/api/start", { targetWeightKg, ratePerKg, targetAmount }, token);
}

export function stopFill(deviceUrl, token) {
  return postCommand(deviceUrl, "/api/stop", {}, token);
}

export function resetFill(deviceUrl, token) {
  return postCommand(deviceUrl, "/api/reset", {}, token);
}

export function applyTare(deviceUrl, tareWeightKg, token) {
  return postCommand(deviceUrl, "/api/tare", { tareWeightKg }, token);
}

export function zeroNet(deviceUrl, token) {
  return postCommand(deviceUrl, "/api/tare-zero", {}, token);
}

export function saveRate(deviceUrl, ratePerKg, token) {
  return postCommand(deviceUrl, "/api/settings", { ratePerKg }, token);
}

export function fetchWeight(deviceUrl) {
  return getJson(deviceUrl, "/api/weight");
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
