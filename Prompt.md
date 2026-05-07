# Codex Task Prompt — LPG Filling Station

> **How this file works:**
> Claude writes tasks in `## TASK` sections. Codex implements them and appends a report under
> `## CODEX REPORT`. Claude reads the report and writes the next task. Keep all history in this
> file — do not delete previous rounds.

---

## Context

**Repository:** `LPG-Filling-ESP` (Windows, PowerShell)  
**Active branch:** `codex/firmware-ui-history`  
**Push target:** `origin codex/firmware-ui-history`

### System architecture

| Component | Path | Tech |
|-----------|------|------|
| Controller firmware | `firmware/lpg_controller/` | Arduino / ESP32 (KC868-A6) |
| Display firmware | `firmware/lpg_display/` | ESP-IDF v5.5.4 / LVGL 8.4 (Waveshare ESP32-S3) |
| Mobile/web app | `apps/lpg-expo-app/` | React Native + Expo SDK 54 |
| Docs (user) | `docs/user/` | Markdown |
| Developer guide | `docs/DEVELOPER_GUIDE.md` | Markdown |

### App overview

The Expo app communicates with the KC868-A6 controller over HTTP/WebSocket/MQTT.

| File | Role |
|------|------|
| `src/state/AppStateProvider.js` | Auth, live status stream, transactions, navigation |
| `src/services/controllerApi.js` | All HTTP calls to controller |
| `src/hooks/useStatusStream.js` | WebSocket → MQTT → REST polling fallback |
| `src/theme.js` | Design tokens `C` (colors) `S` (spacing) `R` (radii) `T` (type sizes) |
| `src/screens/OperatorDashboard.js` | Main operator screen (recently redesigned) |
| `src/components/StatusHeader.js` | Top header bar (recently redesigned) |
| `src/components/ReadinessCard.js` | 4 icon-only readiness strip (recently redesigned) |

### Recent changes made by Claude (this session)

1. **`src/theme.js`** — semantic colors brightened ~15% for outdoor readability; base palette unchanged (dark).
2. **`src/components/StatusHeader.js`** — rewritten. Now 2-row layout. **New props:** `authRole`, `authUsername`, `onSwitchAccount`. Old props `firmware` and `onTapTitle` removed. Has role badge with colour-coded border + dropdown modal.
3. **`src/components/ReadinessCard.js`** — rewritten. 4 `MaterialCommunityIcons` icons (no text labels): `alert-octagon`, `gas-cylinder`, `nozzle`, `scale-balance`. Blink animation on weight-stable when inactive.
4. **`src/screens/OperatorDashboard.js`** — full redesign. Passes new StatusHeader props. Single-screen layout, compact fill setup card, last-5 today transactions list.

---

## Standing Instructions — Apply to Every Round

These rules are permanent and apply regardless of which task is being worked on.
Do not skip them. Do not wait to be reminded.

### 1. Documentation — always keep in sync

After any code change, check whether these docs need updating. If yes, update them in the same commit:

| Doc file | Update when… |
|---|---|
| `docs/DEVELOPER_GUIDE.md` | Firmware change, new build step, new GPIO/pin, new sdkconfig setting, Modbus register added/changed, new known issue |
| `docs/user/USER_MANUAL.md` | Any operator-visible behavior change (screen layout, button label, fill flow, PIN, WiFi, roles) |

Rules for doc edits:
- Edit only the relevant section — do not rewrite unrelated sections.
- Use present tense. Keep tables aligned.
- If a register, pin, or setting is added/changed in code, the corresponding table row in the docs must reflect it in the same commit.

### 2. Modbus register map — always keep in sync

The authoritative Modbus register map lives in two places that must always match:

| File | Role |
|---|---|
| `firmware/lpg_controller/src/ModbusRegisterMap.cpp` | Firmware implementation |
| `docs/DEVELOPER_GUIDE.md` → *Modbus Register Map* section | Human-readable reference |

