/*
 *  libzplay - windows ( WIN32 ) multimedia library for playing mp3, mp2, ogg, wav, flac and raw PCM streams
 *  "Beat per minute" detection class ( WIN32 ) - FFT + peak detection
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


#ifndef _W_BPM_DETECT3_H_
#define _W_BPM_DETECT3_H_

#include "wbpmdetect.h"
#include "wfft.h"

#define BPM_DETECT3_REAL REAL


typedef struct {
	BPM_DETECT3_REAL *BackwardAmplitude;
	BPM_DETECT3_REAL *ForwardAmplitude;
	BPM_DETECT3_REAL *PeakAmplitude;
	BPM_DETECT3_REAL SumBackwardAmplitude;
	BPM_DETECT3_REAL SumForwardAmplitude;
	BPM_DETECT3_REAL SumPeakAmplitude;

	BPM_DETECT3_REAL ForwardTreshold;
	BPM_DETECT3_REAL BackwardTreshold;

	unsigned int Up;
	unsigned int Down;
	unsigned int PossiblePeak;
	unsigned int PeakPos;
	unsigned int LastSamplePos;
	unsigned int BPMCandidate;
	BPM_DETECT3_REAL LastPeakAvgAmplitude;
	BPM_DETECT3_REAL LastBackwardAvgAmplitude;
	BPM_DETECT3_REAL LastForwardAvgAmplitude;


	unsigned int BackwardWindow;
	unsigned int ForwardWindow;
	unsigned int PeakWindow;
	unsigned int ShiftWindow;
	unsigned int BackwardIndex;
	unsigned int ForwardIndex;
	unsigned int PeakIndex;
	unsigned int ShiftIndex;
	unsigned int BackwardLoad;
	unsigned int ForwardLoad;
	unsigned int PeakLoad;
	unsigned int SamplePos;
	unsigned int HistoryBPMPos;
	unsigned int *BPMHistoryHit;
	unsigned int BPM;
} SUBBAND3;



class WBPMDetect3 : public WBPMDetect {
public:
	WBPMDetect3();
	~WBPMDetect3();

	int Initialize(unsigned int nSampleRate, unsigned int nChannel);
	int SetFrequencyBand(unsigned int nLowLimit, unsigned int nHighLimit);
	unsigned int NumOfSamples();
	int PutSamples(short *pSamples, unsigned int nSampleNum);
	unsigned int GetBPM(); 
	void  Release(); 

private:
	unsigned int c_fReady;

	// subbands array
	SUBBAND3 *c_pSubBand;
	// array of amplitudes returned from FFT analyse
	BPM_DETECT3_REAL *c_Amplitudes;
	// array of samples sent to FFT analyse
	BPM_DETECT3_REAL *c_Samples;

	BPM_DETECT3_REAL *c_pflWindow;			// FFT window
	int *c_pnIp;				// work area for bit reversal
	BPM_DETECT3_REAL *c_pflw;				// cos/sin table
	unsigned int c_nChannel;
	unsigned int c_nSampleRate;


	// number of subbands with detected BPM
	unsigned int c_nSubbandBPMDetected;

	int AllocateInternalMemory(unsigned int nSampleRate);
	int FreeInternalMemory();

};








#endif
