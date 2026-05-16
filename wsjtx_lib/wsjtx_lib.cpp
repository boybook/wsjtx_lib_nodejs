#include "wsjtx_lib.h"
#include "wsjtx_decode.h"
#include "wsjtx_encode.h"
#include "constants.h"
#include <memory>
#include <fftw3.h>

static int s_Test = 0;
int wsjtx_libTest() { return ++s_Test; }

wsjtx_lib::wsjtx_lib() { fftwf_init_threads(); }

void wsjtx_lib::setDxCall(const std::string& call) { dx_call_ = call; }
void wsjtx_lib::setDxGrid(const std::string& grid) { dx_grid_ = grid; }

void wsjtx_lib::setDecodeRange(int lowFreq, int highFreq, int tolerance)
{
	decode_low_  = lowFreq;
	decode_high_ = highFreq;
	decode_tol_  = tolerance;
}

void wsjtx_lib::setDecodeStationInfo(const std::string& myCall, const std::string& myGrid,
	const std::string& dxCall, const std::string& dxGrid)
{
	my_call_ = myCall;
	my_grid_ = myGrid;
	dx_call_ = dxCall;
	dx_grid_ = dxGrid;
}

void wsjtx_lib::setDecodeControls(bool apDecode, int decodeDepth, int txFrequency, int qsoProgress)
{
	ap_decode_ = apDecode;
	decode_depth_ = decodeDepth < 1 ? 1 : decodeDepth;
	tx_frequency_ = txFrequency;
	qso_progress_ = qsoProgress < 0 ? 0 : qsoProgress;
}

bool wsjtx_lib::pullMessage(WsjtxMessage &msg) { return messageQueue_.pull(msg); }

void wsjtx_lib::decode(wsjtxMode mode, WsjTxVector &audiosamples, int freq, int thread)
{
	std::unique_ptr<wstjx_decode> ptr = std::make_unique<wstjx_decode>();
	ptr->setStationInfo(my_call_, my_grid_, dx_call_, dx_grid_);
	ptr->setDecodeRange(decode_low_, decode_high_, decode_tol_);
	ptr->setDecodeControls(ap_decode_, decode_depth_, tx_frequency_, qso_progress_);
	wsjtx_set_message_queue(&messageQueue_);
	ptr->decode(mode, audiosamples, freq, thread);
	wsjtx_set_message_queue(nullptr);
}

void wsjtx_lib::decode(wsjtxMode mode, IntWsjTxVector &audiosamples, int freq, int thread)
{
	std::unique_ptr<wstjx_decode> ptr = std::make_unique<wstjx_decode>();
	ptr->setStationInfo(my_call_, my_grid_, dx_call_, dx_grid_);
	ptr->setDecodeRange(decode_low_, decode_high_, decode_tol_);
	ptr->setDecodeControls(ap_decode_, decode_depth_, tx_frequency_, qso_progress_);
	wsjtx_set_message_queue(&messageQueue_);
	ptr->decode(mode, audiosamples, freq, thread);
	wsjtx_set_message_queue(nullptr);
}

int wspr_decode(std::vector<std::complex<float>> &iqdat, int samples,
	decoder_options options, std::vector<struct decoder_results> &decodes, int threads);

std::vector<struct decoder_results> wsjtx_lib::wspr_decode(WsjtxIQSampleVector &iqsignal, decoder_options options)
{
	std::vector<decoder_results> results;
	::wspr_decode(iqsignal, iqsignal.size(), options, results, 4);
	return results;
}

std::vector<float> wsjtx_lib::encode(wsjtxMode mode, int frequency, std::string message, std::string &messagesend)
{
	switch (mode) {
	case FT8: {
		auto ptr = std::make_unique<wsjtx_encode>();
		return ptr->encode_ft8(mode, frequency, message, messagesend);
	}
	case FT4: {
		auto ptr = std::make_unique<wsjtx_encode>();
		return ptr->encode_ft4(mode, frequency, message, messagesend);
	}
	case MSK144: {
		auto ptr = std::make_unique<wsjtx_encode>();
		return ptr->encode_msk144(mode, frequency, message, messagesend);
	}
	default: return {};
	}
}
