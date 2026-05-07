const fs   = require("fs");
const path = require("path");

const root   = path.resolve(__dirname, "..");
const srcDir = path.join(root, "src");

// Read a file relative to src/
const r = (...parts) => fs.readFileSync(path.join(srcDir, ...parts), "utf8");

// Grep src/ recursively for a pattern; returns true if found anywhere
function grepSrc(pattern) {
  const re = typeof pattern === "string" ? new RegExp(pattern) : pattern;
  function walk(dir) {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) { if (walk(full)) return true; }
      else if (entry.name.endsWith(".js") && re.test(fs.readFileSync(full, "utf8"))) return true;
    }
    return false;
  }
  return walk(srcDir);
}

// Hex colors outside theme.js (fail if any found)
function hexColorCount() {
  let count = 0;
  const re = /#[0-9a-fA-F]{3,6}\b/g;
  function walk(dir) {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) { walk(full); }
      else if (entry.name.endsWith(".js") && entry.name !== "theme.js") {
        const m = fs.readFileSync(full, "utf8").match(re);
        if (m) { console.error(`  hex colors in ${path.relative(srcDir, full)}: ${m.join(", ")}`); count += m.length; }
      }
    }
  }
  walk(srcDir);
  return count;
}

const api = r("services", "controllerApi.js");

const checks = [
  [grepSrc("netWeightKg|NetWeight|net_weight"),           "src/ must reference net weight"],
  [grepSrc("targetWeightKg|TargetWeight|target_weight"),  "src/ must reference target weight"],
  [grepSrc("fillMode|fill_mode|\"amount\"|\"weight\""),   "src/ must reference fill mode"],
  [grepSrc("ratePerKg|rate_per_kg|adminRate"),            "src/ must reference rate per kg"],
  [grepSrc("StatusHeader"),                               "StatusHeader must be used in src/"],
  [grepSrc("BottomNav"),                                  "BottomNav must be used in src/"],
  [api.includes("new WebSocket"),                         "API client must support WebSocket"],
  [api.includes("pollStatus"),                            "API client must include HTTP polling fallback"],
  [api.includes("/api/tare-zero"),                        "API client must support tare-zero endpoint"],
  [api.includes("/api/calibrate"),                        "API client must support calibrate endpoint"],
];

const hexCount = hexColorCount();
const failed   = checks.filter(([ok]) => !ok);

if (hexCount > 0) { console.error(`\n${hexCount} hardcoded hex color(s) found outside theme.js - use C.* tokens.\n`); }
if (failed.length) { failed.forEach(([, msg]) => console.error(msg)); }

if (hexCount > 0 || failed.length) process.exit(1);
console.log("All checks passed.");
