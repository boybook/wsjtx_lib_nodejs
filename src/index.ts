/**
 * wsjtx-lib — Node.js binding for the WSJT-X 3.0.0 backend.
 *
 * Public surface:
 *   - WSJTXLib.encode(mode, message, frequency, threadsOrOptions?)
 *   - WSJTXLib.decode(mode, audio, options)
 *   - WSJTXLib.decodeWSPR(audio, options)
 *   - WSJTXLib.convertAudioFormat(audio, target)
 *   - capability/sample-rate query helpers (FT8/FT4/Q65 default encode rate: 12 kHz)
 */

import {
  WSJTXMode,
  type DecodeResult,
  type EncodeResult,
  type WSPRResult,
  type WSPRDecodeOptions,
  type WSJTXMessage,
  type AudioData,
  WSJTXError,
  type WSJTXConfig,
  type ModeCapabilities,
  type DecodeOptions,
  type EncodeOptions,
  type Q65Period,
  type Q65Submode,
} from './types.js';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const require = createRequire(import.meta.url);
const __dirname = path.dirname(fileURLToPath(import.meta.url));

interface NativeBinding {
  WSJTXLib: new (config?: { encodeSampleRate?: number }) => NativeWSJTXLib;
}

interface NativeDecodeOptions {
  frequency: number;
  txFrequency: number;
  threads: number;
  lowFreq: number;
  highFreq: number;
  tolerance: number;
  myCall: string;
  myGrid: string;
  dxCall: string;
  dxGrid: string;
  apDecode: boolean;
  decodeDepth: number;
  qsoProgress: number;
  q65Period: number;
  q65Submode: number;
  q65MaxDrift: number;
  q65ClearAveraging: boolean;
  q65SingleDecode: boolean;
  q65Averaging: boolean;
}

interface NativeEncodeOptions {
  threads: number;
  q65Period: number;
  q65Submode: number;
}

interface NativeWSJTXLib {
  decode(mode: number, audio: AudioData, opts: NativeDecodeOptions, cb: (e: Error | null, r: DecodeResult) => void): void;
  encode(mode: number, message: string, frequency: number, opts: NativeEncodeOptions, cb: (e: Error | null, r: EncodeResult) => void): void;
  decodeWSPR(audio: Float32Array, opts: Record<string, unknown>, cb: (e: Error | null, r: WSPRResult[]) => void): void;
  pullMessages(): WSJTXMessage[];
  isEncodingSupported(mode: number): boolean;
  isDecodingSupported(mode: number): boolean;
  getSampleRate(mode: number): number;
  getTransmissionDuration(mode: number): number;
  convertAudioFormat(audio: AudioData, target: 'float32' | 'int16', cb: (e: Error | null, r: AudioData) => void): void;
}

function loadNativeBinding(): NativeBinding['WSJTXLib'] {
  const binding = require('node-gyp-build')(path.resolve(__dirname, '..', '..')) as NativeBinding;
  return binding.WSJTXLib;
}

const NativeWSJTXLib = loadNativeBinding();

const DEFAULT_CONFIG: Required<WSJTXConfig> = {
  maxThreads: 4,
  encodeSampleRate: 12000,
  debug: false,
  defaultLowFreq: 200,
  defaultHighFreq: 4000,
  defaultTolerance: 20,
};

const FREQ_MIN = 0;
const FREQ_MAX = 30_000_000;
const THREADS_MIN = 1;
const THREADS_MAX = 16;
const MESSAGE_MAX_LEN = 37;
const Q65_PERIODS = new Set<number>([30, 60, 120, 300]);
const Q65_SUBMODES = new Map<string, number>([
  ['A', 0],
  ['B', 1],
  ['C', 2],
  ['D', 3],
  ['E', 4],
]);

export class WSJTXLib {
  private readonly native: NativeWSJTXLib;
  private readonly config: Required<WSJTXConfig>;

  constructor(config: WSJTXConfig = {}) {
    this.config = { ...DEFAULT_CONFIG, ...config };
    this.validateEncodeSampleRate(this.config.encodeSampleRate);
    this.native = new NativeWSJTXLib({ encodeSampleRate: this.config.encodeSampleRate });
  }

