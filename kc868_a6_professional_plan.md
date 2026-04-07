# KC868-A6 Professional Delivery Plan

## 1. Document Purpose

This document defines the full implementation plan for a **KC868-A6 based LPG filling control system** using **ESP32 / KCS V2 web delivery architecture**. It combines:

- system plan
- execution order
- engineering standards
- Codex SOP for development, flashing, validation, and testing
- browser UI test strategy using Playwright
- deployment and rollback process

It is intended to be used as the **master implementation guide** for design, coding, upload/download, commissioning, and validation.

---

## 2. Executive Summary

The system will be implemented as a **self-hosted web-controlled industrial controller** built on **KC868-A6**. The board will:

- operate as the main process controller
- provide WiFi connectivity in **AP mode** and **STA mode**
- host the operator web dashboard
- expose device APIs for control and monitoring
- manage filling logic independently of browser connection state
- store settings locally
- continue process execution even if UI disconnects

The browser-based UI is the primary HMI. The OLED is restricted to commissioning, diagnostics, and fallback monitoring.

This plan assumes the project will be developed and maintained with a coding workflow where **Codex handles software generation, firmware packaging, upload/download procedures, automated UI testing, and controlled release steps**.

---

## 3. System Objectives

### 3.1 Functional Objectives

The system must:

1. host a mobile-friendly HTML dashboard on KC868-A6
2. display live weight and process state
3. accept operator inputs such as:
   - rate per kg
   - target LPG kg
   - target amount
4. start, monitor, and complete filling cycles
5. control valves and outputs safely
6. save configuration and transaction records
7. support AP commissioning mode and STA production mode
8. tolerate WiFi/UI disconnect without stopping a running cycle

### 3.2 Operational Objectives

The system should:

- be easy to commission in the field
- provide reliable local operation without external cloud dependency
- support future expansion to MQTT / SCADA / manager analytics
- allow repeatable firmware updates with rollback discipline

### 3.3 Quality Objectives

The solution must be:

- deterministic in control behavior
- auditable in process flow
- testable at API, UI, and hardware levels
- maintainable with versioned source, configs, and releases

---

## 4. Scope

## In Scope

- KC868-A6 firmware architecture
- WiFi AP and STA access model
- web dashboard hosted on device
- API contract for browser communication
- filling process state machine
- parameter storage
- transaction logging model
- firmware flashing SOP
- config backup / restore SOP
- Playwright browser testing SOP
- validation and commissioning SOP

## Out of Scope for Initial Release

- cloud hosting
- remote internet-based fleet management
- advanced ERP integration
- mobile app binaries
- multi-device orchestration
- final commercial calibration certification

---

## 5. Architecture Overview

## 5.1 Core Architecture

The solution is divided into four layers.

### Layer A — Browser HMI

Runs on a mobile browser and loads from KC868 web server.

Responsibilities:

- render operator dashboard
- display live weight and machine state
- submit start/stop/config commands
- show alarms and completion messages
- recover UI state after reconnect

### Layer B — KC868 Web and Application Layer

Runs on KC868-A6.

Responsibilities:

- serve static HTML/CSS/JS
- expose HTTP APIs
- optionally support WebSocket or MQTT
- authenticate operator or manager modes if required
- manage settings and transaction data

### Layer C — Process Control Layer

Runs locally inside device logic.

Responsibilities:

- manage state machine
- control valves and outputs
- enforce safety conditions
- execute filling independent of UI state
- perform timeout, error, and recovery actions

### Layer D — Measurement / I/O Layer

Responsibilities:

- obtain live weight from HX711 or equivalent subsystem
- read digital inputs and faults
- write output states to actuators
- normalize sensor readings for logic layer

---

## 5.2 Access Modes

### AP Mode

Used for commissioning, maintenance, or standalone operation.

- Device broadcasts SSID
- User connects directly
- UI accessed at `http://192.168.4.1`

### STA Mode

Used for normal site operation.

- Device joins local router
- IP assigned by DHCP or static config
- UI accessed through local network IP

---

## 5.3 Non-Negotiable Design Rule

**Control logic must never depend on active browser presence.**

If WiFi drops, the browser closes, or a phone battery dies:

- the controller must continue safely
- the current cycle must follow defined logic
- status must be recoverable after reconnect

This is the most important industrial behavior requirement in the entire system.

---

## 6. Hardware Role Definition

## 6.1 KC868-A6

Primary roles:

- web host
- API host
- process controller
- settings store
- I/O and automation coordinator

## 6.2 Weight Measurement Side

Possible models:

- directly attached HX711 to controller firmware
- separate measurement subsystem sending value to KC868
- shared variable or Modbus-style integration

The exact integration path must be finalized before coding the hardware abstraction layer.

## 6.3 OLED

OLED is not the operator HMI.

OLED usage is limited to:

