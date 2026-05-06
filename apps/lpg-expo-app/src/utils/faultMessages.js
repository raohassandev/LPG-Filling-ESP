// Map firmware fault / blocker codes to operator-readable guidance.
// Severity drives banner / icon color. Keep titles short — they appear in headers.

export const FAULT_MESSAGES = {
  emergency_stop: {
    title: "Emergency Stop Active",
    meaning: "The E-Stop circuit is open or has been pressed.",
    action: "Confirm the area is safe, then release/reset the E-Stop switch.",
    resetCondition: "Reset becomes available once the E-Stop reads OK.",
    severity: "critical",
  },
  estop_active: {
    title: "Emergency Stop Active",
    meaning: "The E-Stop input shows the circuit is open.",
    action: "Confirm the area is safe, then release/reset the E-Stop.",
    resetCondition: "Reset becomes available once the E-Stop reads OK.",
    severity: "critical",
  },
  nozzle_disengaged: {
    title: "Nozzle Disengaged",
    meaning: "The fill nozzle was removed during a fill.",
    action: "Re-engage the nozzle on the cylinder before retrying.",
    resetCondition: "Reset is allowed once the nozzle is engaged.",
    severity: "critical",
  },
  nozzle_not_engaged: {
    title: "Nozzle Not Engaged",
    meaning: "The fill nozzle is not seated on the cylinder.",
    action: "Engage the nozzle on the cylinder fitting.",
    resetCondition: "Clears automatically when nozzle engages.",
    severity: "warning",
  },
  cylinder_missing: {
    title: "No Cylinder Detected",
    meaning: "The scale does not detect a cylinder.",
    action: "Place a cylinder on the platform.",
    resetCondition: "Clears automatically when cylinder is placed.",
    severity: "warning",
  },
  pressure_invalid: {
    title: "Pressure Reading Invalid",
    meaning: "The pressure sensor returned an out-of-range value.",
    action: "Check pressure sensor wiring and supply pressure.",
    resetCondition: "Reset once pressure is back in range.",
    severity: "critical",
  },
  scale_read_error: {
    title: "Scale Read Error",
    meaning: "The HX711 weight sensor is not responding.",
    action: "Check HX711 wiring (DOUT/SCK) and load cell cable. Power-cycle the controller if needed.",
    resetCondition: "Reset once HX711 reports OK.",
    severity: "critical",
  },
  scale_not_initialized: {
    title: "Scale Not Initialized",
    meaning: "The HX711 driver did not initialize at boot.",
    action: "Check HX711 wiring and reboot the controller.",
    resetCondition: "Resolved on a clean boot with HX711 detected.",
    severity: "critical",
  },
  scale_unstable: {
    title: "Scale Unstable",
    meaning: "Weight reading is fluctuating beyond the stable threshold.",
    action: "Wait for the cylinder to settle. Check for vibration or air drafts.",
    resetCondition: "Clears automatically when readings stabilize.",
    severity: "warning",
  },
  scale_not_calibrated: {
    title: "Scale Not Calibrated",
    meaning: "No valid calibration is stored on the controller.",
    action: "A manufacturer must calibrate the scale before any fill is allowed.",
    resetCondition: "Clears once calibration completes.",
    severity: "critical",
  },
  calibration_invalid: {
    title: "Calibration Invalid",
    meaning: "The stored calibration parameters are out of range or corrupted.",
    action: "Re-run two-point calibration.",
    resetCondition: "Clears once calibration is re-applied.",
    severity: "critical",
  },
  overfill: {
    title: "Overfill Detected",
    meaning: "Net weight exceeded target plus the safety margin.",
    action: "Stop dispensing immediately. Verify the cylinder is safe and inspect the scale calibration.",
    resetCondition: "Reset only after weight has been verified.",
    severity: "critical",
  },
  no_flow: {
    title: "No Flow Detected",
    meaning: "Weight did not increase for the watchdog period after valves opened.",
    action: "Check the supply valve, pump, and hose. Confirm cylinder is connected.",
    resetCondition: "Reset and retry once the cause is fixed.",
    severity: "critical",
  },
  no_flow_detected: {
    title: "No Flow Detected",
    meaning: "Weight did not increase after valves opened.",
    action: "Check supply valve, pump, and hose connection.",
    resetCondition: "Reset and retry once the cause is fixed.",
    severity: "critical",
  },
  fill_timeout: {
    title: "Fill Timed Out",
    meaning: "The fill exceeded the allowed duration.",
    action: "Check supply pressure, valve sizing, and target weight. Reset and retry.",
    resetCondition: "Reset is allowed immediately.",
    severity: "warning",
  },
  max_fill_timeout: {
    title: "Maximum Fill Time Reached",
    meaning: "The fill ran past the absolute maximum allowed time.",
    action: "Verify the system is healthy before retrying.",
    resetCondition: "Reset is allowed immediately.",
    severity: "warning",
  },
  transaction_log_failed: {
    title: "Transaction Log Failed",
    meaning: "The controller could not write the transaction record. Storage may be full.",
    action: "Free space (rotate logs / clear SD) and retry. The fill itself completed.",
    resetCondition: "Reset is allowed; investigate storage afterwards.",
    severity: "warning",
  },
  modbus_write_rejected: {
    title: "Modbus Write Rejected",
    meaning: "A remote Modbus client tried to start a fill but firmware refused (auth or interlock).",
    action: "Check the remote client, role and interlocks.",
    resetCondition: "No reset needed — fill did not start.",
    severity: "info",
  },
  unauthorized: {
    title: "Unauthorized",
    meaning: "Your session does not have permission for this action.",
    action: "Sign in with an account that has the required role.",
    resetCondition: "—",
    severity: "info",
  },
  session_expired: {
    title: "Session Expired",
    meaning: "Your session is no longer valid.",
    action: "Sign in again to continue.",
    resetCondition: "—",
    severity: "info",
  },
  simulation_active: {
    title: "Simulation Mode Active",
    meaning: "The controller is in simulation mode. Real outputs are NOT driven.",
    action: "Disable simulation in firmware before performing any live fill.",
    resetCondition: "Cleared when firmware exits simulation mode.",
    severity: "critical",
  },
};

export function getFaultMessage(code) {
  if (!code) return null;
  return FAULT_MESSAGES[code] || {
    title: String(code).replace(/_/g, " ").toUpperCase(),
    meaning: "An unrecognized fault was reported by the controller.",
    action: "Check the controller logs and contact maintenance.",
    resetCondition: "Reset once the underlying condition is cleared.",
    severity: "warning",
  };
}

export const BLOCKER_UI = {
  active_fault:          { label: "Active fault",            guidance: "Reset fault before starting." },
  estop_active:          { label: "E-Stop active",           guidance: "Check and reset the E-stop switch." },
  cylinder_missing:      { label: "Cylinder not detected",   guidance: "Place cylinder on the scale." },
  nozzle_not_engaged:    { label: "Nozzle not engaged",      guidance: "Connect nozzle before starting." },
  scale_not_initialized: { label: "Scale not initialized",   guidance: "Check HX711 wiring and reboot." },
  scale_read_error:      { label: "Scale read error",        guidance: "Check HX711 connection." },
  scale_unstable:        { label: "Scale unstable",          guidance: "Wait for weight to stabilize." },
  scale_not_calibrated:  { label: "Scale not calibrated",    guidance: "Calibrate scale before filling." },
  simulation_active:     { label: "Simulation active",       guidance: "Disable simulation before live fill." },
};

export function getBlockerUI(code) {
  return BLOCKER_UI[code] || { label: String(code || "Unknown blocker"), guidance: "" };
}
