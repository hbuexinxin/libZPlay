/*
 *  libzplay - windows ( WIN32 ) multimedia library for playing mp3, mp2, ogg, wav, flac and raw PCM streams
 *  "Beat per minute" detection class ( WIN32 ) - FFT + autocorrelation
 *
 *  Copyright (C) 2003-2009 Zoran Cindori ( zcindori@inet.hr )
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * ver: 1.14
 * date: 15. April, 2010.
 *
 *
*/


#ifndef _W_BPM_DETECT1_H_
#define _W_BPM_DETECT1_H_

#include "wfft.h"
#include "wbpmdetect.h"

#define BPM_DETECT1_REAL REAL

typedef struct {
	BPM_DETECT1_REAL *Buffer;
	unsigned int BufferLoad;
	unsigned int *BPMHistoryHit; 
	unsigned int BPM;
} SUBBAND1;



class WBPMDetect1 : public WBPMDetect {
public:
	WBPMDetect1();
	~WBPMDetect1();
	int Initialize(unsigned int nSampleRate, unsigned int nChannel);
	int SetFrequencyBand(unsigned int nLowLimit, unsigned int nHighLimit);
	unsigned int NumOfSamples();
	int PutSamples(short *pSamples, unsigned int nSampleNum);
	unsigned int GetBPM(); 
	void  Release();   

private:
	unsigned int c_nLowLimitIndex;
	unsigned int c_nHighLimitIndex;
	unsigned int c_nBandSizeIndex;


	unsigned int c_fReady;
	unsigned int *c_pnOffset;
	// subbands array
	SUBBAND1 *c_pSubBand;
	// array of amplitudes returned from FFT analyse
	BPM_DETECT1_REAL *c_Amplitudes;
	// array of samples sent to FFT analyse
	BPM_DETECT1_REAL *c_Samples;

	BPM_DETECT1_REAL *c_pflWindow;			// FFT window
	int *c_pnIp;				// work area for bit reversal
	BPM_DETECT1_REAL *c_pflw;				// cos/sin table
	unsigned int c_nChannel;
	unsigned int c_nSampleRate;

	unsigned int c_nBufferSize;
	unsigned int c_nWindowSize;
	unsigned int c_nOverlapSuccessSize;
	unsigned int c_nOverlapFailSize;
	unsigned int c_nOverlapSuccessStart;
	unsigned int c_nOverlapFailStart;


	// number of subbands with detected BPM
	unsigned int c_nSubbandBPMDetected;

	BPM_DETECT1_REAL c_sqrtFFTPoints;


	int AllocateInternalMemory();
	int FreeInternalMemory();

};








#endif