- IP address display
- AP/STA mode indication
- brief weight snapshot
- current machine state
- fault code display
- commissioning help

---

## 6.4 Mandatory Safety Boundary

This project must define and preserve a strict boundary between:

- **process automation functions** handled by KC868-A6 firmware
- **hard safety functions** that must remain effective even if firmware crashes, reboots, locks up, or outputs misbehave

The KC868-A6 may coordinate process logic, but it must not be treated as the sole safety layer for an LPG installation.

### Hard Safety Functions That Must Be Independent of Browser/UI

The following protections must not depend on web UI state, browser connectivity, or operator phone availability:

- emergency stop chain
- main power-safe shutdown path for actuators
- fail-closed valve strategy
- critical interlock loop
- alarm indication required for unsafe conditions

### Hard Safety Functions That Should Be Independent of Firmware Where Site Design Requires

These must be explicitly reviewed in the final electrical design and assigned either to hardwired circuits, safety-rated relays/contactors, or approved independent protection hardware:

- gas leak detection shutdown
- overpressure shutdown
- cabinet/door interlock
- compressor or pump interlock
- grounding/earthing fault response if applicable
- hazardous-area isolation requirements

### Required Safety Design Deliverable

Before implementation is considered design-complete, the project must include a table that defines for every critical signal:

- signal name
- source device
- normal state
- fault state
- loss-of-power state
- controller action
- independent hardware action
- safe final actuator state

### Non-Negotiable Safety Rule

If there is any conflict between convenience, process continuity, and safe shutdown behavior, the design must favor the safe state.

---

## 7. Software Architecture

## 7.1 Recommended Module Structure

### 1. Boot / Init Module

Handles:

- board startup
- pin initialization
- storage mount
- config load
- WiFi startup
- service registration

### 2. Network Module

Handles:

- AP mode
- STA mode
- reconnect behavior
- IP display
- mDNS if supported

### 3. Web Server Module

Handles:

- static file serving
- route registration
- API endpoints
- content headers
- optional auth hooks

### 4. State Store

Handles:

- device settings
- runtime status snapshot
- current batch / current transaction state
- persistence model

### 5. Weight Service

Handles:

- sensor reading
- filtering
- tare support
- stability check
- value normalization

### 6. Fill Process Engine

Handles:

- start validation
- filling sequence
- fast and slow stages
- stop conditions
- completion logic
- failure handling

Must also define:

- fast-to-slow switchover threshold
- final settle window
- overfill tolerance handling
- interrupted-cycle handling
- manual abort behavior
- fault classification and reason codes

### 7. Transaction Log Module

Handles:

- record creation
- timestamping
- result logging
- optional export format

### 8. Diagnostics Module

Handles:

- fault flags
- system heartbeat
- debug view
- OLED messages

### 9. UI Frontend Module

Handles:

- responsive mobile dashboard
- status polling or websocket updates
- form validation
- operator and manager screens

---

## 7.1A Measurement and Commercial Logic Specification

Because this system accepts both weight-based and amount-based operator inputs, the project must define a single measurement and pricing specification before coding is considered complete.

### Required Definitions

- display resolution for live weight
- internal control resolution for weight calculations
- currency precision
- rounding mode for amount calculations
- maximum allowed target overshoot
- weight stability threshold
- minimum stable time before completion
- minimum valid nonzero flow/change threshold
- negative drift handling after valve close
- zero/tare allowed window

### Source of Truth Rules

- if operator enters target kg, the controller computes target amount from rate
- if operator enters target amount, the controller computes target kg from rate
- if both are entered, one field must be authoritative by policy and the UI must show the derived value clearly
- the stored transaction record must indicate:
  - operator-entered source field
  - derived field
  - rate used
  - final net weight
  - final amount
  - rounding rule version

### Completion Policy Must Define

- exact condition for leaving `FILLING_SLOW`
- exact condition for entering `SETTLING`
- exact condition for declaring `COMPLETE`
- behavior when final settled weight exceeds allowed tolerance
- behavior when weight becomes unstable after apparent completion

### Calibration Policy Must Define

- how zero/tare is performed
- how span calibration is performed
- who is allowed to perform calibration
- what evidence is logged for each calibration event
- what tolerance is required for calibration acceptance

---

## 7.2 Recommended API Contract

Minimum API endpoints:

### Device and Status

- `GET /api/status`
- `GET /api/device-info`
- `GET /api/network`
- `GET /api/faults`

### Weight and Process

- `GET /api/weight`
- `POST /api/start`
- `POST /api/stop`
- `POST /api/pause` if supported
- `GET /api/process`

### Configuration

- `GET /api/settings`
- `POST /api/settings`
- `POST /api/tare`
- `POST /api/calibration` only for authorized maintenance mode

### Transaction and History

- `GET /api/transactions`
- `GET /api/transactions/latest`
- `POST /api/transactions/export` if needed later

