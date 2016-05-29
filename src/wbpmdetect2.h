/*
 *  libzplay - windows ( WIN32 ) multimedia library for playing mp3, mp2, ogg, wav, flac and raw PCM streams
 *  "Beat per minute" detection class ( WIN32 ) - wavelets + autocorrelation ( NOT WORKING FOR NOW )
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


#ifndef _W_BPM_DETECT2_H_
#define _W_BPM_DETECT2_H_

#include "wbpmdetect.h"
#include "wwavelet.h"

#define BPM_DETECT2_REAL float


typedef struct {
	BPM_DETECT2_REAL *Buffer;
	unsigned int BufferLoad;
	unsigned int *BPMHistoryHit; 
	unsigned int BPM;
} SUBBAND2;



class WBPMDetect2 : public WBPMDetect {
public:
	WBPMDetect2();
	~WBPMDetect2();
	int Initialize(unsigned int nSampleRate, unsigned int nChannel);
	int SetFrequencyBand(unsigned int nLowLimit, unsigned int nHighLimit);
	unsigned int NumOfSamples();
	int PutSamples(short *pSamples, unsigned int nSampleNum);
	unsigned int GetBPM(); 
	void  Release();   

private:
	WWavelet *wavelet;

	unsigned int c_fReady;
	unsigned int *c_pnOffset;
	// subbands array
	SUBBAND2 *c_pSubBand;
	// array of samples sent to FFT analyse
	BPM_DETECT2_REAL *c_Samples;

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

	int AllocateInternalMemory();
	int FreeInternalMemory();



};








#endif
