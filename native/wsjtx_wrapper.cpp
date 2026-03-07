#include "wsjtx_wrapper.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace wsjtx_nodejs
{

    // ---- WSJTXLibWrapper ----

    Napi::Object WSJTXLibWrapper::Init(Napi::Env env, Napi::Object exports)
    {
        Napi::Function func = DefineClass(env, "WSJTXLib", {
            InstanceMethod("decode", &WSJTXLibWrapper::Decode),
            InstanceMethod("encode", &WSJTXLibWrapper::Encode),
            InstanceMethod("decodeWSPR", &WSJTXLibWrapper::DecodeWSPR),
            InstanceMethod("pullMessages", &WSJTXLibWrapper::PullMessages),
            InstanceMethod("isEncodingSupported", &WSJTXLibWrapper::IsEncodingSupported),
            InstanceMethod("isDecodingSupported", &WSJTXLibWrapper::IsDecodingSupported),
            InstanceMethod("getSampleRate", &WSJTXLibWrapper::GetSampleRate),
            InstanceMethod("getTransmissionDuration", &WSJTXLibWrapper::GetTransmissionDuration),
            InstanceMethod("convertAudioFormat", &WSJTXLibWrapper::ConvertAudioFormat)
        });

        exports.Set("WSJTXLib", func);
        return exports;
    }

    WSJTXLibWrapper::WSJTXLibWrapper(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<WSJTXLibWrapper>(info)
    {
        handle_ = wsjtx_create();
        if (!handle_) {
            Napi::Error::New(info.Env(), "Failed to create wsjtx_lib instance")
                .ThrowAsJavaScriptException();
        }
    }

    WSJTXLibWrapper::~WSJTXLibWrapper()
    {
        if (handle_) {
            wsjtx_destroy(handle_);
            handle_ = nullptr;
        }
    }

    // ---- Decode ----

    Napi::Value WSJTXLibWrapper::Decode(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 5)
        {
            Napi::TypeError::New(env, "Expected 5 arguments: mode, audioData, frequency, threads, callback")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!info[0].IsNumber() || !info[2].IsNumber() || !info[3].IsNumber() || !info[4].IsFunction())
        {
            Napi::TypeError::New(env, "Invalid argument types").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        int frequency = info[2].As<Napi::Number>().Int32Value();
        int threads = info[3].As<Napi::Number>().Int32Value();
        Napi::Function callback = info[4].As<Napi::Function>();

        try {
            ValidateMode(env, mode);
            ValidateFrequency(env, frequency);
            ValidateThreads(env, threads);
        } catch (const std::exception &e) {
            Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
            return env.Null();
        }

        Napi::Value audioData = info[1];
        if (!audioData.IsTypedArray()) {
            Napi::TypeError::New(env, "Audio data must be a typed array").ThrowAsJavaScriptException();
            return env.Null();
        }

        Napi::TypedArray typedArray = audioData.As<Napi::TypedArray>();

        if (typedArray.TypedArrayType() == napi_float32_array) {
            auto floatData = ConvertToFloatArray(env, audioData);
            auto worker = new DecodeWorker(callback, handle_, mode, floatData, frequency, threads);
            worker->Queue();
        } else if (typedArray.TypedArrayType() == napi_int16_array) {
            auto intData = ConvertToIntArray(env, audioData);
            auto worker = new DecodeWorker(callback, handle_, mode, intData, frequency, threads);
            worker->Queue();
        } else {
            Napi::TypeError::New(env, "Audio data must be Float32Array or Int16Array")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        return env.Undefined();
    }

    // ---- Encode ----

    Napi::Value WSJTXLibWrapper::Encode(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 5)
        {
            Napi::TypeError::New(env, "Expected 5 arguments: mode, message, frequency, threads, callback")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!info[0].IsNumber() || !info[1].IsString() || !info[2].IsNumber() ||
            !info[3].IsNumber() || !info[4].IsFunction())
        {
            Napi::TypeError::New(env, "Invalid argument types").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        std::string message = info[1].As<Napi::String>().Utf8Value();
        int frequency = info[2].As<Napi::Number>().Int32Value();
        int threads = info[3].As<Napi::Number>().Int32Value();
        Napi::Function callback = info[4].As<Napi::Function>();

        try {
            ValidateMode(env, mode);
            ValidateFrequency(env, frequency);
            ValidateThreads(env, threads);
            ValidateMessage(env, message);
        } catch (const std::exception &e) {
            Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!wsjtx_is_encoding_supported(mode)) {
            Napi::Error::New(env, "Encoding not supported for this mode")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        auto worker = new EncodeWorker(callback, handle_, mode, message, frequency, threads);
        worker->Queue();

        return env.Undefined();
    }

    // ---- WSPR Decode ----

    Napi::Value WSJTXLibWrapper::DecodeWSPR(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 3)
        {
            Napi::TypeError::New(env, "Expected 3 arguments: iqData, options, callback")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!info[0].IsTypedArray() || !info[1].IsObject() || !info[2].IsFunction())
        {
            Napi::TypeError::New(env, "Invalid argument types").ThrowAsJavaScriptException();
            return env.Null();
        }

        Napi::Float32Array iqArray = info[0].As<Napi::Float32Array>();
        size_t length = iqArray.ElementLength();

        if (length % 2 != 0)
        {
            Napi::Error::New(env, "IQ data length must be even (interleaved I,Q samples)")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        // Copy the interleaved IQ data directly (no complex conversion needed)
        float *data = iqArray.Data();
        std::vector<float> iqInterleaved(data, data + length);

        // Parse decoder options
        Napi::Object optObj = info[1].As<Napi::Object>();
        wsjtx_decoder_options_t options;
        memset(&options, 0, sizeof(options));

        if (optObj.Has("dialFrequency"))
            options.freq = optObj.Get("dialFrequency").As<Napi::Number>().Int32Value();

        if (optObj.Has("callsign")) {
            std::string cs = optObj.Get("callsign").As<Napi::String>().Utf8Value();
            strncpy(options.rcall, cs.c_str(), sizeof(options.rcall) - 1);
        }

        if (optObj.Has("locator")) {
            std::string loc = optObj.Get("locator").As<Napi::String>().Utf8Value();
            strncpy(options.rloc, loc.c_str(), sizeof(options.rloc) - 1);
        }

        if (optObj.Has("quickMode"))
            options.quickmode = optObj.Get("quickMode").As<Napi::Boolean>().Value() ? 1 : 0;

        if (optObj.Has("useHashTable"))
            options.usehashtable = optObj.Get("useHashTable").As<Napi::Boolean>().Value() ? 1 : 0;

        if (optObj.Has("passes"))
            options.npasses = optObj.Get("passes").As<Napi::Number>().Int32Value();

        if (optObj.Has("subtraction"))
            options.subtraction = optObj.Get("subtraction").As<Napi::Boolean>().Value() ? 1 : 0;

        Napi::Function callback = info[2].As<Napi::Function>();

        auto worker = new WSPRDecodeWorker(callback, handle_, iqInterleaved, options);
        worker->Queue();

        return env.Undefined();
    }

    // ---- Pull Messages ----

    Napi::Value WSJTXLibWrapper::PullMessages(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        Napi::Array results = Napi::Array::New(env);
        wsjtx_message_t msg;
        uint32_t count = 0;

        while (wsjtx_pull_message(handle_, &msg) == 1)
        {
            results[count++] = CreateMessageObject(env, msg);
        }

        return results;
    }

    // ---- Query methods ----

    Napi::Value WSJTXLibWrapper::IsEncodingSupported(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();
        if (info.Length() < 1 || !info[0].IsNumber()) {
            Napi::TypeError::New(env, "Expected mode number").ThrowAsJavaScriptException();
            return env.Null();
        }
        int mode = info[0].As<Napi::Number>().Int32Value();
        return Napi::Boolean::New(env, wsjtx_is_encoding_supported(mode) != 0);
    }

    Napi::Value WSJTXLibWrapper::IsDecodingSupported(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();
        if (info.Length() < 1 || !info[0].IsNumber()) {
            Napi::TypeError::New(env, "Expected mode number").ThrowAsJavaScriptException();
            return env.Null();
        }
        int mode = info[0].As<Napi::Number>().Int32Value();
        return Napi::Boolean::New(env, wsjtx_is_decoding_supported(mode) != 0);
    }

    Napi::Value WSJTXLibWrapper::GetSampleRate(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();
        if (info.Length() < 1 || !info[0].IsNumber()) {
            Napi::TypeError::New(env, "Expected mode number").ThrowAsJavaScriptException();
            return env.Null();
        }
        int mode = info[0].As<Napi::Number>().Int32Value();
        return Napi::Number::New(env, wsjtx_get_sample_rate(mode));
    }

    Napi::Value WSJTXLibWrapper::GetTransmissionDuration(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();
        if (info.Length() < 1 || !info[0].IsNumber()) {
            Napi::TypeError::New(env, "Expected mode number").ThrowAsJavaScriptException();
            return env.Null();
        }
        int mode = info[0].As<Napi::Number>().Int32Value();
        return Napi::Number::New(env, wsjtx_get_transmission_duration(mode));
    }

    // ---- Audio Format Conversion ----

    Napi::Value WSJTXLibWrapper::ConvertAudioFormat(const Napi::CallbackInfo& info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 3) {
            Napi::TypeError::New(env, "Expected 3 arguments: audioData, targetFormat, callback")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!info[0].IsTypedArray() || !info[1].IsString() || !info[2].IsFunction()) {
            Napi::TypeError::New(env, "Invalid argument types").ThrowAsJavaScriptException();
            return env.Null();
        }

        std::string target = info[1].As<Napi::String>().Utf8Value();
        AudioConvertWorker::Target tgt;
        if (target == "float32") tgt = AudioConvertWorker::Target::Float32;
        else if (target == "int16") tgt = AudioConvertWorker::Target::Int16;
        else {
            Napi::TypeError::New(env, "targetFormat must be 'float32' or 'int16'")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        Napi::Function callback = info[2].As<Napi::Function>();
        Napi::TypedArray ta = info[0].As<Napi::TypedArray>();

        if (ta.TypedArrayType() == napi_float32_array) {
            auto input = ConvertToFloatArray(env, info[0]);
            auto* worker = new AudioConvertWorker(callback, input, tgt);
            worker->Queue();
        } else if (ta.TypedArrayType() == napi_int16_array) {
            auto input = ConvertToIntArray(env, info[0]);
            auto* worker = new AudioConvertWorker(callback, input, tgt);
            worker->Queue();
        } else {
            Napi::TypeError::New(env, "audioData must be Float32Array or Int16Array")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        return env.Undefined();
    }

    // ---- Helpers ----

    void WSJTXLibWrapper::ValidateMode(Napi::Env env, int mode) {
        if (mode < 0 || mode > WSJTX_MODE_WSPR)
            throw std::invalid_argument("Invalid mode value");
    }

    void WSJTXLibWrapper::ValidateFrequency(Napi::Env env, int frequency) {
        if (frequency < 0 || frequency > 30000000)
            throw std::invalid_argument("Invalid frequency value");
    }

    void WSJTXLibWrapper::ValidateThreads(Napi::Env env, int threads) {
        if (threads < 1 || threads > 16)
            throw std::invalid_argument("Thread count must be between 1 and 16");
    }

    void WSJTXLibWrapper::ValidateMessage(Napi::Env env, const std::string &message) {
        if (message.empty() || message.length() > 22)
            throw std::invalid_argument("Message must be 1-22 characters long");
    }

    std::vector<float> WSJTXLibWrapper::ConvertToFloatArray(Napi::Env env, const Napi::Value& value) {
        Napi::Float32Array array = value.As<Napi::Float32Array>();
        float *data = array.Data();
        return std::vector<float>(data, data + array.ElementLength());
    }

    std::vector<short int> WSJTXLibWrapper::ConvertToIntArray(Napi::Env env, const Napi::Value& value) {
        Napi::Int16Array array = value.As<Napi::Int16Array>();
        int16_t *data = array.Data();
        return std::vector<short int>(data, data + array.ElementLength());
    }

    Napi::Object WSJTXLibWrapper::CreateMessageObject(Napi::Env env, const wsjtx_message_t &msg)
    {
        Napi::Object result = Napi::Object::New(env);
        result.Set("text", Napi::String::New(env, msg.msg));
        result.Set("snr", Napi::Number::New(env, msg.snr));
        result.Set("deltaTime", Napi::Number::New(env, msg.dt));
        result.Set("deltaFrequency", Napi::Number::New(env, msg.freq));
        result.Set("timestamp", Napi::Number::New(env, msg.hh * 3600 + msg.min * 60 + msg.sec));
        result.Set("sync", Napi::Number::New(env, msg.sync));
        return result;
    }

    // ---- Async Workers ----

    AsyncWorkerBase::AsyncWorkerBase(Napi::Function &callback, wsjtx_handle_t handle)
        : Napi::AsyncWorker(callback), handle_(handle) {}

    // DecodeWorker (float)
    DecodeWorker::DecodeWorker(Napi::Function &callback, wsjtx_handle_t handle,
                               int mode, const std::vector<float> &audioData,
                               int frequency, int threads)
        : AsyncWorkerBase(callback, handle), mode_(mode), floatData_(audioData),
          frequency_(frequency), threads_(threads), useFloat_(true) {}

    // DecodeWorker (int16)
    DecodeWorker::DecodeWorker(Napi::Function &callback, wsjtx_handle_t handle,
                               int mode, const std::vector<short int> &audioData,
                               int frequency, int threads)
        : AsyncWorkerBase(callback, handle), mode_(mode), intData_(audioData),
          frequency_(frequency), threads_(threads), useFloat_(false) {}

    void DecodeWorker::Execute()
    {
        int rc;
        if (useFloat_) {
            rc = wsjtx_decode_float(handle_, mode_,
                floatData_.data(), static_cast<int>(floatData_.size()),
                frequency_, threads_);
        } else {
            rc = wsjtx_decode_int16(handle_, mode_,
                reinterpret_cast<int16_t*>(intData_.data()),
                static_cast<int>(intData_.size()),
                frequency_, threads_);
        }
        if (rc != WSJTX_OK) {
            SetError("Decode failed with error code " + std::to_string(rc));
        }
    }

    void DecodeWorker::OnOK()
    {
        Napi::Env env = Env();
        Callback().Call({env.Null(), Napi::Boolean::New(env, true)});
    }

    // EncodeWorker
    EncodeWorker::EncodeWorker(Napi::Function &callback, wsjtx_handle_t handle,
                               int mode, const std::string &message,
                               int frequency, int threads)
        : AsyncWorkerBase(callback, handle), mode_(mode), message_(message),
          frequency_(frequency), threads_(threads) {}

    void EncodeWorker::Execute()
    {
        // FT8 at 48kHz for 12.64s = ~607,000 samples; 1M buffer is plenty
        static const int MAX_SAMPLES = 1024 * 1024;
        audioData_.resize(MAX_SAMPLES);
        int numSamples = 0;
        char msgSent[256] = {0};

        int rc = wsjtx_encode(handle_, mode_, frequency_,
            message_.c_str(),
            audioData_.data(), &numSamples, MAX_SAMPLES,
            msgSent, sizeof(msgSent));

        if (rc != WSJTX_OK) {
            SetError("Encode failed with error code " + std::to_string(rc));
            return;
        }

        audioData_.resize(numSamples);
        messageSent_ = std::string(msgSent);
    }

    void EncodeWorker::OnOK()
    {
        Napi::Env env = Env();

        Napi::Float32Array audioArray = Napi::Float32Array::New(env, audioData_.size());
        std::copy(audioData_.begin(), audioData_.end(), audioArray.Data());

        Napi::Object result = Napi::Object::New(env);
        result.Set("audioData", audioArray);
        result.Set("messageSent", Napi::String::New(env, messageSent_));

        Callback().Call({env.Null(), result});
    }

    // WSPRDecodeWorker
    WSPRDecodeWorker::WSPRDecodeWorker(Napi::Function &callback, wsjtx_handle_t handle,
                                       const std::vector<float> &iqInterleaved,
                                       const wsjtx_decoder_options_t &options)
        : AsyncWorkerBase(callback, handle), iqInterleaved_(iqInterleaved), options_(options) {}

    void WSPRDecodeWorker::Execute()
    {
        static const int MAX_RESULTS = 256;
        results_.resize(MAX_RESULTS);

        int numIqSamples = static_cast<int>(iqInterleaved_.size() / 2);
        int count = wsjtx_wspr_decode(handle_,
            iqInterleaved_.data(), numIqSamples,
            &options_, results_.data(), MAX_RESULTS);

        if (count < 0) {
            SetError("WSPR decode failed with error code " + std::to_string(count));
            results_.clear();
            return;
        }

        results_.resize(count);
    }

    void WSPRDecodeWorker::OnOK()
    {
        Napi::Env env = Env();
        Napi::Array resultsArray = Napi::Array::New(env, results_.size());

        for (size_t i = 0; i < results_.size(); i++)
        {
            const auto &r = results_[i];
            Napi::Object obj = Napi::Object::New(env);

            obj.Set("frequency", Napi::Number::New(env, r.freq));
            obj.Set("sync", Napi::Number::New(env, r.sync));
            obj.Set("snr", Napi::Number::New(env, r.snr));
            obj.Set("deltaTime", Napi::Number::New(env, r.dt));
            obj.Set("drift", Napi::Number::New(env, r.drift));
            obj.Set("jitter", Napi::Number::New(env, r.jitter));
            obj.Set("message", Napi::String::New(env, r.message));
            obj.Set("callsign", Napi::String::New(env, r.call));
            obj.Set("locator", Napi::String::New(env, r.loc));
            obj.Set("power", Napi::String::New(env, r.pwr));
            obj.Set("cycles", Napi::Number::New(env, r.cycles));

            resultsArray[i] = obj;
        }

        Callback().Call({env.Null(), resultsArray});
    }

    // AudioConvertWorker
    void AudioConvertWorker::Execute()
    {
        if (fromFloat_) {
            if (target_ == Target::Float32) {
                floatOut_ = floatInput_;
            } else {
                intOut_.resize(floatInput_.size());
                for (size_t i = 0; i < floatInput_.size(); ++i) {
                    float v = std::max(-1.0f, std::min(1.0f, floatInput_[i]));
                    intOut_[i] = static_cast<short int>(
                        std::max(-32768, std::min(32767,
                            static_cast<int>(std::lround(v * 32768.0f)))));
                }
            }
        } else {
            if (target_ == Target::Int16) {
                intOut_ = intInput_;
            } else {
                floatOut_.resize(intInput_.size());
                for (size_t i = 0; i < intInput_.size(); ++i) {
                    floatOut_[i] = static_cast<float>(intInput_[i]) / 32768.0f;
                }
            }
        }
    }

    void AudioConvertWorker::OnOK()
    {
        Napi::Env env = Env();
        if (target_ == Target::Float32) {
            Napi::Float32Array out = Napi::Float32Array::New(env, floatOut_.size());
            std::copy(floatOut_.begin(), floatOut_.end(), out.Data());
            Callback().Call({env.Null(), out});
        } else {
            Napi::Int16Array out = Napi::Int16Array::New(env, intOut_.size());
            std::copy(intOut_.begin(), intOut_.end(), out.Data());
            Callback().Call({env.Null(), out});
        }
    }

    // Module initialization
    Napi::Object Init(Napi::Env env, Napi::Object exports)
    {
        return WSJTXLibWrapper::Init(env, exports);
    }

    NODE_API_MODULE(wsjtx_lib, Init)

} // namespace wsjtx_nodejs