If you add, remove, rename, or change the type/scale of any Modbus register:
1. Update `ModbusRegisterMap.cpp`.
2. Update the matching row in `docs/DEVELOPER_GUIDE.md`.
3. If the display firmware reads that register (`firmware/lpg_display/`), update the display side too.

### 3. Firmware flashing — flash boards after every firmware change

After any successful firmware build, flash to the physical boards.

**Controller (KC868-A6) — Arduino CLI:**
```powershell
# From repo root — adjust -Port if board is not on COM5
powershell -File scripts/upload_firmware.ps1 -Port COM5
```

**Display (Waveshare ESP32-S3) — ESP-IDF:**
```powershell
# Activate ESP-IDF first (only needed once per shell session)
. "C:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1"

# Then from the display firmware directory
cd firmware/lpg_display
idf.py -p COM6 flash    # adjust COM port if needed
```

If a board is not detected or flash fails, note it in the report under `Blockers` with the error — do not silently skip.
If the round does not touch firmware at all, skip this step and say so in the report.

---

## TASK — Round 1

### Objective

Verify the redesigned Expo app is consistent across all screens and fix any broken references left by the recent rewrites. Then improve two remaining screens to match the new design system.

### Rules

- Work only in `apps/lpg-expo-app/src/`.
- Do not touch `firmware/` in this round.
- Commit each logical group of changes as a separate commit with a clear message.
- Push to `origin codex/firmware-ui-history` at the end.
- Append your full report under `## CODEX REPORT — Round 1` (do not edit this section).

---

### Task 1 — Audit & fix broken references

The following changes may have broken callers. Find and fix every instance.

#### 1a. `StatusHeader` new props

Old signature:
```js
StatusHeader({ status, streamMode, firmware, onTapTitle })
```
New signature:
```js
StatusHeader({ status, streamMode, authRole, authUsername, onSwitchAccount })
```

Search every file that renders `<StatusHeader` and update props. Files likely affected:
`FillProgressScreen.js`, `FillCompleteScreen.js`, `FaultScreen.js`,
`TransactionsScreen.js`, `AdminUsersScreen.js`, `NetworkSettingsScreen.js`,
`CalibrationScreen.js`, `DiagnosticsScreen.js`.

If a screen passes `StatusHeader` it must now get `authRole` and `authUsername` from
`useAppState()` and pass `onSwitchAccount={logout}`.

#### 1b. `AppStateProvider` — expose `logout` everywhere needed

`logout` is already in `AppStateProvider`. Verify it is exported in the context value
so every screen can destructure it. If it is missing, add it.

#### 1c. `ReadinessCard` — verify icon names

`MaterialCommunityIcons` icon names must be exact strings from the MCi library.
Verify these four exist:
- `alert-octagon`
- `gas-cylinder`
- `nozzle`
- `scale-balance`

If any name is wrong, find the correct name and fix it in `ReadinessCard.js`.
Search the MCi icon list at `https://materialdesignicons.com` if needed, or grep the
`@expo/vector-icons` package inside `node_modules`.

---

### Task 2 — FillProgressScreen

File: `src/screens/FillProgressScreen.js`

Read the whole file, then apply these changes:

1. Add `<StatusHeader>` at the top (same pattern as `OperatorDashboard`) with the new props.
   Pull `authRole`, `authUsername`, `logout` from `useAppState()`.
2. The state label currently uses emoji (`"⚡  FAST FILL"`). Replace with text only:
   - FILLING_FAST → `"FAST FILL"`
   - FILLING_SLOW → `"SLOW FILL"`
   - SETTLING → `"SETTLING"`
3. Use the updated theme colors from `theme.js` — no hardcoded hex values.
4. Add a **STOP** button if one is missing. It already calls `stopFill` from `useControllerActions`.

---

### Task 3 — FillCompleteScreen

File: `src/screens/FillCompleteScreen.js`

Read the whole file, then apply these changes:

1. Add `<StatusHeader>` at the top with the new props.
2. The receipt card should display these fields in order (single column, each row label + value):
   - Transaction ID
   - Net weight dispensed (kg)
   - Rate per kg (PKR)
   - Final amount (PKR)
   - Duration
   - Completed at (date + time)
