#pragma once
#include <vector>
#include <string>
#include <complex>
#include "DataBuffer.h"

typedef std::vector<float> WsjTxVector;
typedef std::vector<short int> IntWsjTxVector;
typedef std::vector<std::complex<float>> WsjtxIQSampleVector;

class decoder_results
{
  public:
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
};

class decoder_options
{
  public:
	int freq;
	char rcall[13];
	char rloc[7];
	int quickmode;
	int usehashtable;
	int npasses;
	int subtraction;
	decoder_options();
};

enum wsjtxMode { FT8, FT4, JT4, JT65, JT9, FST4, Q65, FST4W, JT65JT9, WSPR, MSK144 };

class WsjtxMessage
{
  public:
	WsjtxMessage(int chh, int cmm, int css, int csnr, float cdt, int cfreq, std::string cmsg)
		: hh{chh}, min{cmm}, sec{css}, snr{csnr}, dt{cdt}, freq{cfreq}, msg{cmsg}, sync{0.0} {};
	WsjtxMessage() : hh{0}, min{0}, sec{0}, snr{0}, freq{0}, dt{0}, msg{""} {};
	WsjtxMessage(int chh, int cmm, int css, int csnr, float csync, float cdt, int cfreq, std::string cmsg)
		: hh{chh}, min{cmm}, sec{css}, snr{csnr}, sync{csync}, dt{cdt}, freq{cfreq}, msg{cmsg} {};
	int hh, min, sec, snr;
	float sync, dt;
	int freq;
	std::string msg;
};

class wsjtx_lib
{
  public:
	wsjtx_lib();
	void decode(wsjtxMode mode, WsjTxVector &audiosamples, int freq, int thread = 1);
	void decode(wsjtxMode mode, IntWsjTxVector &audiosamples, int freq, int thread = 1);
	std::vector<float> encode(wsjtxMode mode, int frequency, std::string message, std::string &messagesend);
	bool pullMessage(WsjtxMessage &msg);
	std::vector<decoder_results> wspr_decode(WsjtxIQSampleVector &iqsignal, decoder_options options);
	void setDxCall(const std::string& call);
	void setDxGrid(const std::string& grid);
	void setDecodeRange(int lowFreq, int highFreq, int tolerance);
	void setDecodeStationInfo(const std::string& myCall, const std::string& myGrid,
		const std::string& dxCall, const std::string& dxGrid);
	void setDecodeControls(bool apDecode, int decodeDepth, int txFrequency, int qsoProgress);
  private:
	std::string my_call_;
	std::string my_grid_;
	std::string dx_call_;
	std::string dx_grid_;
	int decode_low_  = 200;
	int decode_high_ = 4000;
	int decode_tol_  = 20;
	bool ap_decode_ = true;
	int decode_depth_ = 1;
	int tx_frequency_ = 0;
	int qso_progress_ = 0;
	DataQueue<WsjtxMessage> messageQueue_;
};
