import { WSJTXMode, DecodeResult, EncodeResult, WSPRResult, WSPRDecodeOptions, WSJTXMessage, AudioData, WSJTXError, WSJTXConfig, ModeCapabilities, DecodeOptions } from './types.js';
import { createRequire } from 'node:module'; import { fileURLToPath } from 'node:url'; import path from 'node:path';
const require = createRequire(import.meta.url); const __dirname = path.dirname(fileURLToPath(import.meta.url));
function loadNativeBinding(): any { return require('node-gyp-build')(path.resolve(__dirname, '..', '..')).WSJTXLib; }
const NativeWSJTXLib = loadNativeBinding();

export class WSJTXLib {
  private native: any; private config: WSJTXConfig;
  constructor(config: WSJTXConfig = {}) { this.config = { maxThreads: 4, debug: false, defaultLowFreq: 200, defaultHighFreq: 4000, defaultTolerance: 20, ...config }; this.native = new NativeWSJTXLib(); }

  async decode(mode: WSJTXMode, audioData: AudioData, options: DecodeOptions): Promise<DecodeResult> {
    this.vMode(mode); this.vAudio(audioData); this.vFreq(options.frequency);
    if (!this.isDecodingSupported(mode)) throw new WSJTXError('Decoding not supported', 'UNSUPPORTED');
    const opts = { frequency: options.frequency, threads: options.threads ?? this.config.maxThreads ?? 4, lowFreq: options.lowFreq ?? this.config.defaultLowFreq ?? 200, highFreq: options.highFreq ?? this.config.defaultHighFreq ?? 4000, tolerance: options.tolerance ?? this.config.defaultTolerance ?? 20, dxCall: options.dxCall ?? '', dxGrid: options.dxGrid ?? '' };
    return new Promise((resolve, reject) => { this.native.decode(mode, audioData, opts, (e: any, r: any) => e ? reject(new WSJTXError(e.message, 'DECODE_ERROR')) : resolve(r)); });
  }

  async encode(mode: WSJTXMode, message: string, frequency: number, threads: number = this.config.maxThreads || 4): Promise<EncodeResult> {
    this.vMode(mode); this.vMsg(message); this.vFreq(frequency); this.vThreads(threads);
    if (!this.isEncodingSupported(mode)) throw new WSJTXError('Encoding not supported', 'UNSUPPORTED');
    return new Promise((resolve, reject) => { this.native.encode(mode, message, frequency, threads, (e: any, r: any) => e ? reject(new WSJTXError(e.message, 'ENCODE_ERROR')) : resolve(r)); });
  }

  async decodeWSPR(audioData: Int16Array, options: WSPRDecodeOptions = {}): Promise<WSPRResult[]> {
    if (!(audioData instanceof Int16Array) || audioData.length === 0) throw new WSJTXError('Must be non-empty Int16Array', 'INVALID');
    const o = { dialFrequency: 14095600, callsign: '', locator: '', quickMode: false, useHashTable: true, passes: 2, subtraction: true, ...options };
    return new Promise((resolve, reject) => { this.native.decodeWSPR(audioData, o, (e: any, r: any) => e ? reject(new WSJTXError(e.message, 'WSPR_ERROR')) : resolve(r)); });
  }

  pullMessages(): WSJTXMessage[] { return this.native.pullMessages(); }
  isEncodingSupported(m: WSJTXMode): boolean { return this.native.isEncodingSupported(m); }
  isDecodingSupported(m: WSJTXMode): boolean { return this.native.isDecodingSupported(m); }
  getSampleRate(m: WSJTXMode): number { return this.native.getSampleRate(m); }
  getTransmissionDuration(m: WSJTXMode): number { return this.native.getTransmissionDuration(m); }
  getAllModeCapabilities(): ModeCapabilities[] { return Object.values(WSJTXMode).filter(v => typeof v === 'number').map(m => ({ mode: m as WSJTXMode, encodingSupported: this.isEncodingSupported(m as WSJTXMode), decodingSupported: this.isDecodingSupported(m as WSJTXMode), sampleRate: this.getSampleRate(m as WSJTXMode), duration: this.getTransmissionDuration(m as WSJTXMode) })); }
  async convertAudioFormat(audioData: AudioData, targetFormat: 'float32'|'int16'): Promise<AudioData> { return new Promise((resolve, reject) => { this.native.convertAudioFormat(audioData, targetFormat, (e: any, r: any) => e ? reject(e) : resolve(r)); }); }

  private vMode(m: WSJTXMode) { if (!Object.values(WSJTXMode).includes(m)) throw new WSJTXError('Invalid mode', 'INVALID'); }
  private vFreq(f: number) { if (!Number.isInteger(f) || f < 0 || f > 30000000) throw new WSJTXError('Invalid frequency', 'INVALID'); }
  private vThreads(t: number) { if (!Number.isInteger(t) || t < 1 || t > 16) throw new WSJTXError('Invalid threads', 'INVALID'); }
  private vMsg(m: string) { if (typeof m !== 'string' || m.length === 0 || m.length > 37) throw new WSJTXError('Invalid message', 'INVALID'); }
  private vAudio(a: AudioData) { if (!(a instanceof Float32Array) && !(a instanceof Int16Array) || a.length === 0) throw new WSJTXError('Invalid audio', 'INVALID'); }
}
export { WSJTXMode, WSJTXError }; export type { DecodeResult, EncodeResult, WSPRResult, WSPRDecodeOptions, WSJTXMessage, AudioData, WSJTXConfig, DecodeOptions, ModeCapabilities };
