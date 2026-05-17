'use strict';
// Generate Weintek EasyBuilder Pro importable CSV tag files.
//
// Output files:
//   docs/hmi/weintek_tags_clean_zero_based.csv  — full map, all confirmed registers
//   docs/hmi/weintek_tags_test_liveweight.csv   — minimal test set for LiveWeight diagnosis
//
// CSV format matches Weintek EasyBuilder Pro tag import format.
// Device name "LPG_Controller" must match the device defined in EasyBuilder Pro.
//
// Usage: npm run gen:weintek-tags

const fs   = require('fs');
const path = require('path');

const OUT_DIR  = path.resolve(__dirname, '..', 'docs', 'hmi');
const OUT_FULL = path.join(OUT_DIR, 'weintek_tags_clean_zero_based.csv');
const OUT_TEST = path.join(OUT_DIR, 'weintek_tags_test_liveweight.csv');

// ── CSV helpers ───────────────────────────────────────────────────────────────
const DEVICE = 'MODBUS RTU (Zero-based Addressing)';
const SEP    = ',';

function csvHeader() {
  return 'Tag Name' + SEP + 'Device' + SEP + 'Address Type' + SEP + 'Address' + SEP + 'Comment' + SEP + 'Data Type';
}

function csvRow(tagName, addrType, addr, comment, dataType) {
  return [tagName, DEVICE, addrType, addr, comment, dataType].join(SEP);
}

// ── Tag definitions (derived from firmware ModbusRegisterMap.h) ───────────────
// Address type: "4x" = holding register (FC03), 0-based PDU address
// Float32 in Weintek = "32-bit Float", high-word-first (firmware ABCD encoding)
// NOTE: Weintek "4x address N" maps directly to PDU address N (zero-based)

