import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { ml_kem512 } from '@noble/post-quantum/ml-kem.js';

const hex = (bytes) => Buffer.from(bytes).toString('hex');

function runCase(caseNumber) {
  const keySeed = Uint8Array.from(
    { length: 64 },
    (_, index) => (index * 17 + caseNumber * 29 + 3) & 0xff,
  );
  const randomness = Uint8Array.from(
    { length: 32 },
    (_, index) => (index * 23 + caseNumber * 11 + 7) & 0xff,
  );
  const keys = ml_kem512.keygen(keySeed);
  const expected = ml_kem512.encapsulate(keys.publicKey, randomness);
  const output = execFileSync(
    './build/mlkem_oracle_cli',
    [hex(keys.publicKey), hex(randomness)],
    { encoding: 'utf8', maxBuffer: 1024 * 1024 },
  ).trim().split('\n');

  assert.equal(output[0], hex(expected.cipherText), `ciphertext case ${caseNumber}`);
  assert.equal(output[1], hex(expected.sharedSecret), `shared secret case ${caseNumber}`);
  assert.equal(output[2], '5', `maximum streamed write case ${caseNumber}`);
}

for (let index = 0; index < 8; index += 1) runCase(index);

const keySeed = new Uint8Array(64);
const keys = ml_kem512.keygen(keySeed);
const malformed = Uint8Array.from(keys.publicKey);
malformed[0] = 0xff;
malformed[1] = (malformed[1] & 0xf0) | 0x0f;
const rejection = spawnSync(
  './build/mlkem_oracle_cli',
  [hex(malformed), '00'.repeat(32)],
  { encoding: 'utf8' },
);
assert.notEqual(rejection.status, 0, 'non-canonical public key must be rejected');
assert.match(rejection.stderr, /encapsulation failed: -2/);

console.log('ML-KEM-512 oracle: 8/8 exact vectors + canonical-key rejection passed');

