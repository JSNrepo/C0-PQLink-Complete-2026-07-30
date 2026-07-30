#!/usr/bin/env node
import { createHash, randomBytes } from 'node:crypto';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { generateKeyPair, MODE } from './protocol.mjs';

const outputDirectory = resolve(process.argv[2] ?? 'generated');
await mkdir(outputDirectory, { recursive: true });

const keySeed = randomBytes(64);
const keyPair = generateKeyPair(keySeed);
const psk = randomBytes(32);
const deviceId = randomBytes(8);
const keyId = createHash('sha256')
  .update(keyPair.publicKey)
  .digest()
  .subarray(0, 16);

function cArray(name, bytes, suffix = '') {
  const rows = [];
  for (let offset = 0; offset < bytes.length; offset += 12) {
    rows.push(
      `  ${[...bytes.subarray(offset, offset + 12)]
        .map((value) => `0x${value.toString(16).padStart(2, '0')}`)
        .join(', ')}`,
    );
  }
  return `static const uint8_t ${name}[${bytes.length}]${suffix} = {\n`
    + `${rows.join(',\n')}\n};\n`;
}

const header = `#ifndef C0PQ_DEMO_PROVISIONING_H
#define C0PQ_DEMO_PROVISIONING_H

#include <stdint.h>
#if defined(__AVR__)
#include <avr/pgmspace.h>
#define C0PQ_DEMO_FLASH PROGMEM
#else
#define C0PQ_DEMO_FLASH
#endif

/*
 * DEMO PROVISIONING ONLY. Production PSKs and DRBG state belong in protected,
 * per-device storage, not in a source header.
 */
${cArray('c0pq_demo_device_id', deviceId)}
${cArray('c0pq_demo_psk', psk)}
${cArray('c0pq_demo_server_key_id', keyId)}
${cArray(
  'c0pq_demo_server_public_key',
  Buffer.from(keyPair.publicKey),
  ' C0PQ_DEMO_FLASH',
)}
#endif
`;

const config = {
  host: '0.0.0.0',
  port: 47050,
  epoch: '1',
  mlKemKeySeedHex: keySeed.toString('hex'),
  devices: [{
    deviceIdHex: deviceId.toString('hex'),
    pskHex: psk.toString('hex'),
    mode: 'PQ_BOOTSTRAP_RATCHET',
  }],
};

await Promise.all([
  writeFile(
    resolve(outputDirectory, 'c0pq_demo_provisioning.h'),
    header,
  ),
  writeFile(
    resolve(outputDirectory, 'reference-peer-config.json'),
    `${JSON.stringify(config, null, 2)}\n`,
    { mode: 0o600 },
  ),
]);

process.stdout.write(
  `Wrote demo provisioning to ${outputDirectory}\n`
  + 'Protect reference-peer-config.json: it contains the ML-KEM private seed '
  + 'and device PSK.\n',
);
