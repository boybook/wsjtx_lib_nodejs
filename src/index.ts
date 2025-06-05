/**
 * WSJTX Digital Radio Protocol Library for Node.js
 * 
 * High-level TypeScript wrapper around the native C++ WSJTX library bindings.
 * Provides async/await support, input validation, and convenient utilities
 * for working with digital amateur radio protocols.
 * 
 * @example
 * ```typescript
 * import { WSJTXLib, WSJTXMode } from 'wsjtx-lib';
 * 
 * const lib = new WSJTXLib();
 * 
 * // Decode FT8 audio data
 * const audioData = new Float32Array(48000 * 13); // 13 seconds at 48kHz
 * const result = await lib.decode(WSJTXMode.FT8, audioData, 1500);
 * 
 * // Get decoded messages
 * const messages = lib.pullMessages();
 * console.log('Decoded messages:', messages);
 * ```
 */

import {
  WSJTXMode,
  DecodeResult,
  EncodeResult,
  WSPRResult,
  WSPRDecodeOptions,
  WSJTXMessage,
  AudioData,
  IQData,
  WSJTXError,
  WSJTXConfig,
  VersionInfo,
  ModeCapabilities,
  DecodeCallback,
  EncodeCallback,
  WSPRDecodeCallback
} from './types.js';

import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import fs from 'node:fs';

// Create require function for ES modules
const require = createRequire(import.meta.url);

// Get the directory of this file
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Determine the correct path to the native module
// Priority order:
// 1. Prebuilt binaries (for npm published packages)
// 2. Local build output (for development)
function findNativeModule(): string {
  const platform = process.platform;
  const arch = process.arch;
  
  const possiblePaths = [
    // 1. Prebuilt binaries (npm packages) - highest priority
    path.resolve(__dirname, '..', 'prebuilds', `${platform}-${arch}`, 'wsjtx_lib_nodejs.node'),
    
    // 2. GitHub Actions legacy format (for backward compatibility)
    path.resolve(__dirname, '..', 'prebuilds', `${platform}-latest-${arch}`, 'wsjtx_lib_nodejs.node'),
    path.resolve(__dirname, '..', 'prebuilds', `ubuntu-latest-${arch}`, 'wsjtx_lib_nodejs.node'), // Linux
    path.resolve(__dirname, '..', 'prebuilds', `macos-latest-${arch}`, 'wsjtx_lib_nodejs.node'),  // macOS  
    path.resolve(__dirname, '..', 'prebuilds', `windows-latest-${arch}`, 'wsjtx_lib_nodejs.node'), // Windows
    
    // 3. Local development builds - third priority
    // From dist/src/ - direct build output
    path.resolve(__dirname, '..', '..', 'build', 'wsjtx_lib_nodejs.node'),
    // From dist/src/ - Release subdirectory
    path.resolve(__dirname, '..', '..', 'build', 'Release', 'wsjtx_lib_nodejs.node'),
    
    // 4. Direct build output (when running from src/)
    path.resolve(__dirname, '..', 'build', 'wsjtx_lib_nodejs.node'),
    // Release subdirectory (MSVC, cmake default, etc.)
    path.resolve(__dirname, '..', 'build', 'Release', 'wsjtx_lib_nodejs.node'),
  ];

  for (const modulePath of possiblePaths) {
    if (fs.existsSync(modulePath)) {
      return modulePath;
    }
  }

  // If no module found, throw a helpful error with all attempted paths
  const pathList = possiblePaths.map(p => `  - ${p}`).join('\n');
  throw new Error(
    `Native module not found for ${platform}-${arch}.\n` +
    `Searched in:\n${pathList}\n\n` +
    'Solutions:\n' +
    '1. If you installed via npm, this may be a missing prebuilt binary\n' +
    '2. For development, run "npm run build" to compile the native module\n' +
    '3. Check if your platform/architecture is supported'
  );
}

const nativeModulePath = findNativeModule();

// Import the native module (will be built by cmake-js)
// @ts-ignore - Native module types are defined separately
const { WSJTXLib: NativeWSJTXLib } = require(nativeModulePath);

