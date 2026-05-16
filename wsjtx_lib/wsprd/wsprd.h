/*
 This file is part of program wsprd, a detector/demodulator/decoder
 for the Weak Signal Propagation Reporter (WSPR) mode.

 File name: wsprd.c

 Copyright 2001-2015, Joe Taylor, K1JT

 Much of the present code is based on work by Steven Franke, K9AN,
 which in turn was based on earlier work by K1JT.

 Copyright 2014-2015, Steven Franke, K9AN

 Minor modifications

 Copyright 2016, Guenael Jouchet, VA2GKA

 License: GNU GPL v3

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include "../wsjtx_lib.h"
/* Option & config of decoder (Shared with the wsprd code) */
/*
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
	int freq;		  // Dial frequency
	char rcall[13];   // Callsign of the RX station
	char rloc[7];	 // Locator of the RX station
	int quickmode;	// Decoder option & tweak
	int usehashtable; //  ''
	int npasses;	  //  ''
	int subtraction;  //  ''

	decoder_options();
};
*/
struct cand {
    float  freq;
    float  snr;
    int    shift;
    float  drift;
    float  sync;
};






