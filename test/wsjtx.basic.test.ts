/**
 * WSJTX Library Basic Test Suite
 *
 * Tests core functionalities suitable for CI.
 */

import { describe, it, beforeEach, afterEach } from 'node:test';
import assert from 'node:assert';

// Import WSJTX library and types
import {
  WSJTXLib,
  WSJTXMode,
  WSJTXError,
  WSJTXMessage,
  ModeCapabilities
} from '../src/index.js';

describe('WSJTX Library Basic Tests', () => {
  let lib: WSJTXLib;

  beforeEach(() => {
    lib = new WSJTXLib({
      maxThreads: 4,
      debug: true
    });
  });

  afterEach(() => {
    // Clean up resources if any were created by basic tests
  });

  describe('Basic Functionality Tests', () => {
    it('should create library instance', () => {
      assert.ok(lib instanceof WSJTXLib);
    });

    it('should support custom configuration', () => {
      const customLib = new WSJTXLib({
        maxThreads: 8,
        debug: false
      });
      assert.ok(customLib instanceof WSJTXLib);
    });

    it('should return correct FT8 sample rate', () => {
      const sampleRate = lib.getSampleRate(WSJTXMode.FT8);
      assert.strictEqual(sampleRate, 48000);
    });

    it('should return correct FT8 transmission duration', () => {
      const duration = lib.getTransmissionDuration(WSJTXMode.FT8);
      assert.ok(Math.abs(duration - 12.64) < 0.1);
    });

    it('should correctly check encoding support', () => {
      assert.strictEqual(lib.isEncodingSupported(WSJTXMode.FT8), true);
      assert.strictEqual(lib.isDecodingSupported(WSJTXMode.FT8), true);
    });

    it('should return all mode capabilities', () => {
      const capabilities: ModeCapabilities[] = lib.getAllModeCapabilities();
      assert.ok(capabilities.length > 0);
      assert.ok('mode' in capabilities[0]);
      assert.ok('encodingSupported' in capabilities[0]);
      assert.ok('decodingSupported' in capabilities[0]);
      assert.ok('sampleRate' in capabilities[0]);
      assert.ok('duration' in capabilities[0]);
    });
  });

  describe('Parameter Validation Tests', () => {
    it('should validate mode parameter', async () => {
      const audioData = new Float32Array(1000);
      await assert.rejects(
        lib.decode(999 as WSJTXMode, audioData, 1000),
        WSJTXError
      );
    });

    it('should validate frequency parameter', async () => {
      const audioData = new Float32Array(1000);
      await assert.rejects(
        lib.decode(WSJTXMode.FT8, audioData, -1000),
        WSJTXError
      );
    });

    it('should validate audio data parameter', async () => {
      await assert.rejects(
        lib.decode(WSJTXMode.FT8, new Float32Array(0), 1000),
        WSJTXError
      );
    });

    it('should validate message parameter', async () => {
      await assert.rejects(
        lib.encode(WSJTXMode.FT8, '', 1000),
        WSJTXError
      );

      await assert.rejects(
        lib.encode(WSJTXMode.FT8, 'x'.repeat(30), 1000),
        WSJTXError
      );
    });
  });

  describe('Message Queue Tests', () => {
    it('should pull messages from queue', () => {
      const messages: WSJTXMessage[] = lib.pullMessages();
      assert.ok(Array.isArray(messages));
    });

    it('should clear message queue', () => {
      // Pull messages twice to ensure queue is cleared
      lib.pullMessages();
      const messages = lib.pullMessages();
      assert.strictEqual(messages.length, 0);
    });
  });

  describe('Audio Format Conversion Tests', () => {
    it('should convert Float32Array to Int16Array', () => {
      const floatData = new Float32Array([0.0, 0.5, -0.5, 1.0, -1.0]);
      const intData = WSJTXLib.convertAudioFormat(floatData, 'int16') as Int16Array;

      assert.ok(intData instanceof Int16Array);
      assert.strictEqual(intData.length, floatData.length);
      assert.strictEqual(intData[0], 0);
      assert.ok(Math.abs(intData[1] - 16384) < 10);
      assert.ok(Math.abs(intData[2] + 16384) < 10);
      assert.ok(Math.abs(intData[3] - 32767) < 10);
      assert.ok(Math.abs(intData[4] + 32767) < 10);
    });

    it('should convert Int16Array to Float32Array', () => {
      const intData = new Int16Array([0, 16384, -16384, 32767, -32767]);
      const floatData = WSJTXLib.convertAudioFormat(intData, 'float32') as Float32Array;

      assert.ok(floatData instanceof Float32Array);
      assert.strictEqual(floatData.length, intData.length);
      assert.ok(Math.abs(floatData[0] - 0.0) < 0.001);
      assert.ok(Math.abs(floatData[1] - 0.5) < 0.001);
      assert.ok(Math.abs(floatData[2] + 0.5) < 0.001);
      assert.ok(Math.abs(floatData[3] - 1.0) < 0.001);
      assert.ok(Math.abs(floatData[4] + 1.0) < 0.001);
    });

    it('should handle edge cases in conversion', () => {
      // Test empty arrays
      const emptyFloat = new Float32Array(0);
      const emptyInt = WSJTXLib.convertAudioFormat(emptyFloat, 'int16') as Int16Array;
      assert.strictEqual(emptyInt.length, 0);

      const emptyInt16 = new Int16Array(0);
      const emptyFloat32 = WSJTXLib.convertAudioFormat(emptyInt16, 'float32') as Float32Array;
      assert.strictEqual(emptyFloat32.length, 0);
    });

    it('should maintain precision in round-trip conversion', () => {
      const originalData = new Float32Array(1000);
      for (let i = 0; i < originalData.length; i++) {
        originalData[i] = (Math.random() - 0.5) * 2; // Range -1 to 1
      }

      // Convert to Int16Array and back
      const intData = WSJTXLib.convertAudioFormat(originalData, 'int16') as Int16Array;
      const convertedData = WSJTXLib.convertAudioFormat(intData, 'float32') as Float32Array;

      // Check precision
      let maxError = 0;
      for (let i = 0; i < originalData.length; i++) {
        const error = Math.abs(originalData[i] - convertedData[i]);
        maxError = Math.max(maxError, error);
      }

      // Should be very close (16-bit precision)
      assert.ok(maxError < 0.001);
    });

    it('should handle invalid format parameter', () => {
      const floatData = new Float32Array([0.5]);
      assert.throws(() => {
        WSJTXLib.convertAudioFormat(floatData, 'invalid' as any);
      });
    });
  });

  describe('TypeScript Type Safety Tests', () => {
    it('should provide complete type support for basic types', () => {
      const capabilities: ModeCapabilities[] = lib.getAllModeCapabilities();
      assert.ok(capabilities.length > 0);

      capabilities.forEach((cap: ModeCapabilities) => {
        const modeName: string = WSJTXMode[cap.mode];
        assert.strictEqual(typeof modeName, 'string');
        assert.strictEqual(typeof cap.sampleRate, 'number');
        assert.strictEqual(typeof cap.duration, 'number');
        assert.strictEqual(typeof cap.encodingSupported, 'boolean');
        assert.strictEqual(typeof cap.decodingSupported, 'boolean');
      });
    });

    it('should provide type-safe message objects', () => {
      const messages: WSJTXMessage[] = lib.pullMessages();

      messages.forEach((msg: WSJTXMessage) => {
        assert.strictEqual(typeof msg.text, 'string');
        assert.strictEqual(typeof msg.snr, 'number');
        assert.strictEqual(typeof msg.deltaTime, 'number');
        assert.strictEqual(typeof msg.deltaFrequency, 'number');
      });
    });

    it('should enforce enum constraints', () => {
      const validMode: WSJTXMode = WSJTXMode.FT8;
      assert.strictEqual(typeof validMode, 'number');
      assert.ok(validMode >= 0);
    });
  });

  describe('Error Handling Tests', () => {
    it('should throw WSJTXError for invalid operations (non-encode/decode)', async () => {
      // Example: Invalid mode in getSampleRate (if such validation existed or was more strict)
      // For now, using existing validation tests that fit "basic" criteria.
      try {
        await lib.decode(999 as WSJTXMode, new Float32Array(1000), 1000);
        assert.fail('Should have thrown WSJTXError for invalid mode');
      } catch (error) {
        assert.ok(error instanceof WSJTXError);
        assert.strictEqual(typeof error.message, 'string');
        if (error instanceof WSJTXError) {
          assert.strictEqual(typeof error.code, 'string');
        }
      }
    });

    it('should provide meaningful error messages for basic errors', async () => {
      try {
        await lib.encode(WSJTXMode.FT8, '', 1000); // Empty message
        assert.fail('Should have thrown WSJTXError for empty message');
      } catch (error) {
        assert.ok(error instanceof WSJTXError);
        assert.ok(error.message.length > 0);
        if (error instanceof WSJTXError && error.code) {
          assert.ok(error.code.length > 0);
        }
      }
    });

    it('should validate all error codes are strings for basic errors', async () => {
      const testCases = [
        () => lib.decode(999 as WSJTXMode, new Float32Array(1000), 1000), // Invalid mode
        () => lib.decode(WSJTXMode.FT8, new Float32Array(1000), -1000), // Invalid frequency
        () => lib.decode(WSJTXMode.FT8, new Float32Array(0), 1000), // Invalid audio data
        () => lib.encode(WSJTXMode.FT8, '', 1000), // Empty message
        () => lib.encode(WSJTXMode.FT8, 'x'.repeat(50), 1000) // Message too long
      ];

      for (const testCase of testCases) {
        try {
          await testCase();
          assert.fail('Should have thrown WSJTXError');
        } catch (error) {
          assert.ok(error instanceof WSJTXError);
          if (error instanceof WSJTXError && error.code) {
            assert.strictEqual(typeof error.code, 'string');
            assert.ok(error.code.length > 0);
          }
        }
      }
    });
  });
}); 