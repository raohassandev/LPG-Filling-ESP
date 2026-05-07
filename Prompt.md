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
