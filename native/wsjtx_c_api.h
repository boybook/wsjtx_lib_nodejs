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
#define WSJTX_ERR_INVALID_SAMPLE_RATE -5
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

/* Encode options for v2 API.
 * - threads: currently a worker hint kept for API symmetry.
 * - q65_period: Q65 T/R period in seconds: 30, 60, 120, or 300.
 * - q65_submode: Q65 submode A-E represented as 0-4.
 */
typedef struct {
    int threads;
    int q65_period;
    int q65_submode;
} wsjtx_encode_options_t;

/* Decode options for v2 API.
 * - frequency: nominal QSO frequency in Hz (passed as nfqso to the decoder)
 * - tx_frequency: transmit audio offset in Hz (passed as nftx to the decoder)
 * - threads:   thread hint forwarded to the decoder (1..N)
 * - low_freq:  decoder scan low limit in Hz  (default 200)
 * - high_freq: decoder scan high limit in Hz (default 4000)
 * - tolerance: frequency tolerance in Hz     (default 20)
 * - mycall:    local callsign for AP decode (empty = none)
 * - mygrid:    local grid for AP decode (empty = none)
 * - hiscall:   DX callsign for AP decode (empty = none)
 * - hisgrid:   DX 4-char grid for AP decode (empty = none)
 * - ap_decode: enable AP decode passes (default 1)
 * - decode_depth: WSJT-X decoder depth (default 1)
 * - qso_progress: WSJT-X QSO progress stage (default 0)
 * - q65_period/q65_submode: Q65 period and submode; ignored by other modes.
 * - q65_max_drift: Q65 max drift control.
 * - q65_clear_averaging: clear Q65 averaging state before decode.
 * - q65_single_decode: request single-candidate Q65 decode behavior.
 * - q65_averaging: enable averaged Q65 decode passes.
 */
typedef struct {
    int frequency;
    int tx_frequency;
    int threads;
    int low_freq;
    int high_freq;
    int tolerance;
    int ap_decode;
    int decode_depth;
    int qso_progress;
    int q65_period;
    int q65_submode;
    int q65_max_drift;
    int q65_clear_averaging;
    int q65_single_decode;
    int q65_averaging;
    char mycall[13];
    char mygrid[7];
    char hiscall[13];
    char hisgrid[7];
} wsjtx_decode_options_t;

/* ---- Lifecycle ---- */

WSJTX_API wsjtx_handle_t wsjtx_create(void);
WSJTX_API void wsjtx_destroy(wsjtx_handle_t handle);

/* ---- Decode ---- */

WSJTX_API int wsjtx_decode_float(wsjtx_handle_t handle, int mode,
    float* samples, int num_samples, int freq, int threads);

WSJTX_API int wsjtx_decode_int16(wsjtx_handle_t handle, int mode,
    int16_t* samples, int num_samples, int freq, int threads);

WSJTX_API int wsjtx_decode_float_v2(wsjtx_handle_t handle, int mode,
    const float* samples, int num_samples,
    const wsjtx_decode_options_t* options);

WSJTX_API int wsjtx_decode_int16_v2(wsjtx_handle_t handle, int mode,
    const int16_t* samples, int num_samples,
    const wsjtx_decode_options_t* options);

/* ---- Encode ---- */

WSJTX_API int wsjtx_encode(wsjtx_handle_t handle, int mode, int freq, int sample_rate,
    const char* message,
    float* out_samples, int* out_num_samples, int out_buf_size,
    char* out_message_sent, int out_msg_buf_size);

WSJTX_API int wsjtx_encode_v2(wsjtx_handle_t handle, int mode, int freq, int sample_rate,
    const char* message, const wsjtx_encode_options_t* options,
    float* out_samples, int* out_num_samples, int out_buf_size,
    char* out_message_sent, int out_msg_buf_size);

/* ---- Message queue ---- */

WSJTX_API int wsjtx_pull_message(wsjtx_handle_t handle, wsjtx_message_t* out_msg);

WSJTX_API int wsjtx_pull_messages(wsjtx_handle_t handle,
    wsjtx_message_t* out_messages, int max_messages);

/* ---- WSPR ---- */

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
