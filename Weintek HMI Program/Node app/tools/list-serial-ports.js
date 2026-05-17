'use strict';
// List all available serial ports with vendor/product details.
// Usage: npm run ports

const { SerialPort } = require('serialport');

async function main() {
  const ports = await SerialPort.list();

  if (ports.length === 0) {
    console.log('No serial ports found.');
    return;
  }

  console.log(`\nFound ${ports.length} serial port(s):\n`);
  console.log(
    'PORT'.padEnd(12) +
    'MANUFACTURER'.padEnd(30) +
    'VID:PID'.padEnd(14) +
    'SERIAL'
  );
  console.log('-'.repeat(80));

  for (const p of ports) {
    const vid = p.vendorId  || '----';
    const pid = p.productId || '----';
    console.log(
      (p.path || '').padEnd(12) +
      (p.manufacturer || 'unknown').padEnd(30) +
      `${vid}:${pid}`.padEnd(14) +
      (p.serialNumber || '')
    );
  }

  console.log('');
  console.log('Tip: for USB-RS485 adapters, look for CH340, CP2102, FT232, or PL2303 manufacturer strings.');
  console.log('     The LPG controller bench port is typically COM10 (9600 8N1 slave 1).');
}

main().catch(err => {
  console.error('Error listing ports:', err.message);
  process.exit(1);
});
