const DEFAULT_DEVICE_URL = "http://192.168.0.108";

export function normalizeDeviceUrl(value) {
  const trimmed = String(value || "").trim();
  if (!trimmed) return DEFAULT_DEVICE_URL;
  if (trimmed.startsWith("http://") || trimmed.startsWith("https://")) return trimmed.replace(/\/$/, "");
  return `http://${trimmed}`.replace(/\/$/, "");
}

export function websocketUrl(deviceUrl) {
  return normalizeDeviceUrl(deviceUrl).replace(/^http/, "ws") + "/ws";
}

export async function getJson(deviceUrl, path) {
  const response = await fetch(`${normalizeDeviceUrl(deviceUrl)}${path}`);
  const text = await response.text();
  if (!response.ok) {
    throw new Error(text || `HTTP ${response.status}`);
  }
  return text ? JSON.parse(text) : {};
}

export async function postForm(deviceUrl, path, params = {}) {
  const query = new URLSearchParams(params).toString();
  const suffix = query ? `${path}?${query}` : path;
  return getJson(deviceUrl, suffix, { method: "POST" });
}

export async function postCommand(deviceUrl, path, params = {}) {
  const query = new URLSearchParams(params).toString();
  const response = await fetch(`${normalizeDeviceUrl(deviceUrl)}${path}${query ? `?${query}` : ""}`, {
    method: "POST",
  });
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
      if (!stopped) {
        pollTimer = setTimeout(pollStatus, 1000);
      }
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
    socket.onerror = () => {
      onMode("polling");
    };
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

export async function fetchSettings(deviceUrl) {
  return getJson(deviceUrl, "/api/settings");
}

export async function startFill(deviceUrl, targetWeightKg, ratePerKg, targetAmount) {
  return postCommand(deviceUrl, "/api/start", { targetWeightKg, ratePerKg, targetAmount });
}

export async function stopFill(deviceUrl) {
  return postCommand(deviceUrl, "/api/stop");
}

export async function resetFill(deviceUrl) {
  return postCommand(deviceUrl, "/api/reset");
}

export async function applyTare(deviceUrl, tareWeightKg) {
  return postCommand(deviceUrl, "/api/tare", { tareWeightKg });
}

export async function zeroNet(deviceUrl) {
  return postCommand(deviceUrl, "/api/tare-zero");
}

export async function saveRate(deviceUrl, ratePerKg) {
  return postCommand(deviceUrl, "/api/settings", { ratePerKg });
}
