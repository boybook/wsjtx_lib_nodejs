#pragma once
#include <vector>
#include "DataBuffer.h"
#include "commons.h"
#include "fortran_interface.h"
#include "wsjtx_lib.h"

void wsjtx_set_message_queue(DataQueue<WsjtxMessage>* q);

class wstjx_decode
{
  public:
	void decode(wsjtxMode mode, WsjTxVector &audiosamples, int freq, int threads = 1);
	void decode(wsjtxMode mode, IntWsjTxVector &audiosamples, int freq, int threads = 1);
	void setDxInfo(const std::string& call, const std::string& grid);
	void setStationInfo(const std::string& myCall, const std::string& myGrid,
		const std::string& dxCall, const std::string& dxGrid);
	void setDecodeRange(int low, int high, int tol);
	void setDecodeControls(bool apDecode, int decodeDepth, int txFrequency, int qsoProgress);
  private:
	void decode_msk144(const IntWsjTxVector &audiosamples, int freq, int threads);
	params_t params;
	DataBuffer<float> samplebuffer;
	dec_data_t dec_data;
	int nfa_ = 200, nfb_ = 4000, ntol_ = 20;
	bool ap_decode_ = true;
	int decode_depth_ = 1;
	int tx_frequency_ = 0;
	int qso_progress_ = 0;
	std::string my_call_, my_grid_;
	std::string dx_call_, dx_grid_;
};