### Health

- `GET /api/health`
- `GET /api/version`
- `GET /api/time`
- `GET /api/capabilities`

### Audit and Diagnostics

- `GET /api/events`
- `GET /api/diagnostics`
- `POST /api/ack-fault`
- `POST /api/test-output` only for authorized maintenance mode

### Suggested `/api/status` response structure

```json
{
  "mode": "STA",
  "ip": "192.168.1.25",
  "state": "FILLING_SLOW",
  "weightKg": 12.48,
  "targetKg": 12.50,
  "progressPercent": 98.4,
  "valves": {
    "fast": false,
    "slow": true,
    "main": true
  },
  "faults": [],
  "activeTransactionId": "TXN-20260407-0012",
  "lastStableWeightKg": 12.46,
  "reasonCode": null,
  "requiresOperatorAck": false,
  "uiConnected": true,
  "firmwareVersion": "1.0.0",
  "configVersion": "1.0.0",
  "deviceTime": "2026-04-07T12:00:00+05:00"
}
```

---

## 7.3 State Machine

Recommended states:

- `BOOT`
- `IDLE`
- `READY`
- `VALIDATING`
- `FILLING_FAST`
- `FILLING_SLOW`
- `SETTLING`
- `COMPLETE`
- `ABORTED`
- `FAULT`
- `MAINTENANCE`

### State Rules

- Only valid transitions are allowed
- Every transition must be logged
- Fault entry must shut outputs to safe condition unless process safety demands defined hold behavior
- Browser refresh must not reset state

### Required Transition Table

The design freeze must include a transition table that defines for each state:

- allowed entry events
- allowed exit events
- outputs that must be energized
- outputs that must be de-energized
- timeout behavior
- operator actions accepted
- operator actions rejected
- fault actions

### Required Recovery Policy

The state machine specification must explicitly define boot behavior after:

- clean power-on in idle state
- reboot during `VALIDATING`
- reboot during `FILLING_FAST`
- reboot during `FILLING_SLOW`
- reboot during `SETTLING`
- storage write interruption
- sensor read timeout
- output driver mismatch or feedback failure if available

Default recovery expectation:

- all outputs move to safe state on boot
- interrupted transactions are marked `ABORTED` or `FAULT` with reason code
- automatic process resume is disabled unless a separately approved policy explicitly allows it
- operator acknowledgment is required before returning to `READY`

---

## 8. Reliability and Safety Principles

## 8.1 Reliability Rules

1. loss of UI must not stop controller logic
2. loss of WiFi must not corrupt process state
3. reconnect must reload current state from device
4. APIs must be idempotent where possible
5. invalid commands must be rejected safely

## 8.2 Safety Rules

1. start only when system is in valid ready state
2. outputs must default to safe state on boot
3. emergency stop behavior must override UI commands
4. over-target and abnormal sensor behavior must trigger controlled stop/fault
5. calibration and maintenance functions must be protected

## 8.2A Required Safety Inputs and Conditions

The design must explicitly decide whether each of the following exists, how it is wired, and how firmware responds:

- emergency stop input
- gas leak input
- door or panel interlock
- pressure switch or overpressure alarm
- valve feedback input if available
- motor/pump ready input if used
- mains/power-fail indication if available

For each safety-related input, the project must define:

- normal electrical state
- active fault state
- debounce/filter rule
- required shutdown action
- whether operator acknowledgment is required for reset

## 8.2B Safe Output Philosophy

For every controlled actuator the plan must state:

- whether the device is energized-to-open or energized-to-close
- intended fail-safe position on power loss
- boot default output state
- fault default output state
- watchdog/reset output state

No actuator output mapping is complete until this is documented.

## 8.3 Data Integrity Rules

- settings writes must be validated before commit
- transaction records must include result and reason code
- version and config checksum should be visible in diagnostics

## 8.4 Security and Authority Rules

The system must define local authority boundaries even if internet connectivity is not used.

Minimum required roles:

- operator
- maintenance
- admin/commissioning

Minimum required rules:

- calibration endpoints require maintenance or admin authority
- network and commissioning settings require admin authority
- production start/stop permissions must be explicitly defined
- AP mode access must have a defined password/bootstrap policy
- default credentials must be changed before production release
- all sensitive setting changes must be logged with user/role and timestamp
- session timeout or re-auth policy must be defined for maintenance functions

If authentication is intentionally omitted for an early lab build, that omission must be stated as a temporary exception and must not be allowed in pilot deployment acceptance.

## 8.5 Time, Audit, and Traceability Rules

The project must define how device time is maintained in both AP and STA operation.

Minimum audit requirements:

- all state transitions logged
- all start/stop/abort commands logged
- all config changes logged
- all calibration actions logged
- all faults and acknowledgments logged
- each transaction linked to firmware version and config version