/**
 * Main WSJTX library class providing digital radio protocol processing
 * 
 * This class wraps the native C++ implementation and provides a convenient
 * TypeScript/JavaScript interface with proper error handling, validation,
 * and async/await support.
 */
export class WSJTXLib {
  private native: any;
  private config: WSJTXConfig;

  /**
   * Create a new WSJTX library instance
   * 
   * @param config Optional configuration options
   * @throws {WSJTXError} If the native library fails to initialize
   */
  constructor(config: WSJTXConfig = {}) {
    this.config = {
      maxThreads: 4,
      debug: false,
      ...config
    };

    try {
      this.native = new NativeWSJTXLib();
    } catch (error) {
      throw new WSJTXError(
        `Failed to initialize WSJTX library: ${error instanceof Error ? error.message : String(error)}`,
        'INIT_ERROR'
      );
    }
  }

  /**
   * Decode digital radio signals from audio data
   * 
   * This method processes audio samples and attempts to decode digital
   * messages using the specified protocol mode. The operation is performed
   * asynchronously to avoid blocking the Node.js event loop.
   * 
   * @param mode The digital mode to use for decoding
   * @param audioData Audio samples (Float32Array or Int16Array)
   * @param frequency Center frequency in Hz
   * @param threads Number of threads to use (1-16, default: 4)
   * @returns Promise that resolves when decoding is complete
   * 
   * @throws {WSJTXError} If parameters are invalid or decoding fails
   * 
   * @example
   * ```typescript
   * const audioData = new Float32Array(48000 * 13); // 13 seconds
   * await lib.decode(WSJTXMode.FT8, audioData, 1500);
   * const messages = lib.pullMessages();
   * ```
   */
  async decode(
    mode: WSJTXMode,
    audioData: AudioData,
    frequency: number,
    threads: number = this.config.maxThreads || 4
  ): Promise<DecodeResult> {
    this.validateMode(mode);
    this.validateFrequency(frequency);
    this.validateThreads(threads);
    this.validateAudioData(audioData);

    if (!this.isDecodingSupported(mode)) {
      throw new WSJTXError(`Decoding not supported for mode: ${WSJTXMode[mode]}`, 'UNSUPPORTED_MODE');
    }

    return new Promise((resolve, reject) => {
      const callback: DecodeCallback = (error, result) => {
        if (error) {
          reject(new WSJTXError(error.message, 'DECODE_ERROR'));
        } else {
          // Convert boolean result to DecodeResult object
          resolve({ success: result as boolean });
        }
      };

      try {
        this.native.decode(mode, audioData, frequency, threads, callback);
      } catch (error) {
        reject(new WSJTXError(
          `Decode operation failed: ${error instanceof Error ? error.message : String(error)}`,
          'DECODE_ERROR'
        ));
      }
    });
  }

  /**
   * Encode a message into audio waveform for transmission
   * 
   * Generates the audio waveform that represents the specified message
   * using the given digital mode. The resulting audio can be fed to
   * a radio transmitter or audio interface.
   * 
   * @param mode The digital mode to use for encoding
   * @param message The message text to encode (mode-specific format)
   * @param frequency Center frequency in Hz
   * @param threads Number of threads to use (1-16, default: 4)
   * @returns Promise that resolves with encoded audio data and actual message sent
   * 
   * @throws {WSJTXError} If parameters are invalid or encoding fails
   * 
   * @example
   * ```typescript
   * const result = await lib.encode(WSJTXMode.FT8, 'CQ DX K1ABC FN20', 1500);
   * console.log('Generated audio samples:', result.audioData.length);
   * console.log('Actual message sent:', result.messageSent);
   * ```
   */
  async encode(
    mode: WSJTXMode,
    message: string,
    frequency: number,
    threads: number = this.config.maxThreads || 4
  ): Promise<EncodeResult> {
    this.validateMode(mode);
    this.validateMessage(message);
    this.validateFrequency(frequency);
    this.validateThreads(threads);

    if (!this.isEncodingSupported(mode)) {
      throw new WSJTXError(`Encoding not supported for mode: ${WSJTXMode[mode]}`, 'UNSUPPORTED_MODE');
    }

    return new Promise((resolve, reject) => {
      const callback: EncodeCallback = (error, result) => {
        if (error) {
          reject(new WSJTXError(error.message, 'ENCODE_ERROR'));
        } else {
          resolve(result);
        }
      };

      try {
        this.native.encode(mode, message, frequency, threads, callback);
      } catch (error) {
        reject(new WSJTXError(
          `Encode operation failed: ${error instanceof Error ? error.message : String(error)}`,
          'ENCODE_ERROR'
        ));
      }
    });
  }

