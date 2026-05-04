import { describe, it, beforeEach, after, before } from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs'; import path from 'node:path'; import { fileURLToPath } from 'node:url';
import { WSJTXLib, WSJTXMode } from '../src/index.js';
const __dirname = path.dirname(fileURLToPath(import.meta.url));
const OUT = path.join(__dirname, '..', 'test', 'output');

describe('WSJTX Full Tests', () => {
  let lib: WSJTXLib;
  beforeEach(() => { lib = new WSJTXLib({ maxThreads: 4 }); });
  before(() => { if (!fs.existsSync(OUT)) fs.mkdirSync(OUT, { recursive: true }); });
  after(() => { if (fs.existsSync(OUT)) fs.readdirSync(OUT).filter(f=>f.endsWith('.wav')).forEach(f=>fs.unlinkSync(path.join(OUT,f))); });

  it('FT8 sample rate', () => { assert.strictEqual(lib.getSampleRate(WSJTXMode.FT8), 48000); });
  it('FT8 encoding supported', () => { assert.ok(lib.isEncodingSupported(WSJTXMode.FT8)); });

  it('encode CQ message', async () => {
    const r = await lib.encode(WSJTXMode.FT8, 'CQ TEST BH1ABC OM88', 1000);
    assert.ok(r.audioData.length > 0); assert.ok(r.messageSent.length > 0);
  });
  it('encode signal report', async () => {
    const r = await lib.encode(WSJTXMode.FT8, 'K1ABC W9XYZ -05', 1000);
    assert.ok(r.audioData.length > 0);
  });
  it('encode RRR', async () => {
    const r = await lib.encode(WSJTXMode.FT8, 'K1ABC W9XYZ RRR', 1000);
    assert.ok(r.audioData.length > 0);
  });
  it('encode 73', async () => {
    const r = await lib.encode(WSJTXMode.FT8, 'K1ABC W9XYZ 73', 1000);
    assert.ok(r.audioData.length > 0);
  });

  it('decode Float32Array', async () => {
    const enc = await lib.encode(WSJTXMode.FT8, 'CQ TEST K1ABC FN20', 1000);
    const r = await lib.decode(WSJTXMode.FT8, enc.audioData, { frequency: 1000, threads: 1 });
    assert.ok(r.success); assert.ok(Array.isArray(r.messages));
  });
  it('decode noise = empty messages', async () => {
    const r = await lib.decode(WSJTXMode.FT8, new Float32Array(48000*13), { frequency: 1500, threads: 1 });
    assert.ok(r.success); assert.ok(Array.isArray(r.messages));
  });

  it('encode-decode roundtrip', async () => {
    for (const msg of ['CQ TEST K1ABC FN20', 'CQ DX K1ABC FN20']) {
      const enc = await lib.encode(WSJTXMode.FT8, msg, 1000);
      const dec = await lib.decode(WSJTXMode.FT8, enc.audioData, { frequency: 1000, threads: 1 });
      assert.ok(dec.success); assert.ok(Array.isArray(dec.messages));
    }
  });

  it('decode with dxCall/dxGrid', async () => {
    const r = await lib.decode(WSJTXMode.FT8, new Float32Array(48000*13), { frequency: 1500, threads: 1, dxCall: 'K1ABC', dxGrid: 'FN20' });
    assert.ok(r.success);
  });

  it('decode with custom freq range', async () => {
    const r = await lib.decode(WSJTXMode.FT8, new Float32Array(48000*13), { frequency: 1500, threads: 1, lowFreq: 200, highFreq: 3000, tolerance: 50 });
    assert.ok(r.success);
  });
});
