const fs = require("fs");
const path = require("path");

const root = path.resolve(__dirname, "..");
const app = fs.readFileSync(path.join(root, "App.js"), "utf8");
const api = fs.readFileSync(path.join(root, "src", "api.js"), "utf8");

const checks = [
  [app.includes("Zero Net"), "Operator UI must include Zero Net"],
  [app.includes("By Amount"), "Operator UI must include amount mode"],
  [app.includes("By Weight"), "Operator UI must include weight mode"],
  [app.includes("adminRate"), "Admin UI must include rate input"],
  [api.includes("new WebSocket"), "API client must support WebSocket"],
  [api.includes("pollStatus"), "API client must include HTTP polling fallback"],
  [api.includes("/api/tare-zero"), "API client must support tare-zero endpoint"],
];

const failed = checks.filter(([ok]) => !ok);
if (failed.length) {
  failed.forEach(([, message]) => console.error(message));
  process.exit(1);
}

console.log("Expo app checks passed.");
