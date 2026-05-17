'use strict';
// Modbus RTU live-weight diagnostic probe.
//
// Reads LiveWeight (reg 0-1), DeviceId (reg 24), and nearby registers.
// Decodes Float32 in all four word/byte orderings so you can see which
// one gives a physically plausible kg value.
//
// Firmware facts (from ModbusRegisterCache.cpp setFloat):
//   reg[addr]   = bits >> 16  (HIGH word first)
//   reg[addr+1] = bits & 0xFFFF
//   => ABCD encoding (big-endian, high-word-first, standard Modbus)
//
// Serial settings confirmed on bench (2026-05-08):
//   Port: COM10, Baud: 9600, Format: 8N1, Slave ID: 1
//   COM2 = FTDI FT232 adapter (set Windows FTDI latency timer to 1ms for best RTU framing)
//
// FTDI NOTE: FTDI chips default to 16ms USB latency which breaks RTU inter-character
//   timing. Fix: Device Manager > COM2 > Port Settings > Advanced > Latency Timer = 1ms
//
// Usage:
//   node tools/modbus-liveweight-probe.js --port COM2  --baud 9600 --id 1
//   node tools/modbus-liveweight-probe.js --port COM2  --baud-scan
//   npm run probe          -- --port COM2 --baud 9600 --id 1
//   npm run probe:com2
//   npm run probe:com2:scan
//   npm run probe9600      -- --port COM10
//   npm run probe115200    -- --port COM10

const { Command } = require('commander');
const ModbusRTU  = require('modbus-serial');

const program = new Command();
program
  .name('modbus-liveweight-probe')
  .option('--port <port>',        'Serial port (e.g. COM2, COM10, /dev/ttyUSB0)', 'COM10')
  .option('--baud <baud>',        'Baud rate', '9600')
  .option('--id <id>',            'Modbus slave ID', '1')
  .option('--samples <n>',        'Number of sample loops (0 = infinite)', '20')
  .option('--interval <ms>',      'Milliseconds between samples', '500')
  .option('--timeout <ms>',       'Modbus request timeout ms (0 = auto from baud rate)', '0')
  .option('--retries <n>',        'Retry count per read on error', '3')
  .option('--retry-delay <ms>',   'Delay between retries ms', '100')
  .option('--req-gap <ms>',       'Gap between sequential reads in one sample ms', '20')
  .option('--baud-scan',          'Scan all common baud rates and report which responds')
  .parse(process.argv);

const opts = program.opts();
const PORT         = opts.port;
const BAUD         = parseInt(opts.baud,       10);
const SLAVE_ID     = parseInt(opts.id,         10);
const SAMPLES      = parseInt(opts.samples,    10);
const INTERVAL     = parseInt(opts.interval,   10);
const RETRIES      = parseInt(opts.retries,    10);
const RETRY_DELAY  = parseInt(opts.retryDelay, 10);
const REQ_GAP      = parseInt(opts.reqGap,     10);
const BAUD_SCAN    = opts.baudScan || false;

// Auto-compute recommended timeout from baud rate.
// Formula: max(200, ceil(frame_bytes_at_max_response / baud * 10000) + device_processing_margin)
// Practical minimums validated against ESP32 Modbus response times.
const BAUD_TIMEOUT_MAP = {
    1200:   2000,
    2400:   1000,
    4800:    500,
    9600:    500,
    19200:   300,
    38400:   250,
    57600:   200,
   115200:   200,
};

function recommendedTimeout(baud) {
  return BAUD_TIMEOUT_MAP[baud] || Math.max(200, Math.ceil(37 / baud * 10000) + 200);
}

function recommendedRetryDelay(baud) {
  // At least 3.5 char times (RTU inter-frame gap) plus USB round-trip margin
  const charMs = (1 / baud) * 10 * 1000;
  return Math.max(50, Math.ceil(charMs * 3.5) + 30);
}

const rawTimeout  = parseInt(opts.timeout, 10);
const TIMEOUT     = rawTimeout > 0 ? rawTimeout : recommendedTimeout(BAUD);

// Expected DeviceId from firmware (ModbusRegisterCache.cpp line 61)
const EXPECTED_DEVICE_ID = 0xA601;  // decimal 42497

