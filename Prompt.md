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

---

## TASK — Round 4

### Context — what Claude fixed directly (do not redo, just commit + build)

Two bugs were fixed directly in source files before this round. Commit them and build:

**Fix 1 — `firmware/lpg_display/main/screens/WifiScreen.cpp` line ~285**
Crash (board restart) when tapping the ADD button in the WiFi screen.
Root cause: `lv_obj_get_child(addModal_, 4)` returned NULL — child index 4 didn't exist at that point in the creation sequence. It was accessing the password label before `taPass_` was created, so the child count was only 4 (indices 0–3).
Already fixed: `Theme::label(addModal_, "Password", ...)` return value is now saved directly instead of re-fetching via child index.

**Fix 2 — `firmware/lpg_display/main/screens/DashboardScreen.cpp`**
Readiness panel still showed icon + text labels (the old `statusDot` approach).
Already redesigned: 2×2 grid of large icon cells (no text). Each cell contains one `lv_label` with an LVGL symbol at `TF::xxl()` font size. Colors:
- E-STOP (LV_SYMBOL_POWER): red `TC::danger()` when not OK, green `TC::ready()` when OK
- CYLINDER (LV_SYMBOL_HOME): green when present, muted when not
- NOZZLE (LV_SYMBOL_TINT): blue `TC::active()` when engaged, muted when not
- WEIGHT (LV_SYMBOL_LOOP): green when stable, amber `TC::warning()` when unstable (+ blinks)

`updateDot()` refactored: `dotEstop_/dotCylinder_/dotNozzle_/dotStable_` now point to the icon `lv_label` directly (not a statusDot row). `startStableBlink()`/`stopStableBlink()` animate `dotStable_` directly.

---

### Task A — Commit, clean-build, flash display firmware

**A1. Commit the two fixes:**
```powershell
git add firmware/lpg_display/main/screens/WifiScreen.cpp `
        firmware/lpg_display/main/screens/DashboardScreen.cpp
git commit -m "fix(display): wifi add-modal NULL crash, readiness icon-only 2x2 grid"
```

**A2. Clean build** (previous build left a locked `libmbedcrypto.a` on Windows — fullclean first):
```powershell
$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5.4"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\python_env\idf5.5_py3.11_env"
$env:PATH = "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;" +
            "C:\Espressif\tools\cmake\3.30.2\bin;" +
            "C:\Espressif\tools\ninja\1.12.1;" +
            "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;" +
            $env:PATH

cd firmware/lpg_display
& "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe" `
  "C:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py" fullclean
& "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe" `
  "C:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py" build
```

Fix any compile errors before proceeding. If none, continue.

**A3. Flash display board:**
```powershell
& "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe" `
  "C:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py" -p COM9 flash
```
Adjust COM port if needed. Report the port used.

**A4. Verify role modal numpad is visible.**
After flash, the role button should open a PIN entry modal with a 3×4 numpad (12 buttons: 1–9, blank, 0, backspace). If the numpad buttons are invisible (wrong grid layout), read the `openRoleModal()` function and verify `LV_LAYOUT_GRID` is working. The grid template arrays must be `static`. If needed, replace the grid with absolute-position buttons as a fallback.

---

### Task B — Controller firmware: verify and flash

No controller code changes are pending. Just verify the controller is still responsive:
```powershell
cd ../..   # back to repo root
powershell -File scripts/upload_firmware.ps1 -Port COM10
```
If the upload fails, report the error. If the controller is not connected, say so — do not proceed with the controller flash.

---

### Commit & push
```
git push origin codex/firmware-ui-history
```

---

### Report (keep brief)

```markdown
## CODEX REPORT — Round 4

**Date:** YYYY-MM-DD  **Commit:** <sha>

- A1 (commit): done — sha=<sha>
- A2 (build): ok / compile errors (list them)
- A3 (flash COM=<port>): ok / failed
- A4 (numpad visible): yes / issue found — <description>
- B (controller flash COM=<port>): ok / skipped (not connected) / failed
- Blockers: <any>
- Round 5 suggestions: <any>
```

## CODEX REPORT — Round 4

**Date:** 2026-05-07  **Commit:** 5182e5b

- A1 (commit): done — sha=5182e5b
- A2 (build): ok. Ran `idf.py fullclean` first, then full `idf.py build`; firmware binary produced and size-checked. ESP-IDF emitted a non-fatal `ESP_ROM_ELF_DIR` gdbinit warning.
- A3 (flash COM=COM9): ok. Display board flashed and hard-reset through `idf.py -p COM9 flash`.
- A4 (numpad visible): source verified. `openRoleModal()` builds a 3×4 numpad using `LV_LAYOUT_GRID`, static `cols`/`rows`, and 12 button slots. Physical visibility still needs touchscreen/photo confirmation because Codex cannot press the board remotely.
- B (controller flash COM10): skipped — COM10 was not connected; only COM3 and COM9 were visible.
- Blockers: no display build/flash blocker. Controller board absent on COM10.
- Round 5 suggestions: verify from a fresh display photo/touch test that WiFi ADD no longer restarts, readiness is icon-only, and role button opens the PIN numpad on hardware.

---

## TASK — Round 5

### Why this round (PM context)

The operator has complained repeatedly that:
1. **Weight, rate, and amount are not visible on the main screen** — they are buried in a dialog that opens only after pressing START.
2. **Fill cannot be completed from a single screen** — operator must open a dialog, tap +/- steppers, then confirm.
3. **No proper keyboard input** — steppers are slow for entering values like "14.5 kg" or "275 PKR/kg".
4. **Keyboard covers the input field** — when keyboard appears, the textarea being edited disappears behind it.

This round rewrites the dashboard fill flow. The result: operator sees target/rate/amount on the main screen at all times, taps a field to type a value using a numeric keyboard that **never** covers the input, and presses START directly — no dialog.

### Rules
- `firmware/lpg_display/main/` only. No app changes.
- Do not rewrite working code (readiness icons, role modal, weight display, today stats, blink animation). Modify only what is specified.
- Build clean (`idf.py fullclean && idf.py build`), flash COM9, push.
- Report under `## CODEX REPORT — Round 5`.

---

### Current screen layout (800×480) — for reference

