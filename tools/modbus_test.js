/**
 * LPG Controller — Modbus TCP Validation Script
 * Tests firmware after Modbus-Fix branch changes.
 * Uses only Node.js built-in modules (net).
 */
'use strict';

const net = require('net');

const HOST = '192.168.0.101';
const PORT = 502;
const TIMEOUT_MS = 3000;

// ── Modbus frame builders ─────────────────────────────────────────────────────

let _txId = 0;
function nextTid() { return (++_txId) & 0xFFFF; }

function mbap(tid, unitId, pduLen) {
  const b = Buffer.alloc(7);
  b.writeUInt16BE(tid,    0);
  b.writeUInt16BE(0x0000, 2); // Protocol ID
  b.writeUInt16BE(pduLen + 1, 4); // +1 for unit ID byte in length field
  b[6] = unitId;
  return b;
}

function fc03(unitId, startAddr, qty) {
  const tid = nextTid();
  const pdu = Buffer.from([0x03, startAddr >> 8, startAddr & 0xFF, qty >> 8, qty & 0xFF]);
  return Buffer.concat([mbap(tid, unitId, pdu.length), pdu]);
}

function fc06(unitId, addr, value) {
  const tid = nextTid();
  const pdu = Buffer.from([0x06, addr >> 8, addr & 0xFF, value >> 8, value & 0xFF]);
  return Buffer.concat([mbap(tid, unitId, pdu.length), pdu]);
}

function fc16(unitId, startAddr, words) {
  const tid = nextTid();
  const qty = words.length;
  const data = Buffer.alloc(5 + qty * 2);
  data.writeUInt16BE(startAddr, 0);
  data.writeUInt16BE(qty, 2);
  data[4] = qty * 2;
  for (let i = 0; i < qty; i++) data.writeUInt16BE(words[i] & 0xFFFF, 5 + i * 2);
  const pdu = Buffer.concat([Buffer.from([0x10]), data]);
  return Buffer.concat([mbap(tid, unitId, pdu.length), pdu]);
}

// ── Float helpers (ABCD — big-endian, high word first) ───────────────────────

function floatToRegs(f) {
  const b = Buffer.allocUnsafe(4);
  b.writeFloatBE(f, 0);
  return [b.readUInt16BE(0), b.readUInt16BE(2)];
}

function regsToFloat(hi, lo) {
  const b = Buffer.allocUnsafe(4);
  b.writeUInt16BE(hi, 0);
  b.writeUInt16BE(lo, 2);
  return b.readFloatBE(0);
}

// ── Low-level TCP send/receive ────────────────────────────────────────────────

function modbusRequest(socket, frame) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    const onData = (d) => { chunks.push(d); tryParse(); };
    const onErr  = (e) => { socket.off('data', onData); reject(e); };

    function tryParse() {
      const buf = Buffer.concat(chunks);
      if (buf.length < 7) return;
      const declared = buf.readUInt16BE(4);
      const total = 6 + declared;
      if (buf.length < total) return;
      socket.off('data', onData);
      socket.off('error', onErr);
      resolve(buf.slice(0, total));
    }

    socket.on('data', onData);
    socket.once('error', onErr);
    socket.write(frame);
  });
}

function parseFC03Response(resp) {
  // resp[7] = FC, resp[8] = byte count, resp[9..] = data
  if (resp[7] & 0x80) return { error: resp[8] };
  const byteCount = resp[8];
  const words = [];
  for (let i = 0; i < byteCount; i += 2) {
    words.push(resp.readUInt16BE(9 + i));
  }
  return { words };
}

function parseWriteResponse(resp) {
  if (resp[7] & 0x80) return { error: resp[8] };
  return { ok: true };
}

// ── Connection helper ─────────────────────────────────────────────────────────

function connect(unitId) {
  return new Promise((resolve, reject) => {
    const sock = new net.Socket();
    sock.setTimeout(TIMEOUT_MS);
    sock.on('timeout', () => { sock.destroy(); reject(new Error('timeout')); });
    sock.on('error', reject);
    sock.connect(PORT, HOST, () => resolve(sock));
  });
}

async function tryConnect(unitId) {
  try {
    const sock = await connect(unitId);
    // Quick probe: read device ID register (0x0018)
    const resp = await modbusRequest(sock, fc03(unitId, 0x0018, 1));
    const r = parseFC03Response(resp);
    if (r.error) { sock.destroy(); return null; }
    return { sock, unitId, deviceId: r.words[0] };
  } catch (e) {
    return null;
  }
}

