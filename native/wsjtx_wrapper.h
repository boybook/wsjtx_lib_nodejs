#pragma once

#include <napi.h>
#include <memory>
#include <vector>
#include <string>
#include <complex>

// 跨平台动态库加载
#ifdef _WIN32
    #include <windows.h>
    #define LIB_HANDLE HMODULE
    #define LOAD_LIBRARY(path) LoadLibraryW(path)
    #define GET_SYMBOL(handle, name) GetProcAddress(handle, name)
    #define FREE_LIBRARY(handle) FreeLibrary(handle)
#else
    #include <dlfcn.h>
    #define LIB_HANDLE void*
    #define LOAD_LIBRARY(path) dlopen(path, RTLD_LAZY | RTLD_LOCAL)
    #define GET_SYMBOL(handle, name) dlsym(handle, name)
    #define FREE_LIBRARY(handle) dlclose(handle)
#endif

// 检测MSVC模式（Windows + MSVC编译器）
#if defined(_WIN32) && defined(_MSC_VER)
    #define WSJTX_WINDOWS_MSVC_MODE 1
#else
    #define WSJTX_WINDOWS_MSVC_MODE 0
#endif

// 引用bridge API头文件
#include "../wsjtx_bridge/wsjtx_bridge.h"

// 引用C++类型定义（仅用于类型声明，不会链接wsjtx_lib）
#if !WSJTX_WINDOWS_MSVC_MODE
#include <wsjtx_lib.h>
#else
// MSVC模式下:只使用C bridge类型，避免C++运行时冲突
// 模式枚举：使用bridge的wsjtx_mode_t，但为兼容性保留enum名
enum wsjtxMode {
    FT8 = 0,
    FT4 = 1,
    JT4 = 2,
    JT65 = 3,
    JT9 = 4,
    FT2 = 5,
    WSPR = 6,
    ECHO = 7,
    FST4 = 8,
    Q65 = 9,
    FST4W = 10
};

// 使用C bridge类型作为WsjtxMessage（无std::string，安全跨CRT）
using WsjtxMessage = wsjtx_message_t;

// WSPR相关结构（当前不支持）
struct decoder_options {
    int freq;
    char rcall[13];
    char rloc[7];
    int quickmode;
    int usehashtable;
    int npasses;
    int subtraction;
};

struct decoder_results {
    double freq;
    float sync, snr, dt, drift;
    int jitter;
    char message[23];
    char call[13];
    char loc[7];
    char pwr[3];
    int cycles;
};
#endif

namespace wsjtx_nodejs {

// Type aliases for convenience
using WsjTxVector = std::vector<float>;
using IntWsjTxVector = std::vector<short int>;
using WsjtxIQSampleVector = std::vector<std::complex<float>>;

// C API function pointer types（全局定义，供Worker使用）
typedef wsjtx_handle_t (*wsjtx_create_fn)();
typedef void (*wsjtx_destroy_fn)(wsjtx_handle_t);
typedef int (*wsjtx_decode_fn)(wsjtx_handle_t, wsjtx_mode_t, const float*, int, int, int);
typedef int (*wsjtx_pull_message_fn)(wsjtx_handle_t, wsjtx_message_t*);
typedef int (*wsjtx_encode_fn)(wsjtx_handle_t, wsjtx_mode_t, const char*, int, float*, int*);
typedef int (*wsjtx_get_sample_rate_fn)(wsjtx_mode_t);
typedef int (*wsjtx_get_max_samples_fn)(wsjtx_mode_t);

/**
 * Native WSJTX library wrapper class
 */
class WSJTXLibWrapper : public Napi::ObjectWrap<WSJTXLibWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    WSJTXLibWrapper(const Napi::CallbackInfo& info);
    ~WSJTXLibWrapper();

private:
    // Instance methods
    Napi::Value Decode(const Napi::CallbackInfo& info);
    Napi::Value Encode(const Napi::CallbackInfo& info);
    Napi::Value DecodeWSPR(const Napi::CallbackInfo& info);
    Napi::Value PullMessages(const Napi::CallbackInfo& info);
    Napi::Value IsEncodingSupported(const Napi::CallbackInfo& info);
    Napi::Value IsDecodingSupported(const Napi::CallbackInfo& info);
    Napi::Value GetSampleRate(const Napi::CallbackInfo& info);
    Napi::Value GetTransmissionDuration(const Napi::CallbackInfo& info);
    Napi::Value ConvertAudioFormat(const Napi::CallbackInfo& info);

    // Internal helper methods
    Napi::Object CreateWSJTXMessage(Napi::Env env, const WsjtxMessage& msg);
    Napi::Object CreateDecodeResult(Napi::Env env, wsjtxMode mode, const std::vector<WsjtxMessage>& messages);
    Napi::Object CreateEncodeResult(Napi::Env env, const std::vector<float>& audioData, int sampleRate, const std::string& actualMessage);
    Napi::Object CreateWSPRResult(Napi::Env env, const decoder_results& result);
    
    // Validation helpers
    void ValidateMode(Napi::Env env, int mode);
    void ValidateFrequency(Napi::Env env, int frequency);
    void ValidateThreads(Napi::Env env, int threads);
    void ValidateMessage(Napi::Env env, const std::string& message);
    
