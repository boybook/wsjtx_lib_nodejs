#include "wsjtx_wrapper.h"
#include <chrono>
#include <complex>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

namespace wsjtx_nodejs
{

    // Static mode information
    struct ModeInfo
    {
        int sampleRate;
        double duration;
        bool encodingSupported;
        bool decodingSupported;
    };

    static const std::map<wsjtxMode, ModeInfo> MODE_INFO = {
        {FT8, {48000, 12.64, true, true}},
        {FT4, {48000, 6.0, true, true}},
        {JT4, {11025, 47.1, false, true}},
        {JT65, {11025, 46.8, false, true}},
        {JT9, {12000, 49.0, false, true}},
        {FST4, {12000, 60.0, false, true}},
        {Q65, {12000, 60.0, false, true}},
        {FST4W, {12000, 120.0, false, true}},
        {WSPR, {12000, 110.6, false, true}}};

    // WSJTXLibWrapper implementation
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

    // New method: convertAudioFormat(audioData, targetFormat, callback)
    Napi::Value WSJTXLibWrapper::ConvertAudioFormat(const Napi::CallbackInfo& info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 3)
        {
            Napi::TypeError::New(env, "Expected 3 arguments: audioData, targetFormat, callback").ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!info[0].IsTypedArray() || !info[1].IsString() || !info[2].IsFunction())
        {
            Napi::TypeError::New(env, "Invalid argument types").ThrowAsJavaScriptException();
            return env.Null();
        }

        std::string target = info[1].As<Napi::String>().Utf8Value();
        AudioConvertWorker::Target tgt;
        if (target == "float32") tgt = AudioConvertWorker::Target::Float32;
        else if (target == "int16") tgt = AudioConvertWorker::Target::Int16;
        else {
            Napi::TypeError::New(env, "targetFormat must be 'float32' or 'int16'").ThrowAsJavaScriptException();
            return env.Null();
        }

        Napi::Function callback = info[2].As<Napi::Function>();

        Napi::TypedArray ta = info[0].As<Napi::TypedArray>();
        if (ta.TypedArrayType() == napi_float32_array)
        {
            auto input = ConvertToFloatArray(env, info[0]);
            auto* worker = new AudioConvertWorker(callback, input, tgt);
            worker->Queue();
        }
        else if (ta.TypedArrayType() == napi_int16_array)
        {
            auto input = ConvertToIntArray(env, info[0]);
            auto* worker = new AudioConvertWorker(callback, input, tgt);
            worker->Queue();
        }
        else
        {
            Napi::TypeError::New(env, "audioData must be Float32Array or Int16Array").ThrowAsJavaScriptException();
            return env.Null();
        }

