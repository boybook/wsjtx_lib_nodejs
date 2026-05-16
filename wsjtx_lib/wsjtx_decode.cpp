#include "wsjtx_lib.h"
#include "wsjtx_decode.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>
#include <string>
#include <fftw3.h>
#include <ctime>
#include <time.h>

static thread_local DataQueue<WsjtxMessage>* s_currentQueue = nullptr;
void wsjtx_set_message_queue(DataQueue<WsjtxMessage>* q) { s_currentQueue = q; }

namespace {
constexpr int MSK144_SAMPLE_RATE = 12000;
constexpr int MSK144_BLOCK_SIZE = 7168;
constexpr int MSK144_STEP_SIZE = MSK144_BLOCK_SIZE / 2;
constexpr int MSK144_MAX_12K_SAMPLES = 30 * MSK144_SAMPLE_RATE;

int current_nutc()
{
	auto now = std::chrono::system_clock::now();
	time_t tt = std::chrono::system_clock::to_time_t(now);
	tm local_tm = *localtime(&tt);
	return local_tm.tm_hour * 10000 + local_tm.tm_min * 100 + local_tm.tm_sec;
}

std::string trim_copy(const std::string &s)
{
	const auto first = s.find_first_not_of(" \t\r\n\0", 0);
	if (first == std::string::npos) return {};
	const auto last = s.find_last_not_of(" \t\r\n\0");
	return s.substr(first, last - first + 1);
}

short int float_to_i16(float v)
{
	const int x = static_cast<int>(std::lround(std::max(-1.0f, std::min(1.0f, v)) * 32768.0f));
	return static_cast<short int>(std::max(-32768, std::min(32767, x)));
}

IntWsjTxVector msk144_samples_from_float(const WsjTxVector &samples)
{
	IntWsjTxVector out;
	if (samples.size() > MSK144_MAX_12K_SAMPLES && samples.size() >= 4) {
		out.reserve(samples.size() / 4);
		for (size_t i = 0; i + 3 < samples.size(); i += 4) {
			const float avg = 0.25f * (samples[i] + samples[i + 1] + samples[i + 2] + samples[i + 3]);
			out.push_back(float_to_i16(avg));
		}
	} else {
		out.reserve(samples.size());
		for (float sample : samples) out.push_back(float_to_i16(sample));
	}
	return out;
}

IntWsjTxVector msk144_samples_from_int16(const IntWsjTxVector &samples)
{
	IntWsjTxVector out;
	if (samples.size() > MSK144_MAX_12K_SAMPLES && samples.size() >= 4) {
		out.reserve(samples.size() / 4);
		for (size_t i = 0; i + 3 < samples.size(); i += 4) {
			const int avg = static_cast<int>(std::lround(
				0.25 * (samples[i] + samples[i + 1] + samples[i + 2] + samples[i + 3])));
			out.push_back(static_cast<short int>(std::max(-32768, std::min(32767, avg))));
		}
	} else {
		out = samples;
	}
	return out;
}

void copy_fortran_call(char *dst, size_t len, const std::string &src)
{
	std::memset(dst, ' ', len);
	std::memcpy(dst, src.data(), std::min(len, src.size()));
}

std::string fortran_line_to_string(const char *line, size_t len)
{
	const void *zero = std::memchr(line, '\0', len);
	const size_t used = zero ? static_cast<const char *>(zero) - line : len;
	return trim_copy(std::string(line, used));
}

bool parse_msk144_line(const std::string &line, WsjtxMessage &msg)
{
	if (line.size() < 24 || line.find("DecodeFinished") != std::string::npos) return false;
	try {
		const int nutc = std::stoi(line.substr(0, 6));
		const int snr = std::stoi(line.substr(6, 4));
		const float dt = std::stof(line.substr(10, 5));
		const int freq = std::stoi(line.substr(15, 5));
		const std::string text = trim_copy(line.substr(24, 37));
		if (text.empty()) return false;
		msg = WsjtxMessage(nutc / 10000, (nutc / 100) % 100, nutc % 100, snr, dt, freq, text);
		return true;
	} catch (...) {
		return false;
	}
}
}