  async decode(mode: WSJTXMode, audioData: AudioData, options: DecodeOptions): Promise<DecodeResult> {
    this.validateMode(mode);
    this.validateAudio(audioData);
    this.validateFrequency(options.frequency);
    const threads = options.threads ?? this.config.maxThreads;
    this.validateThreads(threads);
    if (!this.isDecodingSupported(mode)) {
      throw new WSJTXError('Decoding not supported for this mode', 'UNSUPPORTED');
    }

    const q65Period = this.normalizeQ65Period(options.q65Period ?? 60);
    const q65Submode = this.normalizeQ65Submode(options.q65Submode ?? 'A');
    const q65MaxDrift = options.q65MaxDrift ?? 50;
    this.validateNonNegativeInteger(q65MaxDrift, 'q65MaxDrift');

    const opts: NativeDecodeOptions = {
      frequency: options.frequency,
      txFrequency: options.txFrequency ?? options.frequency,
      threads,
      lowFreq: options.lowFreq ?? this.config.defaultLowFreq,
      highFreq: options.highFreq ?? this.config.defaultHighFreq,
      tolerance: options.tolerance ?? this.config.defaultTolerance,
      myCall: options.myCall ?? '',
      myGrid: options.myGrid ?? '',
      dxCall: options.dxCall ?? '',
      dxGrid: options.dxGrid ?? '',
      apDecode: options.apDecode ?? true,
      decodeDepth: options.decodeDepth ?? 1,
      qsoProgress: options.qsoProgress ?? 0,
      q65Period,
      q65Submode,
      q65MaxDrift,
      q65ClearAveraging: options.q65ClearAveraging ?? false,
      q65SingleDecode: options.q65SingleDecode ?? false,
      q65Averaging: options.q65Averaging ?? false,
    };

    return new Promise((resolve, reject) => {
      this.native.decode(mode, audioData, opts, (err, result) => {
        if (err) reject(new WSJTXError(err.message, 'DECODE_ERROR'));
        else resolve(result);
      });
    });
  }

  async encode(
    mode: WSJTXMode,
    message: string,
    frequency: number,
    threadsOrOptions: number | EncodeOptions = this.config.maxThreads,
  ): Promise<EncodeResult> {
    this.validateMode(mode);
    this.validateMessage(message);
    this.validateFrequency(frequency);
    const opts = this.normalizeEncodeOptions(threadsOrOptions);
    this.validateThreads(opts.threads);
    if (!this.isEncodingSupported(mode)) {
      throw new WSJTXError('Encoding not supported for this mode', 'UNSUPPORTED');
    }

    return new Promise((resolve, reject) => {
      this.native.encode(mode, message, frequency, opts, (err, result) => {
        if (err) reject(new WSJTXError(err.message, 'ENCODE_ERROR'));
        else resolve(result);
      });
    });
  }

  async decodeWSPR(audioData: Int16Array, options: WSPRDecodeOptions = {}): Promise<WSPRResult[]> {
    if (!(audioData instanceof Int16Array) || audioData.length === 0) {
      throw new WSJTXError('audioData must be a non-empty Int16Array', 'INVALID');
    }

    const opts = {
      dialFrequency: 14_095_600,
      callsign: '',
      locator: '',
      quickMode: false,
      useHashTable: true,
      passes: 2,
      subtraction: true,
      ...options,
    };

    return new Promise((resolve, reject) => {
      this.native.decodeWSPR(audioData as unknown as Float32Array, opts, (err, results) => {
        if (err) reject(new WSJTXError(err.message, 'WSPR_ERROR'));
        else resolve(results);
      });
    });
  }

  pullMessages(): WSJTXMessage[] {
    return this.native.pullMessages();
  }

  isEncodingSupported(mode: WSJTXMode): boolean {
    return this.native.isEncodingSupported(mode);
  }

  isDecodingSupported(mode: WSJTXMode): boolean {
    return this.native.isDecodingSupported(mode);
  }

  getSampleRate(mode: WSJTXMode): number {
    return this.native.getSampleRate(mode);
  }

  getTransmissionDuration(mode: WSJTXMode): number {
    return this.native.getTransmissionDuration(mode);
  }

