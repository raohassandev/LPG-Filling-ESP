export const DEFAULT_DEVICE_URL = "http://lpg-controller.local";

// Dev/testing credentials — remove this block when deploying to production
export const DEV_AUTO_LOGIN_ROLE = "operator";
export const DEV_CREDENTIALS = {
  operator:    "1234",
  maintenance: "5678",
  admin:       "0000",
};

export const RELAYS = [
  { name: "Relay 1", terminal: "Output 1", purpose: "Fast fill valve" },
  { name: "Relay 2", terminal: "Output 2", purpose: "Slow fill valve" },
  { name: "Relay 3", terminal: "Output 3", purpose: "Main supply valve" },
  { name: "Relay 4", terminal: "Output 4", purpose: "Pump / compressor" },
  { name: "Relay 5", terminal: "Output 5", purpose: "Alarm horn / beacon" },
  { name: "Relay 6", terminal: "Output 6", purpose: "Status indicator" },
];

export const ROLES = ["operator", "admin", "manufacturer"];
export const REPORT_PERIODS = ["today", "week", "month", "year", "all"];