extern "C" {

void wsjtx_decoded_(int *nutc, int *snr, float *dt, int *freq, char *decoded, int len)
{
	char message[38];
	std::strncpy(message, decoded, 37);
	message[37] = '\0';
	for (int i = 37; i != 0; i--) {
		if (message[i] == ' ' || message[i] == '\0') message[i] = '\0';
		else break;
	}
	if (!strstr(message, "DecodeFinished") && s_currentQueue) {
		s_currentQueue->push(WsjtxMessage(*nutc / 10000, (*nutc / 100) % 100, *nutc % 100, *snr, *dt, *freq, std::string(message)));
	}
}

void wsjtx_decoded_fst4_(int *nutc, float *sync, int *snr, float *dt, float *freq, char *decoded, int len)
{
	char message[38];
	std::strncpy(message, decoded, 37);
	message[37] = '\0';
	for (int i = 37; i != 0; i--) {
		if (message[i] == ' ' || message[i] == '\0') message[i] = '\0';
		else break;
	}
	if (!strstr(message, "DecodeFinished") && s_currentQueue) {
		s_currentQueue->push(WsjtxMessage(*nutc / 10000, (*nutc / 100) % 100, *nutc % 100, *snr, *sync, *dt, *freq, std::string(message)));
	}
}
}

void wstjx_decode::setDxInfo(const std::string& call, const std::string& grid) {
	dx_call_ = call;
	dx_grid_ = grid;
}
void wstjx_decode::setStationInfo(const std::string& myCall, const std::string& myGrid,
	const std::string& dxCall, const std::string& dxGrid) {
	my_call_ = myCall;
	my_grid_ = myGrid;
	dx_call_ = dxCall;
	dx_grid_ = dxGrid;
}
void wstjx_decode::setDecodeRange(int low, int high, int tol) { nfa_ = low; nfb_ = high; ntol_ = tol; }
void wstjx_decode::setDecodeControls(bool apDecode, int decodeDepth, int txFrequency, int qsoProgress) {
	ap_decode_ = apDecode;
	decode_depth_ = decodeDepth < 1 ? 1 : decodeDepth;
	tx_frequency_ = txFrequency;
	qso_progress_ = qsoProgress < 0 ? 0 : qsoProgress;
}

void wstjx_decode::decode(wsjtxMode mode, WsjTxVector &audiosamples, int freq, int threads)
{
	if (mode == MSK144) {
		decode_msk144(msk144_samples_from_float(audiosamples), freq, threads);
		return;
	}

	samplebuffer.push(std::move(audiosamples));
	std::memset(&params, 0, sizeof(params));
	params.nmode = 8; params.ntrperiod = 60.0; params.nQSOProgress = qso_progress_;
	params.nfqso = freq; params.nftx = tx_frequency_;
	params.newdat = true; params.npts8 = 74736; params.nfa = nfa_;
	params.nfSplit = 2700; params.nfb = nfb_; params.ntol = ntol_;
	params.kin = 64800; params.nzhsym = 79; params.nsubmode = 0;
	params.nagain = false; params.ndepth = decode_depth_; params.lft8apon = ap_decode_;
	params.lapcqonly = false; params.ljt65apon = true; params.napwid = 75;
	params.ntxmode = 65; params.nmode = 8; params.minw = 0;
	params.nclearave = false; params.minSync = 0; params.emedelay = 0.0;
	params.dttol = 3; params.nlist = 0; params.listutc[0] = '\0';
	params.n2pass = 2; params.nranera = 6; params.naggressive = 0;
	params.nrobust = false; params.nexp_decode = 0;
	if (!my_call_.empty()) std::strncpy(params.mycall, my_call_.c_str(), sizeof(params.mycall) - 1);
	if (!my_grid_.empty()) std::strncpy(params.mygrid, my_grid_.c_str(), sizeof(params.mygrid) - 1);
	if (!dx_call_.empty()) std::strncpy(params.hiscall, dx_call_.c_str(), sizeof(params.hiscall) - 1);
	if (!dx_grid_.empty()) std::strncpy(params.hisgrid, dx_grid_.c_str(), sizeof(params.hisgrid) - 1);
	switch (mode) {
	case FT8: params.nmode = 8; break;
	case FT4: params.nmode = 5; break;
	default: return;
	}
	int nfsample = 12000;
	for (size_t i = 0; i < audiosamples.size(); i++)
		dec_data.d2[i] = (short int)(audiosamples[i] * 32768.0f);
	auto now = std::chrono::system_clock::now();
	time_t tt = std::chrono::system_clock::to_time_t(now);
	tm local_tm = *localtime(&tt);
	params.nutc = local_tm.tm_hour * 10000 + local_tm.tm_min * 100 + local_tm.tm_sec;
	fftwf_plan_with_nthreads(threads);
	multimode_decoder_(dec_data.ss, dec_data.d2, &params, &nfsample);
}

