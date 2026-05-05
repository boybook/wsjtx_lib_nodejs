/**
 * wsjtx-lib — Node.js binding for the WSJT-X 3.0.0 backend.
 *
 * Public surface:
 *   - WSJTXLib.encode(mode, message, frequency)
 *   - WSJTXLib.decode(mode, audio, options)
 *   - WSJTXLib.decodeWSPR(audio, options)
 *   - WSJTXLib.convertAudioFormat(audio, target)
 *   - capability/sample-rate query helpers
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
} from './types.js';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const require = createRequire(import.meta.url);
const __dirname = path.dirname(fileURLToPath(import.meta.url));

interface NativeBinding {
  WSJTXLib: new () => NativeWSJTXLib;
}

interface NativeDecodeOptions {
  frequency: number;
  threads: number;
  lowFreq: number;
  highFreq: number;
  tolerance: number;
  dxCall: string;
  dxGrid: string;
}

interface NativeWSJTXLib {
  decode(mode: number, audio: AudioData, opts: NativeDecodeOptions, cb: (e: Error | null, r: DecodeResult) => void): void;
  encode(mode: number, message: string, frequency: number, threads: number, cb: (e: Error | null, r: EncodeResult) => void): void;
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

export class WSJTXLib {
  private readonly native: NativeWSJTXLib;
  private readonly config: Required<WSJTXConfig>;

  constructor(config: WSJTXConfig = {}) {
    this.config = { ...DEFAULT_CONFIG, ...config };
    this.native = new NativeWSJTXLib();
  }

  async decode(mode: WSJTXMode, audioData: AudioData, options: DecodeOptions): Promise<DecodeResult> {
    this.validateMode(mode);
    this.validateAudio(audioData);
    this.validateFrequency(options.frequency);
    if (!this.isDecodingSupported(mode)) {
      throw new WSJTXError('Decoding not supported for this mode', 'UNSUPPORTED');
    }

    const opts: NativeDecodeOptions = {
      frequency: options.frequency,
      threads: options.threads ?? this.config.maxThreads,
      lowFreq: options.lowFreq ?? this.config.defaultLowFreq,
      highFreq: options.highFreq ?? this.config.defaultHighFreq,
      tolerance: options.tolerance ?? this.config.defaultTolerance,
      dxCall: options.dxCall ?? '',
      dxGrid: options.dxGrid ?? '',
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
    threads: number = this.config.maxThreads,
  ): Promise<EncodeResult> {
    this.validateMode(mode);
    this.validateMessage(message);
    this.validateFrequency(frequency);
    this.validateThreads(threads);
    if (!this.isEncodingSupported(mode)) {
      throw new WSJTXError('Encoding not supported for this mode', 'UNSUPPORTED');
    }

    return new Promise((resolve, reject) => {
      this.native.encode(mode, message, frequency, threads, (err, result) => {
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
}

export { WSJTXMode, WSJTXError };
export type {
  DecodeResult,
  EncodeResult,
  WSPRResult,
  WSPRDecodeOptions,
  WSJTXMessage,
  AudioData,
  WSJTXConfig,
  DecodeOptions,
  ModeCapabilities,
};
