# WSJTX Library for Node.js

[![npm version](https://badge.fury.io/js/wsjtx-lib.svg)](https://badge.fury.io/js/wsjtx-lib)

A high-performance Node.js C++ extension for digital amateur radio protocols, providing TypeScript support and async/await interfaces for WSJTX library functionality.

## Features

- 🚀 **High Performance**: Native C++ implementation with multi-threading support
- 📡 **Multiple Modes**: Support for FT8, FT4, JT4, JT65, JT9, FST4, Q65, FST4W, and WSPR
- 🔧 **TypeScript Support**: Full TypeScript definitions and modern ES modules
- ⚡ **Async/Await**: Promise-based API for non-blocking operations
- 🎵 **Audio Processing**: Support for both Float32Array and Int16Array audio formats
- 🌍 **Cross-Platform**: Prebuilt binaries for Windows, macOS, and Linux
- 📊 **WSPR Decoding**: Specialized support for WSPR IQ data processing

## Supported Modes

| Mode | Encoding | Decoding | Sample Rate | Duration | Bandwidth |
|------|----------|----------|-------------|----------|-----------|
| FT8  | ✅       | ✅       | 48 kHz      | 12.6s    | ~50 Hz    |
| FT4  | ✅       | ✅       | 48 kHz      | 6.0s     | ~80 Hz    |
| JT4  | ❌       | ✅       | 11.025 kHz  | 47.1s    | Variable  |
| JT65 | ❌       | ✅       | 11.025 kHz  | 46.8s    | ~180 Hz   |
| JT9  | ❌       | ✅       | 12 kHz      | 49.0s    | ~16 Hz    |
| FST4 | ❌       | ✅       | 12 kHz      | 60.0s    | Variable  |
| Q65  | ✅       | ✅       | 12 kHz      | 30/60/120/300s | Variable  |
| FST4W| ❌       | ✅       | 12 kHz      | 120.0s   | Variable  |
| WSPR | ❌       | ✅       | 12 kHz      | 110.6s   | ~6 Hz     |

## Installation

### NPM Installation (Recommended)

The package includes prebuilt binaries for major platforms:

```bash
npm install wsjtx-lib
```

**Supported platforms with prebuilt binaries:**
- Linux x64 / arm64
- macOS x64 (Intel) / ARM64 (Apple Silicon)
- Windows x64

Runtime binary loading uses `node-gyp-build` with prebuildify layout
(`prebuilds/<platform>-<arch>/*.node`), and falls back to
`build/Release/*.node` for local development builds.

Linux prebuilds are built and checked so neither `wsjtx_lib_nodejs.node` nor
`libwsjtx_core.so` requires an executable stack. Downstreams should not patch
`PT_GNU_STACK` or require `GLIBC_TUNABLES=glibc.rtld.execstack=2`; if that
condition appears, treat it as a source/build regression.

### Building from Source

Only needed if prebuilt binaries are not available for your platform.

#### Prerequisites

- Node.js 16+ 
- CMake 3.15+
- C++ compiler with C++17 support
- FFTW3 library (single precision)
- Boost libraries
- Fortran compiler (gfortran)

#### macOS

```bash
# Install dependencies using Homebrew
brew install cmake fftw boost gcc pkg-config

# Clone and build
git clone --recursive https://github.com/boybook/wsjtx_lib_nodejs.git
cd wsjtx_lib_nodejs
npm install
npm run build
```

#### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y \
  cmake \
  build-essential \
  gfortran \
  libfftw3-dev \
  libboost-all-dev \
  pkg-config

# Clone and build
git clone --recursive https://github.com/boybook/wsjtx_lib_nodejs.git
cd wsjtx_lib_nodejs
npm install
npm run build
```

#### Windows

Use MSYS2/MinGW-w64 for best compatibility:

```bash
# Install MSYS2, then in MSYS2 MINGW64 terminal:
pacman -S --needed \
  base-devel \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-pkg-config \
  mingw-w64-x86_64-fftw \
  mingw-w64-x86_64-boost \
  mingw-w64-x86_64-gcc-fortran \
  mingw-w64-x86_64-nodejs

# Clone and build
git clone --recursive https://github.com/boybook/wsjtx_lib_nodejs.git
cd wsjtx_lib_nodejs
npm install
npm run build
```

## Quick Start

```typescript
import { WSJTXLib, WSJTXMode } from 'wsjtx-lib';

async function example() {
    // Create library instance
    const lib = new WSJTXLib();
    
    // Encode an FT8 message
    const encodeResult = await lib.encode(
        WSJTXMode.FT8,
        'CQ DX BH1ABC OM88',
        1000  // Audio frequency in Hz (typically 500-3000 Hz)
    );
    
    console.log(`Generated ${encodeResult.audioData.length} audio samples`);
    console.log(`Message sent: "${encodeResult.messageSent}"`);
    
    // Decode audio data
    const audioData = new Float32Array(48000 * 13); // 13 seconds at 48kHz
    // ... fill audioData with actual audio samples ...
    
    const decodeResult = await lib.decode(WSJTXMode.FT8, audioData, {
        frequency: 1000,
        threads: 4
    });
    
    decodeResult.messages.forEach(msg => {
        console.log(`Decoded: "${msg.text}" (SNR: ${msg.snr} dB, ΔT: ${msg.deltaTime}s)`);
    });
}
```

## API Reference

### WSJTXLib Class

#### Constructor

```typescript
new WSJTXLib(config?: WSJTXConfig)
```

Creates a new WSJTX library instance.

**Parameters:**
- `config` (optional): Configuration options
  - `maxThreads`: Maximum number of threads (1-16, default: 4)
  - `encodeSampleRate`: Process-global FT8/FT4/Q65 encode output sample rate (`12000` or `48000`, default: `12000`)
  - `debug`: Enable debug logging (default: false)

#### Methods

##### `decode(mode, audioData, options): Promise<DecodeResult>`

Decode digital radio signals from audio data.

**Parameters:**
- `mode`: WSJTXMode enum value
- `audioData`: Float32Array or Int16Array of audio samples
- `options`: DecodeOptions object
  - `frequency`: Audio frequency in Hz (typically 500-3000 Hz)
  - `txFrequency`: Transmit audio frequency in Hz (optional, defaults to `frequency`)
  - `threads`: Number of threads to use (optional, default: 4)
  - `lowFreq`: Lower decode frequency limit in Hz (optional, default: 200)
  - `highFreq`: Upper decode frequency limit in Hz (optional, default: 4000)
  - `tolerance`: Frequency tolerance in Hz (optional, default: 20)
  - `myCall`, `myGrid`, `dxCall`, `dxGrid`: Optional AP decode context
  - `apDecode`: Enable AP decode passes (optional, default: true)
  - `decodeDepth`: WSJT-X decoder depth (optional, default: 1)
  - `qsoProgress`: WSJT-X QSO progress stage (optional, default: 0)
  - `q65Period`: Q65 period in seconds: `30`, `60`, `120`, or `300` (optional, default: `60`)
  - `q65Submode`: Q65 submode: `'A'`, `'B'`, `'C'`, `'D'`, `'E'`, or `0`-`4` (optional, default: `'A'`)
  - `q65MaxDrift`: Q65 max drift control (optional, default: `50`)
  - `q65ClearAveraging`: Clear Q65 averaging state before decode (optional, default: false)
  - `q65SingleDecode`: Request Q65 single-candidate decode behavior (optional, default: false)
  - `q65Averaging`: Enable Q65 averaged decode passes (optional, default: false)

**Returns:** Promise resolving to DecodeResult with success status and decoded messages

**Note:** Use `lib.getSampleRate(mode)` to determine the expected sample rate for a mode. Q65 uses 12 kHz audio by default.

##### `encode(mode, message, frequency, threadsOrOptions?): Promise<EncodeResult>`

Encode a message into audio waveform for transmission.

**Parameters:**
- `mode`: WSJTXMode enum value
- `message`: Message text to encode (FT8/FT4/Q65 structured messages: 1-37 characters; free text payloads are limited by WSJT-X to 13 characters)
- `frequency`: Audio frequency in Hz (typically 500-3000 Hz)
- `threadsOrOptions`: Either a thread count number or an EncodeOptions object (optional, default: 4)
  - `threads`: Number of threads to use
  - `q65Period`: Q65 period in seconds: `30`, `60`, `120`, or `300` (optional, default: `60`)
  - `q65Submode`: Q65 submode: `'A'`, `'B'`, `'C'`, `'D'`, `'E'`, or `0`-`4` (optional, default: `'A'`)

**Returns:** Promise resolving to EncodeResult with audio data and actual message sent

##### `decodeWSPR(iqData, options?): Promise<WSPRResult[]>`

Decode WSPR signals from IQ data.

**Parameters:**
- `iqData`: Float32Array of interleaved I,Q samples
- `options`: WSPRDecodeOptions (optional)
  - `dialFrequency`: RF dial frequency in Hz (default: 14095600)
  - `callsign`: Station callsign
  - `locator`: Grid locator
  - `quickMode`: Enable quick decode mode (default: false)
  - `useHashTable`: Use hash table optimization (default: true)
  - `passes`: Number of decode passes (default: 2)
  - `subtraction`: Enable signal subtraction (default: true)

**Returns:** Promise resolving to array of WSPR decode results

##### `pullMessages(): WSJTXMessage[]`

Retrieve decoded messages from the internal queue.

**Returns:** Array of decoded messages

##### Utility Methods

- `isEncodingSupported(mode): boolean` - Check if encoding is supported for a mode
- `isDecodingSupported(mode): boolean` - Check if decoding is supported for a mode
- `getSampleRate(mode): number` - Get required sample rate for a mode
- `getTransmissionDuration(mode): number` - Get transmission duration for a mode
- `getAllModeCapabilities(): ModeCapabilities[]` - Get capabilities for all modes

##### Static Methods

- `convertAudioFormat(audioData, targetFormat): AudioData` - Convert between Float32Array and Int16Array

### Enums and Types

#### WSJTXMode

```typescript
enum WSJTXMode {
    FT8 = 0,
    FT4 = 1,
    JT4 = 2,
    JT65 = 3,
    JT9 = 4,
    FST4 = 5,
    Q65 = 6,
    FST4W = 7,
    JT65JT9 = 8,
    WSPR = 9
}
```

#### WSJTXMessage

```typescript
interface WSJTXMessage {
    text: string;           // Decoded message text
    snr: number;            // Signal-to-noise ratio in dB
    deltaTime: number;      // Time offset in seconds
    deltaFrequency: number; // Frequency offset in Hz
    timestamp: number;      // seconds-of-day reported by the decoder
    sync: number;           // Sync quality
}
```

#### EncodeResult

```typescript
interface EncodeResult {
    audioData: Float32Array;  // Generated audio waveform
    messageSent: string;      // Actual message encoded
    sampleRate: number;       // Output sample rate
}
```

#### Q65 Options

```typescript
type Q65Period = 30 | 60 | 120 | 300;
type Q65Submode = 'A' | 'B' | 'C' | 'D' | 'E' | 0 | 1 | 2 | 3 | 4;
```

Q65 options are accepted by both `encode()` and `decode()` so the TX/RX chain can be configured symmetrically.

#### WSPRResult

```typescript
interface WSPRResult {
    frequency: number;    // Signal frequency in Hz
    sync: number;         // Sync quality
    snr: number;          // Signal-to-noise ratio in dB
    deltaTime: number;    // Time offset in seconds
    drift: number;        // Frequency drift in Hz/minute
    jitter: number;       // Jitter metric
    message: string;      // Decoded message
    callsign: string;     // Decoded callsign
    locator: string;      // Decoded grid locator
    power: string;        // Decoded power in dBm
    cycles: number;       // Number of decode cycles
}
```

## Examples

### Complete FT8 Encode-Decode Cycle

```typescript
import { WSJTXLib, WSJTXMode } from 'wsjtx-lib';
import * as fs from 'fs';
import * as wav from 'wav';

async function ft8Example() {
    const lib = new WSJTXLib();
    const message = 'CQ DX BH1ABC OM88';
    const audioFrequency = 1000;
    
    // 1. Encode message
    const encodeResult = await lib.encode(WSJTXMode.FT8, message, audioFrequency);
    console.log(`Encoded: "${encodeResult.messageSent}"`);
    
    // 2. Save as WAV file
    const audioInt16 = new Int16Array(encodeResult.audioData.length);
    for (let i = 0; i < encodeResult.audioData.length; i++) {
        audioInt16[i] = Math.round(encodeResult.audioData[i] * 32767);
    }
    
    const writer = new wav.FileWriter('ft8_test.wav', {
        channels: 1,
        sampleRate: lib.getSampleRate(WSJTXMode.FT8),
        bitDepth: 16
    });
    
    const buffer = Buffer.from(audioInt16.buffer);
    writer.write(buffer);
    writer.end();
    
    // 3. Read back and decode
    const decodeResult = await lib.decode(WSJTXMode.FT8, encodeResult.audioData, {
        frequency: audioFrequency,
        threads: 1
    });
    
    console.log(`Decoded ${decodeResult.messages.length} messages`);
}
```

### Q65 Encode-Decode Cycle

```typescript
import { WSJTXLib, WSJTXMode } from 'wsjtx-lib';

async function q65Example() {
    const lib = new WSJTXLib({ maxThreads: 4 });
    const message = 'CQ K1ABC FN20';
    const audioFrequency = 1500;

    // Encode a Q65-30A frame at 12 kHz.
    const encoded = await lib.encode(WSJTXMode.Q65, message, audioFrequency, {
        threads: 1,
        q65Period: 30,
        q65Submode: 'A'
    });

    console.log(`Encoded: "${encoded.messageSent.trim()}"`);
    console.log(`Samples: ${encoded.audioData.length}`);
    console.log(`Sample rate: ${encoded.sampleRate} Hz`);

    // Decode with matching Q65 period/submode and a wide enough search window.
    const decoded = await lib.decode(WSJTXMode.Q65, encoded.audioData, {
        frequency: audioFrequency,
        txFrequency: audioFrequency,
        threads: 1,
        lowFreq: 0,
        highFreq: 5000,
        tolerance: 5000,
        q65Period: 30,
        q65Submode: 'A',
        q65MaxDrift: 50,
        q65ClearAveraging: true
    });

    decoded.messages.forEach((msg) => {
        console.log(`Decoded: "${msg.text.trim()}" SNR=${msg.snr} dB DT=${msg.deltaTime}s Freq=${msg.deltaFrequency} Hz`);
    });
}
```

For other Q65 variants, use the same API and change the period/submode pair:

```typescript
await lib.encode(WSJTXMode.Q65, 'CQ K1ABC FN20', 1500, {
    q65Period: 120,
    q65Submode: 'E'
});

await lib.decode(WSJTXMode.Q65, audioData, {
    frequency: 1500,
    q65Period: 120,
    q65Submode: 'E',
    q65MaxDrift: 100,
    q65Averaging: true
});
```

### WSPR Decoding

```typescript
import { WSJTXLib } from 'wsjtx-lib';

async function wsprExample() {
    const lib = new WSJTXLib();
    
    // IQ data (interleaved I,Q samples)
    const iqData = new Float32Array(2 * 12000 * 120); // 2 minutes of IQ data
    // ... fill with actual IQ data from SDR ...
    
    const options = {
        dialFrequency: 14095600,  // 20m WSPR frequency
        callsign: 'BH1ABC',
        locator: 'OM88',
        quickMode: false,
        passes: 2
    };
    
    const results = await lib.decodeWSPR(iqData, options);
    
    console.log('WSPR Decode Results:');
    results.forEach(result => {
        console.log(`${result.callsign} ${result.locator} ${result.power}dBm (SNR: ${result.snr}dB)`);
    });
}
```

### Audio Format Conversion

```typescript
import { WSJTXLib } from 'wsjtx-lib';

// Convert Float32Array to Int16Array
const floatData = new Float32Array([0.5, -0.5, 0.25, -0.25]);
const intData = WSJTXLib.convertAudioFormat(floatData, 'int16');
console.log(intData); // Int16Array [16384, -16384, 8192, -8192]

// Convert back to Float32Array
const backToFloat = WSJTXLib.convertAudioFormat(intData, 'float32');
console.log(backToFloat); // Float32Array [0.5, -0.5, 0.25, -0.25] (approximately)
```

### Multiple Message Types

```typescript
import { WSJTXLib, WSJTXMode } from 'wsjtx-lib';

async function multipleMessages() {
    const lib = new WSJTXLib();
    const audioFrequency = 1000;
    
    const messages = [
        'CQ DX BH1ABC OM88',      // CQ call
        'BH1ABC BH2DEF +05',      // Signal report
        'BH2DEF BH1ABC R-12',     // Report acknowledgment
        'BH1ABC BH2DEF RRR',      // Received acknowledgment
        'BH2DEF BH1ABC 73'        // End contact
    ];
    
    for (const message of messages) {
        const result = await lib.encode(WSJTXMode.FT8, message, audioFrequency);
        console.log(`"${message}" -> "${result.messageSent}" (${result.audioData.length} samples)`);
    }
}
```

## Error Handling

The library throws `WSJTXError` for all operation failures:

```typescript
import { WSJTXError } from 'wsjtx-lib';

try {
    await lib.decode(WSJTXMode.FT8, audioData, {
        frequency: 1000
    });
} catch (error) {
    if (error instanceof WSJTXError) {
        console.error(`WSJTX Error [${error.code}]: ${error.message}`);
        
        // Common error codes:
        // - INVALID_MODE: Invalid mode parameter
        // - INVALID_FREQUENCY: Invalid frequency parameter
        // - INVALID_AUDIO_DATA: Invalid audio data format/size
        // - INVALID_MESSAGE: Invalid message text
        // - DECODE_ERROR: Decoding operation failed
        // - ENCODE_ERROR: Encoding operation failed
    } else {
        console.error('Unexpected error:', error);
    }
}
```

## Important Notes

1. **Audio Frequency**: The `frequency` parameter is the audio tone frequency within your audio passband (typically 500-3000 Hz), not the RF frequency.

2. **Sample Rates**: Different modes require different sample rates. Use `lib.getSampleRate(mode)` to get the correct rate.

3. **Q65 Parameters**: Q65 TX and RX must use the same period/submode pair. Supported periods are `30`, `60`, `120`, and `300`; supported submodes are `A` through `E`.

4. **Audio Resampling**: Input audio should match the sample rate expected by the selected mode. Q65 expects 12 kHz audio.

5. **Thread Safety**: Each WSJTXLib instance should be used from a single thread. Create separate instances for concurrent operations.

6. **Message Queue**: `decode()` returns decoded messages directly. `pullMessages()` is also available for compatibility with the internal message queue.

## Building from Source (Advanced)

For detailed build instructions when prebuilt binaries are not available, see [BUILD.md](BUILD.md).

```bash
# Clone with submodules
git clone --recursive https://github.com/boybook/wsjtx_lib_nodejs.git
cd wsjtx_lib_nodejs

# Install dependencies
npm install

# Build native module and TypeScript
npm run build

# Run tests
npm test

# Run comprehensive tests
npm run test:full

# Run examples
node examples/examples.js
```

## Development

### Project Structure

```
wsjtx_lib_nodejs/
├── src/                 # TypeScript source files
├── native/              # C++ wrapper code
├── wsjtx_lib/          # Git submodule (wsjtx_lib library)
├── test/               # Test files
├── examples/           # Usage examples
├── dist/               # Compiled TypeScript output
├── prebuilds/          # Prebuilt binaries for distribution
└── build/              # CMake build directory
```

### Scripts

- `npm run build` - Build both native module and TypeScript
- `npm run build:native` - Build only the native C++ module
- `npm run build:ts` - Build only TypeScript
- `npm test` - Run basic tests (CI-friendly)
- `npm run test:full` - Run comprehensive tests
- `npm run clean` - Clean build artifacts
- `npm run package` - Package prebuilt binaries for distribution

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit a pull request

## License

This project is licensed under the GPL-3.0 License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Based on the excellent [wsjtx_lib](https://github.com/paulh002/wsjtx_lib) library by PA0PHH
- WSJT-X development team for the original algorithms by K1JT
- Amateur radio community for protocol specifications