// Register addresses (0-based PDU, matching firmware kHR_* constants)
const REG_LIVEWEIGHT_HI = 0x0000;
const REG_DEVICEID      = 0x0018;  // decimal 24

// ── Float32 decoders ─────────────────────────────────────────────────────────

function decodeFloat_ABCD(hi, lo) {
  const buf = Buffer.alloc(4);
  buf.writeUInt16BE(hi, 0);
  buf.writeUInt16BE(lo, 2);
  return buf.readFloatBE(0);
}

function decodeFloat_CDAB(hi, lo) {
  const buf = Buffer.alloc(4);
  buf.writeUInt16BE(lo, 0);
  buf.writeUInt16BE(hi, 2);
  return buf.readFloatBE(0);
}

function decodeFloat_BADC(hi, lo) {
  const buf = Buffer.alloc(4);
  buf.writeUInt16LE(hi, 0);
  buf.writeUInt16LE(lo, 2);
  return buf.readFloatBE(0);
}

function decodeFloat_DCBA(hi, lo) {
  const buf = Buffer.alloc(4);
  buf.writeUInt16LE(lo, 0);
  buf.writeUInt16LE(hi, 2);
  return buf.readFloatBE(0);
}

function isPlausibleKg(v) {
  return isFinite(v) && v >= -5 && v <= 100;
}

function fmtFloat(v) {
  if (!isFinite(v)) return v.toString().padStart(12);
  return v.toFixed(4).padStart(12);
}

function fmtWord(w) {
  return `0x${w.toString(16).toUpperCase().padStart(4, '0')} (${w})`;
}

function delay(ms) {
  return new Promise(r => setTimeout(r, ms));
}

// ── Retry wrapper ─────────────────────────────────────────────────────────────

async function readWithRetry(client, reg, count) {
  let lastErr;
  for (let attempt = 0; attempt <= RETRIES; attempt++) {
    try {
      return await client.readHoldingRegisters(reg, count);
    } catch (e) {
      lastErr = e;
      if (attempt < RETRIES) {
        await delay(RETRY_DELAY);
      }
    }
  }
  throw lastErr;
}

let sampleCount  = 0;
let commOkCount  = 0;
let commErrCount = 0;

// ── Single sample ─────────────────────────────────────────────────────────────