  /**
   * Decode WSPR signals from IQ data
   * 
   * WSPR (Weak Signal Propagation Reporter) is a specialized protocol
   * for studying radio propagation. This method takes IQ (complex)
   * samples and attempts to decode WSPR transmissions.
   * 
   * @param iqData Interleaved I,Q samples as Float32Array
   * @param options Decoder options (frequency, callsign, etc.)
   * @returns Promise that resolves with array of decoded WSPR results
   * 
   * @throws {WSJTXError} If parameters are invalid or decoding fails
   * 
   * @example
   * ```typescript
   * const iqData = new Float32Array(2 * 12000 * 111); // 2 minutes of IQ data
   * const options = {
   *   dialFrequency: 14095600, // 20m WSPR frequency
   *   callsign: 'K1ABC',
   *   locator: 'FN20'
   * };
   * const results = await lib.decodeWSPR(iqData, options);
   * ```
   */
  async decodeWSPR(
    iqData: IQData,
    options: WSPRDecodeOptions = {}
  ): Promise<WSPRResult[]> {
    this.validateIQData(iqData);

    const defaultOptions: Required<WSPRDecodeOptions> = {
      dialFrequency: 14095600, // 20m WSPR frequency
      callsign: '',
      locator: '',
      quickMode: false,
      useHashTable: true,
      passes: 2,
      subtraction: true,
      ...options
    };

    return new Promise((resolve, reject) => {
      const callback: WSPRDecodeCallback = (error, results) => {
        if (error) {
          reject(new WSJTXError(error.message, 'WSPR_DECODE_ERROR'));
        } else {
          resolve(results);
        }
      };

      try {
        this.native.decodeWSPR(iqData, defaultOptions, callback);
      } catch (error) {
        reject(new WSJTXError(
          `WSPR decode failed: ${error instanceof Error ? error.message : String(error)}`,
          'WSPR_DECODE_ERROR'
        ));
      }
    });
  }

  /**
   * Retrieve decoded messages from the internal queue
   * 
   * Messages are added to an internal queue as they are decoded.
   * This method retrieves and removes all pending messages from the queue.
   * 
   * @returns Array of decoded messages
   * 
   * @example
   * ```typescript
   * const messages = lib.pullMessages();
   * messages.forEach(msg => {
   *   console.log(`${msg.text} (SNR: ${msg.snr} dB, ΔT: ${msg.deltaTime}s)`);
   * });
   * ```
   */
  pullMessages(): WSJTXMessage[] {
    try {
      return this.native.pullMessages();
    } catch (error) {
      throw new WSJTXError(
        `Failed to pull messages: ${error instanceof Error ? error.message : String(error)}`,
        'PULL_ERROR'
      );
    }
  }

  /**
   * Check if encoding is supported for a specific mode
   * 
   * @param mode The mode to check
   * @returns True if encoding is supported
   */
  isEncodingSupported(mode: WSJTXMode): boolean {
    return this.native.isEncodingSupported(mode);
  }

  /**
   * Check if decoding is supported for a specific mode
   * 
   * @param mode The mode to check
   * @returns True if decoding is supported
   */
  isDecodingSupported(mode: WSJTXMode): boolean {
    return this.native.isDecodingSupported(mode);
  }

  /**
   * Get the required sample rate for a specific mode
   * 
   * @param mode The mode to query
   * @returns Sample rate in Hz
   */
  getSampleRate(mode: WSJTXMode): number {
    return this.native.getSampleRate(mode);
  }

  /**
   * Get the transmission duration for a specific mode
   * 
   * @param mode The mode to query
   * @returns Duration in seconds
   */
  getTransmissionDuration(mode: WSJTXMode): number {
    return this.native.getTransmissionDuration(mode);
  }