const ALL_TAGS = [
  // ── Section 1: Process data ────────────────────────────────────────────────
  ['LiveWeight',       '4x',  0,   'kg; Float32 ABCD (high-word-first)',              '32-bit Float'],
  ['TareWeight',       '4x',  2,   'kg',                                               '32-bit Float'],
  ['NetWeight',        '4x',  4,   'kg',                                               '32-bit Float'],
  ['TargetWeight',     '4x',  6,   'kg',                                               '32-bit Float'],
  ['RatePerKg',        '4x',  8,   'PKR/kg',                                           '32-bit Float'],
  ['TargetAmount',     '4x',  10,  'PKR',                                              '32-bit Float'],
  ['CurrentAmount',    '4x',  12,  'PKR; net x rate',                                 '32-bit Float'],
  ['FillState',        '4x',  14,  '0 Idle,1 Ready,2 Validating,3 Fast,4 Slow,5 Settling,6 Complete,7 Aborted,8 Fault,9 Maintenance', '16-bit Unsigned'],
  ['EstopOk',          '4x',  15,  '1=OK',                                             '16-bit Unsigned'],
  ['CylinderPresent',  '4x',  16,  '1=present',                                        '16-bit Unsigned'],
  ['NozzleEngaged',    '4x',  17,  '1=engaged',                                        '16-bit Unsigned'],
  ['WeightStable',     '4x',  18,  '1=stable',                                         '16-bit Unsigned'],
  ['TxnCount',         '4x',  19,  'total transactions',                               '32-bit Unsigned'],
  ['UptimeSec',        '4x',  21,  'seconds since boot',                               '32-bit Unsigned'],
  ['Legacy_Command',   '4x',  23,  'legacy command; prefer HMI block at 128',         '16-bit Unsigned'],
  ['DeviceId',         '4x',  24,  'expected 0xA601 / decimal 42497',                 '16-bit Unsigned'],
  ['RtuSlaveAddr',     '4x',  25,  '1-247',                                            '16-bit Unsigned'],
  ['RtuBaud',          '4x',  26,  'baud rate (UINT32)',                               '32-bit Unsigned'],
  ['RtuParity',        '4x',  28,  '0=N,1=E,2=O',                                    '16-bit Unsigned'],
  ['RtuStopBits',      '4x',  29,  '1 or 2',                                           '16-bit Unsigned'],
  ['TcpPort',          '4x',  30,  'normally 502',                                     '16-bit Unsigned'],
  ['MqttConnected',    '4x',  31,  '0/1',                                              '16-bit Unsigned'],

  // ── Section 3: RTC ────────────────────────────────────────────────────────
  ['RtcYear',          '4x',  32,  'e.g. 2026',                                        '16-bit Unsigned'],
  ['RtcMonth',         '4x',  33,  '1-12',                                             '16-bit Unsigned'],
  ['RtcDay',           '4x',  34,  '1-31',                                             '16-bit Unsigned'],
  ['RtcHour',          '4x',  35,  '0-23',                                             '16-bit Unsigned'],
  ['RtcMinute',        '4x',  36,  '0-59',                                             '16-bit Unsigned'],
  ['RtcSecond',        '4x',  37,  '0-59',                                             '16-bit Unsigned'],
  ['RtcUnix',          '4x',  38,  'seconds since epoch (UINT32)',                    '32-bit Unsigned'],

  // ── Section 4: All-time stats ─────────────────────────────────────────────
  ['StatAllCompleted', '4x',  40,  'all-time completed fills',                         '32-bit Unsigned'],
  ['StatAllFailed',    '4x',  42,  'all-time failed fills',                            '32-bit Unsigned'],
  ['StatAllKg',        '4x',  44,  'all-time kg sold',                                '32-bit Float'],
  ['StatAllAmount',    '4x',  46,  'all-time amount PKR',                             '32-bit Float'],

  // ── Section 5: Periodic stats ─────────────────────────────────────────────
  ['StatTodayCompleted', '4x', 48, 'completed today',                                 '16-bit Unsigned'],
  ['StatTodayFailed',  '4x',  49,  'failed today',                                    '16-bit Unsigned'],
  ['StatTodayKg',      '4x',  50,  'kg today',                                        '32-bit Float'],
  ['StatTodayAmount',  '4x',  52,  'amount today PKR',                               '32-bit Float'],
  ['StatWeekCompleted','4x',  54,  'completed this week',                             '16-bit Unsigned'],
  ['StatWeekFailed',   '4x',  55,  'failed this week',                               '16-bit Unsigned'],
  ['StatWeekKg',       '4x',  56,  'kg this week',                                   '32-bit Float'],
  ['StatWeekAmount',   '4x',  58,  'amount this week PKR',                          '32-bit Float'],
  ['StatMonthCompleted','4x', 60,  'completed this month',                           '16-bit Unsigned'],
  ['StatMonthFailed',  '4x',  61,  'failed this month',                             '16-bit Unsigned'],
  ['StatMonthKg',      '4x',  62,  'kg this month',                                 '32-bit Float'],
  ['StatMonthAmount',  '4x',  64,  'amount this month PKR',                        '32-bit Float'],
  ['StatYearCompleted','4x',  66,  'completed this year',                            '16-bit Unsigned'],
  ['StatYearFailed',   '4x',  67,  'failed this year',                              '16-bit Unsigned'],
  ['StatYearKg',       '4x',  68,  'kg this year',                                  '32-bit Float'],
  ['StatYearAmount',   '4x',  70,  'amount this year PKR',                         '32-bit Float'],

  // ── Section 6: Diagnostics ────────────────────────────────────────────────
  ['AlarmCode',        '4x',  72,  '0 none; 1 estop; 2 nozzle; 3 cylinder; 4 scale read; 5 unstable; 6 not calibrated; 7 overfill; 8 timeout; 9 no flow', '16-bit Unsigned'],
  ['AlarmSeverity',    '4x',  73,  '0 none,1 info,2 warning,3 alarm/fault',          '16-bit Unsigned'],
  ['ReadinessMask',    '4x',  74,  'bit0 estop,bit1 cyl,bit2 nozzle,bit3 stable,bit4 cal,bit5 scale ready', '16-bit Unsigned'],
  ['BlockerMask',      '4x',  75,  'bit0 fault,bit1 estop,bit2 cyl,bit3 nozzle,bit4 scale init,bit5 read,bit6 stable,bit7 cal,bit8 sim', '16-bit Unsigned'],
  ['ScaleInitialized', '4x',  76,  '1=HX711 initialized',                            '16-bit Unsigned'],
  ['ScaleReadError',   '4x',  77,  '1=read error',                                   '16-bit Unsigned'],
  ['CalibrationValid', '4x',  78,  '1=calibration valid',                            '16-bit Unsigned'],
  ['SimulationActive', '4x',  79,  '1=simulated weight active',                     '16-bit Unsigned'],
  ['AlarmSource',      '4x',  80,  '0 none,1 safety,2 scale,3 process,4 operator',  '16-bit Unsigned'],

  // ── Section 7: Board resources ────────────────────────────────────────────
  ['ApplicationLoadPercent', '4x', 81,  'main loop load estimate %',               '32-bit Float'],
  ['LoopAvgMs',        '4x',  83,  'average loop ms',                                '32-bit Float'],
  ['LoopMaxMs',        '4x',  85,  'max loop ms',                                    '32-bit Float'],
  ['HeapTotalBytes',   '4x',  87,  'bytes',                                           '32-bit Unsigned'],
  ['HeapFreeBytes',    '4x',  89,  'bytes',                                           '32-bit Unsigned'],
  ['HeapMinFreeBytes', '4x',  91,  'bytes',                                           '32-bit Unsigned'],
  ['HeapFreePercent',  '4x',  93,  'percent',                                         '32-bit Float'],
  ['PsramTotalBytes',  '4x',  95,  'bytes',                                           '32-bit Unsigned'],
  ['PsramFreeBytes',   '4x',  97,  'bytes',                                           '32-bit Unsigned'],
  ['PsramFreePercent', '4x',  99,  'percent',                                         '32-bit Float'],
  ['FlashSizeBytes',   '4x', 101,  'bytes',                                           '32-bit Unsigned'],
  ['SketchSizeBytes',  '4x', 103,  'bytes',                                           '32-bit Unsigned'],
  ['FreeSketchBytes',  '4x', 105,  'bytes',                                           '32-bit Unsigned'],
  ['ChipTemperatureC', '4x', 107,  'ESP32 internal temperature C',                  '32-bit Float'],
  ['WifiRssi',         '4x', 109,  'dBm',                                             '16-bit Signed'],
  ['WifiStatus',       '4x', 110,  'ESP32 WiFi status',                             '16-bit Unsigned'],
  ['MqttClientState',  '4x', 111,  '0 disconnected,1 connected',                   '16-bit Signed'],
  ['LastResetReason',  '4x', 112,  'ESP reset reason',                              '16-bit Unsigned'],
  ['FirmwareBuildMode','4x', 113,  '0 production,1 prototype,2 development',       '16-bit Unsigned'],
  ['RtuReqCount',      '4x', 114,  'Modbus RTU request count',                     '32-bit Unsigned'],
  ['RtuErrCount',      '4x', 116,  'Modbus RTU error count',                       '32-bit Unsigned'],
  ['ControllerHeartbeat','4x',118, 'controller heartbeat counter',                 '32-bit Unsigned'],

  // ── Section 8: HMI operation block (base 0x0080 = addr 128) ─────────────
  ['HMI_CommandCode',  '4x', 128, 'write command code first; kHmiCmd_*',           '16-bit Unsigned'],
  ['HMI_CommandSeq',   '4x', 129, 'increment after CommandCode',                  '16-bit Unsigned'],
  ['HMI_LastAcceptedSeq','4x',130,'echo of last accepted sequence',               '16-bit Unsigned'],
  ['HMI_CommandResult','4x', 131, '0 idle,1 accepted,2 busy,3 rejected,4 done,5 failed', '16-bit Unsigned'],
  ['HMI_CommandErrCode','4x',132, 'error detail',                                  '16-bit Unsigned'],
  ['HMI_CommandBusy',  '4x', 133, '1=processing',                                 '16-bit Unsigned'],
  ['HMI_FillMode',     '4x', 134, '0=by kg,1=by amount',                          '16-bit Unsigned'],
  ['HMI_PreparedFlag', '4x', 135, '1=preset validated',                           '16-bit Unsigned'],
  ['HMI_ReadyToPrepare','4x',136, '1=ready for Prepare',                          '16-bit Unsigned'],
  ['HMI_ReadyToStart', '4x', 137, '1=ready for Start',                            '16-bit Unsigned'],
  ['HMI_CanTare',      '4x', 138, '1=tare safe now',                              '16-bit Unsigned'],
  ['HMI_CanStop',      '4x', 139, '1=fill active',                                '16-bit Unsigned'],
  ['HMI_Heartbeat',    '4x', 140, 'HMI writes periodically',                      '16-bit Unsigned'],
  ['HMI_WdtTimeoutSec','4x', 141, '0 disables watchdog; default 30',              '16-bit Unsigned'],
  ['HMI_HeartbeatAgeSec','4x',142,'seconds since last HMI heartbeat',             '16-bit Unsigned'],
  ['HMI_Reserved',     '4x', 143, 'reserved',                                     '16-bit Unsigned'],
  ['HMI_PresetTare',   '4x', 144, 'tare kg; write both words together (FC16)',   '32-bit Float'],
  ['HMI_PresetTargetKg','4x',146, 'target fill kg',                               '32-bit Float'],
  ['HMI_PresetRatePerKg','4x',148,'PKR/kg',                                        '32-bit Float'],
  ['HMI_PresetAmount', '4x', 150, 'target amount PKR',                            '32-bit Float'],
  ['HMI_PresetValid',  '4x', 152, '1=preset validated',                           '16-bit Unsigned'],
  ['HMI_PresetErrCode','4x', 153, 'preset error code',                            '16-bit Unsigned'],
  ['HMI_LastFillResult','4x',154, 'last fill result',                             '16-bit Unsigned'],
  ['HMI_LastFillErrCode','4x',155,'last fill error code',                         '16-bit Unsigned'],
];