If network time is unavailable, the system must define:

- how timestamps are represented
- whether operator time-set is allowed
- how unsynchronized time is flagged in exported records

## 8.6 Alarm Management Philosophy

Alarm handling must be defined as a disciplined operating model, not just a collection of fault messages.

Minimum alarm priority classes:

- critical
- high
- medium
- low

Minimum rules:

- critical alarms force immediate safe-state action
- high alarms stop or inhibit production according to the safety matrix
- medium alarms may allow controlled completion only if explicitly approved by policy
- low alarms are advisory and must not be used to hide safety-relevant abnormal conditions
- alarm priority, latch behavior, and acknowledgment requirement must be defined for every alarm
- alarm text shown to operators must be short, specific, and action-oriented
- cleared alarms and acknowledged alarms must remain visible in audit history
- nuisance alarm suppression rules must be documented and justified

The design freeze must include an alarm register containing:

- alarm id
- alarm title
- priority
- trigger condition
- automatic machine action
- operator-visible text
- acknowledgment requirement
- reset condition
- audit/event code

## 8.7 Performance and Timing Budget

The system must define timing budgets before firmware implementation is treated as complete.

Minimum timing items to freeze:

- controller scan/update cycle target
- measurement sampling rate target
- UI polling or push-update target
- input debounce times
- nozzle-lock verification timeout
- state transition timeout values
- interlock reaction target
- watchdog supervision window
- boot-to-safe-state target time

Each timing value must include:

- nominal target
- maximum acceptable limit
- validation method

If actual platform performance cannot meet a timing requirement, the requirement must be revised explicitly before release rather than degraded silently.

---

## 9. Execution Order

This section defines the implementation sequence Codex must follow.

## Phase 0 — Discovery and Constraints Freeze

### Deliverables

- confirmed board model and firmware environment
- confirmed KCS V2 capabilities and limits
- confirmed I/O mapping
- confirmed weight integration method
- confirmed actuator output mapping
- confirmed network mode requirements
- confirmed hard safety boundary and interlock ownership
- confirmed measurement subsystem contract

### Mandatory Checks

- verify exact flashing toolchain
- verify file system/web asset support
- verify whether WebSocket is supported in chosen build
- verify persistent storage method
- verify OTA support or lack of OTA
- verify boot behavior of output pins during reset
- verify flash/storage endurance assumptions

### Exit Criteria

- technical assumptions list approved
- pin map approved
- firmware strategy approved
- safety boundary approved
- recovery policy approved

---

## Phase 1 — System Design Freeze

### Deliverables

- API specification
- frontend screen map
- state machine definition
- settings schema
- transaction schema
- failure mode table
- safety input/output truth table
- measurement and pricing specification
- authority/role matrix
- recovery matrix

### Exit Criteria

- all routes named
- all process states defined
- all operator actions mapped to logic handlers
- all fault actions mapped to safe output behavior
- all restart scenarios assigned explicit outcomes

---

## Phase 2 — Repository and Build Foundation

### Deliverables

- source repository structure
- firmware project skeleton
- frontend asset structure
- environment config template
- release versioning format
- scripts for build, flash, backup, and test

### Suggested repository structure

```text
project-root/
  firmware/
    src/
    include/
    lib/
    data/
    platformio.ini or equivalent
  webui/
    src/
    dist/
  scripts/
    flash/
    backup/
    restore/
    test/
  docs/
    architecture/
    sop/
    release-notes/
  tests/
    api/
    playwright/
    hardware/
```

### Exit Criteria

- one-command local build works
- one-command asset packaging works
- version stamping works

---

## Phase 3 — Core Firmware Development

### Build Order

1. boot/init
2. storage/config
3. network manager
4. web server
5. status endpoints
6. weight read abstraction
7. output control abstraction
8. process engine
9. transaction logging
10. diagnostics and OLED output

### Exit Criteria

- device boots consistently
- status endpoint returns valid JSON
- settings persist across restart
- outputs are controllable in test mode
- outputs assume documented safe states on boot/reset
- reason codes are emitted for aborted and faulted runs

---

## Phase 4 — Web UI Development

### Build Order

1. dashboard shell
2. live status card
3. operator input form
4. start/stop controls
5. progress and stage view
6. fault banner
7. settings page
8. transaction history page
9. manager/maintenance view if required

### UX Requirements

- mobile first
- large controls
- high contrast
- readable status at distance
- clear complete/fault screens

### Exit Criteria

- UI loads from device
- form validation works
- reconnect reloads live state correctly

---

## Phase 5 — Integration

### Integrate

- API to firmware
- UI to API
- state machine to outputs
- sensor data to live display
- transaction save to completion event

### Exit Criteria

- start command flows end to end
- live status reflects real device state
- stop/fault paths behave correctly
- interrupted-run behavior matches documented recovery policy
- transaction log contains complete audit fields