async function readAndPrint(client) {
  sampleCount++;
  const ts = new Date().toISOString().replace('T', ' ').substring(0, 23);
  console.log(`\n${'═'.repeat(72)}`);
  console.log(`Sample ${sampleCount}  |  ${ts}`);
  console.log(`${'═'.repeat(72)}`);

  // 1. DeviceId sanity check
  let deviceIdOk = false;
  try {
    const dr = await readWithRetry(client, REG_DEVICEID, 1);
    const did = dr.data[0];
    const ok  = did === EXPECTED_DEVICE_ID;
    deviceIdOk = ok;
    if (ok) {
      console.log(`DeviceId (reg 24):  ${fmtWord(did)}  ✓ CORRECT (0xA601)`);
    } else {
      console.log(`DeviceId (reg 24):  ${fmtWord(did)}  ✗ WRONG — expected 0xA601 (${EXPECTED_DEVICE_ID})`);
      console.log('  WARNING: Wrong DeviceId means wrong slave ID, baud, parity, or wiring.');
      console.log('  Do not diagnose Float32 until DeviceId is correct.');
    }
  } catch (e) {
    commErrCount++;
    console.log(`DeviceId read FAILED (${RETRIES} retries exhausted): ${e.message}`);
    console.log('  → Check port, baud rate, slave ID, RS485 wiring, and DE/RE pin.');
    console.log('  → On FTDI/COM2: set Latency Timer to 1ms in Device Manager.');
    return;
  }

  commOkCount++;
  await delay(REQ_GAP);

  // 2. Read registers 0-15
  let regs = null;
  try {
    const rr = await readWithRetry(client, 0, 16);
    regs = rr.data;
  } catch (e) {
    console.log(`Register scan (0-15) FAILED: ${e.message}`);
    return;
  }

  console.log('\nRaw holding registers 0-15:');
  console.log('  ADDR  HEX     DEC');
  for (let i = 0; i < regs.length; i++) {
    const mark = (i === 0) ? ' ← LiveWeight Hi' : (i === 1) ? ' ← LiveWeight Lo' : '';
    console.log(`  [${String(i).padStart(2)}]  0x${regs[i].toString(16).toUpperCase().padStart(4,'0')}  ${String(regs[i]).padStart(5)}${mark}`);
  }

  // 3. Float32 decoding
  const hi = regs[0];
  const lo = regs[1];
  console.log('\nFloat32 decode of reg[0]=Hi, reg[1]=Lo:');
  console.log(`  Hi word: ${fmtWord(hi)}`);
  console.log(`  Lo word: ${fmtWord(lo)}`);
  console.log('');

  const decodings = [
    { label: 'ABCD  (Hi-word-first, big-endian)   ← FIRMWARE ENCODING', fn: decodeFloat_ABCD },
    { label: 'CDAB  (Lo-word-first, word-swap)     ← try if ABCD wrong', fn: decodeFloat_CDAB },
    { label: 'BADC  (byte-swap within Hi-word-first)', fn: decodeFloat_BADC },
    { label: 'DCBA  (full little-endian)            ← try if CDAB wrong', fn: decodeFloat_DCBA },
  ];

  for (const d of decodings) {
    const v     = d.fn(hi, lo);
    const plaus = isPlausibleKg(v) ? '  ← PLAUSIBLE kg' : '';
    const firm  = d.label.includes('FIRMWARE') ? ' *** USE THIS ***' : '';
    console.log(`  ${d.label}`);
    console.log(`    = ${fmtFloat(v)} kg${plaus}${firm}`);
  }

  // NetWeight from the same 0-15 scan (regs 4-5) — no extra read needed
  const net = decodeFloat_ABCD(regs[4], regs[5]);
  console.log(`\nNetWeight (reg 4-5, ABCD): ${fmtFloat(net)} kg`);

  await delay(REQ_GAP);

  // 4. Scale diagnostics
  try {
    const sr = await readWithRetry(client, 0x004C, 4);
    const scaleInit    = sr.data[0];
    const scaleReadErr = sr.data[1];
    const calValid     = sr.data[2];
    const simActive    = sr.data[3];
    console.log('\nScale diagnostics:');
    console.log(`  ScaleInitialized (0x4C): ${scaleInit}  ${scaleInit ? '✓' : '✗ HX711 not ready — LiveWeight will be 0.0'}`);
    console.log(`  ScaleReadError   (0x4D): ${scaleReadErr}  ${scaleReadErr ? '✗ hardware read error' : '✓ OK'}`);
    console.log(`  CalibrationValid (0x4E): ${calValid}  ${calValid ? '✓' : '✗ not calibrated — weight may be wrong'}`);
    console.log(`  SimulationActive (0x4F): ${simActive}  ${simActive ? '! using simulated weight' : '✓ live sensor'}`);
  } catch (e) {
    console.log(`Scale diagnostics read failed: ${e.message}`);
  }

  // 5. Stability hint
  const liveABCD = decodeFloat_ABCD(hi, lo);
  if (isFinite(liveABCD) && !isPlausibleKg(liveABCD)) {
    console.log(`\n⚠  ABCD live weight (${liveABCD.toFixed(4)} kg) is outside plausible 0-100 kg range.`);
    console.log('   Check calibration (CalibrationValid above) and ABCD byte order in Weintek.');
  }

  console.log(`\nComm stats: ${commOkCount} OK / ${commErrCount} errors`);
  if (!deviceIdOk) {
    console.log('\n⚠  DeviceId mismatch — diagnose serial settings before continuing.');
  }
}

// ── Baud rate scanner ─────────────────────────────────────────────────────────

const SCAN_BAUDS = [9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200];