3. A single `"New Fill"` button at the bottom that calls `resetFill()` and navigates to `"dashboard"`.
4. No hardcoded hex values — use `C.*` from theme.

---

### Task 4 — TransactionsScreen compact rows

File: `src/screens/TransactionsScreen.js`

Read the whole file. The transaction list currently renders each item as a multi-line card.
Replace the per-item renderer with the same compact single-line row format introduced in
`OperatorDashboard`:

```
● TXN-0042  ·  12.0 kg  ·  PKR 3,000  ·  Jan 5, 3:45 PM  [status dot color]
```

Row structure:
```jsx
<View style={txnRow}>
  <View style={[dot, { backgroundColor: isOk ? C.ready : C.warning }]} />
  <Text style={txnId}>{txn.transactionId || `#${txn.id}`}</Text>
  <Text style={txnKg}>{kg(txn.netKg ?? txn.finalKg)} kg</Text>
  <Text style={txnAmt}>PKR {money(txn.finalAmount)}</Text>
  <Text style={txnTime}>{shortDateTime(txn.endTime || txn.startTime)}</Text>
</View>
```

Keep the period filter (Today / Week / Month) and KPI summary row — only the per-item renderer changes.

---

### Task 5 — FaultScreen

File: `src/screens/FaultScreen.js`

Read the whole file, then:

1. Add `<StatusHeader>` at the top with the new props.
2. Ensure the fault code message uses `getFaultMessage(code)` from `utils/faultMessages.js`
   (not `getFaultUI` or any other name — check for import mismatches).
3. Make sure the RESET button is only shown when `status.readyToReset` is true **or**
   the fault severity is not `"critical"`. If the field doesn't exist on status, show
   the button always.

---

### Commit & push

After all tasks above:

```
git add apps/lpg-expo-app/src/
git commit -m "fix(app): fix StatusHeader props, compact TxnRow across all screens, receipt card"
git push origin codex/firmware-ui-history
```

---

### Reporting format

Append the following section to this file after completing all tasks:

```markdown
## CODEX REPORT — Round 1

**Date:** YYYY-MM-DD  
**Commit:** <sha>

### Task 1 — Audit & fix broken references
- [ ] 1a. StatusHeader props updated in: <list files changed>
- [ ] 1b. logout exported from AppStateProvider: yes/no, what changed
- [ ] 1c. Icon names verified: <list any corrections made>

### Task 2 — FillProgressScreen
<what changed, any issues found>

### Task 3 — FillCompleteScreen
<what changed, any issues found>

### Task 4 — TransactionsScreen
<what changed, any issues found>

### Task 5 — FaultScreen
<what changed, any issues found>

### Issues encountered
<anything that blocked a task, workarounds applied, things Claude should know>