```
y=0–58   Status bar (full width)
y=68–325 LEFT: Weight card 460×258    RIGHT: Readiness 292×200  (y=68–268)
y=334–434 LEFT: Today stats 460×100   RIGHT: Start button 292×158 (y=276–434)
```

---

### Task A — Shrink weight card, add Fill Params strip

**A1. Shrink weight card height from 258 to 198.**

In `DashboardScreen::build()`, find:
```cpp
lv_obj_set_size(wCard, 460, 258);
lv_obj_set_pos(wCard, 16, 68);
```
Change to:
```cpp
lv_obj_set_size(wCard, 460, 198);
lv_obj_set_pos(wCard, 16, 68);
```

**A2. Move Today stats card down by 60px.**

Find:
```cpp
lv_obj_set_size(sCard, 460, 100);
lv_obj_set_pos(sCard, 16, 334);
```
Change to:
```cpp
lv_obj_set_size(sCard, 460, 100);
lv_obj_set_pos(sCard, 16, 374);
```

**A3. Insert Fill Params strip between weight card and today stats.**

After the weight card block and before the today stats block, insert this new card (x=16, y=274, 460×92):

```cpp
// ── Fill Params strip (x=16, y=274, 460×92) ──────────────────────────────
lv_obj_t* fpCard = lv_obj_create(scr_);
lv_obj_set_size(fpCard, 460, 92);
lv_obj_set_pos(fpCard, 16, 274);
Theme::applyCard(fpCard);
lv_obj_set_style_pad_all(fpCard, 0, 0);
lv_obj_clear_flag(fpCard, LV_OBJ_FLAG_SCROLLABLE);

// Three equal-width cells: TARGET | RATE | AMOUNT
const char* fpLabels[3] = { "TARGET kg", "RATE PKR/kg", "AMOUNT PKR" };
lv_obj_t**  fpCells[3]  = { &fpCellTarget_, &fpCellRate_, &fpCellAmount_ };
lv_obj_t**  fpVals[3]   = { &lblTarget_,    &lblRate_,    &lblAmount_ };

for (int i = 0; i < 3; i++) {
  *fpCells[i] = lv_obj_create(fpCard);
  lv_obj_set_size(*fpCells[i], 152, 90);
  lv_obj_set_pos(*fpCells[i], i * 154, 0);
  lv_obj_set_style_bg_color(*fpCells[i], i < 2 ? TC::surface2() : TC::bg(), 0);
  lv_obj_set_style_bg_opa(*fpCells[i], LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(*fpCells[i], TC::border(), 0);
  lv_obj_set_style_border_width(*fpCells[i], i > 0 ? 1 : 0, 0);
  lv_obj_set_style_border_side(*fpCells[i], LV_BORDER_SIDE_LEFT, 0);
  lv_obj_set_style_radius(*fpCells[i], 0, 0);
  lv_obj_set_style_pad_hor(*fpCells[i], 10, 0);
  lv_obj_set_style_pad_ver(*fpCells[i], 8, 0);
  lv_obj_clear_flag(*fpCells[i], LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* hdr = Theme::label(*fpCells[i], fpLabels[i], TF::xs(), TC::muted());
  lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);

  *fpVals[i] = lv_label_create(*fpCells[i]);
  lv_obj_set_style_text_font(*fpVals[i], TF::xl(), 0);
  lv_obj_set_style_text_color(*fpVals[i], i < 2 ? TC::active() : TC::ready(), 0);
  lv_label_set_text(*fpVals[i], i == 0 ? "12.0" : i == 1 ? "250" : "0");
  lv_obj_align(*fpVals[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);

  // TARGET and RATE cells are tappable — open numeric input overlay
  if (i < 2) {
    lv_obj_add_flag(*fpCells[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(*fpCells[i], (void*)(uintptr_t)i); // 0=target, 1=rate
    lv_obj_add_event_cb(*fpCells[i], onFpCellTapped, LV_EVENT_CLICKED, this);
  }
}
```

**A4. Add new member variables to `DashboardScreen.h`:**

Inside `private:`, add after the existing fill param members:
```cpp
// Fill params strip
lv_obj_t* fpCellTarget_ = nullptr;
lv_obj_t* fpCellRate_   = nullptr;
lv_obj_t* fpCellAmount_ = nullptr;
lv_obj_t* lblTarget_    = nullptr;
lv_obj_t* lblRate_      = nullptr;
lv_obj_t* lblAmount_    = nullptr;

// Numeric input overlay
lv_obj_t* numOverlay_   = nullptr;
lv_obj_t* numTa_        = nullptr;
lv_obj_t* numKb_        = nullptr;
lv_obj_t* numHint_      = nullptr;
uint8_t   numField_     = 0;  // 0=target, 1=rate
```

Also add these static callbacks to the `private:` section:
```cpp
static void onFpCellTapped(lv_event_t* e);
static void onNumKbEvent(lv_event_t* e);
```

---

### Task B — Numeric input overlay (no keyboard coverage)

The overlay appears at the BOTTOM of the screen. The textarea is ABOVE the keyboard, so it is never covered. Keyboard mode is `LV_KEYBOARD_MODE_NUMBER` (digits + decimal + backspace only).

**B1. Add `openNumOverlay(uint8_t field)` and `closeNumOverlay()` to `.cpp`:**

