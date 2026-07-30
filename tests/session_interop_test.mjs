import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import readline from 'node:readline';
import {
  C0PQPeer,
  decodeHeader,
  FRAME,
  generateKeyPair,
  MODE,
} from '../reference-peer/protocol.mjs';

const keySeed = Uint8Array.from(
  { length: 64 },
  (_, index) => (index * 13 + 7) & 0xff,
);
const psk = Buffer.from(Uint8Array.from(
  { length: 32 },
  (_, index) => (index * 19 + 11) & 0xff,
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
      { psk, mode: MODE.PQ_BOOTSTRAP_RATCHET },
    ],
  ]),
  random: (length) => Buffer.from(Uint8Array.from(
    { length },
    (_, index) => (index * 31 + 9) & 0xff,
  )),
  onPlaintext: (plaintext) => {
    received.push(Buffer.from(plaintext));
    return Buffer.from(`ack:${plaintext.toString('utf8')}`);
  },
});

const child = spawn('./build/session_interop_cli', [
  Buffer.from(keyPair.publicKey).toString('hex'),
  psk.toString('hex'),
  keyId.toString('hex'),
  String(MODE.PQ_BOOTSTRAP_RATCHET),
], {
  stdio: ['pipe', 'pipe', 'pipe'],
});
let stderr = '';
let deviceFinishedFrame;
child.stderr.setEncoding('utf8');
child.stderr.on('data', (chunk) => { stderr += chunk; });
const lines = readline.createInterface({ input: child.stdout });
const timedOut = {
  challenge: false,
  fragment7: false,
  finished: false,
  data: false,
};
const attacked = {
  fragment: false,
  finished: false,
  record: false,
};
let done;

for await (const line of lines) {
  if (line.startsWith('DONE ')) {
    done = Buffer.from(line.slice(5), 'hex').toString('utf8');
    continue;
  }
  assert.match(line, /^TX [0-9a-f]+$/);
  const frame = Buffer.from(line.slice(3), 'hex');
  const header = decodeHeader(frame);
  if (header.type === FRAME.DEVICE_FINISHED) {
    deviceFinishedFrame = Buffer.from(frame);
  }
  if (
    header.type === FRAME.CIPHERTEXT_FRAGMENT
    && frame[10] === 5
    && !attacked.fragment
  ) {
    attacked.fragment = true;
    const forged = Buffer.from(frame);
    forged[13] ^= 1;
    assert.throws(
      () => peer.handleFrame(forged),
      /fragment authentication failed/,
    );
    const session = peer.sessions.get(header.sessionId);
    assert.equal(session.received[5], false);
  } else if (
    header.type === FRAME.DEVICE_FINISHED
    && !attacked.finished
  ) {
    attacked.finished = true;
    const forged = Buffer.from(frame);
    forged[25] ^= 1;
    assert.throws(
      () => peer.handleFrame(forged),
      /Finished verification failed/,
    );
    assert.equal(peer.sessions.get(header.sessionId).established, false);
  } else if (header.type === FRAME.DATA && !attacked.record) {
    attacked.record = true;
    const forged = Buffer.from(frame);
    forged[forged.length - 1] ^= 1;
    assert.throws(
      () => peer.handleFrame(forged),
      /record authentication failed/,
    );
    assert.equal(peer.sessions.get(header.sessionId).receiveSequence, 0n);
  }
  const response = peer.handleFrame(frame);
  assert.ok(response, `peer response for frame ${header.type}`);
  let injectTimeout = false;
  if (header.type === FRAME.HELLO && !timedOut.challenge) {
    timedOut.challenge = true;
    injectTimeout = true;
  } else if (
    header.type === FRAME.CIPHERTEXT_FRAGMENT
    && frame[10] === 7
    && !timedOut.fragment7
  ) {
    timedOut.fragment7 = true;
    injectTimeout = true;
  } else if (
    header.type === FRAME.DEVICE_FINISHED
    && !timedOut.finished
  ) {
    timedOut.finished = true;
    injectTimeout = true;
  } else if (header.type === FRAME.DATA && !timedOut.data) {
    timedOut.data = true;
    injectTimeout = true;
  }
  child.stdin.write(
    injectTimeout ? 'TIMEOUT\n' : `RX ${response.toString('hex')}\n`,
  );
}

const exitCode = await new Promise((resolve) => {
  child.on('close', resolve);
});
assert.equal(exitCode, 0, stderr);
assert.equal(received.length, 1);
assert.equal(received[0].toString('utf8'), 'temperature=24.5');
assert.equal(done, 'ack:temperature=24.5');
assert.deepEqual(timedOut, {
  challenge: true,
  fragment7: true,
  finished: true,
  data: true,
});
assert.deepEqual(attacked, {
  fragment: true,
  finished: true,
  record: true,
});
const [session] = peer.sessions.values();
assert.equal(session.established, true);
assert.equal(session.received.every(Boolean), true);
assert.ok(deviceFinishedFrame);
assert.deepEqual(
  peer.handleFrame(deviceFinishedFrame),
  session.serverFinishedFrame,
  'authenticated duplicate Finished is idempotent',
);
const forgedDuplicateFinished = Buffer.from(deviceFinishedFrame);
forgedDuplicateFinished[25] ^= 1;
assert.throws(
  () => peer.handleFrame(forgedDuplicateFinished),
  /Finished verification failed/,
  'forged duplicate Finished must not receive a cached response',
);
console.log(
  'Live-session interop: ML-KEM, Finished, Ascon ratchet, tamper rejection, '
  + 'and handshake/data loss recovery passed',
);