### Next suggestions
<anything you noticed that should be addressed in Round 2>
```

## CODEX REPORT — Round 1

**Date:** 2026-05-07  
**Commit:** c76382e

### Task 1 — Audit & fix broken references
- [x] 1a. StatusHeader props updated in: `src/screens/FillProgressScreen.js`, `src/screens/FillCompleteScreen.js`, `src/screens/FaultScreen.js`
- [x] 1b. logout exported from AppStateProvider: yes; already present, no code change required
- [x] 1c. Icon names verified: changed invalid MaterialCommunityIcons `nozzle` icon to valid `fuel` in `src/components/ReadinessCard.js`

### Task 2 — FillProgressScreen
Added StatusHeader with auth role/user and switch-account callback, removed emoji/symbol state labels, kept STOP as a clear `STOP FILL` action, and kept fill colors on theme tokens only.

### Task 3 — FillCompleteScreen
Added StatusHeader, rebuilt receipt rows in the requested order, wired `New Fill` to reset the controller then navigate to dashboard, and removed hardcoded color literals from the screen.

### Task 4 — TransactionsScreen
Replaced each transaction item with a compact single-line row using status dot, transaction id, kg, amount, and short timestamp while preserving filters, KPI summary, and SD archive rendering.

### Task 5 — FaultScreen
Updated StatusHeader props, imported and used `getFaultMessage(code)`, and gated reset visibility through `readyToReset` or non-critical fault severity. The fallback still shows reset when firmware does not expose `readyToReset`.

### Issues encountered
`npm.cmd run lint` executes but fails because `scripts/check-app.js` still searches for old UI strings inside `App.js`; those strings now live in split screen files such as `OperatorDashboard.js` and `NetworkSettingsScreen.js`. Targeted grep checks for hardcoded fill-screen colors, old StatusHeader props, and the invalid icon name passed. No firmware files or board ports were touched.

### Next suggestions
Update `apps/lpg-expo-app/scripts/check-app.js` to inspect the split screen files, then add a lightweight render/smoke check for the fill, complete, transaction, and fault routes.

---

## TASK — Round 2

### Rules
- `apps/lpg-expo-app/` only. No new npm packages. No hardcoded hex — `C.*` from `src/theme.js`.
- One commit, then push. Report under `## CODEX REPORT — Round 2`.

---

### Task A — FillProgressScreen improvements

File: `src/screens/FillProgressScreen.js`

Make these targeted changes (do not rewrite the file):

**A1. Animated progress bar** — insert below the phase label, above the STOP button:
```jsx
// progressAnim = useRef(new Animated.Value(0)).current — add to existing refs
// On each render: Animated.spring(progressAnim, { toValue: pct, useNativeDriver: false }).start()
// where: const pct = Math.min((status?.netWeightKg ?? 0) / (status?.targetWeightKg || 1), 1)
const barColor = state === "FILLING_FAST" ? C.active : state === "FILLING_SLOW" ? C.warning : C.settling;

<View style={{ flexDirection:"row", alignItems:"center", gap: S.sm, marginVertical: S.md }}>
  <View style={{ flex:1, height:14, backgroundColor: C.surface2, borderRadius: R.pill, overflow:"hidden" }}>
    <Animated.View style={{ height:14, borderRadius: R.pill, backgroundColor: barColor,
      width: progressAnim.interpolate({ inputRange:[0,1], outputRange:["0%","100%"] }) }} />
  </View>
  <Text style={{ fontSize: T.lg, fontWeight:"900", color: C.text, minWidth: 48, textAlign:"right" }}>
    {Math.round(pct * 100)}%
  </Text>
</View>
```

**A2. Key numbers row** — replace any existing weight display with:
```jsx
{[
  { label:"NET kg",   value: (status?.netWeightKg  ?? 0).toFixed(3) },
  { label:"TARGET",   value: (status?.targetWeightKg ?? 0).toFixed(3) },
  { label:"PKR",      value: Math.round(status?.currentAmount ?? 0).toLocaleString() },
].map(({ label, value }) => (
  <View key={label} style={{ flex:1, alignItems:"center" }}>
    <Text style={{ fontSize: T.xs, fontWeight:"700", color: C.muted, letterSpacing:0.5 }}>{label}</Text>
    <Text style={{ fontSize: T.xl, fontWeight:"900", color: C.text, fontVariant:["tabular-nums"] }}>{value}</Text>
  </View>
))}
```
Wrap the three items in `<View style={{ flexDirection:"row", marginBottom: S.md }}>`.

**A3. STOP button** — ensure it is at the very bottom of the screen with `marginTop:"auto"` and full width. No style changes to the button itself.

---

### Task B — FillCompleteScreen improvements

File: `src/screens/FillCompleteScreen.js`

Make these targeted changes:

**B1. Success header** — insert as the first element inside the scroll content (after StatusHeader):
```jsx
<View style={{ alignItems:"center", paddingVertical: S.xl }}>
  <View style={{ width:72, height:72, borderRadius:999, backgroundColor: C.ready+"22",
    alignItems:"center", justifyContent:"center", marginBottom: S.md }}>
    <Text style={{ fontSize:36, color: C.ready }}>✓</Text>
  </View>
  <Text style={{ fontSize: T.xl, fontWeight:"900", color: C.text, letterSpacing:1 }}>FILL COMPLETE</Text>
</View>
```