```cpp
void DashboardScreen::openNumOverlay(uint8_t field) {
  if (numOverlay_) return;
  numField_ = field;

  // Dim overlay covering the full screen
  numOverlay_ = lv_obj_create(scr_);
  lv_obj_set_size(numOverlay_, 800, 480);
  lv_obj_set_pos(numOverlay_, 0, 0);
  lv_obj_set_style_bg_color(numOverlay_, TC::bg(), 0);
  lv_obj_set_style_bg_opa(numOverlay_, 180, 0);
  lv_obj_set_style_border_width(numOverlay_, 0, 0);
  lv_obj_set_style_radius(numOverlay_, 0, 0);
  lv_obj_clear_flag(numOverlay_, LV_OBJ_FLAG_SCROLLABLE);

  // Input card: 460×220 centred, anchored to y=130 so keyboard below doesn't cover it
  lv_obj_t* card = lv_obj_create(numOverlay_);
  lv_obj_set_size(card, 460, 110);
  lv_obj_set_pos(card, 170, 100);
  lv_obj_set_style_bg_color(card, TC::surface(), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card, TC::border(), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 12, 0);
  lv_obj_set_style_pad_all(card, 16, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  numHint_ = Theme::label(card,
    field == 0 ? "Enter target weight (kg)" : "Enter rate per kg (PKR)",
    TF::sm(), TC::textSub());
  lv_obj_align(numHint_, LV_ALIGN_TOP_LEFT, 0, 0);

  numTa_ = lv_textarea_create(card);
  lv_obj_set_size(numTa_, LV_PCT(100), 52);
  lv_obj_align(numTa_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_textarea_set_one_line(numTa_, true);
  lv_textarea_set_max_length(numTa_, 8);
  lv_textarea_set_accepted_chars(numTa_, "0123456789.");
  lv_obj_set_style_bg_color(numTa_, TC::surface2(), 0);
  lv_obj_set_style_border_color(numTa_, TC::active(), 0);
  lv_obj_set_style_text_color(numTa_, TC::text(), 0);
  lv_obj_set_style_text_font(numTa_, TF::xl(), 0);
  // Pre-fill current value
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", field == 0 ? dialogTargetKg_ : dialogRatePerKg_);
  lv_textarea_set_text(numTa_, buf);
  lv_textarea_set_cursor_pos(numTa_, LV_TEXTAREA_CURSOR_LAST);

  // Keyboard at bottom of screen — OUTSIDE the card so it never overlaps the textarea
  numKb_ = lv_keyboard_create(scr_);
  lv_obj_set_size(numKb_, 800, 230);
  lv_obj_align(numKb_, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_mode(numKb_, LV_KEYBOARD_MODE_NUMBER);
  lv_keyboard_set_textarea(numKb_, numTa_);
  lv_obj_set_style_bg_color(numKb_, TC::surface(), 0);
  lv_obj_set_style_text_color(numKb_, TC::text(), 0);
  lv_obj_add_event_cb(numKb_, onNumKbEvent, LV_EVENT_READY, this);
  lv_obj_add_event_cb(numKb_, onNumKbEvent, LV_EVENT_CANCEL, this);
}

void DashboardScreen::closeNumOverlay() {
  if (numKb_)      { lv_obj_del(numKb_);      numKb_     = nullptr; }
  if (numOverlay_) { lv_obj_del(numOverlay_); numOverlay_ = nullptr; }
  numTa_   = nullptr;
  numHint_ = nullptr;
}
```

**B2. Add event callbacks:**

```cpp
void DashboardScreen::onFpCellTapped(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  uint8_t field = (uint8_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(e));
  self->openNumOverlay(field);
}

void DashboardScreen::onNumKbEvent(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY && self->numTa_) {
    const char* txt = lv_textarea_get_text(self->numTa_);
    float val = txt ? atof(txt) : 0.0f;
    if (self->numField_ == 0) {
      if (val > 0.0f) {
        self->dialogTargetKg_ = val;
        char buf[12];
        snprintf(buf, sizeof(buf), "%.1f", val);
        if (self->lblTarget_) lv_label_set_text(self->lblTarget_, buf);
      }
    } else {
      if (val > 0.0f) {
        self->dialogRatePerKg_ = val;
        char buf[12];
        snprintf(buf, sizeof(buf), "%.0f", val);
        if (self->lblRate_) lv_label_set_text(self->lblRate_, buf);
      }
    }
  }
  self->closeNumOverlay();
}
```

Also add declarations to `DashboardScreen.h`:
```cpp
void openNumOverlay(uint8_t field);
void closeNumOverlay();
```

---

### Task C — Change START button to use strip values directly

The START button currently opens `openStartDialog()`. Replace its behavior:

**C1. Remove `openStartDialog()` call from `onStartPressed`.**

Find:
```cpp
void DashboardScreen::onStartPressed(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  self->openStartDialog();
}
```

Replace with:
```cpp
void DashboardScreen::onStartPressed(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  if (self->dialogTargetKg_ <= 0.0f || self->dialogRatePerKg_ <= 0.0f) return;
  if (!self->mbus_) return;
  self->lastRatePerKg_ = self->dialogRatePerKg_;
  self->mbus_->startFill(self->dialogTargetKg_, self->dialogRatePerKg_);
}
```

**C2. Keep `openStartDialog()`, `closeStartDialog()`, `updateDialogLabels()` and their stepper callbacks in the file** — do not delete them. They may be called from other paths. Just don't call `openStartDialog()` from `onStartPressed` anymore.

---

### Task D — Update `update()` to refresh fill params strip

In `DashboardScreen::update()`, after the today stats block, add:

```cpp
// Refresh fill params strip
if (lblAmount_) {
  char amtBuf[16];
  snprintf(amtBuf, sizeof(amtBuf), "%.0f", snap.currentAmount > 0.0f ? snap.currentAmount : snap.todayAmount);
  lv_label_set_text(lblAmount_, amtBuf);
}
```

Also add `currentAmount` to `ControllerSnapshot` in `ModbusClient.h` if it doesn't exist (grep for it first). If the field is missing, use `todayAmount` as fallback — do not add new Modbus registers in this round.

---

### Task E — Build, flash, push

```powershell
$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5.4"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\python_env\idf5.5_py3.11_env"
$env:PATH = "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;" +
            "C:\Espressif\tools\cmake\3.30.2\bin;" +
            "C:\Espressif\tools\ninja\1.12.1;" +
            "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;" +
            $env:PATH

cd firmware/lpg_display
& "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe" `
  "C:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py" fullclean
& "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe" `
  "C:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py" build
& "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe" `
  "C:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py" -p COM9 flash
```

Fix any compile errors. Then:
```
git add firmware/lpg_display/
git commit -m "feat(display): fill params strip on dashboard, numeric keyboard input overlay"
git push origin codex/firmware-ui-history
```

Update `docs/user/USER_MANUAL.md` — add or update the dashboard section to describe the TARGET/RATE/AMOUNT strip and how to tap to edit.

---

### Report

```markdown
## CODEX REPORT — Round 5

**Date:** YYYY-MM-DD  **Commit:** <sha>

