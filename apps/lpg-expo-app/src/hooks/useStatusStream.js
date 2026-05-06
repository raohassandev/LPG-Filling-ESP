import { useCallback, useEffect, useState } from "react";
import { connectStatusStream } from "../services/controllerApi";

// Encapsulates the status stream lifecycle.
// Returns the latest status object, the active stream mode, and a mergeStatus
// helper used by callers that fetch additional fields out-of-band (settings).
export function useStatusStream({ url, token, mqttConfig, enabled = true, connectionKey = 0 }) {
  const [status, setStatus] = useState({});
  const [streamMode, setStreamMode] = useState("connecting");

  useEffect(() => {
    if (!enabled || !url) return;
    const stop = connectStatusStream(url, setStatus, setStreamMode, mqttConfig, token);
    return () => stop();
  }, [url, token, mqttConfig, enabled, connectionKey]);

  const mergeStatus = useCallback((patch) => {
    if (!patch) return;
    setStatus(prev => ({ ...prev, ...patch }));
  }, []);

  return { status, streamMode, mergeStatus, setStatus };
}