async function baudScan() {
  console.log(`\nBaud Rate Scanner — Port: ${PORT}  Slave ID: ${SLAVE_ID}`);
  console.log('Trying each baud rate with 3 attempts at DeviceId (reg 24)...\n');
  console.log('  Baud     Result');
  console.log('  ───────  ──────────────────────────────────────────');

  for (const baud of SCAN_BAUDS) {
    const tmo = recommendedTimeout(baud);
    const client = new ModbusRTU();
    client.setTimeout(tmo);
    try {
      await client.connectRTUBuffered(PORT, { baudRate: baud, dataBits: 8, parity: 'none', stopBits: 1 });
      client.setID(SLAVE_ID);
    } catch (e) {
      console.log(`  ${String(baud).padStart(6)}   ✗ port open failed: ${e.message}`);
      continue;
    }

    let result = '✗ no response';
    for (let attempt = 0; attempt < 3; attempt++) {
      try {
        const dr = await client.readHoldingRegisters(REG_DEVICEID, 1);
        const did = dr.data[0];
        if (did === EXPECTED_DEVICE_ID) {
          result = `✓ DeviceId=0x${did.toString(16).toUpperCase()} CORRECT — USE THIS BAUD`;
        } else {
          result = `✓ responded, DeviceId=0x${did.toString(16).toUpperCase()} (expected 0xA601 — wrong slave ID or device)`;
        }
        break;
      } catch (e) {
        if (attempt === 2) result = `✗ ${e.message}`;
        else await delay(recommendedRetryDelay(baud));
      }
    }

    console.log(`  ${String(baud).padStart(6)}   ${result}`);
    client.close();
    await delay(200);
  }

  console.log('\nScan complete.');
  console.log('\nFTDI / COM2 tip: If all bauds fail, set Latency Timer to 1ms in Device Manager:');
  console.log('  Device Manager → Ports (COM & LPT) → COM2 → Properties');
  console.log('  → Port Settings → Advanced → Latency Timer (msec): 1');
}

// ── Main ──────────────────────────────────────────────────────────────────────

async function main() {
  if (BAUD_SCAN) {
    await baudScan();
    return;
  }

  const recTimeout    = recommendedTimeout(BAUD);
  const recRetryDelay = recommendedRetryDelay(BAUD);

  console.log(`\nLPG Controller — Modbus RTU Live-Weight Probe`);
  console.log(`Port: ${PORT}  Baud: ${BAUD}  Slave: ${SLAVE_ID}  Samples: ${SAMPLES === 0 ? '∞' : SAMPLES}  Interval: ${INTERVAL}ms`);
  console.log(`Timeout: ${TIMEOUT}ms  Retries: ${RETRIES}  RetryDelay: ${RETRY_DELAY}ms  ReqGap: ${REQ_GAP}ms`);
  console.log(`\nRecommended settings for ${BAUD} baud:`);
  console.log(`  Timeout:     ${recTimeout}ms`);
  console.log(`  Retry delay: ${recRetryDelay}ms`);
  console.log(`  Retries:     3`);
  console.log(`  Req gap:     20ms (between back-to-back reads in one sample)`);
  if (PORT === 'COM2' || PORT === 'com2') {
    console.log(`\n  FTDI/COM2: Ensure Latency Timer = 1ms in Device Manager for reliable RTU framing.`);
  }
  console.log(`\nExpected DeviceId: 0x${EXPECTED_DEVICE_ID.toString(16).toUpperCase()} (${EXPECTED_DEVICE_ID})`);
  console.log(`Firmware Float32 encoding: ABCD (high-word-first, big-endian)`);
  console.log(`Weintek setting: "High word first" with zero-based 4x addressing\n`);

  const client = new ModbusRTU();
  client.setTimeout(TIMEOUT);

  try {
    await client.connectRTUBuffered(PORT, {
      baudRate: BAUD,
      dataBits: 8,
      parity:   'none',
      stopBits:  1,
    });
    client.setID(SLAVE_ID);
    console.log(`Connected to ${PORT} @ ${BAUD} baud 8N1`);
  } catch (e) {
    console.error(`Cannot open ${PORT}: ${e.message}`);
    console.error('Run "npm run ports" to list available ports.');
    process.exit(1);
  }

  let n = 0;
  while (SAMPLES === 0 || n < SAMPLES) {
    try {
      await readAndPrint(client);
    } catch (e) {
      console.error(`Unexpected error: ${e.message}`);
    }
    n++;
    if (SAMPLES !== 0 && n >= SAMPLES) break;
    await delay(INTERVAL);
  }

  client.close();
  console.log(`\nDone. Total samples: ${n}  OK: ${commOkCount}  Errors: ${commErrCount}`);
}

main().catch(err => {
  console.error('Fatal:', err.message);
  process.exit(1);
});
