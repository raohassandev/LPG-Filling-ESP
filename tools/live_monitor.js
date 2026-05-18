// live_monitor.js
// Watches fillState, preparedFlag, readyStart, cmdResult, alarmCode
// every 300 ms. Press Prepare on the HMI while this runs.
// Ctrl+C to stop.
// Run: node tools/live_monitor.js

const net = require('net');
const HOST = '192.168.0.101', PORT = 502, UNIT = 1;
let tid = 1;

function fc03frame(addr, count) {
  const buf = Buffer.alloc(12);
  buf.writeUInt16BE(tid++, 0); buf.writeUInt16BE(0, 2); buf.writeUInt16BE(6, 4);
  buf[6] = UNIT; buf[7] = 3;
  buf.writeUInt16BE(addr, 8); buf.writeUInt16BE(count, 10);
  return buf;
}

function send(sock, frame) {
  return new Promise((res, rej) => {
    const t = setTimeout(() => rej(new Error('timeout')), 3000);
    sock.once('data', d => { clearTimeout(t); res(d); });
    sock.write(frame);
  });
}

async function readRegs(sock, addr, count) {
  const r = await send(sock, fc03frame(addr, count));
  const out = [];
  for (let i = 0; i < count; i++) out.push(r.readUInt16BE(9 + i*2));
  return out;
}

const STATES = ['Idle','Ready','FillingFast','FillingSlow','Settling','Validating','Complete','Aborted','Fault'];

async function main() {
  const sock = net.createConnection({ host: HOST, port: PORT });
  await new Promise(r => sock.once('connect', r));
  console.log(`Connected. Press PREPARE on HMI now. Ctrl+C to stop.\n`);
  console.log('Time      fillState     prepared  readyStart  cmdResult  alarmCode  canStop');
  console.log('─'.repeat(78));

  const t0 = Date.now();
  let prev = '';

  while (true) {
    try {
      // Read 4x14 (fillState) and 4x72 (alarmCode)
      const [fs] = await readRegs(sock, 14, 1);
      const [alarm] = await readRegs(sock, 72, 1);
      // Read HMI block: 4x131=cmdResult, 4x135=prepared, 4x137=readyStart, 4x139=canStop
      const hmi = await readRegs(sock, 131, 9);
      const cmdResult = hmi[0];
      const prepared  = hmi[4];
      const rts       = hmi[6];
      const canStop   = hmi[8];

      const key = `${fs}${prepared}${rts}${cmdResult}${alarm}${canStop}`;
      if (key !== prev) {
        const elapsed = ((Date.now() - t0) / 1000).toFixed(1);
        const fsStr = (STATES[fs] ?? `#${fs}`).padEnd(12);
        console.log(`${String(elapsed).padStart(6)}s   ${fsStr}  ${prepared}         ${rts}          ${cmdResult}          ${alarm}          ${canStop}`);
        prev = key;
      }
    } catch(e) {
      const elapsed = ((Date.now()-t0)/1000).toFixed(1);
      console.log(`${String(elapsed).padStart(6)}s   [read error: ${e.message}]`);
      prev = '';
    }
    await new Promise(r => setTimeout(r, 300));
  }
}

main().catch(e => console.error(e.message));
