import { createContext, useContext, useEffect, useMemo, useReducer } from "react";
import { DEFAULT_DEVICE_URL } from "../constants/device";
import { connectStatusStream, fetchSettings, fetchTransactions, normalizeDeviceUrl } from "../services/controllerApi";
import { money } from "../utils/format";
import { summarizeTransactions } from "../utils/reports";

const initialState = {
  deviceUrlInput: DEFAULT_DEVICE_URL,
  activeUrl: DEFAULT_DEVICE_URL,
  role: "operator",
  fillMode: "weight",
  streamMode: "connecting",
  reportPeriod: "today",
  status: {},
  transactions: [],
  form: {
    tareWeight: "0.00",
    targetWeight: "11.80",
    targetAmount: "2950.00",
    rate: "250.00",
    adminRate: "250.00",
  },
};

const AppStateContext = createContext(null);

function reducer(state, action) {
  switch (action.type) {
    case "SET_DEVICE_INPUT":
      return { ...state, deviceUrlInput: action.value };
    case "CONNECT_DEVICE":
      return { ...state, activeUrl: normalizeDeviceUrl(state.deviceUrlInput), streamMode: "connecting" };
    case "SET_ROLE":
      return { ...state, role: action.role };
    case "SET_FILL_MODE":
      return { ...state, fillMode: action.mode };
    case "SET_STREAM_MODE":
      return { ...state, streamMode: action.mode };
    case "SET_REPORT_PERIOD":
      return { ...state, reportPeriod: action.period };
    case "SET_STATUS":
      return {
        ...state,
        status: action.status,
        form: {
          ...state.form,
          tareWeight: state.form.tareWeight === "" ? "" : money(action.status.tareWeightKg),
          rate: state.form.rate === "" ? "" : money(action.status.ratePerKg),
          adminRate: state.form.adminRate === "" ? "" : money(action.status.ratePerKg),
        },
      };
    case "SET_TRANSACTIONS":
      return { ...state, transactions: action.transactions };
    case "PATCH_FORM":
      return { ...state, form: { ...state.form, ...action.patch } };
    default:
      return state;
  }
}

export function AppStateProvider({ children }) {
  const [state, dispatch] = useReducer(reducer, initialState);

  useEffect(() => {
    const stop = connectStatusStream(
      state.activeUrl,
      (status) => dispatch({ type: "SET_STATUS", status }),
      (mode) => dispatch({ type: "SET_STREAM_MODE", mode })
    );

    const refreshSlowData = async () => {
      try {
        const [transactions, settings] = await Promise.all([
          fetchTransactions(state.activeUrl),
          fetchSettings(state.activeUrl),
        ]);
        dispatch({ type: "SET_TRANSACTIONS", transactions });
        if (settings.ratePerKg) {
          dispatch({
            type: "PATCH_FORM",
            patch: { rate: money(settings.ratePerKg), adminRate: money(settings.ratePerKg) },
          });
        }
      } catch {}
    };

    refreshSlowData();
    const timer = setInterval(refreshSlowData, 5000);
    return () => {
      stop();
      clearInterval(timer);
    };
  }, [state.activeUrl]);

  const value = useMemo(() => {
    const ready = Boolean(state.status.nozzleEngaged && state.status.cylinderPresent && state.status.emergencyStopOk);
    const summary = summarizeTransactions(state.transactions, state.reportPeriod);
    return { state, dispatch, ready, summary };
  }, [state]);

  return <AppStateContext.Provider value={value}>{children}</AppStateContext.Provider>;
}

export function useAppState() {
  const context = useContext(AppStateContext);
  if (!context) {
    throw new Error("useAppState must be used inside AppStateProvider");
  }
  return context;
}