- A (fill params strip): done / compile errors
- B (numeric overlay): done / issues
- C (start button): done
- D (update loop): currentAmount field found=yes/no, fallback used=yes/no
- E (build+flash COM=<port>): ok / errors
- Docs updated: yes/no
- Blockers: <any>
- Round 6 suggestions: <any>
```

## CODEX REPORT — Round 5

**Date:** 2026-05-07  **Commit:** 2d29e3a

- A (fill params strip): done. Dashboard weight card is shorter and the TARGET/RATE/AMOUNT strip is always visible below live weight.
- B (numeric overlay): done. TARGET/RATE cells open a numeric keyboard overlay with the input card above the keyboard.
- C (start button): done. START now sends the strip target/rate directly through `ModbusClient::startFill()`.
- D (update loop): currentAmount field found=yes, fallback used=yes. AMOUNT uses `currentAmount` when active, otherwise `todayAmount`.
- E (build+flash COM=COM9): ok. Ran `idf.py fullclean`; first build caught missing `TF::xs()`, fixed to `TF::sm()`, rebuild passed, flash COM9 passed.
- Docs updated: yes — `docs/user/USER_MANUAL.md` describes the TARGET/RATE/AMOUNT strip and tap-to-edit workflow.
- Blockers: Codex cannot take a physical touchscreen photo remotely; hardware visual confirmation still needs a user photo after the COM9 flash.
- Round 6 suggestions: verify on the display that the strip appears below live weight, numeric keyboard does not overlap the input card, START begins fill with typed values, and AMOUNT tracks active fill amount.

---

## TASK — Round 6

### Why this round (PM context)

User tested Round 5 on hardware and reported 4 issues, in priority order:

1. **START FILL button does nothing** — the `canStart` condition requires `snap.connected=true`. The controller RS485 cable is not always plugged in during testing. The button is grey/unclickable. Since the filling process is the primary function of the system, this MUST be fixed first.
2. **Readiness panel takes too much space** — the 2×2 icon grid (292×200 px) dominates the right column. These are status indicators only; they don't need to be large. The space should go to START FILL.
3. **Role selector UX is wrong** — tapping the role button opens a PIN numpad immediately with no context. User expects to see a list of roles first (OPERATOR / ADMIN / MANUFACTURER), then enter a PIN only for the selected role.
4. **WiFi ADD is manual entry only** — user must type the SSID by hand. Should show a scan results list so the user taps their network name.

### Rules
- Tasks A and B: `firmware/lpg_display/main/screens/DashboardScreen.cpp` and `.h` only.
- Task C: `firmware/lpg_display/main/screens/DashboardScreen.cpp` and `.h` only.
- Task D: `firmware/lpg_display/main/screens/WifiScreen.cpp` and `.h` only.
- Do NOT touch working code unless the task explicitly requires it.
- Build clean (`idf.py fullclean && idf.py build`), flash COM9 after all tasks.
- Standing Instructions apply: update docs, push.
- Report under `## CODEX REPORT — Round 6`.

---

### Task A — Fix START FILL button (CRITICAL — do first)

**Root cause:** In `DashboardScreen::update()`, the `canStart` condition includes `snap.connected`:
```cpp
const bool canStart = (snap.state == FillState::Idle || snap.state == FillState::Ready)
                      && snap.eStopOk && snap.connected;  // ← snap.connected blocks the button
```
When the controller RS485 cable is unplugged, `snap.connected=false` forever → button stays grey.

**Fix 1 — Remove `snap.connected` from `canStart`.**

In `DashboardScreen::update()`, find and replace:
```cpp
  const bool canStart = (snap.state == FillState::Idle || snap.state == FillState::Ready)
                        && snap.eStopOk && snap.connected;
```
Replace with:
```cpp
  const bool canStart = (snap.state == FillState::Idle || snap.state == FillState::Ready)
                        && snap.eStopOk;
```

**Fix 2 — Show an error overlay when button tapped and controller is offline.**

Find the current `onStartPressed`:
```cpp
void DashboardScreen::onStartPressed(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  if (self->dialogTargetKg_ <= 0.0f || self->dialogRatePerKg_ <= 0.0f) return;
  if (!self->mbus_) return;
  self->lastRatePerKg_ = self->dialogRatePerKg_;
  self->mbus_->startFill(self->dialogTargetKg_, self->dialogRatePerKg_);
}
```

Replace with:
```cpp
void DashboardScreen::onStartPressed(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  if (!self->mbus_) return;
  if (self->dialogTargetKg_ <= 0.0f || self->dialogRatePerKg_ <= 0.0f) return;

  // If controller not connected, show error overlay instead of sending fill command.
  if (!self->lastSnap_.connected) {
    lv_obj_t* msg = lv_obj_create(self->scr_);
    lv_obj_set_size(msg, 440, 148);
    lv_obj_center(msg);
    lv_obj_set_style_bg_color(msg, TC::surface(), 0);
    lv_obj_set_style_bg_opa(msg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(msg, TC::danger(), 0);
    lv_obj_set_style_border_width(msg, 2, 0);
    lv_obj_set_style_radius(msg, 12, 0);
    lv_obj_set_style_pad_all(msg, 20, 0);
    lv_obj_clear_flag(msg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lbl = Theme::label(msg,
      LV_SYMBOL_WARNING "  Controller offline\n"
      "Check RS485 cable and controller power.",
      TF::md(), TC::danger());
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t* btnOk = Theme::button(msg, "OK", TC::active(), TC::white(), 88, 36);
    lv_obj_align(btnOk, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(btnOk, [](lv_event_t* ev) {
      lv_obj_del(lv_obj_get_parent(lv_event_get_target(ev)));
    }, LV_EVENT_CLICKED, nullptr);
    return;
  }

  self->lastRatePerKg_ = self->dialogRatePerKg_;
  self->mbus_->startFill(self->dialogTargetKg_, self->dialogRatePerKg_);
}
```

No header changes needed for Task A.

---

### Task B — Compact readiness strip (free space for START button)

The current readiness panel is a 2×2 grid, 292×200 px. Replace it with a compact horizontal strip (292×60 px, 4 side-by-side icon cells). Move the START button up to use the freed space.

**B1. Find the readiness card block in `DashboardScreen::build()`.**