void wstjx_decode::decode(wsjtxMode mode, IntWsjTxVector &audiosamples, int freq, int threads)
{
	if (mode == MSK144) {
		decode_msk144(msk144_samples_from_int16(audiosamples), freq, threads);
		return;
	}

	std::memset(&params, 0, sizeof(params));
	params.nmode = 8; params.ntrperiod = 60.0; params.nQSOProgress = qso_progress_;
	params.nfqso = freq; params.nftx = tx_frequency_;
	params.newdat = true; params.npts8 = 74736; params.nfa = nfa_;
	params.nfSplit = 2700; params.nfb = nfb_; params.ntol = ntol_;
	params.kin = 64800; params.nzhsym = 50; params.nsubmode = 0;
	params.nagain = false; params.ndepth = decode_depth_; params.lft8apon = ap_decode_;
	params.lapcqonly = false; params.ljt65apon = true; params.napwid = 75;
	params.ntxmode = 65;
	switch (mode) {
	case FT8: params.nmode = 8; break;
	case FT4: params.nmode = 5; break;
	default: return;
	}
	params.minw = 0; params.nclearave = false; params.minSync = 0;
	params.emedelay = 0.0; params.dttol = 3; params.nlist = 0;
	params.listutc[0] = '\0'; params.n2pass = 2; params.nranera = 6;
	params.naggressive = 0; params.nrobust = false; params.nexp_decode = 0;
	if (!my_call_.empty()) std::strncpy(params.mycall, my_call_.c_str(), sizeof(params.mycall) - 1);
	if (!my_grid_.empty()) std::strncpy(params.mygrid, my_grid_.c_str(), sizeof(params.mygrid) - 1);
	if (!dx_call_.empty()) std::strncpy(params.hiscall, dx_call_.c_str(), sizeof(params.hiscall) - 1);
	if (!dx_grid_.empty()) std::strncpy(params.hisgrid, dx_grid_.c_str(), sizeof(params.hisgrid) - 1);
	int nfsample = 12000;
	for (size_t i = 0; i < audiosamples.size(); i++)
		dec_data.d2[i] = (short int)audiosamples[i];
	auto now = std::chrono::system_clock::now();
	time_t tt = std::chrono::system_clock::to_time_t(now);
	tm local_tm = *localtime(&tt);
	params.nutc = local_tm.tm_hour * 10000 + local_tm.tm_min * 100 + local_tm.tm_sec;
	fftwf_plan_with_nthreads(threads);
	multimode_decoder_(dec_data.ss, dec_data.d2, &params, &nfsample);
}

void wstjx_decode::decode_msk144(const IntWsjTxVector &audiosamples, int freq, int threads)
{
	if (audiosamples.size() < MSK144_BLOCK_SIZE) return;

	int nutc = current_nutc();
	int ntol = ntol_;
	int nrxfreq = freq;
	int ndepth = decode_depth_;
	bool bshmsg = true;
	bool btrain = false;
	bool bswl = false;
	double pcoeffs[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
	char mycall[12];
	char hiscall[12];
	char datadir[500];
	char line[80];
	short int block[MSK144_BLOCK_SIZE];

	copy_fortran_call(mycall, sizeof(mycall), my_call_);
	copy_fortran_call(hiscall, sizeof(hiscall), dx_call_);
	std::memset(datadir, ' ', sizeof(datadir));
	datadir[0] = '.';

	fftwf_plan_with_nthreads(threads);
	for (size_t pos = 0; pos + MSK144_BLOCK_SIZE <= audiosamples.size(); pos += MSK144_STEP_SIZE) {
		std::copy_n(audiosamples.data() + pos, MSK144_BLOCK_SIZE, block);
		std::memset(line, 0, sizeof(line));
		float tsec = static_cast<float>(pos) / MSK144_SAMPLE_RATE;

		mskrtd_(block, &nutc, &tsec, &ntol, &nrxfreq, &ndepth,
			mycall, hiscall, &bshmsg, &btrain, pcoeffs, &bswl,
			datadir, line, 12, 12, 500, 80);

		WsjtxMessage msg;
		if (s_currentQueue && parse_msk144_line(fortran_line_to_string(line, sizeof(line)), msg)) {
			s_currentQueue->push(msg);
		}
	}
}