**B2. Receipt — Final amount row** — find the Final Amount row and change its value `Text` style to:
```js
{ fontSize: T.xl, fontWeight:"900", color: C.ready, fontVariant:["tabular-nums"] }
```
Ensure rows are in this order: Transaction ID → Net weight → Rate per kg → **Final amount** → Duration → Completed at. Add Rate per kg row if missing:
```jsx
<ReceiptRow label="Rate / kg" value={`PKR ${(txn?.ratePerKg ?? status?.ratePerKg ?? 0).toFixed(2)}`} />
```

**B3. Share hint** — add below the receipt card, above the New Fill button:
```jsx
<Text style={{ fontSize: T.xs, color: C.muted, textAlign:"center", marginVertical: S.sm }}>
  Screenshot this receipt or tap New Fill to continue.
</Text>
```

---

### Task C — BottomNav component (new file + wire to 3 screens)

**C1. Create** `src/components/BottomNav.js`:
```jsx
import { Pressable, ScrollView, StyleSheet, Text, View } from "react-native";
import { useAppState } from "../state/AppStateProvider";
import { C, S, T } from "../theme";

const NAV = {
  operator:     [["dashboard","Dashboard"],["transactions","History"],["__logout","Sign Out"]],
  admin:        [["dashboard","Dashboard"],["transactions","History"],["network","Settings"],["users","Users"],["__logout","Sign Out"]],
  manufacturer: [["dashboard","Dashboard"],["transactions","History"],["network","Settings"],["users","Users"],["diagnostics","Diagnostics"],["calibration","Calibration"],["__logout","Sign Out"]],
};

export default function BottomNav({ active }) {
  const { authRole, navigate, logout } = useAppState();
  const items = NAV[authRole] || NAV.operator;
  return (
    <View style={styles.bar}>
      <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.row}>
        {items.map(([key, label]) => {
          const isActive = key === active;
          const onPress = key === "__logout" ? logout : () => navigate(key);
          return (
            <Pressable key={key} style={[styles.item, isActive && styles.itemActive]} onPress={onPress}>
              <Text style={[styles.label, isActive && styles.labelActive]}>{label}</Text>
            </Pressable>
          );
        })}
      </ScrollView>
    </View>
  );
}

const styles = StyleSheet.create({
  bar:         { height:52, backgroundColor: C.surface, borderTopWidth:1, borderTopColor: C.border },
  row:         { flexDirection:"row", alignItems:"stretch" },
  item:        { paddingHorizontal: S.lg, justifyContent:"center", borderTopWidth:2, borderTopColor:"transparent" },
  itemActive:  { borderTopColor: C.primary },
  label:       { fontSize: T.xs, fontWeight:"800", color: C.muted, textTransform:"uppercase", letterSpacing:0.7 },
  labelActive: { color: C.primary },
});
```

**C2. Wire to 3 screens** — for each file below, add `import BottomNav from "../components/BottomNav";` and replace the existing `navRow` / bottom nav section with `<BottomNav active="<key>" />`. If no nav section exists, append it just before the closing scroll/view.

| File | active key |
|------|------------|
| `src/screens/OperatorDashboard.js` | `"dashboard"` |
| `src/screens/TransactionsScreen.js` | `"transactions"` |
| `src/screens/NetworkSettingsScreen.js` | `"network"` |

Do NOT add BottomNav to FillProgressScreen, FillCompleteScreen, FaultScreen, or any auth screen.

---

### Task D — LoginScreen: 3 surgical changes

File: `src/screens/LoginScreen.js`

1. Find the URL `TextInput`. Add prop: `placeholder="http://lpg-controller.local"` and `placeholderTextColor={C.muted}`.

