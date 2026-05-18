// watch_prepared.js
// Poll preparedFlag (4x135) and readyStart (4x137) every 200 ms for 15 s after cmd14.
// Run: node tools/watch_prepared.js

const net = require('net');

const HOST = '192.168.0.101';
const PORT = 502;
const UNIT = 1;

let tid = 1;

function mbap(pduLen) {
  const buf = Buffer.alloc(6);
  buf.writeUInt16BE(tid++, 0);
  buf.writeUInt16BE(0, 2);
  buf.writeUInt16BE(pduLen + 1, 4);
  return buf;
}

function fc03(addr, count) {
  const pdu = Buffer.alloc(5);
  pdu[0] = 0x03;
  pdu.writeUInt16BE(addr, 1);
  pdu.writeUInt16BE(count, 3);
  const h = mbap(pdu.length);
  const frame = Buffer.alloc(7 + pdu.length);
  h.copy(frame);
  frame[6] = UNIT;
  pdu.copy(frame, 7);
  return frame;
}

function fc16(addr, words) {
  const pdu = Buffer.alloc(5 + 1 + words.length * 2);
  pdu[0] = 0x10;
  pdu.writeUInt16BE(addr, 1);
  pdu.writeUInt16BE(words.length, 3);
  pdu[5] = words.length * 2;
  for (let i = 0; i < words.length; i++) pdu.writeUInt16BE(words[i], 6 + i * 2);
  const h = mbap(pdu.length);
  const frame = Buffer.alloc(7 + pdu.length);
  h.copy(frame);
  frame[6] = UNIT;
  pdu.copy(frame, 7);
  return frame;
}

function floatToWords(f) {
  const b = Buffer.alloc(4);
  b.writeFloatBE(f, 0);
  return [b.readUInt16BE(0), b.readUInt16BE(2)];
}

function send(sock, frame) {
  return new Promise((res, rej) => {
    sock.once('data', d => res(d));
    sock.write(frame);
    setTimeout(() => rej(new Error('timeout')), 2000);
  });
}

async function readRegs(sock, addr, count) {
  const resp = await send(sock, fc03(addr, count));
  const words = [];
  for (let i = 0; i < count; i++) words.push(resp.readUInt16BE(9 + i * 2));
  return words;
}

async function writeRegs(sock, addr, words) {
  await send(sock, fc16(addr, words));
}

function stateStr(s) {
  return ['Idle','Ready','FillingFast','FillingSlow','Settling','Validating','Complete','Aborted','Fault'][s] ?? `#${s}`;
}

async function main() {
  const sock = net.createConnection({ host: HOST, port: PORT });
  await new Promise(r => sock.once('connect', r));
  console.log(`Connected to ${HOST}:${PORT}\n`);

  // Write presets: fillMode=0 at 4x134, then kg/rate/amount at 4x146
  const kg = 3.0, rate = 300.0, amount = kg * rate;
  const [kgH, kgL] = floatToWords(kg);
  const [rH, rL]   = floatToWords(rate);
  const [aH, aL]   = floatToWords(amount);
  await writeRegs(sock, 134, [0]);                         // fillMode
  await writeRegs(sock, 146, [kgH, kgL, rH, rL, aH, aL]); // kg+rate+amount

  // Send cmd14 with seq=99
  await writeRegs(sock, 128, [14, 99]);
  console.log('cmd14 sent (seq=99). Watching for 15 seconds...\n');
  console.log('Time(ms)  fillState  preparedFlag  readyToPrepare  readyStart  canStop  weightStable');
  console.log('─'.repeat(88));

  const t0 = Date.now();
  let prev = '';

  for (let i = 0; i < 75; i++) {           // 75 × 200 ms = 15 s
    await new Promise(r => setTimeout(r, 200));

    // Read block: 4x14..4x18 (5 regs) + gap + 4x131..4x139 (9 regs)
    const [fs, , , , ws] = await readRegs(sock, 14, 5);   // fillState, _, _, _, weightStable
    const hmi = await readRegs(sock, 131, 9);              // full HMI block

    const prepared     = hmi[4];  // 4x135
    const rtp          = hmi[5];  // 4x136
    const rts          = hmi[6];  // 4x137
    const canStop      = hmi[8];  // 4x139

    const line = `${String(Date.now()-t0).padStart(6)} ms   ${stateStr(fs).padEnd(11)} ${prepared}             ${rtp}               ${rts}          ${canStop}        ${ws}`;

    // Only print if something changed
    const key = `${fs}${prepared}${rtp}${rts}${canStop}${ws}`;
    if (key !== prev || i === 0) {
      console.log(line);
      prev = key;
    }
  }

  console.log('\nDone. Closing connection.');
  sock.destroy();
}

main().catch(e => { console.error(e.message); process.exit(1); });
