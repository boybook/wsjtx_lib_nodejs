#pragma once

#include <napi.h>
#include <memory>
#include <vector>
#include <string>
#include <complex>
#include <wsjtx_lib.h>

namespace wsjtx_nodejs {

// Type aliases for convenience
using WsjTxVector = std::vector<float>;
using IntWsjTxVector = std::vector<short int>;
using WsjtxIQSampleVector = std::vector<std::complex<float>>;

/**
 * Native WSJTX library wrapper class
 */
class WSJTXLibWrapper : public Napi::ObjectWrap<WSJTXLibWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    WSJTXLibWrapper(const Napi::CallbackInfo& info);

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

    // Native library instance
    std::unique_ptr<wsjtx_lib> lib_;
};

/**
 * Base class for async workers
 */
class AsyncWorkerBase : public Napi::AsyncWorker {
public:
    AsyncWorkerBase(Napi::Function& callback, wsjtx_lib* lib);
    virtual ~AsyncWorkerBase() = default;

protected:
    wsjtx_lib* lib_;
};

/**
 * Async worker for decode operations
 */
class DecodeWorker : public AsyncWorkerBase {
public:
    DecodeWorker(Napi::Function& callback, 
                 wsjtx_lib* lib,
                 wsjtxMode mode,
                 const std::vector<float>& audioData,
                 int frequency,
                 int threads);
    
    DecodeWorker(Napi::Function& callback, 
                 wsjtx_lib* lib,
                 wsjtxMode mode,
                 const std::vector<short int>& audioData,
                 int frequency,
                 int threads);

    ~DecodeWorker() = default;

protected:
    void Execute() override;
    void OnOK() override;

private:
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
                 wsjtx_lib* lib,
                 wsjtxMode mode,
                 const std::string& message,
                 int frequency,
                 int threads);

    ~EncodeWorker() = default;

protected:
    void Execute() override;
    void OnOK() override;

private:
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
 */
class WSPRDecodeWorker : public AsyncWorkerBase {
public:
    WSPRDecodeWorker(Napi::Function& callback,
                     wsjtx_lib* lib,
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