2. After the URL input, insert a connection indicator (pull `streamMode` from `useAppState()` if not already destructured):
```jsx
{(() => {
  const dot = streamMode==="offline" ? C.danger : streamMode==="connecting" ? C.warning : C.ready;
  const lbl = streamMode==="offline" ? "Offline" : streamMode==="connecting" ? "Connecting…" : "Connected";
  return (
    <View style={{ flexDirection:"row", alignItems:"center", gap: S.xs, marginTop: S.xs }}>
      <View style={{ width:7, height:7, borderRadius:999, backgroundColor: dot }} />
      <Text style={{ fontSize: T.xs, color: dot }}>{lbl}</Text>
    </View>
  );
})()}
```

3. Find the Sign In button. Add `disabled={!!authLoading}` and change its label to:
```jsx
{authLoading ? "Signing in…" : "Sign In"}
```

---

### Commit & push

```
git add apps/lpg-expo-app/
git commit -m "feat(app): progress bar, receipt polish, BottomNav, login indicators"
git push origin codex/firmware-ui-history
```

---

### Report (keep brief)

```markdown
## CODEX REPORT — Round 2

**Date:** YYYY-MM-DD  **Commit:** <sha>

- A (FillProgress): <done / issues>
- B (FillComplete): <done / issues>
- C (BottomNav): <done / issues>
- D (Login): <done / issues>
- Blockers: <any>
- Round 3 suggestions: <any>
```

## CODEX REPORT — Round 2

**Date:** 2026-05-07  **Commit:** 61a31a5

- A (FillProgress): Done. Replaced the large weight/progress block with a 0-100% animated progress bar, NET/TARGET/PKR key numbers row, text-only phase label, and bottom-pinned full-width STOP FILL button.
- B (FillComplete): Done. Added the centered success header, kept the receipt order as Transaction ID -> Net weight -> Rate / kg -> Final amount -> Duration -> Completed at, styled final amount with `C.ready`, and added the screenshot/New Fill hint.
- C (BottomNav): Done. Added `src/components/BottomNav.js` and wired it to Dashboard, History, and Settings. Used the existing `admin-users` route for Users so the nav matches `App.js`.
- D (Login): Done. Added controller URL placeholder/color, connection status indicator, and disabled/loading sign-in behavior.
- Blockers: `npm.cmd run lint` still fails because `scripts/check-app.js` searches old UI strings in `App.js`; `npx.cmd expo export --platform web --output-dir .expo-check` passed. Firmware was not touched, so COM9/COM10 flashing was skipped by app-only rule.
- Round 3 suggestions: Update `scripts/check-app.js` to inspect split screen files, then consider wiring BottomNav into admin users, diagnostics, and calibration for manufacturer consistency.

---

## TASK — Round 3

### Why these tasks (PM context — read before starting)

**Task A — check-app.js** has failed for 2 consecutive rounds. It uses brittle single-file string searches against `App.js`, but UI code moved to split screen files in Round 1. The fix replaces it with a `grepSrc()` scanner across all of `src/` so it can never break again when files are reorganised. A hex color audit and calibration API check are added at the same time. Do not patch the old checks — rewrite the file as shown.

**Task B — BottomNav** currently covers only 3 of 6 navigable screens. Admin and Manufacturer roles navigate to Users, Diagnostics, and Calibration but those screens have no nav bar — operators are stranded. Three surgical one-line additions complete coverage. Zero risk of regression.

**Task C — Firmware commit + flash** is the most critical task this round. The physical display board is still running **old firmware** — all the display UI work (role button, PIN modal, icon readiness panel, float fix) was coded but never built or flashed. The untracked `WifiManager.cpp/h` and `WifiScreen.cpp/h` files are already `#include`d in `ScreenManager.cpp` so the build will fail from a clean clone until they are committed. `CMakeLists.txt` uses `GLOB_RECURSE` so no manual source list changes are needed. Build first, fix any compile errors, then flash both boards. Report exact COM ports.

