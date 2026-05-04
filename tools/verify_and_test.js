#!/usr/bin/env node
// Verify board state and run a full fill cycle test
// Auto-discovers the board on the local network.

const os = require("os");

// ── Discovery ────────────────────────────────────────────────────────────────

async function probeUrl(url) {
  try {
    const r = await fetch(`${url}/api/status`, { signal: AbortSignal.timeout(1500) });
    if (!r.ok) return null;
    const j = await r.json();
    return typeof j.state === "string" ? url : null;
  } catch {
    return null;
  }
}

function localSubnets() {
  const subnets = new Set();
  for (const ifaces of Object.values(os.networkInterfaces())) {
    for (const iface of ifaces) {
      if (iface.family === "IPv4" && !iface.internal) {
        const parts = iface.address.split(".");
        subnets.add(`${parts[0]}.${parts[1]}.${parts[2]}`);
      }
    }
  }
  return [...subnets];
}

async function scanSubnet(subnet) {
  const batch = 30;
  for (let start = 1; start <= 254; start += batch) {
    const promises = [];
    for (let i = start; i < start + batch && i <= 254; i++) {
      promises.push(probeUrl(`http://${subnet}.${i}`));
    }
    const results = await Promise.all(promises);
    const found = results.find(Boolean);
    if (found) return found;
  }
  return null;
}

async function findBoard() {
  // 1. Try env override
  if (process.env.DEVICE_URL) {
    console.log(`  Using DEVICE_URL: ${process.env.DEVICE_URL}`);
    return process.env.DEVICE_URL;
  }

  // 2. Try mDNS hostname
  const mdns = await probeUrl("http://lpg-controller.local");
  if (mdns) { console.log("  Found via mDNS: lpg-controller.local"); return mdns; }

  // 3. Scan each local subnet
  const subnets = localSubnets();
  console.log(`  Scanning subnets: ${subnets.join(", ")} ...`);
  for (const subnet of subnets) {
    const url = await scanSubnet(subnet);
    if (url) { console.log(`  Found board at: ${url}`); return url; }
  }

  return null;
}

// ── Helpers ──────────────────────────────────────────────────────────────────

let BASE;
let TOKEN = "";

async function get(path) {
  const r = await fetch(`${BASE}${path}`, { signal: AbortSignal.timeout(5000) });
  if (!r.ok) throw new Error(`GET ${path} => ${r.status}`);
  return r.json();
}

async function post(path, params = {}) {
  const allParams = (TOKEN && TOKEN !== "__open__") ? { ...params, token: TOKEN } : params;
  const q = new URLSearchParams(allParams).toString();
  const url = `${BASE}${path}${q ? "?" + q : ""}`;
  const r = await fetch(url, { method: "POST", signal: AbortSignal.timeout(5000) });
  const text = await r.text();
  if (!r.ok) throw new Error(`POST ${path} => ${r.status}: ${text}`);
  try { return JSON.parse(text); } catch { return text; }
}

// Credentials to try in order
const CREDENTIALS = [
  { username: "admin",    password: "0000" },
  { username: "operator", password: "1234" },
  { username: "admin",    password: "admin" },
];

async function loginBoard() {
  for (const cred of CREDENTIALS) {
    try {
      const res = await post("/api/login", cred);
      if (res && res.token) { TOKEN = res.token; return cred.username; }
    } catch (e) {
      if (/not found|404/i.test(e.message)) { TOKEN = "__open__"; return "open"; }
    }
  }
  throw new Error("Could not authenticate with any known credential");
}

function sleep(ms) { return new Promise(res => setTimeout(res, ms)); }

let failures = 0;
function pass(msg) { console.log("  [PASS]", msg); }
function fail(msg) { console.log("  [FAIL]", msg); failures++; }
function info(msg) { console.log("  [INFO]", msg); }
function section(msg) { console.log("\n=== " + msg + " ==="); }
function check(cond, okMsg, failMsg) { cond ? pass(okMsg) : fail(failMsg); }

// ── Main ─────────────────────────────────────────────────────────────────────

