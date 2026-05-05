/**
 * wsjtx_c_api.h - Pure C interface for wsjtx_lib
 *
 * This header provides a stable C ABI boundary between the wsjtx_core
 * shared library (compiled with MinGW/GCC on Windows, or system compiler
 * on Linux/macOS) and the Node.js N-API binding (compiled with MSVC on
 * Windows, or system compiler on Linux/macOS).
 *
 * All types are C-compatible. No C++ headers or types are exposed.
 */

#ifndef WSJTX_C_API_H
#define WSJTX_C_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
  #ifdef WSJTX_CORE_EXPORTS
    #define WSJTX_API __declspec(dllexport)
  #else
    #define WSJTX_API __declspec(dllimport)
  #endif
#else
  #define WSJTX_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle to the library instance */
typedef void* wsjtx_handle_t;

/* Error codes */
#define WSJTX_OK                  0
#define WSJTX_ERR_INVALID_HANDLE -1
#define WSJTX_ERR_INVALID_MODE   -2
#define WSJTX_ERR_ENCODE_FAILED  -3
#define WSJTX_ERR_BUFFER_TOO_SMALL -4
#define WSJTX_ERR_EXCEPTION      -99

/* Mode enumeration (must match wsjtxMode in wsjtx_lib.h) */
typedef enum {
    WSJTX_MODE_FT8     = 0,
    WSJTX_MODE_FT4     = 1,
    WSJTX_MODE_JT4     = 2,
    WSJTX_MODE_JT65    = 3,
    WSJTX_MODE_JT9     = 4,
    WSJTX_MODE_FST4    = 5,
    WSJTX_MODE_Q65     = 6,
    WSJTX_MODE_FST4W   = 7,
    WSJTX_MODE_JT65JT9 = 8,
    WSJTX_MODE_WSPR    = 9
} wsjtx_mode_t;

/* Decoded message (C-compatible version of WsjtxMessage) */
typedef struct {
    int hh;
    int min;
    int sec;
    int snr;
    int freq;
    float sync;
    float dt;
    char msg[64];
} wsjtx_message_t;

/* WSPR decoder options (C-compatible version of decoder_options) */
typedef struct {
    int freq;
    char rcall[13];
    char rloc[7];
    int quickmode;
    int usehashtable;
    int npasses;
    int subtraction;
} wsjtx_decoder_options_t;

/* WSPR decoder result (C-compatible version of decoder_results) */
typedef struct {
    double freq;
    float sync;
    float snr;
    float dt;
    float drift;
    int jitter;
    char message[23];
    char call[13];
    char loc[7];
    char pwr[3];
    int cycles;
} wsjtx_decoder_result_t;

/* Decode options for v2 API.
 * - frequency: nominal QSO frequency in Hz (passed as nfqso to the decoder)
 * - threads:   thread hint forwarded to the decoder (1..N)
 * - low_freq:  decoder scan low limit in Hz  (default 200)
 * - high_freq: decoder scan high limit in Hz (default 4000)
 * - tolerance: frequency tolerance in Hz     (default 20)
 * - hiscall:   DX callsign for AP decode (empty = none)
 * - hisgrid:   DX 4-char grid for AP decode (empty = none)
 */
typedef struct {
    int frequency;
    int threads;
    int low_freq;
    int high_freq;
    int tolerance;
    char hiscall[13];
    char hisgrid[7];
} wsjtx_decode_options_t;

/* ---- Lifecycle ---- */

WSJTX_API wsjtx_handle_t wsjtx_create(void);
WSJTX_API void wsjtx_destroy(wsjtx_handle_t handle);

/* ---- Decode ---- */

/**
 * Decode audio samples (float format) — legacy API.
 * Results are placed in the internal message queue; use wsjtx_pull_message() to retrieve.
 * Returns WSJTX_OK on success, negative error code on failure.
 */
WSJTX_API int wsjtx_decode_float(wsjtx_handle_t handle, int mode,
    float* samples, int num_samples, int freq, int threads);

/**
 * Decode audio samples (int16 format) — legacy API.
 * Results are placed in the internal message queue; use wsjtx_pull_message() to retrieve.
 * Returns WSJTX_OK on success, negative error code on failure.
 */
WSJTX_API int wsjtx_decode_int16(wsjtx_handle_t handle, int mode,
    int16_t* samples, int num_samples, int freq, int threads);

/**
 * Decode audio samples (float format) with full options — v2 API.
 * Applies dxCall/dxGrid (for A8 list decode) and the decode frequency range
 * before invoking the decoder. Results are placed in the internal queue;
 * use wsjtx_pull_messages() to retrieve them in batch.
 */
WSJTX_API int wsjtx_decode_float_v2(wsjtx_handle_t handle, int mode,
    const float* samples, int num_samples,
    const wsjtx_decode_options_t* options);

/**
 * Decode audio samples (int16 format) with full options — v2 API.
 */
WSJTX_API int wsjtx_decode_int16_v2(wsjtx_handle_t handle, int mode,
    const int16_t* samples, int num_samples,
    const wsjtx_decode_options_t* options);

/* ---- Encode ---- */

/**
 * Encode a message into audio samples.
 *
 * @param out_samples      Caller-allocated buffer for output audio samples
 * @param out_num_samples  On return, the number of samples written
 * @param out_buf_size     Size of out_samples buffer (in floats)
 * @param out_message_sent Caller-allocated buffer for the actual message sent
 * @param out_msg_buf_size Size of out_message_sent buffer (in bytes)
 *
 * Returns WSJTX_OK on success, WSJTX_ERR_BUFFER_TOO_SMALL if buffer is insufficient.
 */
WSJTX_API int wsjtx_encode(wsjtx_handle_t handle, int mode, int freq,
    const char* message,
    float* out_samples, int* out_num_samples, int out_buf_size,
    char* out_message_sent, int out_msg_buf_size);

/* ---- Message queue ---- */

/**
 * Pull one decoded message from the queue.
 * Returns 1 if a message was retrieved, 0 if the queue is empty.
 */
WSJTX_API int wsjtx_pull_message(wsjtx_handle_t handle, wsjtx_message_t* out_msg);

/**
 * Pull up to `max_messages` decoded messages from the queue in one call.
 * Returns the number of messages written into `out_messages` (>= 0).
 */
WSJTX_API int wsjtx_pull_messages(wsjtx_handle_t handle,
    wsjtx_message_t* out_messages, int max_messages);

/* ---- WSPR ---- */

/**
 * Decode WSPR from IQ data.
 *
 * @param iq_interleaved  Interleaved float array [re0, im0, re1, im1, ...]
 * @param num_iq_samples  Number of IQ sample pairs (array length / 2)
 * @param options         Decoder options
 * @param out_results     Caller-allocated array for results
 * @param max_results     Maximum number of results to write
 *
 * Returns the number of results decoded (>= 0), or negative error code.
 */
WSJTX_API int wsjtx_wspr_decode(wsjtx_handle_t handle,
    float* iq_interleaved, int num_iq_samples,
    wsjtx_decoder_options_t* options,
    wsjtx_decoder_result_t* out_results, int max_results);

/* ---- Stateless queries ---- */

WSJTX_API int wsjtx_is_encoding_supported(int mode);
WSJTX_API int wsjtx_is_decoding_supported(int mode);
WSJTX_API int wsjtx_get_sample_rate(int mode);
WSJTX_API double wsjtx_get_transmission_duration(int mode);

#ifdef __cplusplus
}
#endif

#endif /* WSJTX_C_API_H */