### Rules
- Tasks A and B: `apps/lpg-expo-app/` only.
- Task C: `firmware/` only — do not touch app code.
- No hardcoded hex — `C.*` from `src/theme.js`.
- One commit per task group (A+B together, C separately), then push both.
- Report under `## CODEX REPORT — Round 3`.
- Standing Instructions apply: update docs and flash boards after firmware changes.

---

### Task A — Fix `scripts/check-app.js` once and for all

File: `apps/lpg-expo-app/scripts/check-app.js`

Rewrite the file completely. The old version searched for UI strings inside `App.js` which no longer contains them (they moved to split screen files). Replace with a scan across `src/`:

```js
const fs   = require("fs");
const path = require("path");

const root  = path.resolve(__dirname, "..");
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

if (hexCount > 0) { console.error(`\n${hexCount} hardcoded hex color(s) found outside theme.js — use C.* tokens.\n`); }
if (failed.length) { failed.forEach(([, msg]) => console.error(msg)); }

if (hexCount > 0 || failed.length) process.exit(1);
console.log("All checks passed.");
```

After writing the file, run it: `node scripts/check-app.js` from `apps/lpg-expo-app/`. It must exit 0. If any check fails, fix the root cause (do not comment out the check).

---

### Task B — BottomNav: wire to remaining 3 screens

For each file below, add `import BottomNav from "../components/BottomNav";` (if not already present) and place `<BottomNav active="<key>" />` at the very bottom of the screen's root view, just before the final closing tag. Do not restructure the file otherwise.

| File | active key |
|---|---|
| `src/screens/AdminUsersScreen.js` | `"admin-users"` |
| `src/screens/DiagnosticsScreen.js` | `"diagnostics"` |
| `src/screens/CalibrationScreen.js` | `"calibration"` |

---

### Task C — Firmware: commit untracked files, build, and flash

**C1. Stage all pending firmware changes.**

The following are uncommitted — add and commit them as-is (do not modify):
- All modified firmware files: `firmware/lpg_controller/` and `firmware/lpg_display/`
- Untracked new files: `firmware/lpg_display/main/WifiManager.cpp`, `WifiManager.h`, `screens/WifiScreen.cpp`, `screens/WifiScreen.h`
- Modified scripts: `scripts/build_firmware.ps1`, `scripts/upload_firmware.ps1`

```powershell
git add firmware/ scripts/
git commit -m "feat(firmware): wifi screen, display updates, controller settings improvements"
```

**C2. Build controller firmware.**
```powershell
powershell -File scripts/build_firmware.ps1
```
If build fails, fix compile errors before proceeding.

**C3. Flash controller (KC868-A6).**
```powershell
powershell -File scripts/upload_firmware.ps1 -Port COM10
```
Adjust COM port if needed. Report actual port used.

**C4. Build and flash display (Waveshare ESP32-S3).**
```powershell
. "C:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1"
cd firmware/lpg_display
idf.py build
idf.py -p COM9 flash
```
Adjust COM port if needed. Report actual port used.

**C5. Update docs.**
After flashing, check if the WifiScreen or any controller changes introduced new settings, pins, or behaviors. If yes, update `docs/DEVELOPER_GUIDE.md` and/or `docs/user/USER_MANUAL.md` in the same commit as the firmware.

---

### Commit & push

```
# App changes (Tasks A + B)
git add apps/lpg-expo-app/
git commit -m "fix(app): check-app.js rewritten, BottomNav wired to all screens"

# Firmware already committed in C1 above

git push origin codex/firmware-ui-history
```

---

### Report (keep brief)

```markdown
## CODEX REPORT — Round 3

**Date:** YYYY-MM-DD  **Commits:** <app sha> / <firmware sha>

- A (check-app.js): exit code=<0 or 1>, checks that failed (if any)
- B (BottomNav): wired to AdminUsers / Diagnostics / Calibration — yes/no per file
- C (Firmware): controller build=ok/fail, flash COM=<port> ok/fail; display build=ok/fail, flash COM=<port> ok/fail
- Docs updated: yes/no — what changed
- Blockers: <any>
- Round 4 suggestions: <any>
```