  /**
   * Get capabilities for all supported modes
   * 
   * @returns Array of mode capability information
   */
  getAllModeCapabilities(): ModeCapabilities[] {
    const modes = Object.values(WSJTXMode).filter(v => typeof v === 'number') as WSJTXMode[];
    
    return modes.map(mode => ({
      mode,
      encodingSupported: this.isEncodingSupported(mode),
      decodingSupported: this.isDecodingSupported(mode),
      sampleRate: this.getSampleRate(mode),
      duration: this.getTransmissionDuration(mode)
    }));
  }

  /**
   * Convert audio format between Float32Array and Int16Array
   * 
   * @param audioData Input audio data
   * @param targetFormat Target format ('float32' or 'int16')
   * @returns Converted audio data
   */
  static convertAudioFormat(
    audioData: AudioData,
    targetFormat: 'float32' | 'int16'
  ): AudioData {
    if (targetFormat !== 'float32' && targetFormat !== 'int16') {
      throw new Error(`Invalid target format: ${targetFormat}. Must be 'float32' or 'int16'`);
    }

    if (targetFormat === 'float32') {
      if (audioData instanceof Float32Array) {
        return audioData;
      }
      // Convert Int16Array to Float32Array
      const result = new Float32Array(audioData.length);
      for (let i = 0; i < audioData.length; i++) {
        result[i] = audioData[i] / 32768.0;
      }
      return result;
    } else {
      if (audioData instanceof Int16Array) {
        return audioData;
      }
      // Convert Float32Array to Int16Array
      const result = new Int16Array(audioData.length);
      for (let i = 0; i < audioData.length; i++) {
        result[i] = Math.max(-32768, Math.min(32767, Math.round(audioData[i] * 32768)));
      }
      return result;
    }
  }

  // Validation methods
  private validateMode(mode: WSJTXMode): void {
    if (!Object.values(WSJTXMode).includes(mode)) {
      throw new WSJTXError(`Invalid mode: ${mode}`, 'INVALID_MODE');
    }
  }

  private validateFrequency(frequency: number): void {
    if (!Number.isInteger(frequency) || frequency < 0 || frequency > 30000000) {
      throw new WSJTXError(
        `Invalid frequency: ${frequency}. Must be between 0 and 30,000,000 Hz`,
        'INVALID_FREQUENCY'
      );
    }
  }

  private validateThreads(threads: number): void {
    if (!Number.isInteger(threads) || threads < 1 || threads > 16) {
      throw new WSJTXError(
        `Invalid thread count: ${threads}. Must be between 1 and 16`,
        'INVALID_THREADS'
      );
    }
  }

  private validateMessage(message: string): void {
    if (typeof message !== 'string' || message.length === 0 || message.length > 22) {
      throw new WSJTXError(
        `Invalid message: "${message}". Must be 1-22 characters long`,
        'INVALID_MESSAGE'
      );
    }
  }

  private validateAudioData(audioData: AudioData): void {
    if (!(audioData instanceof Float32Array) && !(audioData instanceof Int16Array)) {
      throw new WSJTXError(
        'Invalid audio data: must be Float32Array or Int16Array',
        'INVALID_AUDIO_DATA'
      );
    }
    if (audioData.length === 0) {
      throw new WSJTXError('Audio data cannot be empty', 'INVALID_AUDIO_DATA');
    }
  }

  private validateIQData(iqData: IQData): void {
    if (!(iqData instanceof Float32Array)) {
      throw new WSJTXError(
        'Invalid IQ data: must be Float32Array with interleaved I,Q samples',
        'INVALID_IQ_DATA'
      );
    }
    if (iqData.length === 0 || iqData.length % 2 !== 0) {
      throw new WSJTXError(
        'IQ data length must be even (interleaved I,Q samples)',
        'INVALID_IQ_DATA'
      );
    }
  }
}

// Re-export types for convenience
export {
  WSJTXMode,
  WSJTXError,
};

export type {
  DecodeResult,
  EncodeResult,
  WSPRResult,
  WSPRDecodeOptions,
  WSJTXMessage,
  AudioData,
  IQData,
  WSJTXConfig,
  VersionInfo,
  ModeCapabilities
};
