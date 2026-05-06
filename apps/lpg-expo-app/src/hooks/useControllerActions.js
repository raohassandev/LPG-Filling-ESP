import { useCallback, useState } from "react";
import { Platform } from "react-native";
import {
  applyTare as apiApplyTare,
  hwTare as apiHwTare,
  resetFill as apiResetFill,
  startFill as apiStartFill,
  stopFill as apiStopFill,
  zeroNet as apiZeroNet,
} from "../services/controllerApi";
import { useAppState } from "../state/AppStateProvider";

function showAlert(title, msg) {
  if (Platform.OS === "web") { try { window.alert(`${title}\n${msg}`); } catch {} return; }
  try { require("react-native").Alert.alert(title, msg); } catch {}
}

// Encapsulates command-style actions against the controller.
// Surface-level error handling shows an Alert; callers can pass onError to override.
export function useControllerActions() {
  const { activeUrl, authToken } = useAppState();
  const [busy, setBusy] = useState(false);

  const run = useCallback(async (fn, { successMsg, onError } = {}) => {
    setBusy(true);
    try {
      const r = await fn();
      if (successMsg) showAlert("Done", successMsg);
      return r;
    } catch (err) {
      if (onError) onError(err);
      else showAlert("Error", err?.message || "Command failed");
      throw err;
    } finally {
      setBusy(false);
    }
  }, []);

  const startFill = useCallback((targetWeightKg, ratePerKg, targetAmount, opts) =>
    run(() => apiStartFill(activeUrl, targetWeightKg, ratePerKg, targetAmount, authToken), opts),
    [activeUrl, authToken, run]);

  const stopFill = useCallback((opts) =>
    run(() => apiStopFill(activeUrl, authToken), opts),
    [activeUrl, authToken, run]);

  const resetFill = useCallback((opts) =>
    run(() => apiResetFill(activeUrl, authToken), opts),
    [activeUrl, authToken, run]);

  const applyTare = useCallback((kgValue, opts) =>
    run(() => apiApplyTare(activeUrl, kgValue, authToken), opts),
    [activeUrl, authToken, run]);

  const zeroNet = useCallback((opts) =>
    run(() => apiZeroNet(activeUrl, authToken), opts),
    [activeUrl, authToken, run]);

  const hwTare = useCallback((opts) =>
    run(() => apiHwTare(activeUrl, authToken), opts),
    [activeUrl, authToken, run]);

  return { busy, run, startFill, stopFill, resetFill, applyTare, zeroNet, hwTare };
}