## CODEX REPORT — Round 3

**Date:** 2026-05-07  **Commits:** 484be0e / no firmware commit (no pending firmware diff)

- A (check-app.js): exit code=0. Rewritten to scan `src/`, include calibrate endpoint coverage, and fail on hardcoded hex outside `theme.js`.
- B (BottomNav): wired to AdminUsers=yes, Diagnostics=yes, Calibration=yes.
- C (Firmware): controller build=ok, flash COM=COM10 ok; display build=ok, flash COM=COM9 ok. ESP-IDF activation required process-local ExecutionPolicy bypass and PATH preference for `C:\Espressif\python_env\idf5.5_py3.11_env\Scripts` because system Python 3.14 pointed export.ps1 at a missing IDF venv.
- Docs updated: yes — `docs/user/USER_MANUAL.md` now says BottomNav is available on Dashboard, History, Settings, Users, Diagnostics, and Calibration.
- Blockers: none. Round 3 expected untracked firmware WiFi files, but `git status --short firmware scripts` was clean before Task C; there was nothing to commit under firmware/scripts.
- Round 4 suggestions: Verify the physical display UI from a fresh photo after the COM9 flash, then decide whether to commit a small helper script for ESP-IDF activation that prefers the installed Python 3.11 venv on this machine.

### Round 3 report update — photo/display verification

**Follow-up commit:** 0c21bd8

- The supplied photo does not prove the board was running a separate pre-session binary. It shows the current source dashboard shape, including the role button (`OPERATOR`) and separate `ADMIN` button.
- The real confirmed hardware issue in the photo is float rendering: labels show `f kg` / `f` because LVGL `%f` formatting is unreliable on the display build.
- Fixed in `0c21bd8`: added `DisplayFormat.h`, replaced display-screen float label formatting with `display_label_setf()`, gave dashboard weight labels fixed widths/clip modes to reduce layout drift while Modbus values change, rebuilt display firmware, and flashed COM9 successfully.
- Docs updated: `docs/DEVELOPER_GUIDE.md` now documents `display_label_setf()` for display float labels. `docs/user/USER_MANUAL.md` BottomNav update from Round 3 remains valid.
- Updated firmware result: display build=ok, flash COM=COM9 ok after the float/layout patch. Controller build/flash remains COM10 ok from the original Round 3 run.
- Round 4 suggestion: verify with a fresh display photo after `0c21bd8`; if the required touchscreen readiness panel is icon-only rather than the current icon+text rows, make that an explicit UI task because the current source still renders text labels.

### Round 3 report update — dashboard header/restart/time verification

**Follow-up work:** local changes built/flashed to COM9 on 2026-05-07.

- The 6:03 PM photo shows the float-label fix is live, but also confirms the dashboard header was overcrowded for the physical panel: the duplicate `ADMIN` button clipped at the right edge and header controls overlapped.
- Fixed in source and flashed to display COM9: dashboard header now uses fixed positions, compact state labels, WiFi icon, RTC time, and one role/PIN button. The separate clipped `ADMIN` button was removed because role/PIN access is already handled by the role button.
- Crash-risk fix: `firmware/lpg_display/main/Bsp.cpp` LVGL port stack increased from 8 KB to 32 KB to match the documented `uiTask` size and reduce resets when event callbacks create modals/widgets.
- Time behavior changed: dashboard now displays `--:--` until valid RTC registers are received from the controller instead of showing misleading `00:00`.
- Docs updated: `docs/user/USER_MANUAL.md` and `docs/display-firmware-plan.md` now match the dashboard header/role button behavior.
- Verification: ESP-IDF display build=ok; flash COM=COM9 ok. Serial port opened after reset but produced no boot text in the 8-second read window, so physical screen/touch verification is still required.
- Remaining check: if the top/left blank space remains after this flash, next step is LCD RGB timing/touch calibration against the exact Waveshare panel values; those timings were not changed in this pass to avoid making touch alignment worse blindly.