---

## Phase 6 — Automated Testing

### Includes

- API tests
- UI tests with Playwright
- negative tests
- reconnect tests
- browser refresh state recovery tests
- schema compatibility tests
- transaction/audit log tests

### Exit Criteria

- critical user journey passes automatically
- no major API contract mismatch
- all critical reason codes validated in automated or simulated tests

---

## Phase 7 — Hardware Validation

### Includes

- dry-run testing without gas
- sensor simulation
- output verification
- disconnect scenarios
- power cycle behavior
- boot/reset output-state verification
- brownout and watchdog recovery checks
- safety input trip verification
- prolonged soak run with repeated cycles

### Exit Criteria

- all dry-run scenarios pass
- no unsafe output behavior on restart/fault
- all required interlocks verified against expected safe state
- rollback rehearsal on target hardware passes

---

## Phase 8 — Pilot Deployment

### Includes

- controlled site install
- known-good firmware release
- config backup
- operator SOP handoff
- observation period
- authority/account setup
- acceptance of site-specific safety matrix

### Exit Criteria

- stable operation in target environment
- signed acceptance checklist
- documented operator and maintenance signoff completed

---

## 10. Codex SOP

This section defines how Codex should handle development and operational workflow.

## 10.1 Codex Role

Codex is responsible for generating and maintaining:

- firmware source code
- web UI source code
- API definitions
- upload/flash scripts
- backup/restore scripts
- test scripts
- Playwright tests
- release notes and SOP artifacts

Codex should not assume hardware behavior that has not been verified. Any uncertainty must be marked as an assumption in code comments and documentation.

---

## 10.2 Codex Working Rules

1. never change pin mappings without updating docs and tests
2. never change API contracts without version note
3. never deploy untagged firmware
4. always back up device config before flashing production unit
5. always run automated tests before release packaging
6. always record firmware version, git commit, and test result summary
7. always preserve rollback package for previous stable release

---

## 10.3 Codex Development Flow

### Step 1 — Read Inputs

Codex must first collect:

- board variant
- current firmware base
- upload method
- pin map
- endpoint list
- UI screen requirements
- test environment addresses

### Step 2 — Build/Modify Code

Codex generates:

- firmware modules
- UI assets
- tests
- scripts

### Step 3 — Static Validation

Codex runs:

- formatting
- linting
- compile/build checks
- JSON/schema validation

### Step 4 — Package Assets

Codex prepares:

- firmware binary
- web asset bundle
- manifest/version file
- release note draft

### Step 5 — Flash / Upload

Codex executes approved flash SOP only after backup step succeeds.

### Step 6 — Verification

Codex validates:

- device reachable
- version endpoint correct
- APIs responding
- UI reachable
- smoke tests passing

### Step 7 — Automated Browser Test

Codex runs Playwright against device UI.

### Step 8 — Record Outcome

Codex stores:

- version
- date/time
- target device
- config backup reference
- pass/fail evidence

---

## 10.4 Pre-Flash SOP

Before any upload to a real board, Codex must perform the following sequence.

### Pre-Flash Checklist

- confirm target device identifier
- confirm correct serial/USB port
- confirm power stability
- confirm current firmware version
- back up config if device already commissioned
- save previous binary if readable and available
- confirm release tag and checksum
- confirm dry-run tests already passed

### Required Artifacts Before Flash

- firmware binary
- filesystem/web assets package if separate
- config backup file
- release notes
- rollback package

---

## 10.5 Upload / Flash SOP

Because KC868-A6 may be deployed under different firmware workflows, the exact command depends on the approved toolchain. Codex must parameterize this rather than hardcoding assumptions.

### Generic Flash Workflow

1. connect board by USB/serial
2. detect port
3. validate target identity
4. erase only if required by release policy
5. flash firmware image
6. upload filesystem/web assets if separate
7. reboot device
8. poll health endpoint
9. verify firmware version endpoint
10. run smoke tests

### Example Script Responsibilities

#### `scripts/flash/flash_device.sh`

Should:

- accept device port and environment name
- print target summary
- flash firmware
- upload web assets if needed
- output success/failure clearly

#### `scripts/backup/backup_config.sh`

Should:

- call export or read config path
- save file with device id and timestamp
- verify backup integrity

#### `scripts/restore/restore_config.sh`

Should:

- push prior config to device
- reboot if needed
- verify settings match expected checksum

---

## 10.6 Post-Flash Verification SOP

Immediately after upload, Codex must verify:

1. device boots without fault loop
2. AP or STA network comes up as expected
3. `/api/health` returns success
4. `/api/version` matches intended release
5. `/api/status` returns valid state payload
6. UI homepage loads correctly
7. start/stop endpoints reject invalid states safely
8. weight display updates
9. persistent settings survive reboot

No firmware is considered deployed until these checks pass.