  getAllModeCapabilities(): ModeCapabilities[] {
    const numericModes = Object.values(WSJTXMode).filter((v): v is number => typeof v === 'number');
    return numericModes.map((mode) => ({
      mode: mode as WSJTXMode,
      encodingSupported: this.isEncodingSupported(mode as WSJTXMode),
      decodingSupported: this.isDecodingSupported(mode as WSJTXMode),
      sampleRate: this.getSampleRate(mode as WSJTXMode),
      duration: this.getTransmissionDuration(mode as WSJTXMode),
    }));
  }

  async convertAudioFormat(audioData: AudioData, targetFormat: 'float32' | 'int16'): Promise<AudioData> {
    return new Promise((resolve, reject) => {
      this.native.convertAudioFormat(audioData, targetFormat, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
  }

  private normalizeEncodeOptions(threadsOrOptions: number | EncodeOptions): NativeEncodeOptions {
    const options = typeof threadsOrOptions === 'number' ? { threads: threadsOrOptions } : threadsOrOptions;
    const threads = options.threads ?? this.config.maxThreads;
    return {
      threads,
      q65Period: this.normalizeQ65Period(options.q65Period ?? 60),
      q65Submode: this.normalizeQ65Submode(options.q65Submode ?? 'A'),
    };
  }

  private validateMode(mode: WSJTXMode): void {
    if (!Object.values(WSJTXMode).includes(mode)) {
      throw new WSJTXError('Invalid mode', 'INVALID');
    }
  }

  private validateFrequency(freq: number): void {
    if (!Number.isInteger(freq) || freq < FREQ_MIN || freq > FREQ_MAX) {
      throw new WSJTXError('Invalid frequency', 'INVALID');
    }
  }

  private validateEncodeSampleRate(sampleRate: number): void {
    if (sampleRate !== 12000 && sampleRate !== 48000) {
      throw new WSJTXError('encodeSampleRate must be 12000 or 48000', 'INVALID');
    }
  }

  private validateThreads(threads: number): void {
    if (!Number.isInteger(threads) || threads < THREADS_MIN || threads > THREADS_MAX) {
      throw new WSJTXError(`Threads must be ${THREADS_MIN}..${THREADS_MAX}`, 'INVALID');
    }
  }

  private validateMessage(message: string): void {
    if (typeof message !== 'string' || message.length === 0 || message.length > MESSAGE_MAX_LEN) {
      throw new WSJTXError(`Message must be 1..${MESSAGE_MAX_LEN} characters`, 'INVALID');
    }
  }

  private validateAudio(audio: AudioData): void {
    const isTyped = audio instanceof Float32Array || audio instanceof Int16Array;
    if (!isTyped || audio.length === 0) {
      throw new WSJTXError('audioData must be a non-empty Float32Array or Int16Array', 'INVALID');
    }
  }

  private normalizeQ65Period(period: number): number {
    if (!Number.isInteger(period) || !Q65_PERIODS.has(period)) {
      throw new WSJTXError('q65Period must be one of 30, 60, 120, or 300', 'INVALID');
    }
    return period;
  }

  private normalizeQ65Submode(submode: Q65Submode): number {
    if (typeof submode === 'number') {
      if (!Number.isInteger(submode) || submode < 0 || submode > 4) {
        throw new WSJTXError('q65Submode must be A-E or 0..4', 'INVALID');
      }
      return submode;
    }
    const normalized = Q65_SUBMODES.get(submode.toUpperCase());
    if (normalized === undefined) {
      throw new WSJTXError('q65Submode must be A-E or 0..4', 'INVALID');
    }
    return normalized;
  }

  private validateNonNegativeInteger(value: number, name: string): void {
    if (!Number.isInteger(value) || value < 0) {
      throw new WSJTXError(`${name} must be a non-negative integer`, 'INVALID');
    }
  }
}

export { WSJTXMode, WSJTXError };
export type {
  DecodeResult,
  EncodeResult,
  EncodeOptions,
  Q65Period,
  Q65Submode,
  WSPRResult,
  WSPRDecodeOptions,
  WSJTXMessage,
  AudioData,
  WSJTXConfig,
  DecodeOptions,
  ModeCapabilities,
};
