import { describe, it, beforeEach, afterEach } from 'node:test';
import assert from 'node:assert';
import { WSJTXLib, WSJTXMode, WSJTXError } from '../src/index.js';

describe('WSJTX Library Basic Tests', () => {
  let lib: WSJTXLib;
  beforeEach(() => { lib = new WSJTXLib({ maxThreads: 4 }); });
  afterEach(() => {});

  it('creates instance', () => { assert.ok(lib instanceof WSJTXLib); });
  it('FT8 sample rate 48000', () => { assert.strictEqual(lib.getSampleRate(WSJTXMode.FT8), 48000); });
  it('FT8 encoding supported', () => { assert.ok(lib.isEncodingSupported(WSJTXMode.FT8)); });
  it('FT8 decoding supported', () => { assert.ok(lib.isDecodingSupported(WSJTXMode.FT8)); });
  it('WSPR=9', () => { assert.strictEqual(WSJTXMode.WSPR, 9); });
  it('JT65JT9=8', () => { assert.strictEqual(WSJTXMode.JT65JT9, 8); });
  it('all capabilities returned', () => { assert.ok(lib.getAllModeCapabilities().length > 0); });

  it('rejects invalid mode', async () => {
    await assert.rejects(() => lib.decode(999 as any, new Float32Array(1000), { frequency: 1500 }), { name: 'WSJTXError' });
  });
  it('rejects negative frequency', async () => {
    await assert.rejects(() => lib.decode(WSJTXMode.FT8, new Float32Array(1000), { frequency: -1 }), { name: 'WSJTXError' });
  });
  it('rejects empty audio', async () => {
    await assert.rejects(() => lib.decode(WSJTXMode.FT8, new Float32Array(0), { frequency: 1500 }), { name: 'WSJTXError' });
  });
  it('pullMessages returns array', () => { assert.ok(Array.isArray(lib.pullMessages())); });

  it('audio convert Float32→Int16', async () => {
    const r = await lib.convertAudioFormat(new Float32Array([-1,0,0.5,1]), 'int16');
    assert.ok(r instanceof Int16Array);
  });
  it('WSJTXError extends Error', () => {
    const e = new WSJTXError('test', 'CODE');
    assert.ok(e instanceof Error); assert.strictEqual(e.code, 'CODE');
  });

  it('decode returns messages array', async () => {
    const r = await lib.decode(WSJTXMode.FT8, new Float32Array(48000*13), { frequency: 1500, threads: 1 });
    assert.ok(r.success); assert.ok(Array.isArray(r.messages));
  });

  it('decode with dxCall/dxGrid options', async () => {
    const r = await lib.decode(WSJTXMode.FT8, new Float32Array(48000*13), { frequency: 1500, threads: 1, dxCall: 'K1ABC', dxGrid: 'FN20' });
    assert.ok(r.success); assert.ok(Array.isArray(r.messages));
  });
});