---

## 10.7 Rollback SOP

Rollback must be possible within one controlled procedure.

### Rollback Triggers

- boot instability
- API failure
- UI asset corruption
- unsafe output behavior
- repeated runtime fault after upgrade

### Rollback Steps

1. stop production use of unit
2. flash previous stable firmware
3. restore previous config
4. verify health and version
5. run smoke tests
6. document rollback cause

---

## 11. Playwright Test SOP

Playwright is used for browser-level HMI validation.

## 11.1 Purpose

To prove that the KC868-hosted web UI works in real browser conditions, including mobile-like viewport and reconnect behavior.

The HMI test strategy must also confirm that the interface supports fast operator comprehension under normal and abnormal conditions.

## 11.1A HMI Design Philosophy

The user interface must be designed primarily for safe and efficient operation, not visual novelty.

Minimum HMI rules:

- normal operation should remain visually calm and low-noise
- abnormal conditions must stand out immediately
- critical actions must be obvious and reachable with minimal navigation
- values that drive operator decisions must be readable at a glance
- screens must avoid dense layouts that hide machine state or alarms
- labels and controls must use consistent terminology across API, firmware, UI, and SOP documentation

## 11.1B HMI Screen Hierarchy

The design should define screens by operational purpose.

Minimum screen classes:

- overview/status screen
- unit control/operator screen
- equipment detail/diagnostic screen
- maintenance/commissioning screen

Each screen class must define:

- target user role
- actions allowed
- critical values shown
- alarms shown
- navigation path back to safe operational control

## 11.2 Test Environment

Tests should run against:

- AP mode IP
- STA mode IP when available
- a stable test board or simulator if hardware is not always available

### Variables to Parameterize

- base URL
- login/auth mode if used
- polling interval expectations
- mock or real sensor mode
- role under test
- device time mode
- viewport profile

---

## 11.3 Minimum Playwright Test Cases

### Smoke

- homepage loads
- dashboard elements visible
- status card populates

### Operator Flow

- enter rate per kg
- enter target kg or amount
- submit start
- status changes to filling state
- progress updates
- complete screen appears
- final amount and final weight displayed consistently
- transaction identity fields shown when policy requires

### Fault Handling

- invalid input blocked
- API fault banner shown
- disconnected device handled gracefully
- operator acknowledgment flow validated
- unauthorized maintenance action rejected

### Reconnect Handling

- page refresh during run
- browser reconnect after temporary network loss
- state resync successful
- reconnect after controller reboot shows interrupted state correctly

### Settings

- open settings page
- save valid settings
- invalid settings rejected
- reload confirms persistence
- protected settings require proper authority

### HMI Hierarchy and Clarity

- overview screen shows machine readiness and active alarms clearly
- operator screen exposes only the controls needed for production use
- maintenance screen is isolated from operator flow
- terminology remains consistent between screens and alarm messages

### Safety-Oriented UI Checks

- start button disabled in invalid state
- stop command visible during active run
- fault screen clearly visible
- maintenance/test-output controls hidden from operator role

---

## 11.4 Suggested Playwright Structure

```text
tests/playwright/
  smoke.spec.ts
  operator-flow.spec.ts
  reconnect.spec.ts
  settings.spec.ts
  faults.spec.ts
  helpers/
    api.ts
    selectors.ts
    device-control.ts
```

### Expected Practices

- use stable selectors such as `data-testid`
- avoid brittle CSS selectors
- separate test data from test logic
- capture screenshot on failure
- save trace/video on critical flows

---

## 11.5 Playwright Execution Order

1. health check device
2. open dashboard
3. verify baseline state
4. run smoke tests
5. run operator flow
6. run reconnect tests
7. run settings tests
8. archive artifacts

---

## 12. Test Strategy Beyond Playwright

## 12.1 API Tests

Validate:

- status schema
- settings validation
- command response codes
- fault payload format
- persistence behavior
- role/permission enforcement
- audit/event schema
- reason code catalog completeness
- transaction identity field handling

## 12.2 Hardware-in-the-Loop Tests

Validate:

- valve output mapping
- sensor reading stability
- weight progression logic
- state transitions under real timing
- safe output state on boot/reset
- interlock response time
- calibration repeatability check

## 12.3 Failure Injection Tests

Validate:

- WiFi disconnect
- UI disconnect
- sensor invalid data
- power restart in idle
- restart during noncritical operation if policy allows
- restart during active filling
- brownout during storage write
- stuck-weight or frozen-sensor behavior
- commanded output without expected physical effect if feedback exists

---

## 13. Configuration Management SOP

## 13.1 Configuration Categories

- network settings
- process thresholds
- calibration values
- pricing/rate settings
- operator preferences
- manager settings

## 13.2 Rules

- every config schema has defaults
- every config change is validated
- every config export includes version
- incompatible config versions must be rejected or migrated explicitly

