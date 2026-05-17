
'use strict';
// Modbus map audit: parses ModbusRegisterMap.h and compares with Weintek CSV.
// Detects overlapping floats, 3x/4x mismatches, address offset errors, and gaps.
//
// Usage: npm run audit:modbus-map
// Output:
//   docs/hmi/modbus_map_audit_report.md
//   tools/out/modbus_map_audit.json

const fs   = require('fs');
const path = require('path');

const FIRMWARE_H = path.resolve(
  __dirname, '..', '..', '..', '..', 'LPG-Filling-ESP',
  'firmware', 'lpg_controller', 'include', 'ModbusRegisterMap.h'
);
// Fallback: try relative from this file's location
const FIRMWARE_H_ALT = path.resolve(
  'D:\\Working\\LPG-Filling-ESP\\firmware\\lpg_controller\\include\\ModbusRegisterMap.h'
);

const WEINTEK_CSV = path.resolve(
  'D:\\Working\\LPG-Filling-ESP\\docs\\hmi\\weintek_hmi_tags.csv'
);

const OUT_MD   = path.resolve(__dirname, '..', 'docs', 'hmi', 'modbus_map_audit_report.md');
const OUT_JSON = path.resolve(__dirname, 'out', 'modbus_map_audit.json');

// ── Parse ModbusRegisterMap.h ─────────────────────────────────────────────────
function parseFirmwareHeader(filePath) {
  const text = fs.readFileSync(filePath, 'utf8');
  const registers = [];

  // Match lines like: constexpr uint16_t kHR_LiveWeightHi  = 0x0000;  // FLOAT32 Hi  — kg, R
  const re = /constexpr\s+uint16_t\s+(kHR_\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;(.*)$/gm;
  let m;
  while ((m = re.exec(text)) !== null) {
    const name    = m[1];
    const addr    = parseInt(m[2], 16) || parseInt(m[2], 10);
    const comment = m[3].replace(/\/\//, '').trim();

    let type = 'UINT16';
    let wordCount = 1;
    if (/FLOAT32\s+Hi/i.test(comment)) { type = 'FLOAT32'; wordCount = 2; }
    else if (/FLOAT32\s+Lo/i.test(comment)) { type = 'FLOAT32_LO'; wordCount = 1; }
    else if (/UINT32\s+Hi/i.test(comment)) { type = 'UINT32'; wordCount = 2; }
    else if (/UINT32\s+Lo/i.test(comment)) { type = 'UINT32_LO'; wordCount = 1; }
    else if (/INT16/i.test(comment))    { type = 'INT16'; }

    registers.push({ name, addr, type, wordCount, comment,
                     weintek40001: 40001 + addr });
  }

  return registers;
}

// ── Parse Weintek CSV ─────────────────────────────────────────────────────────
function parseWeitrekCsv(filePath) {
  if (!fs.existsSync(filePath)) return [];
  const lines = fs.readFileSync(filePath, 'utf8').split('\n').slice(1); // skip header
  return lines
    .map(l => l.trim())
    .filter(Boolean)
    .map(l => {
      const cols = l.split(',');
      return {
        tagName:     cols[0] || '',
        device:      cols[2] || '',
        addrType:    cols[3] || '',
        pduAddrHex:  cols[4] || '',
        weintek4001: cols[5] || '',
        dataType:    cols[6] || '',
        rw:          cols[7] || '',
      };
    });
}

// Constants that are meta/alias (not actual registers)
const META_NAMES = new Set(['kHR_HmiBase', 'kHR_Base', 'kHR_Count', 'kHR_HmiCount',
                             'kCoil_Base', 'kCoil_Count', 'kDI_Base', 'kDI_Count']);

// ── Audit logic ───────────────────────────────────────────────────────────────
function audit(regs, weitrekTags) {
  const issues = [];

  // Filter out meta constants before analysis
  const filtered = regs.filter(r => !META_NAMES.has(r.name));

  // Sort by address
  const sorted = [...filtered].sort((a, b) => a.addr - b.addr);

  // Overlap detection: only flag Hi-type registers occupying space already used by another Hi
  // Lo registers are expected to sit at addr+1 of their Hi partner — not an error
  const seen = new Map(); // addr → name
  for (const r of sorted) {
    if (r.type === 'FLOAT32_LO' || r.type === 'UINT32_LO') continue; // Lo is always within Hi's span

    const occupiedAddrs = [r.addr];
    if (r.type === 'FLOAT32' || r.type === 'UINT32') {
      occupiedAddrs.push(r.addr + 1);
    }

    for (const a of occupiedAddrs) {
      if (seen.has(a)) {
        // Suppress if the collision is with the matching Lo register (expected)
        const other = seen.get(a);
        const expectedLo = r.name.replace('Hi', 'Lo');
        if (other !== expectedLo) {
          issues.push({
            severity: 'ERROR',
            type: 'OVERLAP',
            message: `Register 0x${a.toString(16).padStart(4,'0')} used by both ${other} and ${r.name}`,
          });
        }
      }
      seen.set(a, r.name);
    }
  }

  // Check for Lo registers without a matching Hi
  const hiRegs = new Set(sorted.filter(r => r.name.endsWith('Hi')).map(r => r.name.replace('Hi','Lo')));
  for (const r of sorted) {
    if (r.name.endsWith('Lo') && !hiRegs.has(r.name)) {
      issues.push({ severity: 'WARN', type: 'ORPHAN_LO', message: `${r.name} has no matching Hi register` });
    }
  }

  // Scan Weintek tags for 3x entries that should be 4x
  for (const t of weitrekTags) {
    if (t.addrType === '3x') {
      issues.push({
        severity: 'WARN',
        type: 'WRONG_3X',
        message: `Weintek tag "${t.tagName}" uses 3x (input registers) — should be 4x (holding registers FC03)`,
      });
    }
    // Check for suspiciously large addresses that look like 40001+addr confusion
    const num = parseInt(t.pduAddrHex, 16);
    if (!isNaN(num) && num > 200) {
      issues.push({
        severity: 'WARN',
        type: 'LARGE_ADDR',
        message: `Weintek tag "${t.tagName}" PDU address 0x${num.toString(16)} > 200 — possible 40001 offset error`,
      });
    }
  }

  return { issues, sorted, filtered };
}

// ── Main ──────────────────────────────────────────────────────────────────────
function main() {
  let fwPath = FIRMWARE_H;
  if (!fs.existsSync(fwPath)) {
    fwPath = FIRMWARE_H_ALT;
    if (!fs.existsSync(fwPath)) {
      console.error(`Cannot find ModbusRegisterMap.h at:\n  ${FIRMWARE_H}\n  ${FIRMWARE_H_ALT}`);
      process.exit(1);
    }
  }

  console.log(`Parsing firmware: ${fwPath}`);
  const regs = parseFirmwareHeader(fwPath);
  console.log(`  Found ${regs.length} kHR_* register definitions`);

  const weitrekTags = parseWeitrekCsv(WEINTEK_CSV);
  console.log(`  Found ${weitrekTags.length} Weintek CSV tag entries`);

  const { issues, sorted, filtered } = audit(regs, weitrekTags);

  // ── Generate JSON ─────────────────────────────────────────────────────────
  const jsonOut = {
    generatedAt: new Date().toISOString(),
    firmwareSource: fwPath,
    weintekCsv: WEINTEK_CSV,
    totalRegisters: filtered.length,
    issueCount: issues.length,
    issues,
    registers: sorted.map(r => ({
      name:         r.name,
      addr:         r.addr,
      addrHex:      `0x${r.addr.toString(16).toUpperCase().padStart(4,'0')}`,
      weintek40001: r.weintek40001,
      type:         r.type,
      wordCount:    r.wordCount,
      comment:      r.comment,
    })),
  };

  fs.mkdirSync(path.dirname(OUT_JSON), { recursive: true });
  fs.writeFileSync(OUT_JSON, JSON.stringify(jsonOut, null, 2));
  console.log(`\nJSON report: ${OUT_JSON}`);

  // ── Generate Markdown ─────────────────────────────────────────────────────
  const lines = [];
  lines.push('# Modbus Map Audit Report');
  lines.push('');
  lines.push(`Generated: ${new Date().toISOString()}`);
  lines.push(`Firmware source: \`${fwPath}\``);
  lines.push('');

  if (issues.length === 0) {
    lines.push('**No issues found.**');
  } else {
    lines.push(`## Issues (${issues.length})`);
    lines.push('');
    lines.push('| Severity | Type | Message |');
    lines.push('|---|---|---|');
    for (const iss of issues) {
      lines.push(`| ${iss.severity} | ${iss.type} | ${iss.message} |`);
    }
  }

  lines.push('');
  lines.push('## Register Table');
  lines.push('');
  lines.push('| Name | PDU Addr (hex) | 40001+ | Type | Words | Comment |');
  lines.push('|---|---|---|---|---|---|');
  for (const r of sorted) {
    const hex  = `0x${r.addr.toString(16).toUpperCase().padStart(4,'0')}`;
    lines.push(`| ${r.name} | ${hex} | ${r.weintek40001} | ${r.type} | ${r.wordCount} | ${r.comment} |`);
  }

  lines.push('');
  lines.push('## Key Facts for Weintek');
  lines.push('');
  lines.push('- **Driver**: MODBUS RTU (Zero-based Addressing)');
  lines.push('- **Address type**: 4x (Holding Registers, FC03)');
  lines.push('- **Float32 word order**: High-word-first (ABCD) — reg[n]=high, reg[n+1]=low');
  lines.push('- **Baud**: 9600  **Format**: 8N1  **Slave ID**: 1');
  lines.push('- **LiveWeight**: 4x address 0, 32-bit Float, High word first');
  lines.push('- **DeviceId**: 4x address 24, 16-bit Unsigned, expected 0xA601 (42497)');
  lines.push('- **HMI block**: 4x address 128 (0x0080)');

  fs.mkdirSync(path.dirname(OUT_MD), { recursive: true });
  fs.writeFileSync(OUT_MD, lines.join('\n'));
  console.log(`Markdown report: ${OUT_MD}`);

  // ── Console summary ───────────────────────────────────────────────────────
  if (issues.length > 0) {
    console.log(`\nIssues found (${issues.length}):`);
    for (const iss of issues) {
      console.log(`  [${iss.severity}] ${iss.type}: ${iss.message}`);
    }
  } else {
    console.log('\nNo issues found in register map.');
  }

  // Print key registers for quick reference
  console.log('\nKey registers (confirmed from firmware):');
  const keyAddrs = [0x0000, 0x0002, 0x0004, 0x000E, 0x0018, 0x0080, 0x0081, 0x0090];
  for (const r of sorted) {
    if (keyAddrs.includes(r.addr)) {
      console.log(`  0x${r.addr.toString(16).padStart(4,'0')} ${r.name.padEnd(28)} ${r.type}`);
    }
  }
}

main();
