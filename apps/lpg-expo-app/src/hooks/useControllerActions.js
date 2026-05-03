import { Alert } from "react-native";
import { applyTare, resetFill, saveRate, startFill, stopFill, zeroNet } from "../services/controllerApi";
import { kg, money } from "../utils/format";
import { useAppState } from "../state/AppStateProvider";

export function useControllerActions() {
  const { state, dispatch } = useAppState();

  const patchForm = (patch) => dispatch({ type: "PATCH_FORM", patch });

  function syncTargets(nextMode = state.fillMode, changed, value) {
    const rateValue = Number(state.form.rate || 0);
    if (rateValue <= 0) return;

    if (nextMode === "amount" || changed === "amount") {
      const amount = Number(changed === "amount" ? value : state.form.targetAmount);
      patchForm({
        targetWeight: kg(amount / rateValue),
        ...(changed === "amount" ? { targetAmount: value } : {}),
      });
      return;
    }

    const weight = Number(changed === "weight" ? value : state.form.targetWeight);
    patchForm({
      targetAmount: money(weight * rateValue),
      ...(changed === "weight" ? { targetWeight: value } : {}),
    });
  }

  function setFillMode(mode) {
    dispatch({ type: "SET_FILL_MODE", mode });
    syncTargets(mode);
  }

  async function runCommand(action, success) {
    try {
      await action();
      Alert.alert("Done", success);
    } catch (error) {
      Alert.alert("Rejected", error.message);
    }
  }

  return {
    patchForm,
    setFillMode,
    syncTargets,
    applyTare: () => runCommand(() => applyTare(state.activeUrl, state.form.tareWeight), "Tare saved"),
    zeroNet: () => runCommand(() => zeroNet(state.activeUrl), "Net weight zeroed"),
    startFill: () =>
      runCommand(
        () => startFill(state.activeUrl, state.form.targetWeight, state.form.rate, state.form.targetAmount),
        "Fill started"
      ),
    stopFill: () => runCommand(() => stopFill(state.activeUrl), "Fill stopped"),
    resetFill: () => runCommand(() => resetFill(state.activeUrl), "Controller reset"),
    saveRate: () => runCommand(() => saveRate(state.activeUrl, state.form.adminRate), "Rate saved"),
  };
}