        return env.Undefined();
    }

    WSJTXLibWrapper::WSJTXLibWrapper(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<WSJTXLibWrapper>(info)
    {
        lib_ = std::make_unique<wsjtx_lib>();
    }

    // Decode method - supports Float32Array and Int16Array audio data
    Napi::Value WSJTXLibWrapper::Decode(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 5)
        {
            Napi::TypeError::New(env, "Expected 5 arguments: mode, audioData, frequency, threads, callback")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        // Validate arguments
        if (!info[0].IsNumber() || !info[2].IsNumber() || !info[3].IsNumber() || !info[4].IsFunction())
        {
            Napi::TypeError::New(env, "Invalid argument types").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        int frequency = info[2].As<Napi::Number>().Int32Value();
        int threads = info[3].As<Napi::Number>().Int32Value();
        Napi::Function callback = info[4].As<Napi::Function>();

        // Validate parameters
        try
        {
            ValidateMode(env, mode);
            ValidateFrequency(env, frequency);
            ValidateThreads(env, threads);
        }
        catch (const std::exception &e)
        {
            Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
            return env.Null();
        }

        wsjtxMode wsjtxModeVal = ConvertToWSJTXMode(mode);

        // Check if audio data is Float32Array or Int16Array
        Napi::Value audioData = info[1];

        if (audioData.IsTypedArray())
        {
            Napi::TypedArray typedArray = audioData.As<Napi::TypedArray>();

            if (typedArray.TypedArrayType() == napi_float32_array)
            {
                // Float32Array
                auto floatData = ConvertToFloatArray(env, audioData);
                auto worker = new DecodeWorker(callback, lib_.get(), wsjtxModeVal, floatData, frequency, threads);
                worker->Queue();
            }
            else if (typedArray.TypedArrayType() == napi_int16_array)
            {
                // Int16Array
                auto intData = ConvertToIntArray(env, audioData);
                auto worker = new DecodeWorker(callback, lib_.get(), wsjtxModeVal, intData, frequency, threads);
                worker->Queue();
            }
            else
            {
                Napi::TypeError::New(env, "Audio data must be Float32Array or Int16Array")
                    .ThrowAsJavaScriptException();
                return env.Null();
            }
        }
        else
        {
            Napi::TypeError::New(env, "Audio data must be a typed array")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        return env.Undefined();
    }

    // Encode method - generates audio waveform for transmission
    Napi::Value WSJTXLibWrapper::Encode(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 5)
        {
            Napi::TypeError::New(env, "Expected 5 arguments: mode, message, frequency, threads, callback")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        // Validate arguments
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

        // Validate parameters
        try
        {
            ValidateMode(env, mode);
            ValidateFrequency(env, frequency);
            ValidateThreads(env, threads);
            ValidateMessage(env, message);
        }
        catch (const std::exception &e)
        {
            Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
            return env.Null();
        }

        wsjtxMode wsjtxModeVal = ConvertToWSJTXMode(mode);

        // Check encoding support
        auto it = MODE_INFO.find(wsjtxModeVal);
        if (it == MODE_INFO.end() || !it->second.encodingSupported)
        {
            Napi::Error::New(env, "Encoding not supported for this mode")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        auto worker = new EncodeWorker(callback, lib_.get(), wsjtxModeVal, message, frequency, threads);
        worker->Queue();

        return env.Undefined();
    }

    // WSPR specific decode method with IQ data and options
    Napi::Value WSJTXLibWrapper::DecodeWSPR(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 3)
        {
            Napi::TypeError::New(env, "Expected 3 arguments: iqData, options, callback")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        // Validate arguments
        if (!info[0].IsTypedArray() || !info[1].IsObject() || !info[2].IsFunction())
        {
            Napi::TypeError::New(env, "Invalid argument types").ThrowAsJavaScriptException();
            return env.Null();
        }

        // Convert IQ data (interleaved I,Q samples)
        Napi::Float32Array iqArray = info[0].As<Napi::Float32Array>();
        size_t length = iqArray.ElementLength();

        if (length % 2 != 0)
        {
            Napi::Error::New(env, "IQ data length must be even (interleaved I,Q samples)")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        WsjtxIQSampleVector iqData;
        iqData.reserve(length / 2);

        float *data = iqArray.Data();
        for (size_t i = 0; i < length; i += 2)
        {
            iqData.emplace_back(data[i], data[i + 1]);
        }

        // Parse decoder options
        Napi::Object options = info[1].As<Napi::Object>();
        decoder_options decoderOptions;

        if (options.Has("dialFrequency"))
        {
            decoderOptions.freq = options.Get("dialFrequency").As<Napi::Number>().Int32Value();
        }

        if (options.Has("callsign"))
        {
            std::string callsign = options.Get("callsign").As<Napi::String>().Utf8Value();
            strncpy(decoderOptions.rcall, callsign.c_str(), sizeof(decoderOptions.rcall) - 1);
            decoderOptions.rcall[sizeof(decoderOptions.rcall) - 1] = '\0';
        }

        if (options.Has("locator"))
        {
            std::string locator = options.Get("locator").As<Napi::String>().Utf8Value();
            strncpy(decoderOptions.rloc, locator.c_str(), sizeof(decoderOptions.rloc) - 1);
            decoderOptions.rloc[sizeof(decoderOptions.rloc) - 1] = '\0';
        }

        if (options.Has("quickMode"))
        {
            decoderOptions.quickmode = options.Get("quickMode").As<Napi::Boolean>().Value() ? 1 : 0;
        }

        if (options.Has("useHashTable"))
        {
            decoderOptions.usehashtable = options.Get("useHashTable").As<Napi::Boolean>().Value() ? 1 : 0;
        }

        if (options.Has("passes"))
        {
            decoderOptions.npasses = options.Get("passes").As<Napi::Number>().Int32Value();
        }

        if (options.Has("subtraction"))
        {
            decoderOptions.subtraction = options.Get("subtraction").As<Napi::Boolean>().Value() ? 1 : 0;
        }

        Napi::Function callback = info[2].As<Napi::Function>();

        auto worker = new WSPRDecodeWorker(callback, lib_.get(), iqData, decoderOptions);
        worker->Queue();

        return env.Undefined();
    }

    // Pull decoded messages from the queue
    Napi::Value WSJTXLibWrapper::PullMessages(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        Napi::Array results = Napi::Array::New(env);
        WsjtxMessage msg;
        uint32_t count = 0;

        while (lib_->pullMessage(msg))
        {
            results[count++] = CreateWSJTXMessage(env, msg);
        }

        return results;
    }

    // Check if encoding is supported for a mode
    Napi::Value WSJTXLibWrapper::IsEncodingSupported(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 1 || !info[0].IsNumber())
        {
            Napi::TypeError::New(env, "Expected mode number").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        wsjtxMode wsjtxModeVal = ConvertToWSJTXMode(mode);

        auto it = MODE_INFO.find(wsjtxModeVal);
        bool supported = (it != MODE_INFO.end()) && it->second.encodingSupported;

        return Napi::Boolean::New(env, supported);
    }

    // Check if decoding is supported for a mode
    Napi::Value WSJTXLibWrapper::IsDecodingSupported(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 1 || !info[0].IsNumber())
        {
            Napi::TypeError::New(env, "Expected mode number").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        wsjtxMode wsjtxModeVal = ConvertToWSJTXMode(mode);

        auto it = MODE_INFO.find(wsjtxModeVal);
        bool supported = (it != MODE_INFO.end()) && it->second.decodingSupported;

        return Napi::Boolean::New(env, supported);
    }

    // Get sample rate for a mode
    Napi::Value WSJTXLibWrapper::GetSampleRate(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 1 || !info[0].IsNumber())
        {
            Napi::TypeError::New(env, "Expected mode number").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        wsjtxMode wsjtxModeVal = ConvertToWSJTXMode(mode);

        auto it = MODE_INFO.find(wsjtxModeVal);
        int sampleRate = (it != MODE_INFO.end()) ? it->second.sampleRate : 12000;

        return Napi::Number::New(env, sampleRate);
    }

    // Get transmission duration for a mode
    Napi::Value WSJTXLibWrapper::GetTransmissionDuration(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 1 || !info[0].IsNumber())
        {
            Napi::TypeError::New(env, "Expected mode number").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        wsjtxMode wsjtxModeVal = ConvertToWSJTXMode(mode);

        auto it = MODE_INFO.find(wsjtxModeVal);
        double duration = (it != MODE_INFO.end()) ? it->second.duration : 60.0;

        return Napi::Number::New(env, duration);
    }

    // Helper functions
    wsjtxMode ConvertToWSJTXMode(int mode)
    {
        return static_cast<wsjtxMode>(mode);
    }

    void WSJTXLibWrapper::ValidateMode(Napi::Env env, int mode)
    {
        if (mode < 0 || mode > WSPR)
        {
            throw std::invalid_argument("Invalid mode value");
        }
    }

    void WSJTXLibWrapper::ValidateFrequency(Napi::Env env, int frequency)
    {
        if (frequency < 0 || frequency > 30000000)
        { // 30 MHz max
            throw std::invalid_argument("Invalid frequency value");
        }
    }

    void WSJTXLibWrapper::ValidateThreads(Napi::Env env, int threads)
    {
        if (threads < 1 || threads > 16)
        {
            throw std::invalid_argument("Thread count must be between 1 and 16");
        }
    }

    void WSJTXLibWrapper::ValidateMessage(Napi::Env env, const std::string &message)
    {
        if (message.empty() || message.length() > 22)
        {
            throw std::invalid_argument("Message must be 1-22 characters long");
        }
    }

    std::vector<float> WSJTXLibWrapper::ConvertToFloatArray(Napi::Env env, const Napi::Value& value)
    {
        Napi::Float32Array array = value.As<Napi::Float32Array>();
        size_t length = array.ElementLength();
        float *data = array.Data();

        return WsjTxVector(data, data + length);
    }

    std::vector<short int> WSJTXLibWrapper::ConvertToIntArray(Napi::Env env, const Napi::Value& value)
    {
        Napi::Int16Array array = value.As<Napi::Int16Array>();
        size_t length = array.ElementLength();
        int16_t *data = array.Data();

        return IntWsjTxVector(data, data + length);
    }

    Napi::Object WSJTXLibWrapper::CreateWSJTXMessage(Napi::Env env, const WsjtxMessage &msg)
    {
        Napi::Object result = Napi::Object::New(env);

        result.Set("text", Napi::String::New(env, msg.msg));
        result.Set("snr", Napi::Number::New(env, msg.snr));
        result.Set("deltaTime", Napi::Number::New(env, msg.dt));
        result.Set("deltaFrequency", Napi::Number::New(env, msg.freq));
        result.Set("timestamp", Napi::Number::New(env,
                                                  msg.hh * 3600 + msg.min * 60 + msg.sec));
        result.Set("sync", Napi::Number::New(env, msg.sync));

        return result;
    }

    // Async Workers Implementation

    // Base async worker class
    AsyncWorkerBase::AsyncWorkerBase(Napi::Function &callback, wsjtx_lib *lib)
        : Napi::AsyncWorker(callback), lib_(lib) {}

    // Decode Worker
    DecodeWorker::DecodeWorker(Napi::Function &callback, wsjtx_lib *lib,
                               wsjtxMode mode, const std::vector<float> &audioData,
                               int frequency, int threads)
        : AsyncWorkerBase(callback, lib), mode_(mode), floatData_(audioData),
          frequency_(frequency), threads_(threads), useFloat_(true) {}

    DecodeWorker::DecodeWorker(Napi::Function &callback, wsjtx_lib *lib,
                               wsjtxMode mode, const std::vector<short int> &audioData,
                               int frequency, int threads)
        : AsyncWorkerBase(callback, lib), mode_(mode), intData_(audioData),
          frequency_(frequency), threads_(threads), useFloat_(false) {}

    void DecodeWorker::Execute()
    {
        try
        {
            if (useFloat_)
            {
                std::vector<float> data = floatData_; // Copy for thread safety
                lib_->decode(mode_, data, frequency_, threads_);
            }
            else
            {
                std::vector<short int> data = intData_; // Copy for thread safety
                lib_->decode(mode_, data, frequency_, threads_);
            }
        }
        catch (const std::exception &e)
        {
            SetError(e.what());
        }
    }

    void DecodeWorker::OnOK()
    {
        Napi::Env env = Env();
        Callback().Call({env.Null(), Napi::Boolean::New(env, true)});
    }

    // Encode Worker
    EncodeWorker::EncodeWorker(Napi::Function &callback, wsjtx_lib *lib,
                               wsjtxMode mode, const std::string &message,
                               int frequency, int threads)
        : AsyncWorkerBase(callback, lib), mode_(mode), message_(message),
          frequency_(frequency), threads_(threads) {}

    void EncodeWorker::Execute()
    {
        try
        {
            std::string messageSend;
            audioData_ = lib_->encode(mode_, frequency_, message_, messageSend);
            messageSent_ = messageSend;
        }
        catch (const std::exception &e)
        {
            SetError(e.what());
        }
    }

    void EncodeWorker::OnOK()
    {
        Napi::Env env = Env();

        // Create Float32Array for audio data
        Napi::Float32Array audioArray = Napi::Float32Array::New(env, audioData_.size());
        std::copy(audioData_.begin(), audioData_.end(), audioArray.Data());

        Napi::Object result = Napi::Object::New(env);
        result.Set("audioData", audioArray);
        result.Set("messageSent", Napi::String::New(env, messageSent_));

        Callback().Call({env.Null(), result});
    }

    // WSPR Decode Worker
    WSPRDecodeWorker::WSPRDecodeWorker(Napi::Function &callback, wsjtx_lib *lib,
                                       const std::vector<std::complex<float>> &iqData,
                                       const decoder_options &options)
        : AsyncWorkerBase(callback, lib), iqData_(iqData), options_(options) {}

    void WSPRDecodeWorker::Execute()
    {
        try
        {
            std::vector<std::complex<float>> data = iqData_; // Copy for thread safety
            results_ = lib_->wspr_decode(data, options_);
        }
        catch (const std::exception &e)
        {
            SetError(e.what());
        }
    }

    void WSPRDecodeWorker::OnOK()
    {
        Napi::Env env = Env();

        Napi::Array resultsArray = Napi::Array::New(env, results_.size());

        for (size_t i = 0; i < results_.size(); i++)
        {
            const auto &result = results_[i];
            Napi::Object obj = Napi::Object::New(env);

            obj.Set("frequency", Napi::Number::New(env, result.freq));
            obj.Set("sync", Napi::Number::New(env, result.sync));
            obj.Set("snr", Napi::Number::New(env, result.snr));
            obj.Set("deltaTime", Napi::Number::New(env, result.dt));
            obj.Set("drift", Napi::Number::New(env, result.drift));
            obj.Set("jitter", Napi::Number::New(env, result.jitter));
            obj.Set("message", Napi::String::New(env, result.message));
            obj.Set("callsign", Napi::String::New(env, result.call));
            obj.Set("locator", Napi::String::New(env, result.loc));
            obj.Set("power", Napi::String::New(env, result.pwr));
            obj.Set("cycles", Napi::Number::New(env, result.cycles));

            resultsArray[i] = obj;
        }

        Callback().Call({env.Null(), resultsArray});
    }

    // AudioConvertWorker implementation
    void AudioConvertWorker::Execute()
    {
        if (fromFloat_)
        {
            if (target_ == Target::Float32)
            {
                // No-op copy
                floatOut_ = floatInput_;
            }
            else
            {
                intOut_.resize(floatInput_.size());
                for (size_t i = 0; i < floatInput_.size(); ++i)
                {
                    float v = floatInput_[i];
                    // Clamp then scale
                    if (v > 1.0f) v = 1.0f;
                    else if (v < -1.0f) v = -1.0f;
                    intOut_[i] = static_cast<short int>(std::max(-32768, std::min(32767, static_cast<int>(std::lround(v * 32768.0f)))));
                }
            }
        }
        else
        {
            if (target_ == Target::Int16)
            {
                // No-op copy
                intOut_ = intInput_;
            }
            else
            {
                floatOut_.resize(intInput_.size());
                for (size_t i = 0; i < intInput_.size(); ++i)
                {
                    floatOut_[i] = static_cast<float>(intInput_[i]) / 32768.0f;
                }
            }
        }
    }

    void AudioConvertWorker::OnOK()
    {
        Napi::Env env = Env();
        if (target_ == Target::Float32)
        {
            Napi::Float32Array out = Napi::Float32Array::New(env, floatOut_.size());
            std::copy(floatOut_.begin(), floatOut_.end(), out.Data());
            Callback().Call({ env.Null(), out });
        }
        else
        {
            Napi::Int16Array out = Napi::Int16Array::New(env, intOut_.size());
            std::copy(intOut_.begin(), intOut_.end(), out.Data());
            Callback().Call({ env.Null(), out });
        }
    }

    // Module initialization
    Napi::Object Init(Napi::Env env, Napi::Object exports)
    {
        return WSJTXLibWrapper::Init(env, exports);
    }

    NODE_API_MODULE(wsjtx_lib, Init)

} // namespace wsjtx_nodejs