## 13.3 Configuration Classes

Every configuration item must be assigned one of these classes:

- commissioning-only
- maintenance-only
- operator-adjustable
- runtime-derived and read-only

Each class must define:

- who can change it
- whether reboot is required
- whether change is logged
- whether change is allowed during active process

## 13.4 Required Backup Contents

The backup/restore procedure must explicitly state whether it includes:

- network configuration
- process thresholds
- calibration values
- pricing/rate defaults
- user/account settings
- transaction history
- event history

If some categories are intentionally excluded from backup or restore, that limitation must be documented.

## 13.5 Transaction Identity Requirements

The transaction model must explicitly define which identity fields are required, optional, or not used in the target deployment.

Candidate identity fields include:

- transaction id
- device id
- station id
- operator id
- cylinder id or batch/reference id
- nozzle lock verification result
- active recipe or product profile id if applicable

For each identity field the design must define:

- source of value
- validation rule
- when it is captured
- whether operator entry is manual or scanned
- whether the field is mandatory to start or complete a transaction
- whether the field is included in exports and audit logs

If operator identity is required for the target site, the plan must define the authentication method separately from the transaction schema.

---

## 14. Release Management SOP

## 14.1 Versioning

Use semantic versioning:

- MAJOR for breaking changes
- MINOR for feature additions
- PATCH for fixes

Example:

- `1.0.0`
- `1.1.0`
- `1.1.3`

## 14.2 Release Package Must Include

- firmware binary
- web assets package
- config schema version
- release notes
- checksum file
- rollback version reference
- automated test summary

## 14.3 Release Gates

A release is deployable only if:

- build passes
- API tests pass
- Playwright smoke passes
- core hardware verification passes
- backup/rollback package exists
- fault/recovery matrix verification passes
- safe boot/reset behavior verified on hardware
- rollback rehearsal performed for release candidate
- release notes include known limitations and safety assumptions

---

## 15. Documentation Deliverables

Codex should maintain these documents during the project:

1. architecture overview
2. pin mapping table
3. API specification
4. state machine definition
5. flashing SOP
6. commissioning SOP
7. rollback SOP
8. test plan
9. release notes per version
10. operator quick guide
11. safety boundary and interlock matrix
12. recovery matrix
13. reason code catalog
14. transaction and audit schema
15. calibration SOP
16. site acceptance checklist
17. alarm register
18. timing budget and performance validation sheet
19. HMI screen hierarchy and navigation map
20. preventive maintenance and proving SOP

---

## 16. Acceptance Criteria

The project is considered successful when all of the following are true:

1. user can connect by AP or STA mode
2. device serves working browser UI
3. live weight appears on dashboard
4. operator can configure and start a cycle
5. controller continues safely if browser disconnects
6. state resync works after reconnect
7. transaction completes and is recorded
8. settings persist after reboot
9. automated UI tests pass
10. flash/rollback procedure is repeatable and documented
11. boot or reset during an active cycle leads to documented safe recovery behavior
12. all critical safety inputs force the documented safe state
13. transaction log contains operator/action/result/reason/version fields
14. authority restrictions work for maintenance and commissioning actions
15. site acceptance checklist is completed and signed for pilot deployment
16. alarm priority, latch, acknowledgment, and reset behavior are documented and verified
17. timing and response targets are measured and within approved limits
18. HMI hierarchy supports operator, diagnostic, and maintenance use without unsafe overlap
19. periodic proving and maintenance procedures are documented for field operation

---

## 17. Risks and Mitigations

## Risk 1 — Firmware Capability Mismatch

KCS V2 build may not support every desired web feature.

### Mitigation

- verify features before locking architecture
- keep fallback path using HTTP polling

## Risk 2 — Sensor Noise / Weight Instability

### Mitigation

- implement filtering and settling rules
- add stability criteria before completion

## Risk 3 — UI-Control Coupling

### Mitigation

- keep state machine entirely local
- treat UI as command/monitor client only

## Risk 4 — Unsafe Update Process

### Mitigation

- mandatory backup before flash
- mandatory rollback package
- post-flash smoke verification

## Risk 5 — Incomplete Hardware Test Coverage

### Mitigation

- define dry-run and HIL test matrix
- require signoff before live operation

## Risk 6 — Unsafe Safety-Boundary Assumptions

Firmware may be assigned responsibility that should belong to independent protection hardware.

### Mitigation

- freeze safety boundary before implementation
- require electrical review of all critical shutdown paths
- document which protections are firmware-assisted versus independent

## Risk 7 — Ambiguous Recovery After Power Loss

Restart behavior during active filling may be inconsistent or unsafe if not defined early.

### Mitigation

- define a recovery matrix before coding process logic
- test reboot and brownout scenarios on hardware
- require operator acknowledgment after interrupted transactions