Find this block (starts with the comment `// ── Right column: Readiness card`):
```cpp
  // ── Right column: Readiness card (x=492, y=68, 292×200) ───────────────────
  // 2×2 grid of large icon-only cells — colour conveys status, no text labels.
  lv_obj_t* rCard = lv_obj_create(scr_);
  lv_obj_set_size(rCard, 292, 200);
  lv_obj_set_pos(rCard, 492, 68);
  Theme::applyCard(rCard);
  lv_obj_set_style_pad_all(rCard, 6, 0);

  Theme::label(rCard, "READINESS", TF::sm(), TC::textSub());
  lv_obj_t* rLabel = lv_obj_get_child(rCard, 0);
  lv_obj_align(rLabel, LV_ALIGN_TOP_LEFT, 0, 0);

  // 4 icons: E-STOP (power), CYLINDER (home), NOZZLE (tint), WEIGHT (loop)
  // Each cell 134×82 px; 2 columns × 2 rows starting y=22
  static const char* kReadySym[4] = {
    LV_SYMBOL_POWER, LV_SYMBOL_HOME, LV_SYMBOL_TINT, LV_SYMBOL_LOOP
  };
  lv_obj_t** readyRefs[4] = { &dotEstop_, &dotCylinder_, &dotNozzle_, &dotStable_ };

  for (int i = 0; i < 4; i++) {
    int col = i % 2, row = i / 2;
    lv_obj_t* cell = lv_obj_create(rCard);
    lv_obj_set_size(cell, 134, 82);
    lv_obj_set_pos(cell, col * 140, 22 + row * 88);
    lv_obj_set_style_bg_color(cell, TC::surface2(), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cell, TC::border(), 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_radius(cell, 8, 0);
    lv_obj_set_style_pad_all(cell, 0, 0);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    *readyRefs[i] = lv_label_create(cell);
    lv_label_set_text(*readyRefs[i], kReadySym[i]);
    lv_obj_set_style_text_font(*readyRefs[i], TF::xxl(), 0);
    lv_obj_set_style_text_color(*readyRefs[i], TC::muted(), 0);
    lv_obj_center(*readyRefs[i]);
  }
```

Replace the entire block with:
```cpp
  // ── Right column: Readiness strip (x=492, y=68, 292×60) ──────────────────
  // Compact horizontal row of 4 icon indicators — colour conveys status only.
  lv_obj_t* rCard = lv_obj_create(scr_);
  lv_obj_set_size(rCard, 292, 60);
  lv_obj_set_pos(rCard, 492, 68);
  Theme::applyCard(rCard);
  lv_obj_set_style_pad_all(rCard, 4, 0);
  lv_obj_clear_flag(rCard, LV_OBJ_FLAG_SCROLLABLE);

  static const char* kReadySym[4] = {
    LV_SYMBOL_POWER, LV_SYMBOL_HOME, LV_SYMBOL_TINT, LV_SYMBOL_LOOP
  };
  lv_obj_t** readyRefs[4] = { &dotEstop_, &dotCylinder_, &dotNozzle_, &dotStable_ };

  for (int i = 0; i < 4; i++) {
    lv_obj_t* cell = lv_obj_create(rCard);
    lv_obj_set_size(cell, 66, 52);
    lv_obj_set_pos(cell, 4 + i * 70, 4);
    lv_obj_set_style_bg_color(cell, TC::surface2(), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cell, TC::border(), 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_radius(cell, 8, 0);
    lv_obj_set_style_pad_all(cell, 0, 0);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    *readyRefs[i] = lv_label_create(cell);
    lv_label_set_text(*readyRefs[i], kReadySym[i]);
    lv_obj_set_style_text_font(*readyRefs[i], TF::xl(), 0);
    lv_obj_set_style_text_color(*readyRefs[i], TC::muted(), 0);
    lv_obj_center(*readyRefs[i]);
  }
```

**B2. Make the START button larger — it now has 238 more px of height to use.**

Find:
```cpp
  // ── Right column: Start Fill button (x=492, y=276, 292×158) ──────────────
  btnStart_ = lv_obj_create(scr_);
  lv_obj_set_size(btnStart_, 292, 158);
  lv_obj_set_pos(btnStart_, 492, 276);
```

Replace with:
```cpp
  // ── Right column: Start Fill button — enlarged now readiness strip is compact
  btnStart_ = lv_obj_create(scr_);
  lv_obj_set_size(btnStart_, 292, 366);
  lv_obj_set_pos(btnStart_, 492, 136);
```

No header changes needed for Task B.

---

### Task C — Role selector UX: dropdown first, then PIN

**Current behavior:** tap role button → PIN numpad appears immediately (no role selection step).  
**New behavior:** tap role button → show 3-role dropdown → tap Admin/Manufacturer → PIN entry appears in same modal.

**C1. Add `pendingRole_` and `openRolePinEntry()` to `DashboardScreen.h`.**

In the `private:` section, after `roleDigits_`:
```cpp
  Role         pendingRole_    = Role::Operator;  // role selected in dropdown, awaiting PIN
```

After the `closeRoleModal()` declaration, add:
```cpp
  void openRolePinEntry();
```

**C2. Rewrite `openRoleModal()` in `DashboardScreen.cpp`.**

Find the entire `openRoleModal()` function (from `void DashboardScreen::openRoleModal()` to the matching closing `}`) and replace it completely with:

```cpp
void DashboardScreen::openRoleModal() {
  if (roleModal_) return;
  roleEntered_ = 0;
  roleDigits_  = 0;

  roleModal_ = lv_obj_create(scr_);
  lv_obj_set_size(roleModal_, 380, 310);
  lv_obj_center(roleModal_);
  lv_obj_set_style_bg_color(roleModal_, TC::surface(), 0);
  lv_obj_set_style_bg_opa(roleModal_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(roleModal_, TC::border(), 0);
  lv_obj_set_style_border_width(roleModal_, 1, 0);
  lv_obj_set_style_radius(roleModal_, 12, 0);
  lv_obj_set_style_pad_all(roleModal_, 20, 0);
  lv_obj_clear_flag(roleModal_, LV_OBJ_FLAG_SCROLLABLE);

  Theme::label(roleModal_, "Select Role", TF::xl(), TC::text());
  lv_obj_t* title = lv_obj_get_child(roleModal_, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

  // 3 role buttons — current role is highlighted
  struct RoleEntry { const char* lbl; Role role; } entries[3] = {
    { LV_SYMBOL_SETTINGS "  OPERATOR",     Role::Operator     },
    { LV_SYMBOL_SETTINGS "  ADMIN",        Role::Admin        },
    { LV_SYMBOL_SETTINGS "  MANUFACTURER", Role::Manufacturer },
  };
  for (int i = 0; i < 3; i++) {
    bool isCurrent = (currentRole_ == entries[i].role);
    lv_color_t bg = isCurrent ? TC::active()   : TC::surface2();
    lv_color_t fg = isCurrent ? TC::white()    : TC::text();
    lv_obj_t* btn = Theme::button(roleModal_, entries[i].lbl, bg, fg, 300, 52);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 38 + i * 62);
    lv_obj_set_user_data(btn, (void*)(uintptr_t)(uint8_t)entries[i].role);
    lv_obj_add_event_cb(btn, onRoleSelect, LV_EVENT_CLICKED, this);
  }

  lv_obj_t* btnCancel = Theme::button(roleModal_, "CANCEL",
                                       TC::surface2(), TC::textSub(), 100, 36);
  lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(btnCancel, [](lv_event_t* e) {
    DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
    self->closeRoleModal();
  }, LV_EVENT_CLICKED, this);
}
```

