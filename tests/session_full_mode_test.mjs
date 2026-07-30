import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import readline from 'node:readline';
import {
  C0PQPeer,
  generateKeyPair,
  MODE,
} from '../reference-peer/protocol.mjs';

const keySeed = Uint8Array.from(
  { length: 64 },
  (_, index) => (index * 7 + 41) & 0xff,
);
const psk = Buffer.from(Uint8Array.from(
  { length: 32 },
  (_, index) => (index * 5 + 23) & 0xff,
));
const keyPair = generateKeyPair(keySeed);
const keyId = createHash('sha256')
  .update(keyPair.publicKey)
  .digest()
  .subarray(0, 16);
const received = [];
const peer = new C0PQPeer({
  keyPair,
  epoch: 1n,
  devices: new Map([
    [
      '0102030405060708',
      { psk, mode: MODE.FULL_PQ_EACH_SESSION },
    ],
  ]),
  random: (length) => Buffer.from(Uint8Array.from(
    { length },
    (_, index) => (index * 11 + 3) & 0xff,
  )),
  onPlaintext: (plaintext) => {
    received.push(Buffer.from(plaintext));
    return Buffer.from('full-mode-accepted');
  },
});

const child = spawn('./build/session_interop_cli', [
  Buffer.from(keyPair.publicKey).toString('hex'),
  psk.toString('hex'),
  keyId.toString('hex'),
  String(MODE.FULL_PQ_EACH_SESSION),
], {
  stdio: ['pipe', 'pipe', 'pipe'],
});
let stderr = '';
let done;
child.stderr.setEncoding('utf8');
child.stderr.on('data', (chunk) => { stderr += chunk; });
const lines = readline.createInterface({ input: child.stdout });

for await (const line of lines) {
  if (line.startsWith('DONE ')) {
    done = Buffer.from(line.slice(5), 'hex').toString('utf8');
    continue;
  }
  assert.match(line, /^TX [0-9a-f]+$/);
  const response = peer.handleFrame(Buffer.from(line.slice(3), 'hex'));
  assert.ok(response);
  child.stdin.write(`RX ${response.toString('hex')}\n`);
}

const exitCode = await new Promise((resolve) => {
  child.on('close', resolve);
});
assert.equal(exitCode, 0, stderr);
assert.deepEqual(received.map((value) => value.toString('utf8')), [
  'temperature=24.5',
]);
assert.equal(done, 'full-mode-accepted');
const [session] = peer.sessions.values();
assert.equal(session.established, true);
assert.equal(session.receiveSequence, 1n);
assert.equal(session.sendSequence, 1n);
console.log('Live-session FULL_PQ_EACH_SESSION interoperability passed');