## Risk 8 — Pricing or Measurement Discrepancy

Target amount, target kg, and final billed value can diverge if rounding and tolerance rules are not frozen.

### Mitigation

- publish metering and pricing specification
- log authoritative input field and derived values
- test edge cases around rounding, overshoot, and settling

## Risk 9 — Weak Local Access Control

Commissioning or maintenance functions may be reachable by unauthorized users in AP or STA mode.

### Mitigation

- define roles and protected endpoints
- enforce password/bootstrap policy
- block pilot release unless authority model is enabled

## Risk 10 — Inadequate Traceability

Faults, config changes, or calibration events may not be reconstructable after field issues.

### Mitigation

- implement event logging and export
- link transactions to firmware/config versions
- require audit schema verification in testing

## Risk 11 — Alarm Flooding or Ambiguous Alarm Behavior

Poor alarm design can hide the real abnormal situation or create operator confusion during a critical event.

### Mitigation

- define alarm priorities and latching rules
- maintain an alarm register with explicit actions
- test alarm presentation and acknowledgment flows

## Risk 12 — Timing Budget Drift

Network, storage, or UI work may gradually erode deterministic control behavior if timing budgets are not frozen and measured.

### Mitigation

- publish timing targets and maximum limits
- measure performance during validation
- isolate critical control logic from noncritical services

## Risk 13 — HMI Cognitive Overload

An interface that mixes operator and maintenance concerns can increase human error during filling.

### Mitigation

- define screen hierarchy by role and purpose
- keep production flow visually simple
- isolate maintenance actions behind role controls

## Risk 14 — Missing Field Identity Data

Transactions may be operationally or commercially incomplete if operator, cylinder, or station identity is not defined.

### Mitigation

- freeze transaction identity schema
- define mandatory versus optional identifiers
- validate identity capture during testing

---

## 18A. Operational Maintenance and Proving Philosophy

The plan must include a post-deployment verification and maintenance regimen so the system remains trustworthy after commissioning.

Minimum maintenance/proving categories:

- routine operator verification
- scheduled calibration verification
- scheduled interlock verification
- scheduled alarm verification
- backup and restore rehearsal
- firmware/version inventory review

The maintenance SOP must define:

- task frequency
- responsible role
- acceptance tolerance
- lockout condition when a check fails
- required evidence or log entry

Recommended examples to formalize for the target site:

- start-of-shift or weekly scale verification
- scheduled emergency stop functional test
- scheduled nozzle lock verification test
- scheduled gas detector functional check if installed
- periodic review of audit log integrity and storage usage

---

## 18B. Future Enterprise Roadmap Appendix

The initial release remains local-first and self-hosted. However, the long-term roadmap may include enterprise integration if explicitly approved in a later phase.

Potential future expansion areas:

- MQTT telemetry
- Modbus TCP/RTU integration
- SCADA historian integration
- centralized fleet monitoring
- remote release orchestration
- predictive maintenance analytics

Rules for roadmap items:

- future architecture must not weaken local autonomous safety behavior
- cloud or remote features must be additive, not a dependency for basic operation
- metrologically significant behavior must remain controlled and auditable at the device level
- every roadmap item requires a separate scope, security, and compliance review before implementation

---

## 18. Final Engineering Direction

This project should be executed as a **device-hosted industrial web controller**, not as a display-centric embedded gadget. The KC868-A6 is the control and delivery node. The phone browser is the HMI. The OLED is secondary. The process engine is local and autonomous.

The most important engineering principle is this:

**The machine must remain safe and logically correct even when the web UI disappears.**

Everything in code, testing, upload procedure, and release management must reinforce that rule.

---

## 19. Immediate Next Build Order

The recommended immediate work sequence is:

1. freeze pin map and I/O list
2. confirm exact firmware toolchain for KC868-A6
3. freeze hard safety boundary and interlock ownership
4. define measurement subsystem contract
5. define API schema, authority model, and state machine
6. define recovery matrix and reason code catalog
7. publish measurement, pricing, and calibration rules
8. define alarm register and timing budget
9. define HMI screen hierarchy and transaction identity schema
10. scaffold firmware and web project
11. implement `/api/health`, `/api/version`, `/api/status`
12. implement weight abstraction and output abstraction
13. implement filling process engine
14. build dashboard UI
15. add settings persistence
16. add audit/event logging
17. add Playwright smoke and operator flow tests
18. prepare flash, backup, and rollback scripts
19. validate on real hardware
20. publish maintenance/proving SOP for field use

---

## 20. Codex Operational Command Summary

Codex should follow this operational order every cycle:

1. read requirements
2. update code
3. build and lint
4. package firmware and UI
5. back up current device config
6. flash device
7. verify device endpoints
8. run Playwright tests
9. archive results
10. promote or rollback based on result

This is the working SOP for disciplined project delivery.