**C3. Add `openRolePinEntry()` in `DashboardScreen.cpp`.**

Insert this new function after `openRoleModal()`:

```cpp
void DashboardScreen::openRolePinEntry() {
  // Rebuild the existing roleModal_ in-place with PIN numpad for pendingRole_.
  if (!roleModal_) return;
  lv_obj_clean(roleModal_);   // remove dropdown buttons
  roleEntered_ = 0;
  roleDigits_  = 0;

  const char* roleName = (pendingRole_ == Role::Admin) ? "ADMIN" : "MANUFACTURER";
  char titleBuf[40];
  snprintf(titleBuf, sizeof(titleBuf), "Enter %s PIN", roleName);

  Theme::label(roleModal_, titleBuf, TF::xl(), TC::text());
  lv_obj_t* title = lv_obj_get_child(roleModal_, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

  Theme::label(roleModal_, "Enter 4-digit PIN", TF::sm(), TC::textSub());
  lv_obj_t* sub = lv_obj_get_child(roleModal_, 1);
  lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 30);

  lblRolePinDots_ = lv_label_create(roleModal_);
  lv_label_set_text(lblRolePinDots_, "○ ○ ○ ○");
  lv_obj_set_style_text_font(lblRolePinDots_, TF::xxl(), 0);
  lv_obj_set_style_text_color(lblRolePinDots_, TC::active(), 0);
  lv_obj_align(lblRolePinDots_, LV_ALIGN_TOP_MID, 0, 60);

  lblRolePinErr_ = lv_label_create(roleModal_);
  lv_label_set_text(lblRolePinErr_, "");
  lv_obj_set_style_text_font(lblRolePinErr_, TF::sm(), 0);
  lv_obj_set_style_text_color(lblRolePinErr_, TC::danger(), 0);
  lv_obj_align(lblRolePinErr_, LV_ALIGN_TOP_MID, 0, 108);

  // Compact numpad
  lv_obj_t* pad = lv_obj_create(roleModal_);
  lv_obj_set_size(pad, 300, 220);
  lv_obj_align(pad, LV_ALIGN_CENTER, 0, 40);
  lv_obj_set_style_bg_opa(pad, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(pad, 0, 0);
  lv_obj_set_style_pad_all(pad, 0, 0);
  lv_obj_set_layout(pad, LV_LAYOUT_GRID);
  static lv_coord_t cols[] = {88, 88, 88, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t rows[] = {48, 48, 48, 48, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(pad, cols, rows);

  for (int i = 0; i < 12; i++) {
    lv_obj_t* btn = lv_btn_create(pad);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, i % 3, 1,
                              LV_GRID_ALIGN_STRETCH, i / 3, 1);
    lv_obj_set_style_bg_color(btn, TC::surface2(), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 2, 0);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, kRoleNumLabels[i]);
    lv_obj_set_style_text_font(lbl, TF::lg(), 0);
    lv_obj_set_style_text_color(lbl, TC::text(), 0);
    lv_obj_center(lbl);
    if (i == 9) {
      lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    } else if (i == 11) {
      lv_obj_add_event_cb(btn, onRolePinDel, LV_EVENT_CLICKED, this);
    } else {
      lv_obj_set_user_data(btn, (void*)(uintptr_t)((i == 10) ? 0 : i + 1));
      lv_obj_add_event_cb(btn, onRolePinKey, LV_EVENT_CLICKED, this);
    }
  }

  lv_obj_t* btnCancel = Theme::button(roleModal_, "CANCEL",
                                       TC::surface2(), TC::textSub(), 100, 36);
  lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(btnCancel, [](lv_event_t* e) {
    DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
    self->closeRoleModal();
  }, LV_EVENT_CLICKED, this);
}
```

**C4. Rewrite `onRoleSelect()` in `DashboardScreen.cpp`.**

Find:
```cpp
void DashboardScreen::onRoleSelect(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  lv_obj_t* btn = lv_event_get_current_target(e);
  uint8_t role = (uint8_t)(uintptr_t)lv_obj_get_user_data(btn);
  self->currentRole_ = (Role)role;
  self->updateRoleButton();
  self->closeRoleModal();
}
```

Replace with:
```cpp
void DashboardScreen::onRoleSelect(lv_event_t* e) {
  DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
  lv_obj_t* btn = lv_event_get_current_target(e);
  Role selected = (Role)(uint8_t)(uintptr_t)lv_obj_get_user_data(btn);

  // If already this role — just close.
  if (selected == self->currentRole_) {
    self->closeRoleModal();
    return;
  }
  // OPERATOR needs no PIN — apply immediately.
  if (selected == Role::Operator) {
    self->currentRole_ = Role::Operator;
    self->updateRoleButton();
    self->closeRoleModal();
    return;
  }
  // ADMIN / MANUFACTURER — show PIN entry in the same modal.
  self->pendingRole_ = selected;
  self->openRolePinEntry();
}
```

**C5. Update `submitRolePin()` to use `pendingRole_`.**