async function main() {
  console.log("Board verify + fill cycle test  |  " + new Date().toISOString());

  section("Discovery");
  BASE = await findBoard();
  if (!BASE) {
    console.error("  [FATAL] Board not found on any local subnet.");
    process.exit(1);
  }

  // ── Login ──────────────────────────────────────────────────────────────────
  section("Authentication");
  const role = await loginBoard();
  info(`Logged in as: ${role}  (token: ${TOKEN === "__open__" ? "open-access" : TOKEN.slice(0, 8) + "..."})`);

  // ── 1. Health ──────────────────────────────────────────────────────────────
  section("Health");
  let st = await get("/api/status");
  check(st.state === "IDLE", "State is IDLE", `State is ${st.state}, expected IDLE`);
  info(`State=${st.state}  weight=${st.weightKg}kg  rate=${st.ratePerKg} PKR/kg`);

  // ── 2. Calibration ─────────────────────────────────────────────────────────
  section("Calibration");
  let w = await get("/api/weight");
  check(w.calMode === "two-point", "Two-point cal active", `calMode=${w.calMode}`);
  info(`calLow:  ${w.calLowKg}kg @ raw ${w.calLowRaw}`);
  info(`calHigh: ${w.calHighKg}kg @ raw ${w.calHighRaw}`);
  info(`live: ${w.weightKg}kg  raw=${w.rawValue}  stable=${w.stable}`);

  // ── 3. Two-point accuracy (simulation) ────────────────────────────────────
  section("Two-point accuracy (sim)");
  const testPoints = [
    { sim: 0.0,  expect: 0.0,  tol: 0.05 },
    { sim: 2.0,  expect: 2.0,  tol: 0.10 },
    { sim: 5.0,  expect: 5.0,  tol: 0.20 },
    { sim: 10.0, expect: 10.0, tol: 0.20 },
    { sim: 12.0, expect: 12.0, tol: 0.10 },
    { sim: 15.0, expect: 15.0, tol: 0.30 },
    { sim: 20.0, expect: 20.0, tol: 0.40 },
  ];
  for (const pt of testPoints) {
    await post("/api/sim-weight", { weightKg: pt.sim });
    await sleep(300);
    const wt = await get("/api/weight");
    const diff = Math.abs(wt.weightKg - pt.expect);
    check(diff <= pt.tol,
      `sim ${pt.sim}kg => read ${wt.weightKg.toFixed(3)}kg  (err ${diff.toFixed(3)} <= ${pt.tol})`,
      `sim ${pt.sim}kg => read ${wt.weightKg.toFixed(3)}kg  (err ${diff.toFixed(3)} > ${pt.tol})`);
  }
  await post("/api/sim-clear");

  // ── 4. Settings ────────────────────────────────────────────────────────────
  section("Settings");
  await post("/api/settings", { ratePerKg: 250 });
  const cfg = await get("/api/settings");
  check(cfg.ratePerKg === 250, "Rate saved as 250 PKR/kg", `Rate is ${cfg.ratePerKg}`);

  // ── 5. Transactions baseline ───────────────────────────────────────────────
  section("Transactions");
  let txBefore = await get("/api/transactions");
  const countBefore = Array.isArray(txBefore.transactions) ? txBefore.transactions.length : 0;
  info(`Transactions before fill: ${countBefore}`);

  // ── 6. Fill cycle ──────────────────────────────────────────────────────────
  section("Fill cycle  (target=11.8 kg @ 250 PKR/kg)");
  const TARGET_KG = 11.8;
  const RATE = 250;
  const EXPECTED_AMT = Math.round(TARGET_KG * RATE);

  await post("/api/sim-weight", { weightKg: 0.0 });
  await post("/api/sim-inputs", { cylinderPresent: 1, nozzleEngaged: 1 });
  await sleep(400);

  await post("/api/start", { targetWeightKg: TARGET_KG, ratePerKg: RATE, targetAmount: EXPECTED_AMT });
  await sleep(300);

  st = await get("/api/status");
  check(st.state === "FILLING_FAST" || st.state === "FILLING",
    `Fill started: state=${st.state}`, `Fill did not start: state=${st.state}`);

  info("Ramping 0 -> 11 kg...");
  for (let kg = 1; kg <= 11; kg++) {
    await post("/api/sim-weight", { weightKg: kg });
    await sleep(150);
  }

  info("Crossing 95% threshold (11.21 kg)...");
  await post("/api/sim-weight", { weightKg: 11.21 });
  await sleep(500);
  st = await get("/api/status");
  info(`At 11.21 kg: state=${st.state}`);

  info("Reaching target (11.8 kg)...");
  await post("/api/sim-weight", { weightKg: TARGET_KG + 0.05 });
  await sleep(1000);
  st = await get("/api/status");
  if (st.state !== "COMPLETE") { await sleep(1500); st = await get("/api/status"); }

  check(st.state === "COMPLETE", `Fill completed: state=COMPLETE`, `Fill did not complete: state=${st.state}`);
  if (st.state === "COMPLETE") {
    info(`netWeightKg=${st.netWeightKg}  currentAmount=${st.currentAmount}`);
    check(Math.abs((st.netWeightKg || 0) - TARGET_KG) < 0.5,
      `netWeightKg ${st.netWeightKg} within 0.5 kg of target`,
      `netWeightKg ${st.netWeightKg} too far from target ${TARGET_KG}`);
  }

  // ── 7. Transaction logged ──────────────────────────────────────────────────
  section("Transaction log");
  let txAfter = await get("/api/transactions");
  const countAfter = Array.isArray(txAfter.transactions) ? txAfter.transactions.length : 0;
  check(countAfter === countBefore + 1,
    `Transaction recorded (${countBefore} -> ${countAfter})`,
    `No new transaction (before=${countBefore} after=${countAfter})`);
  if (countAfter > countBefore) {
    const last = txAfter.transactions[txAfter.transactions.length - 1];
    info(`Last tx: finalKg=${last.finalKg}  finalAmount=${last.finalAmount}  status=${last.status}  id=${last.transactionId}`);
  }

  // ── 8. Reset ───────────────────────────────────────────────────────────────
  section("Reset to IDLE");
  await post("/api/reset");
  await post("/api/sim-clear");
  await post("/api/sim-inputs-clear");
  await sleep(400);
  st = await get("/api/status");
  check(st.state === "IDLE", "Reset to IDLE", `After reset: state=${st.state}`);

  // ── Summary ────────────────────────────────────────────────────────────────
  section("Summary");
  if (failures === 0) {
    console.log("\n  ALL CHECKS PASSED\n");
  } else {
    console.log(`\n  ${failures} check(s) FAILED\n`);
    process.exit(1);
  }
}

main().catch(err => { console.error("FATAL:", err.message); process.exit(1); });