// ── Report helpers ────────────────────────────────────────────────────────────

const SEP = '─'.repeat(60);
function section(title) { console.log(`\n${SEP}\n  ${title}\n${SEP}`); }
function pass(msg)  { console.log(`  ✓  ${msg}`); }
function fail(msg)  { console.log(`  ✗  ${msg}`); }
function info(msg)  { console.log(`     ${msg}`); }

function fillStateName(code) {
  return ['Idle','Ready','Validating','FillingFast','FillingSlow',
          'Settling','Complete','Aborted','Fault','Maintenance'][code] ?? `Unknown(${code})`;
}
function resultName(code) {
  return ['Idle','Accepted','Busy','Rejected','Done','Failed'][code] ?? `Unknown(${code})`;
}
function errName(code) {
  const names = ['None','InvalidCmd','InvalidSeq','Busy','NotPrepared','SafetyNotReady',
                 'ScaleNotReady','Unstable','CalInvalid','InvalidPreset','NotAllowed',
                 'Timeout','FaultActive','AlreadyFilling','CylinderMissing',
                 'NozzleNotEngaged','CommLost'];
  return names[code] ?? `Unknown(${code})`;
}

function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

// ── Main test sequence ────────────────────────────────────────────────────────

async function run() {
  console.log(`\n${'═'.repeat(60)}`);
  console.log('  LPG Controller — Modbus TCP Validation');
  console.log(`  Target: ${HOST}:${PORT}`);
  console.log(`${'═'.repeat(60)}`);

  // ── Find working unit ID ──────────────────────────────────────────────────
  section('Connection — probing unit IDs 1, 255, 0');
  let conn = null;
  for (const uid of [1, 255, 0]) {
    info(`Trying unit ID ${uid}...`);
    conn = await tryConnect(uid);
    if (conn) {
      pass(`Connected!  Unit ID = ${conn.unitId}  Device ID = 0x${conn.deviceId.toString(16).toUpperCase().padStart(4,'0')}`);
      if (conn.deviceId === 0xA601) pass('Device ID = 0xA601 — correct LPG controller');
      else                           fail(`Device ID unexpected: 0x${conn.deviceId.toString(16)}`);
      break;
    }
    fail(`Unit ID ${uid}: no response`);
  }
  if (!conn) {
    fail('No response on any unit ID. Board unreachable or Modbus TCP disabled.');
    process.exit(1);
  }

  const { sock, unitId } = conn;

  async function read(addr, qty) {
    const resp = await modbusRequest(sock, fc03(unitId, addr, qty));
    return parseFC03Response(resp);
  }
  async function write06(addr, val) {
    const resp = await modbusRequest(sock, fc06(unitId, addr, val));
    return parseWriteResponse(resp);
  }
  async function write16(addr, words) {
    const resp = await modbusRequest(sock, fc16(unitId, addr, words));
    return parseWriteResponse(resp);
  }

  // ── TEST 1: Basic live status ─────────────────────────────────────────────
  section('TEST 1 — Basic live status (0x0000–0x0012)');
  {
    // Live + Tare + Net + Target + Rate + Amount
    const r1 = await read(0x0000, 12);
    if (r1.error) { fail(`FC03 error: 0x${r1.error.toString(16)}`); }
    else {
      const liveKg = regsToFloat(r1.words[0], r1.words[1]);
      const tareKg = regsToFloat(r1.words[2], r1.words[3]);
      const netKg  = regsToFloat(r1.words[4], r1.words[5]);
      info(`Live Weight : ${liveKg.toFixed(3)} kg`);
      info(`Tare Weight : ${tareKg.toFixed(3)} kg`);
      info(`Net Weight  : ${netKg.toFixed(3)} kg`);
    }

    // Fill state + interlocks
    const r2 = await read(0x000E, 5);
    if (r2.error) { fail(`FC03 error: 0x${r2.error.toString(16)}`); }
    else {
      const [fillState, estop, cylinder, nozzle, stable] = r2.words;
      info(`Fill State      : ${fillState} (${fillStateName(fillState)})`);
      info(`E-Stop OK       : ${estop}`);
      info(`Cylinder Present: ${cylinder}`);
      info(`Nozzle Engaged  : ${nozzle}`);
      info(`Weight Stable   : ${stable}`);
      estop   === 1 ? pass('E-Stop OK') : fail('E-Stop NOT OK — safety interlock active');
      stable  === 1 ? pass('Weight stable') : fail('Weight NOT stable — prepare/start will reject');
    }

    // Alarm code
    const r3 = await read(0x0048, 1);
    if (!r3.error) info(`Alarm Code      : ${r3.words[0]}`);
  }

  // ── TEST 2: Confirm new register 0x008F (WdtMode) ─────────────────────────
  section('TEST 2 — WdtMode register 0x008F (new firmware only)');
  {
    const r1 = await read(0x008F, 1);
    if (r1.error) {
      fail(`Read 0x008F failed: Modbus exception 0x${r1.error.toString(16)}`);
      fail('FIRMWARE VERDICT: Register 0x008F does not exist — OLD firmware running');
    } else {
      info(`0x008F (WdtMode) = ${r1.words[0]}`);
      const wr = await write06(0x008F, 0);
      if (wr.error) fail(`Write 0x008F failed: exception 0x${wr.error.toString(16)}`);
      else {
        const r2 = await read(0x008F, 1);
        if (!r2.error && r2.words[0] === 0) pass('Write/read 0x008F = 0 confirmed — NEW firmware running');
        else fail(`WdtMode readback unexpected: ${r2.words?.[0]}`);
      }
    }
  }

  // ── TEST 3: Activity-based liveness (no heartbeat write) ──────────────────
  section('TEST 3 — Activity-based liveness (polling only, no heartbeat write)');
  {
    info('Reading live weight 10× at 500 ms intervals (5 s total)...');
    info('No write to 0x008C (heartbeat). If new firmware: watchdog should NOT fire.');
    for (let i = 0; i < 10; i++) {
      await sleep(500);
      const r = await read(0x0000, 2);
      if (!r.error) {
        const kg = regsToFloat(r.words[0], r.words[1]);
        process.stdout.write(`\r     Poll ${i + 1}/10  LiveWeight = ${kg.toFixed(3)} kg  `);
      }
    }
    console.log();
    const ra = await read(0x008E, 1); // hbAgeSec
    const rt = await read(0x008D, 1); // wdtTimeout
    if (!ra.error && !rt.error) {
      info(`Heartbeat age (0x008E) = ${ra.words[0]} s   WDT timeout (0x008D) = ${rt.words[0]} s`);
      if (ra.words[0] <= rt.words[0] || rt.words[0] === 0) {
        pass('Watchdog NOT expired — activity-based liveness is working');
      } else {
        fail('Heartbeat age > WDT timeout — reads alone did not refresh. Old firmware path?');
      }
    }
  }

  // ── TEST 4: Command 14 — Atomic Prepare Cylinder ──────────────────────────
  section('TEST 4 — Command 14: Atomic Prepare Cylinder');
  {
    // Read current live weight for reference
    const rLive = await read(0x0000, 2);
    const liveKgBefore = rLive.error ? NaN : regsToFloat(rLive.words[0], rLive.words[1]);
    info(`Live weight before prepare: ${liveKgBefore.toFixed(3)} kg`);

    // Write presets: fillMode(0x0086) + target(0x0092/93) + rate(0x0094/95) + amount(0x0096/97)
    // All as a single FC16 burst from 0x0086
    const [tHi, tLo] = floatToRegs(3.0);
    const [rHi, rLo] = floatToRegs(321.45);
    const [aHi, aLo] = floatToRegs(964.35);

    info('Writing presets via FC16 (0x0086–0x0097):');
    info(`  fillMode = 0, targetKG = 3.0, rate = 321.45, amount = 964.35`);
    info(`  targetKG floatRegs: [0x${tHi.toString(16)}, 0x${tLo.toString(16)}]`);

    // 0x0086 = fillMode (1 reg), then gap to 0x0092 (registers 0x0087–0x0091 are status, skip)
    // We can't write the gap as one burst since status regs are read-only.
    // Write fillMode separately, then presets as one burst from 0x0092.
    const wMode = await write06(0x0086, 0);
    if (wMode.error) fail(`fillMode write failed: 0x${wMode.error.toString(16)}`);
    else pass('fillMode = 0 written');

    // Burst write 0x0092–0x0097: targetHi, targetLo, rateHi, rateLo, amtHi, amtLo
    const wPreset = await write16(0x0092, [tHi, tLo, rHi, rLo, aHi, aLo]);
    if (wPreset.error) fail(`Preset write failed: 0x${wPreset.error.toString(16)}`);
    else pass('Preset burst written (target=3.0 kg, rate=321.45, amount=964.35)');

    // Send command 14 via FC16: [code=14, counter=1] to 0x0080
    info('Sending command 14 (PrepareCylinder) via FC16 0x0080–0x0081...');
    const wCmd = await write16(0x0080, [14, 1]);
    if (wCmd.error) fail(`Command FC16 write failed: 0x${wCmd.error.toString(16)}`);

    // Wait for firmware tick to execute
    await sleep(250);

    // Read result block
    const rResult = await read(0x0083, 5); // result, errCode, [busy], fillMode, preparedFlag
    const rPrep   = await read(0x0087, 3); // preparedFlag, readyToPrepare, readyToStart
    const rWeights= await read(0x0002, 6); // tare, net, target (each float32)

    if (rResult.error) { fail(`Result read failed: 0x${rResult.error.toString(16)}`); }
    else {
      const cmdResult  = rResult.words[0];
      const cmdErrCode = rResult.words[1];
      info(`CMD Result    : ${cmdResult} (${resultName(cmdResult)})`);
      info(`CMD Error Code: ${cmdErrCode} (${errName(cmdErrCode)})`);

      if (cmdResult === 4 /* Done */ && cmdErrCode === 0) {
        pass('Command 14 executed: Done, no error');
      } else if (cmdResult === 3 /* Rejected */ && cmdErrCode === 11 /* Timeout */) {
        fail('REJECTED with error 11 (Timeout/CommLost) — activity-based liveness NOT working');
        fail('DIAGNOSIS: Old firmware OR notifyModbusActivity() patch not active');
      } else if (cmdResult === 3 && cmdErrCode === 7 /* Unstable */) {
        fail('REJECTED: Weight not stable — place stable load and retry');
      } else if (cmdResult === 3 && cmdErrCode === 14 /* CylinderMissing */) {
        fail('REJECTED: Cylinder not detected on scale');
      } else if (cmdResult === 3 && cmdErrCode === 5 /* SafetyNotReady */) {
        fail('REJECTED: E-Stop or safety interlock not satisfied');
      } else if (cmdResult === 3 && cmdErrCode === 12 /* FaultActive */) {
        fail('REJECTED: Controller in Fault state — send Reset (cmd 32) first');
      } else if (cmdResult === 3 && cmdErrCode === 16 /* CommLost */) {
        fail('REJECTED: CommLost (new code 16) — hbAgeSec exceeded wdtTimeout');
      } else if (cmdResult === 3) {
        fail(`REJECTED with error ${cmdErrCode} (${errName(cmdErrCode)})`);
      } else if (cmdResult === 0 /* Idle */) {
        fail('Result still Idle — command may not have fired (counter duplicate or pendWrite not reached)');
      } else {
        info(`Result: ${resultName(cmdResult)} / ${errName(cmdErrCode)}`);
      }
    }

    if (!rPrep.error) {
      const [prepFlag, rtp, rts] = rPrep.words;
      info(`Prepared Flag   : ${prepFlag}`);
      info(`ReadyToPrepare  : ${rtp}`);
      info(`ReadyToStart    : ${rts}`);
      prepFlag === 1 ? pass('Prepared Flag = 1') : fail('Prepared Flag = 0 — prepare did not complete');
      rts      === 1 ? pass('ReadyToStart = 1') : info('ReadyToStart = 0 (may need weight stable)');
    }

    if (!rWeights.error) {
      const tareKg   = regsToFloat(rWeights.words[0], rWeights.words[1]);
      const netKg    = regsToFloat(rWeights.words[2], rWeights.words[3]);
      const targetKg = regsToFloat(rWeights.words[4], rWeights.words[5]);
      info(`Tare Weight : ${tareKg.toFixed(3)} kg  (expected ≈ ${liveKgBefore.toFixed(3)} kg)`);
      info(`Net Weight  : ${netKg.toFixed(3)} kg  (expected ≈ 0)`);
      info(`Target KG   : ${targetKg.toFixed(3)} kg  (expected = 3.000)`);
      Math.abs(tareKg - liveKgBefore) < 0.5 ? pass('Tare ≈ live weight before prepare') : fail(`Tare mismatch: ${tareKg.toFixed(3)} vs ${liveKgBefore.toFixed(3)}`);
      Math.abs(netKg)  < 0.1           ? pass('Net ≈ 0') : fail(`Net not zero: ${netKg.toFixed(3)}`);
      Math.abs(targetKg - 3.0) < 0.01  ? pass('Target = 3.0 kg') : fail(`Target wrong: ${targetKg.toFixed(3)}`);
    }
  }

  // ── TEST 5: Command counter duplicate protection ───────────────────────────
  section('TEST 5 — Command counter duplicate protection');
  {
    // First: re-send same counter value (1) — should NOT re-execute
    info('Sending command 14 again with same counter value (1)...');
    await write16(0x0080, [14, 1]);
    await sleep(250);
    // Read lastAcceptedSeq
    const r = await read(0x0082, 1);
    if (!r.error) info(`LastAcceptedSeq (0x0082) = ${r.words[0]}  (should still = 1 if dedup works)`);

    // Second: increment counter to 2 — should fire again
    info('Sending command 14 with counter = 2 (new counter)...');
    await write16(0x0080, [14, 2]);
    await sleep(250);
    const r2 = await read(0x0083, 2);
    if (!r2.error) {
      info(`CMD Result = ${r2.words[0]} (${resultName(r2.words[0])})  Error = ${r2.words[1]} (${errName(r2.words[1])})`);
      r2.words[0] === 4 ? pass('Counter=2 re-executed successfully') : info(`Counter=2 result: ${resultName(r2.words[0])}`);
    }
  }

  // ── TEST 6: Start and Stop ────────────────────────────────────────────────
  section('TEST 6 — Start (cmd 30) then Stop (cmd 31)');
  {
    const rPrep = await read(0x0087, 1);
    if (rPrep.error || rPrep.words[0] !== 1) {
      info('Skipping Start test — PreparedFlag not set (prepare must succeed first)');
    } else {
      info('PreparedFlag = 1. Sending Start (cmd 30, counter = 3)...');
      await write16(0x0080, [30, 3]);
      await sleep(300);

      const rStart = await read(0x0083, 2);
      const rState = await read(0x000E, 1);
      const rCanStop = await read(0x008B, 1);

      if (!rStart.error) {
        info(`Start CMD Result : ${rStart.words[0]} (${resultName(rStart.words[0])})`);
        info(`Start CMD Err    : ${rStart.words[1]} (${errName(rStart.words[1])})`);
        rStart.words[0] === 1 /* Accepted */ ? pass('Start accepted') : fail(`Start not accepted: ${resultName(rStart.words[0])}`);
      }
      if (!rState.error)  info(`Fill State : ${rState.words[0]} (${fillStateName(rState.words[0])})`);
      if (!rCanStop.error) info(`Can Stop   : ${rCanStop.words[0]}`);

      // Immediately stop
      info('Sending Stop (cmd 31, counter = 4)...');
      await write16(0x0080, [31, 4]);
      await sleep(300);

      const rStop  = await read(0x0083, 2);
      const rState2= await read(0x000E, 1);
      if (!rStop.error)  info(`Stop CMD Result : ${rStop.words[0]} (${resultName(rStop.words[0])})`);
      if (!rState2.error) {
        info(`Fill State after stop: ${rState2.words[0]} (${fillStateName(rState2.words[0])})`);
        (rState2.words[0] === 7 /* Aborted */ || rState2.words[0] === 0 /* Idle */)
          ? pass('Fill stopped/aborted') : info(`State after stop: ${fillStateName(rState2.words[0])}`);
      }
    }
  }

  // ── TEST 7: Fill record registers ─────────────────────────────────────────
  section('TEST 7 — Fill record registers (0x009C–0x00B0)');
  {
    const r = await read(0x009C, 21);
    if (r.error) {
      fail(`Read 0x009C failed: exception 0x${r.error.toString(16)}`);
      fail('FIRMWARE VERDICT: Fill record registers do not exist — OLD firmware or kHR_Count not updated');
    } else {
      const [idHi, idLo, result, errCode, mode,
             tgtHi, tgtLo, actHi, actLo, rateHi, rateLo,
             tgtAmtHi, tgtAmtLo, finalAmtHi, finalAmtLo,
             tareHi, tareLo, durHi, durLo, pendingAck, /*ack*/] = r.words;

      const recId   = (idHi << 16) | idLo;
      const target  = regsToFloat(tgtHi, tgtLo);
      const actual  = regsToFloat(actHi, actLo);
      const rate    = regsToFloat(rateHi, rateLo);
      const tgtAmt  = regsToFloat(tgtAmtHi, tgtAmtLo);
      const finalAmt= regsToFloat(finalAmtHi, finalAmtLo);
      const tare    = regsToFloat(tareHi, tareLo);
      const durSec  = (durHi << 16) | durLo;

      pass('Fill record registers readable (new firmware confirmed)');
      info(`Record ID    : ${recId}`);
      info(`Result       : ${result} (${resultName(result)})`);
      info(`Error Code   : ${errCode} (${errName(errCode)})`);
      info(`Mode         : ${mode} (${mode === 0 ? 'by-kg' : 'by-amount'})`);
      info(`Target KG    : ${target.toFixed(3)}`);
      info(`Actual Net KG: ${actual.toFixed(3)}`);
      info(`Rate PKR/kg  : ${rate.toFixed(2)}`);
      info(`Target Amount: ${tgtAmt.toFixed(2)}`);
      info(`Final Amount : ${finalAmt.toFixed(2)}`);
      info(`Tare KG      : ${tare.toFixed(3)}`);
      info(`Duration sec : ${durSec}`);
      info(`Pending Ack  : ${pendingAck}`);

      if (pendingAck === 1) {
        info('Acking fill record (writing 1 to 0x00B0)...');
        const wr = await write06(0x00B0, 1);
        if (!wr.error) {
          await sleep(200);
          const r2 = await read(0x00AF, 1);
          if (!r2.error && r2.words[0] === 0) pass('PendingAck cleared after ack write');
          else fail(`PendingAck not cleared: ${r2.words?.[0]}`);
        }
      } else {
        info('PendingAck = 0 (no completed fill yet — run a fill to populate record)');
      }
    }
  }

  // ── TEST 8: Watchdog mode ─────────────────────────────────────────────────
  section('TEST 8 — Watchdog mode register 0x008F');
  {
    // Write 0 (warn-only)
    await write06(0x008F, 0);
    const r0 = await read(0x008F, 1);
    if (!r0.error) {
      r0.words[0] === 0 ? pass('WdtMode = 0 (warn-only) confirmed') : fail(`WdtMode readback: ${r0.words[0]}`);
    }

    // Write 1 (stop-fill)
    await write06(0x008F, 1);
    const r1 = await read(0x008F, 1);
    if (!r1.error) {
      r1.words[0] === 1 ? pass('WdtMode = 1 (stop-fill) confirmed') : fail(`WdtMode readback: ${r1.words[0]}`);
    }

    // Reset to safe default
    await write06(0x008F, 0);
    pass('WdtMode reset to 0 (warn-only)');
  }

  // ── Summary ───────────────────────────────────────────────────────────────
  section('SUMMARY');
  {
    const rFw = await read(0x0018, 1); // DeviceId
    const rUp = await read(0x0015, 2); // Uptime UINT32
    if (!rFw.error) info(`Device ID : 0x${rFw.words[0].toString(16).toUpperCase()}`);
    if (!rUp.error) {
      const upSec = (rUp.words[0] << 16) | rUp.words[1];
      info(`Uptime    : ${upSec} s  (${Math.floor(upSec/60)} min)`);
    }
    // Read full HMI block summary
    const rHmi = await read(0x0080, 16);
    if (!rHmi.error) {
      const [code, seq, lastSeq, result, errCode, busy, fillMode, prepFlag,
             rtp, rts, canTare, canStop, hb, wdtTimeout, hbAge, wdtMode] = rHmi.words;
      info(`HMI CommandCode : ${code}   Seq : ${seq}   LastAccepted : ${lastSeq}`);
      info(`HMI Result : ${result} (${resultName(result)})   Err : ${errCode} (${errName(errCode)})`);
      info(`PreparedFlag : ${prepFlag}   RTP : ${rtp}   RTS : ${rts}`);
      info(`HbAge : ${hbAge} s   WdtTimeout : ${wdtTimeout} s   WdtMode : ${wdtMode}`);
    }
    info('');
    info('Test complete.');
  }

  sock.destroy();
}

run().catch(e => {
  console.error('\n[FATAL]', e.message);
  process.exit(1);
});