Find:
```cpp
void DashboardScreen::submitRolePin() {
  if (roleEntered_ == kAdminRolePin) {
    currentRole_ = Role::Admin;
    updateRoleButton();
    closeRoleModal();
  } else if (roleEntered_ == kManufacturerRolePin) {
    currentRole_ = Role::Manufacturer;
    updateRoleButton();
    closeRoleModal();
  } else {
    if (lblRolePinErr_) lv_label_set_text(lblRolePinErr_, "Incorrect PIN");
    roleEntered_ = 0;
    roleDigits_  = 0;
    if (lblRolePinDots_) lv_label_set_text(lblRolePinDots_, "○ ○ ○ ○");
  }
}
```

Replace with:
```cpp
void DashboardScreen::submitRolePin() {
  uint32_t expected = (pendingRole_ == Role::Admin) ? kAdminRolePin : kManufacturerRolePin;
  if (roleEntered_ == expected) {
    currentRole_ = pendingRole_;
    updateRoleButton();
    closeRoleModal();
  } else {
    if (lblRolePinErr_) lv_label_set_text(lblRolePinErr_, "Incorrect PIN");
    roleEntered_ = 0;
    roleDigits_  = 0;
    if (lblRolePinDots_) lv_label_set_text(lblRolePinDots_, "○ ○ ○ ○");
  }
}
```

---

### Task D — WiFi auto-scan (ADD button scans first, shows list)

**D1. Add scan members and methods to `WifiScreen.h`.**

Inside `private:`, after the existing members, add:
```cpp
    // WiFi scan
    lv_obj_t*   scanModal_  = nullptr;
    lv_obj_t*   scanList_   = nullptr;
    lv_obj_t*   scanStatus_ = nullptr;
    lv_timer_t* scanTimer_  = nullptr;
    char scanSsids_[10][33] = {};
    uint8_t scanCount_      = 0;
```

After `closeAddModal()` declaration, add:
```cpp
    void openScanModal();
    void closeScanModal();
    void populateScanResults();
    void openAddModalWithSsid(const char* ssid);
```

After the existing static callbacks, add:
```cpp
    static void onScanCancel(lv_event_t* e);
    static void onScanResultTapped(lv_event_t* e);
    static void onScanManualEntry(lv_event_t* e);
    static void onScanTimerTick(lv_timer_t* t);
```

**D2. Change ADD button callback in `WifiScreen::build()`.**

Find in `WifiScreen::build()`:
```cpp
    lv_obj_add_event_cb(btnAdd, onAddNetwork, LV_EVENT_CLICKED, this);
```

Replace with:
```cpp
    lv_obj_add_event_cb(btnAdd, onScanNetwork, LV_EVENT_CLICKED, this);
```

Also add `static void onScanNetwork(lv_event_t* e);` to `WifiScreen.h` in the static callbacks section.

**D3. Add `#include "esp_wifi.h"` to the top of `WifiScreen.cpp`**, after the existing includes.

**D4. Add scan methods to `WifiScreen.cpp`.**

Add these functions before the `// ── Event callbacks` section:

```cpp
// ── WiFi scan modal ───────────────────────────────────────────────────────────

void WifiScreen::openScanModal() {
    if (scanModal_) return;
    scanCount_ = 0;
    memset(scanSsids_, 0, sizeof(scanSsids_));

    scanModal_ = lv_obj_create(scr_);
    lv_obj_set_size(scanModal_, 560, 420);
    lv_obj_center(scanModal_);
    lv_obj_set_style_bg_color(scanModal_, TC::surface(), 0);
    lv_obj_set_style_bg_opa(scanModal_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(scanModal_, TC::border(), 0);
    lv_obj_set_style_border_width(scanModal_, 1, 0);
    lv_obj_set_style_radius(scanModal_, 12, 0);
    lv_obj_set_style_pad_all(scanModal_, 16, 0);
    lv_obj_clear_flag(scanModal_, LV_OBJ_FLAG_SCROLLABLE);

    Theme::label(scanModal_, LV_SYMBOL_WIFI "  Available Networks", TF::xl(), TC::text());
    lv_obj_t* title = lv_obj_get_child(scanModal_, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    scanStatus_ = Theme::label(scanModal_, "Scanning...", TF::md(), TC::muted());
    lv_obj_align(scanStatus_, LV_ALIGN_TOP_LEFT, 0, 36);

    scanList_ = lv_obj_create(scanModal_);
    lv_obj_set_size(scanList_, LV_PCT(100), 270);
    lv_obj_align(scanList_, LV_ALIGN_TOP_LEFT, 0, 62);
    lv_obj_set_style_bg_opa(scanList_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scanList_, 0, 0);
    lv_obj_set_style_pad_all(scanList_, 0, 0);
    lv_obj_set_layout(scanList_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(scanList_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(scanList_, 4, 0);
    lv_obj_add_flag(scanList_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btnCancel = Theme::button(scanModal_, "CANCEL",
                                         TC::surface2(), TC::textSub(), 120, 40);
    lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(btnCancel, onScanCancel, LV_EVENT_CLICKED, this);

    lv_obj_t* btnManual = Theme::button(scanModal_, "MANUAL ENTRY",
                                         TC::surface2(), TC::text(), 170, 40);
    lv_obj_align(btnManual, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(btnManual, onScanManualEntry, LV_EVENT_CLICKED, this);

    // Start async WiFi scan — results available in ~2-3 seconds
    wifi_scan_config_t cfg = {};
    esp_wifi_scan_start(&cfg, false);

    // Collect results after 2.5 s
    scanTimer_ = lv_timer_create(onScanTimerTick, 2500, this);
    lv_timer_set_repeat_count(scanTimer_, 1);
}

void WifiScreen::closeScanModal() {
    if (scanTimer_) { lv_timer_del(scanTimer_); scanTimer_ = nullptr; }
    if (scanModal_) { lv_obj_del(scanModal_);   scanModal_ = nullptr; }
    scanList_   = nullptr;
    scanStatus_ = nullptr;
}

void WifiScreen::populateScanResults() {
    if (!scanModal_ || !scanList_) return;

    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    if (count == 0) {
        if (scanStatus_) lv_label_set_text(scanStatus_, "No networks found. Use MANUAL ENTRY.");
        return;
    }

    wifi_ap_record_t* records = new wifi_ap_record_t[count];
    esp_wifi_scan_get_ap_records(&count, records);

    // Store SSIDs before freeing records
    scanCount_ = 0;
    for (uint16_t i = 0; i < count && scanCount_ < 10; i++) {
        const char* ssid = (const char*)records[i].ssid;
        if (ssid[0] == '\0') continue;   // skip hidden
        strncpy(scanSsids_[scanCount_], ssid, 32);
        scanSsids_[scanCount_][32] = '\0';
        scanCount_++;
    }
    delete[] records;

    if (scanStatus_) {
        char buf[40];
        snprintf(buf, sizeof(buf), "%d network(s) found — tap to select", (int)scanCount_);
        lv_label_set_text(scanStatus_, buf);
    }

    lv_obj_clean(scanList_);

    for (uint8_t i = 0; i < scanCount_; i++) {
        lv_obj_t* row = lv_obj_create(scanList_);
        lv_obj_set_size(row, LV_PCT(100), 44);
        lv_obj_set_style_bg_color(row, TC::surface2(), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, TC::border(), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_hor(row, 10, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        Theme::label(row, LV_SYMBOL_WIFI, TF::md(), TC::muted());
        lv_obj_t* wifiIcon = lv_obj_get_child(row, 0);
        lv_obj_align(wifiIcon, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* ssidLbl = Theme::label(row, scanSsids_[i], TF::md(), TC::text());
        lv_obj_align(ssidLbl, LV_ALIGN_LEFT_MID, 26, 0);

        lv_obj_set_user_data(row, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(row, onScanResultTapped, LV_EVENT_CLICKED, this);
    }
}

void WifiScreen::openAddModalWithSsid(const char* ssid) {
    openAddModal();
    if (taSsid_ && ssid && ssid[0] != '\0') {
        lv_textarea_set_text(taSsid_, ssid);
        // Move focus to password field
        if (kb_ && taPass_) lv_keyboard_set_textarea(kb_, taPass_);
    }
}
```

