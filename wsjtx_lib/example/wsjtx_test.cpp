#include <iostream>
#include "wave.h"
#include <wsjtx_lib.h>
#include <fftw3.h>
#include <memory.h>
#include <chrono>
#include "date.h"

using namespace std;

IntWsjTxVector audioSignal;
WsjtxIQSampleVector iqdat;

wsjtx_lib decoder;

//***************************************************************************
unsigned long readc2file(char *ptr_to_infile, WsjtxIQSampleVector &iqdat,
						 double *freq, int *wspr_type)
{
	float *buffer;
	double dfreq;
	int i, ntrmin;
	char c2file[15];
	size_t nr;
	FILE *fp;

	iqdat.clear();
	iqdat.resize(45000);
	fp = fopen(ptr_to_infile, "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "Cannot open data file '%s'\n", ptr_to_infile);
		return 1;
	}
	nr = fread(c2file, sizeof(char), 14, fp);
	nr = fread(&ntrmin, sizeof(int), 1, fp);
	nr = fread(&dfreq, sizeof(double), 1, fp);
	*freq = dfreq;

	nr = fread(iqdat.data(), sizeof(float), 2 * 45000, fp);
	fclose(fp);

	*wspr_type = ntrmin;

	for (i = 0; i < 45000; i++)
	{
		iqdat[i].imag(iqdat[i].imag() * -1.0);
	}


	if (nr == 2 * 45000)
	{
		return (unsigned long)nr / 2;
	}
	else
	{
		return 1;
	}
}

// Possible PATIENCE options: FFTW_ESTIMATE, FFTW_ESTIMATE_PATIENT,
// FFTW_MEASURE, FFTW_PATIENT, FFTW_EXHAUSTIVE
#define PATIENCE FFTW_ESTIMATE
fftwf_plan PLAN1, PLAN2, PLAN3;

//***************************************************************************
unsigned long readwavfile(char *ptr_to_infile, int ntrmin, WsjtxIQSampleVector &iqdat)
{
	size_t i, j, npoints, nr;
	int nfft1, nfft2, nh2, i0;
	double df;

	nfft2 = 46080; //this is the number of downsampled points that will be returned
	nh2 = nfft2 / 2;

	if (ntrmin == 2)
	{
		nfft1 = nfft2 * 32; //need to downsample by a factor of 32
		df = 12000.0 / nfft1;
		i0 = 1500.0 / df + 0.5;
		npoints = 114 * 12000;
	}
	else if (ntrmin == 15)
	{
		nfft1 = nfft2 * 8 * 32;
		df = 12000.0 / nfft1;
		i0 = (1500.0 + 112.5) / df + 0.5;
		npoints = 8 * 114 * 12000;
	}
	else
	{
		fprintf(stderr, "This should not happen\n");
		return 1;
	}

	float *realin;
	fftwf_complex *fftin, *fftout;

	FILE *fp;
	short int *buf2;

	fp = fopen(ptr_to_infile, "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "Cannot open data file '%s'\n", ptr_to_infile);
		return 1;
	}

	buf2 = (short int *)calloc(npoints, sizeof(short int));
	nr = fread(buf2, 2, 22, fp);	  //Read and ignore header
	nr = fread(buf2, 2, npoints, fp); //Read raw data
	fclose(fp);
	if (nr == 0)
	{
		fprintf(stderr, "No data in file '%s'\n", ptr_to_infile);
		return 1;
	}

	realin = (float *)fftwf_malloc(sizeof(float) * nfft1);
	fftout = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * (nfft1 / 2 + 1));
	PLAN1 = fftwf_plan_dft_r2c_1d(nfft1, realin, fftout, PATIENCE);

	for (i = 0; i < npoints; i++)
	{
		realin[i] = buf2[i] / 32768.0;
	}

	for (i = npoints; i < (size_t)nfft1; i++)
	{
		realin[i] = 0.0;
	}
	free(buf2);

	fftwf_execute(PLAN1);
	fftwf_free(realin);

	fftin = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * nfft2);

	for (i = 0; i < (size_t)nfft2; i++)
	{
		j = i0 + i;
		if (i > (size_t)nh2)
			j = j - nfft2;
		fftin[i][0] = fftout[j][0];
		fftin[i][1] = fftout[j][1];
	}

	fftwf_free(fftout);
	fftout = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * nfft2);
	PLAN2 = fftwf_plan_dft_1d(nfft2, fftin, fftout, FFTW_BACKWARD, PATIENCE);
	fftwf_execute(PLAN2);

	for (i = 0; i < (size_t)nfft2; i++)
	{
		complex<float> iq;
		iq.real(fftout[i][0] / 1000.0);
		iq.imag(fftout[i][1] / 1000.0);
		iqdat.push_back(iq);
	}

	fftwf_free(fftin);
	fftwf_free(fftout);
	return nfft2;
}


int main(int argc, char *argv[])
{
	auto startTime = std::chrono::high_resolution_clock::now();
	auto today = date::floor<date::days>(startTime);
	
	int num_samples = 15 * 48000;
	int sample_rate = 48000;
	WsjtxMessage msg;

	audioSignal.resize(15 * 48000 * 2);	
	for (int i =0 ; i < 2; i++)
	{
		num_samples = 15 * 48000;
		load_wav_int((short int *)audioSignal.data(), &num_samples, &sample_rate, "/home/pi/samples/FT8/210703_133430.wav");
		decoder.decode(FT8, audioSignal, 1000, 4);
		load_wav_int((short int *)audioSignal.data(), &num_samples, &sample_rate, "/home/pi/samples/FT4/000000_000002.wav");
		decoder.decode(FT4, audioSignal, 1000, 4);
		while (decoder.pullMessage(msg))
		{
			printf("hh %d min %d s %d, snr %d dt %f, freq %d, message %s, \n",msg.hh,
						  msg.min,
						  msg.sec,
						  msg.snr,
						  msg.dt,
						  msg.freq,
						  msg.msg.c_str());
		}
	}

	decoder_options options;

	double freq;
	int wspr_type = 2;
	char filestr[] = "./150426_0918.wav";
	std::vector<decoder_results> results;

	unsigned int points = readwavfile(filestr, wspr_type,iqdat);
	options.freq = 70000000;
	strcpy(options.rcall, "PA0PHH");
	strcpy(options.rloc, "JO22");

	//wspr_decode(iqdat, iqdat.size(), options, results, 4);
	results = decoder.wspr_decode(iqdat, options);
	printf("UTC                   dB    DT     Freq     Drift    Call    dbm\n");
	auto now = std::chrono::system_clock::now();
	for (auto col : results)
	{
		
		cout << date::make_time(now - today) << "   ";
		printf("%2.0f", col.snr);
		cout << "    ";
		printf("%2.0f", col.dt);
		cout << "    ";
		printf("%1.0f", col.freq);
		cout << "    ";
		printf("%2.0f", col.drift);
		cout << "  ";
		printf("%s", col.call);
		cout << "  ";
		printf("%s", col.loc);
		cout << "    ";
		printf("%s", col.pwr);
		cout << "\n";
	}
	return 0;
}