    // Audio data conversion
    std::vector<float> ConvertToFloatArray(Napi::Env env, const Napi::Value& audioData);
    std::vector<short int> ConvertToIntArray(Napi::Env env, const Napi::Value& audioData);

    // 跨平台动态库加载（所有平台统一）
    LIB_HANDLE dll_handle_;
    wsjtx_handle_t lib_handle_;

    // C API function pointers（实例变量）
    wsjtx_create_fn wsjtx_create_;
    wsjtx_destroy_fn wsjtx_destroy_;
    wsjtx_decode_fn wsjtx_decode_;
    wsjtx_pull_message_fn wsjtx_pull_message_;
    wsjtx_encode_fn wsjtx_encode_;
    wsjtx_get_sample_rate_fn wsjtx_get_sample_rate_;
    wsjtx_get_max_samples_fn wsjtx_get_max_samples_;

    // Helper methods for bridge loading
    void LoadBridge();
    void UnloadBridge();
    std::string GetBridgePath();
};

/**
 * Base class for async workers (已弃用，保留for WSPRDecodeWorker兼容)
 */
class AsyncWorkerBase : public Napi::AsyncWorker {
public:
    AsyncWorkerBase(Napi::Function& callback);
    virtual ~AsyncWorkerBase() = default;
};

/**
 * Async worker for decode operations
 */
class DecodeWorker : public AsyncWorkerBase {
public:
    // Float32Array constructor
    DecodeWorker(Napi::Function& callback,
                 wsjtx_handle_t handle,
                 wsjtx_decode_fn decode_fn,
                 wsjtx_pull_message_fn pull_fn,
                 wsjtxMode mode,
                 const std::vector<float>& audioData,
                 int frequency,
                 int threads);

    // Int16Array constructor
    DecodeWorker(Napi::Function& callback,
                 wsjtx_handle_t handle,
                 wsjtx_decode_fn decode_fn,
                 wsjtx_pull_message_fn pull_fn,
                 wsjtxMode mode,
                 const std::vector<short int>& audioData,
                 int frequency,
                 int threads);

    ~DecodeWorker() = default;

protected:
    void Execute() override;
    void OnOK() override;

private:
    wsjtx_handle_t handle_;
    wsjtx_decode_fn decode_fn_;
    wsjtx_pull_message_fn pull_fn_;
    wsjtxMode mode_;
    std::vector<float> floatData_;
    std::vector<short int> intData_;
    bool useFloat_;
    int frequency_;
    int threads_;
    std::vector<WsjtxMessage> results_;
};

/**
 * Async worker for encode operations
 */
class EncodeWorker : public AsyncWorkerBase {
public:
    EncodeWorker(Napi::Function& callback,
                 wsjtx_handle_t handle,
                 wsjtx_encode_fn encode_fn,
                 wsjtx_get_max_samples_fn get_max_samples_fn,
                 wsjtxMode mode,
                 const std::string& message,
                 int frequency,
                 int threads);

    ~EncodeWorker() = default;

protected:
    void Execute() override;
    void OnOK() override;

private:
    wsjtx_handle_t handle_;
    wsjtx_encode_fn encode_fn_;
    wsjtx_get_max_samples_fn get_max_samples_fn_;
    wsjtxMode mode_;
    std::string message_;
    int frequency_;
    int threads_;
    std::vector<float> audioData_;
    std::string actualMessage_;
    std::string messageSent_;
    int sampleRate_;
};

/**
 * Async worker for WSPR decode operations
 * Note: WSPR is currently not supported in the bridge architecture
 */
class WSPRDecodeWorker : public AsyncWorkerBase {
public:
    WSPRDecodeWorker(Napi::Function& callback,
                     const std::vector<std::complex<float>>& iqData,
                     const decoder_options& options);

    ~WSPRDecodeWorker() = default;

protected:
    void Execute() override;
    void OnOK() override;

private:
    std::vector<std::complex<float>> iqData_;
    decoder_options options_;
    std::vector<decoder_results> results_;
};

/**
 * Async worker for simple audio format conversion
 */
class AudioConvertWorker : public Napi::AsyncWorker {
public:
    enum class Target { Float32, Int16 };

    // From Float32Array to Int16Array
    AudioConvertWorker(Napi::Function& callback,
                       const std::vector<float>& input,
                       Target target)
        : Napi::AsyncWorker(callback), floatInput_(input), target_(target), fromFloat_(true) {}

    // From Int16Array to Float32Array
    AudioConvertWorker(Napi::Function& callback,
                       const std::vector<short int>& input,
                       Target target)
        : Napi::AsyncWorker(callback), intInput_(input), target_(target), fromFloat_(false) {}

    ~AudioConvertWorker() = default;

protected:
    void Execute() override;
    void OnOK() override;

private:
    std::vector<float> floatInput_;
    std::vector<short int> intInput_;
    std::vector<float> floatOut_;
    std::vector<short int> intOut_;
    Target target_;
    bool fromFloat_;
};

// Module initialization functions
Napi::String GetVersion(const Napi::CallbackInfo& info);
Napi::Array GetSupportedModes(const Napi::CallbackInfo& info);

// Utility functions
wsjtxMode ConvertToWSJTXMode(int mode);
int GetSampleRateForMode(wsjtxMode mode);
double GetTransmissionDurationForMode(wsjtxMode mode);
bool IsModeSupported(wsjtxMode mode, bool forEncoding);

} // namespace wsjtx_nodejs
