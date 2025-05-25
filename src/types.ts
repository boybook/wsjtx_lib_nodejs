/**
 * WSJTX Digital Radio Protocol Library for Node.js
 * 
 * This library provides encoding and decoding capabilities for various
 * digital amateur radio protocols including FT8, FT4, WSPR, and others.
 * 
 * The library is a Node.js C++ extension that wraps the wsjtx_lib C library,
 * providing high-performance digital signal processing capabilities with
 * multi-platform support (Windows, macOS, Linux).
 * 
 * @version 1.0.0
 * @author WSJTX Development Team
 * @license GPL-3.0
 */

/**
 * Supported WSJTX digital radio modes
 * 
 * Each mode has different characteristics in terms of symbol rate,
 * bandwidth, transmission duration, and sensitivity.
 */
export enum WSJTXMode {
    /** 
     * FT8 - 8-FSK modulation, 15-second transmissions
     * - Sample rate: 48 kHz
     * - Duration: 12.64 seconds
     * - Bandwidth: ~50 Hz
     * - Sensitivity: -24 dB
     */
    FT8 = 0,

    /** 
     * FT4 - 4-FSK modulation, 6-second transmissions
     * - Sample rate: 48 kHz  
     * - Duration: 6.0 seconds
     * - Bandwidth: ~80 Hz
     * - Sensitivity: -17 dB
     */
    FT4 = 1,

    /** 
     * JT4 - Weak signal mode for microwave EME
     * - Sample rate: 11.025 kHz
     * - Duration: 47.1 seconds
     * - Multiple bandwidth options
     */
    JT4 = 2,

    /** 
     * JT65 - Popular EME and HF weak signal mode
     * - Sample rate: 11.025 kHz
     * - Duration: 46.8 seconds
     * - Bandwidth: ~180 Hz
     */
    JT65 = 3,

    /** 
     * JT9 - Low data rate, narrow bandwidth mode
     * - Sample rate: 12 kHz
     * - Duration: 49.0 seconds
     * - Bandwidth: ~16 Hz
     */
    JT9 = 4,

    /** 
     * FST4 - Flexible format for 15-900 second transmissions
     * - Sample rate: 12 kHz
     * - Variable duration (15s, 30s, 60s, 120s, 300s, 900s)
     * - Ultra-weak signal capability
     */
    FST4 = 5,

    /** 
     * Q65 - Optimized for EME on VHF and higher
     * - Sample rate: 12 kHz
     * - Duration: 60 seconds
     * - Multiple bandwidth options
     */
    Q65 = 6,

    /** 
     * FST4W - Weak signal beacons
     * - Sample rate: 12 kHz
     * - Duration: 120 seconds
     * - Optimized for propagation studies
     */
    FST4W = 7,

    /** 
     * WSPR - Weak Signal Propagation Reporter
     * - Sample rate: 12 kHz
     * - Duration: 110.6 seconds
     * - 4-FSK modulation, very weak signal capability
     */
    WSPR = 8
}

/**
 * Audio data formats supported by the library
 * Can be either 32-bit floating point or 16-bit signed integer samples
 */
export type AudioData = Float32Array | Int16Array;

/**
 * IQ (In-phase/Quadrature) data for WSPR decoding
 * Interleaved I,Q samples as 32-bit floating point values
 */
export type IQData = Float32Array;

/**
 * Time information for decoded messages
 */
export interface WSJTXTime {
    /** Hour (0-23) */
    hour: number;
    /** Minute (0-59) */
    minute: number;
    /** Second (0-59) */
    second: number;
}

/**
 * A decoded WSJTX message containing timing and signal information
 */
export interface WSJTXMessage {
    /** The decoded message text */
    text: string;
    /** Signal-to-noise ratio in dB */
    snr: number;
    /** Time offset from start of transmission period in seconds */
    deltaTime: number;
    /** Frequency offset from dial frequency in Hz */
    deltaFrequency: number;
    /** Unix timestamp when the message was decoded */
    timestamp: number;
    /** Sync quality metric (mode-dependent) */
    sync: number;
}

/**
 * Result from a decode operation
 */
export interface DecodeResult {
    /** Whether the decode operation completed successfully */
    success: boolean;
    /** Optional error message if decode failed */
    error?: string;
}

/**
 * Result from an encode operation
 */
export interface EncodeResult {
    /** Generated audio waveform data */
    audioData: Float32Array;
    /** The actual message that was encoded (may differ from input) */
    messageSent: string;
}

/**
 * Single WSPR decode result
 */
export interface WSPRResult {
    /** Frequency of the decoded signal in Hz */
    frequency: number;
    /** Sync quality metric */
    sync: number;
    /** Signal-to-noise ratio in dB */
    snr: number;
    /** Time offset in seconds */
    deltaTime: number;
    /** Frequency drift in Hz/minute */
    drift: number;
    /** Jitter metric */
    jitter: number;
    /** Decoded message text */
    message: string;
    /** Decoded callsign */
    callsign: string;
    /** Decoded grid locator */
    locator: string;
    /** Decoded power in dBm */
    power: string;
    /** Number of decode cycles */
    cycles: number;
}

/**
 * Options for WSPR decoding
 */
export interface WSPRDecodeOptions {
    /** Dial frequency in Hz (default: 14095600 for 20m WSPR) */
    dialFrequency?: number;
    /** Receiving station callsign for better decoding */
    callsign?: string;
    /** Receiving station grid locator */
    locator?: string;
    /** Enable quick decode mode (faster but less sensitive) */
    quickMode?: boolean;
    /** Use hash table for callsign/locator lookup */
    useHashTable?: boolean;
    /** Number of decoding passes (1-3, default: 2) */
    passes?: number;
    /** Enable signal subtraction for better weak signal decoding */
    subtraction?: boolean;
}

/**
 * Error thrown by WSJTX library operations
 */
export class WSJTXError extends Error {
    constructor(message: string, public code?: string) {
        super(message);
        this.name = 'WSJTXError';
    }
}

/**
 * Configuration options for the WSJTX library
 */
export interface WSJTXConfig {
    /** Maximum number of threads to use for processing (1-16) */
    maxThreads?: number;
    /** Enable debug logging */
    debug?: boolean;
}

/**
 * Library version information
 */
export interface VersionInfo {
    /** WSJTXLib wrapper version */
    wrapperVersion: string;
    /** Underlying wsjtx_lib version */
    libraryVersion: string;
    /** Node.js version used to build */
    nodeVersion: string;
    /** Build timestamp */
    buildDate: string;
}

/**
 * Mode capabilities information
 */
export interface ModeCapabilities {
    /** Mode identifier */
    mode: WSJTXMode;
    /** Whether encoding is supported */
    encodingSupported: boolean;
    /** Whether decoding is supported */
    decodingSupported: boolean;
    /** Required sample rate in Hz */
    sampleRate: number;
    /** Transmission duration in seconds */
    duration: number;
    /** Typical bandwidth in Hz */
    bandwidth?: number;
    /** Typical sensitivity in dB */
    sensitivity?: number;
}

/**
 * Callback function for decode operations
 * The native module returns a boolean indicating completion status
 */
export type DecodeCallback = (error: Error | null, result: boolean) => void;

/**
 * Callback function type for asynchronous encode operations
 */
export type EncodeCallback = (error: Error | null, result: EncodeResult) => void;

/**
 * Callback function type for asynchronous WSPR decode operations
 */
export type WSPRDecodeCallback = (error: Error | null, results: WSPRResult[]) => void;
  