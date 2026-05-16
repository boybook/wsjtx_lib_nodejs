#pragma once
#include <vector>
#include <complex>

void subtract_signal(std::vector<std::complex<float>> &iqdat,
					 long np,
					 float f0,
					 int shift,
					 float drift,
					 unsigned char *channel_symbols);
void subtract_signal2(std::vector<std::complex<float>> &iqdat,
					  long np,
					  float f0,
					  int shift,
					  float drift,
					  unsigned char *channel_symbols);
int wspr_decode(std::vector<std::complex<float>> &iqdat,
				int samples,
				decoder_options options,
				std::vector<decoder_results> &decodes,
				int threads);