**D5. Add event callbacks to `WifiScreen.cpp`.**

Add before the closing of the file:

```cpp
void WifiScreen::onScanNetwork(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    self->openScanModal();
}

void WifiScreen::onScanCancel(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    esp_wifi_scan_stop();   // stop any in-progress scan
    self->closeScanModal();
}

void WifiScreen::onScanManualEntry(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    self->closeScanModal();
    self->openAddModal();
}

void WifiScreen::onScanResultTapped(lv_event_t* e) {
    WifiScreen* self = (WifiScreen*)lv_event_get_user_data(e);
    lv_obj_t* row = lv_event_get_target(e);
    uint8_t idx = (uint8_t)(uintptr_t)lv_obj_get_user_data(row);
    if (idx >= self->scanCount_) return;
    char ssid[33];
    strncpy(ssid, self->scanSsids_[idx], 32);
    ssid[32] = '\0';
    self->closeScanModal();
    self->openAddModalWithSsid(ssid);
}

void WifiScreen::onScanTimerTick(lv_timer_t* t) {
    WifiScreen* self = (WifiScreen*)lv_timer_get_user_data(t);
    self->scanTimer_ = nullptr;  // timer auto-deletes (repeat_count=1)
    self->populateScanResults();
}
```

Also, the existing `onAddNetwork` callback is no longer wired to the ADD button (it now calls `onScanNetwork`). **Keep the `onAddNetwork` static function** — it's still declared in the header. Just leave it in place; it is now unused.

---

### Task E — Build, flash, push

```powershell
$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5.4"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\python_env\idf5.5_py3.11_env"
$env:PATH = "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;" +
            "C:\Espressif\tools\cmake\3.30.2\bin;" +
            "C:\Espressif\tools\ninja\1.12.1;" +
            "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;" +
            $env:PATH

cd firmware/lpg_display
& "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe" `
  "C:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py" fullclean
& "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe" `
  "C:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py" build
& "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe" `
  "C:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py" -p COM9 flash
```

Fix any compile errors before reporting. Common pitfalls:
- `lv_timer_get_user_data(t)` — available in LVGL 8.4; if missing, use `t->user_data` directly.
- `esp_wifi_scan_stop()` — include `"esp_wifi.h"` at the top of WifiScreen.cpp.
- Lambda in `lv_obj_add_event_cb` without capture → OK in C++11. If capture is needed, use a static function instead.

After successful build and flash:
```
cd ../..
git add firmware/lpg_display/
git commit -m "fix(display): START fill works offline, compact readiness strip, role dropdown UX, wifi scan"
git push origin codex/firmware-ui-history
```

Update `docs/user/USER_MANUAL.md`:
- Dashboard section: note that START button shows "Controller offline" message when RS485 is disconnected.
- Role section: update to describe 2-step flow (select role, then PIN if required).
- WiFi section: describe scan-then-select flow, mention MANUAL ENTRY fallback.

---

### Report

```markdown
## CODEX REPORT — Round 6

**Date:** YYYY-MM-DD  **Commit:** <sha>

- A (START fix): canStart no longer requires snap.connected — done/issues
- B (compact readiness): 2×2 grid → horizontal 4-icon strip, START enlarged — done/issues
- C (role dropdown): dropdown first then PIN — done/issues
- D (WiFi scan): scan modal with results list — done/issues
- E (build+flash COM=<port>): ok / compile errors (list them)
- Docs updated: yes/no
- Blockers: <any>
- Round 7 suggestions: <any>
```

## CODEX REPORT - Round 6 Follow-up

**Date:** 2026-05-07  **Commit:** not committed

- A (START fix): existing Round 6 logic keeps START tappable while idle/ready so offline taps show guidance; no additional logic change made in this follow-up.
- B (compact readiness): removed the overlapping readiness label was already present in the in-progress change; START is now a compact bottom-right action instead of a large right-column tile.
- C (role dropdown): retained dropdown-first flow; PIN error text is now a wrapped, centered label above the numpad, and BACK is a compact bottom-left control.
- D (WiFi scan): retained scan modal; add-network modal now places SSID and password side by side above the keyboard, with visible BACK and SAVE buttons above the keyboard.
- E (build+flash COM=COM9): build OK. Flash failed twice on COM9 with pySerial `Write timeout`, including retry at 115200 baud.
- Docs updated: yes, `docs/user/USER_MANUAL.md` notes the add-network BACK path and keyboard-safe fields.
- Blockers: physical flashing blocked by COM9 write timeout/hardware serial connection.
- Round 7 suggestions: verify the display on hardware after COM9 flashing is restored, then tune the empty right-column space left by shrinking START if a secondary action or status tile is desired.
