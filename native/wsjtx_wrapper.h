#pragma once

#include <napi.h>
#include <vector>
#include <string>
#include "wsjtx_c_api.h"

namespace wsjtx_nodejs {

/**
 * Native WSJTX library wrapper class.
 * Uses the pure C API (wsjtx_c_api.h) for all interactions with the core library.
 */
class WSJTXLibWrapper : public Napi::ObjectWrap<WSJTXLibWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    WSJTXLibWrapper(const Napi::CallbackInfo& info);
    ~WSJTXLibWrapper();

private:
    Napi::Value Decode(const Napi::CallbackInfo& info);
    Napi::Value Encode(const Napi::CallbackInfo& info);
    Napi::Value DecodeWSPR(const Napi::CallbackInfo& info);
    Napi::Value PullMessages(const Napi::CallbackInfo& info);
    Napi::Value IsEncodingSupported(const Napi::CallbackInfo& info);
    Napi::Value IsDecodingSupported(const Napi::CallbackInfo& info);
    Napi::Value GetSampleRate(const Napi::CallbackInfo& info);
    Napi::Value GetTransmissionDuration(const Napi::CallbackInfo& info);
    Napi::Value ConvertAudioFormat(const Napi::CallbackInfo& info);

    Napi::Object CreateMessageObject(Napi::Env env, const wsjtx_message_t& msg);

    void ValidateMode(Napi::Env env, int mode);
    void ValidateFrequency(Napi::Env env, int frequency);
    void ValidateThreads(Napi::Env env, int threads);
    void ValidateMessage(Napi::Env env, const std::string& message);

    std::vector<float> ConvertToFloatArray(Napi::Env env, const Napi::Value& audioData);
    std::vector<short int> ConvertToIntArray(Napi::Env env, const Napi::Value& audioData);

    wsjtx_handle_t handle_;
};

/**
 * Base class for async workers that need the library handle
 */
class AsyncWorkerBase : public Napi::AsyncWorker {
public:
    AsyncWorkerBase(Napi::Function& callback, wsjtx_handle_t handle);
    virtual ~AsyncWorkerBase() = default;

protected:
    wsjtx_handle_t handle_;
};

/**
 * Async worker for decode operations
 */
class DecodeWorker : public AsyncWorkerBase {
public:
    DecodeWorker(Napi::Function& cb, wsjtx_handle_t h, int mode, const std::vector<float>& d, const wsjtx_decode_options_t& o);
    DecodeWorker(Napi::Function& cb, wsjtx_handle_t h, int mode, const std::vector<short int>& d, const wsjtx_decode_options_t& o);
protected:
    void Execute() override; void OnOK() override;
private:
    static constexpr int MAX_MSGS = 200;
    int mode_; std::vector<float> floatData_; std::vector<short int> intData_; bool useFloat_;
    wsjtx_decode_options_t options_; std::vector<wsjtx_message_t> messages_; int numMessages_ = 0;
};

/**
 * Async worker for encode operations
 */
class EncodeWorker : public AsyncWorkerBase {
public:
    EncodeWorker(Napi::Function& callback, wsjtx_handle_t handle,
                 int mode, const std::string& message,
                 int frequency, int threads);

protected:
    void Execute() override;
    void OnOK() override;

private:
    int mode_;
    std::string message_;
    int frequency_;
    int threads_;
    std::vector<float> audioData_;
    std::string messageSent_;
};

/**
 * Async worker for WSPR decode operations
 */
class WSPRDecodeWorker : public AsyncWorkerBase {
public:
    WSPRDecodeWorker(Napi::Function& callback, wsjtx_handle_t handle,
                     const std::vector<float>& iqInterleaved,
                     const wsjtx_decoder_options_t& options);

protected:
    void Execute() override;
    void OnOK() override;

private:
    std::vector<float> iqInterleaved_;
    wsjtx_decoder_options_t options_;
    std::vector<wsjtx_decoder_result_t> results_;
};

/**
 * Async worker for audio format conversion (no library handle needed)
 */
class AudioConvertWorker : public Napi::AsyncWorker {
public:
    enum class Target { Float32, Int16 };

    AudioConvertWorker(Napi::Function& callback,
                       const std::vector<float>& input, Target target)
        : Napi::AsyncWorker(callback), floatInput_(input), target_(target), fromFloat_(true) {}

    AudioConvertWorker(Napi::Function& callback,
                       const std::vector<short int>& input, Target target)
        : Napi::AsyncWorker(callback), intInput_(input), target_(target), fromFloat_(false) {}

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

} // namespace wsjtx_nodejs