// ── Minimal test set for diagnosing LiveWeight ─────────────────────────────────
const TEST_TAGS = [
  // DeviceId first — prove communication before diagnosing float
  ['DeviceId',         '4x',  24,  'expected 0xA601 / decimal 42497',              '16-bit Unsigned'],
  // Raw words so you can see HI/LO independently
  ['LiveWeight_HiWord','4x',   0,  'Float32 high word (reg 0) — 16-bit view',     '16-bit Unsigned'],
  ['LiveWeight_LoWord','4x',   1,  'Float32 low word  (reg 1) — 16-bit view',     '16-bit Unsigned'],
  // Float interpreted as ABCD (correct firmware encoding)
  ['LiveWeight_Float', '4x',   0,  'ABCD high-word-first Float32 — THIS IS CORRECT', '32-bit Float'],
  // NetWeight
  ['NetWeight_HiWord', '4x',   4,  'Float32 high word (reg 4) — 16-bit view',     '16-bit Unsigned'],
  ['NetWeight_LoWord', '4x',   5,  'Float32 low word  (reg 5) — 16-bit view',     '16-bit Unsigned'],
  ['NetWeight_Float',  '4x',   4,  'ABCD high-word-first Float32',                '32-bit Float'],
  // Scale diagnostics
  ['ScaleInitialized', '4x',  76,  '1=HX711 initialized',                         '16-bit Unsigned'],
  ['ScaleReadError',   '4x',  77,  '1=read error',                                '16-bit Unsigned'],
  ['CalibrationValid', '4x',  78,  '1=calibration valid',                         '16-bit Unsigned'],
  ['SimulationActive', '4x',  79,  '1=simulation active',                         '16-bit Unsigned'],
  // Fill state
  ['FillState',        '4x',  14,  '0=Idle,1=Ready,...',                          '16-bit Unsigned'],
  // HMI command block
  ['HMI_CommandCode',  '4x', 128,  'write command code first',                    '16-bit Unsigned'],
  ['HMI_CommandSeq',   '4x', 129,  'increment after CommandCode',                 '16-bit Unsigned'],
  ['HMI_CommandResult','4x', 131,  '0 idle,1 accepted,2 busy,3 rejected,4 done,5 failed', '16-bit Unsigned'],
  ['HMI_ReadyToPrepare','4x',136,  '1=ready for Prepare',                         '16-bit Unsigned'],
  ['HMI_ReadyToStart', '4x', 137,  '1=ready for Start',                           '16-bit Unsigned'],
  ['HMI_Heartbeat',    '4x', 140,  'HMI writes periodically',                     '16-bit Unsigned'],
];

function writeCsv(filePath, rows) {
  const lines = [csvHeader()];
  for (const r of rows) {
    lines.push(csvRow(r[0], r[1], r[2], r[3], r[4]));
  }
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, lines.join('\r\n'));
  console.log(`Written: ${filePath}  (${rows.length} tags)`);
}

function main() {
  writeCsv(OUT_FULL, ALL_TAGS);
  writeCsv(OUT_TEST, TEST_TAGS);

  console.log('\nKey Float32 setting in Weintek EasyBuilder Pro:');
  console.log('  Address type : 4x (Holding Register)');
  console.log('  Data type    : 32-bit Float');
  console.log('  Word order   : High word first  ← CRITICAL (firmware stores ABCD)');
  console.log('  Address      : 0  (zero-based, NOT 40001)');
  console.log('');
  console.log('Numeric display:');
  console.log('  Integer digits : 3 or 4');
  console.log('  Decimal digits : 2 or 3');
  console.log('  Total width must be large enough — *** means the value does not fit');
  console.log('  Min/Max range  : -10 to 150 kg should be more than enough');
}

main();
