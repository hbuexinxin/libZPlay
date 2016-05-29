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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>

#include "debug.h"
#include "wbpmdetect1.h"


#define PI 3.1415926535897932384626433832795029L

// number of fft points for analyse
#define BPM_DETECT_FFT_POINTS1 64


// buffer size in milliseconds
#define BPM_BUFFER_SIZE 4000
// window size in millisecods
#define BPM_WINDOW_SIZE 2000

// overlap size in milliseconds
#define BPM_OVERLAP_SUCCESS_SIZE  2000
#define BPM_OVERLAP_FAIL_SIZE 3500



// number of subbands
#define BPM_NUMBER_OF_SUBBANDS 16


#define BPM_HISTORY_TOLERANCE 1
#define BPM_HISTORY_HIT 5


typedef struct {
	unsigned int BPM;
	unsigned int Hit;
	unsigned int Sum;
} BPM_MOD_STRUCT;



// left safe margim used by autocorrelation
#define BPM_DETECT_MIN_MARGIN 1
// right safe margin used by autocorrelation
#define BPM_DETECT_MAX_MARGIN 1

#define BPM_DETECT_MIN_BPM 55
#define BPM_DETECT_MAX_BPM 125


WBPMDetect1::WBPMDetect1()
{
// initialize
	c_fReady = 0;
	c_pSubBand = 0;
	c_Amplitudes = 0;
	c_Samples = 0;
	c_pflWindow = 0;
	c_pnIp = 0;		
	c_pflw = 0;
	c_pnOffset = 0;
	c_nChannel = 0;
	c_nSampleRate = 0;

	c_nLowLimitIndex = 0;
	c_nHighLimitIndex = BPM_NUMBER_OF_SUBBANDS;
	c_nBandSizeIndex = BPM_NUMBER_OF_SUBBANDS;

}

WBPMDetect1::~WBPMDetect1()
{
	FreeInternalMemory();
}


int WBPMDetect1::Initialize(unsigned int nSampleRate, unsigned int nChannel)
{
	if(nSampleRate == 0 || nChannel == 0)
		return 0;


	c_nBufferSize = (nSampleRate * BPM_BUFFER_SIZE) / (1000 * BPM_DETECT_FFT_POINTS1);
	if(c_nBufferSize == 0)
		c_nBufferSize = 1;

	c_nWindowSize = (nSampleRate * BPM_WINDOW_SIZE) / (1000 * BPM_DETECT_FFT_POINTS1);
	if(c_nWindowSize == 0)
		c_nWindowSize = 1;

	c_nOverlapSuccessSize = (nSampleRate * BPM_OVERLAP_SUCCESS_SIZE) / (1000 * BPM_DETECT_FFT_POINTS1);
	if(c_nOverlapSuccessSize >= c_nBufferSize)
		c_nOverlapSuccessSize = c_nBufferSize - 1;	

	c_nOverlapFailSize = (nSampleRate * BPM_OVERLAP_FAIL_SIZE) / (1000 * BPM_DETECT_FFT_POINTS1);
	if(c_nOverlapFailSize >= c_nBufferSize)
		c_nOverlapFailSize = c_nBufferSize - 1;	

	c_nOverlapSuccessStart = c_nBufferSize - c_nOverlapSuccessSize;
	c_nOverlapFailStart = c_nBufferSize - c_nOverlapFailSize;
	c_nSubbandBPMDetected = 0;

	c_sqrtFFTPoints = sqrt((double) BPM_DETECT_FFT_POINTS1);

	if(c_fReady == 0 || c_nSampleRate != nSampleRate || c_nChannel != nChannel)
	{
		if(AllocateInternalMemory() == 0)
			return 0;

		c_nSampleRate = nSampleRate;
		c_nChannel = nChannel;

		unsigned int i;
		for(i = BPM_DETECT_MIN_BPM - BPM_DETECT_MIN_MARGIN - 1; i <= BPM_DETECT_MAX_BPM + BPM_DETECT_MAX_MARGIN + 1; i++)
			c_pnOffset[i] = (unsigned int) (((double) (60 * c_nSampleRate) / (double) ( i * BPM_DETECT_FFT_POINTS1)) + 0.5);

	}

	unsigned int i;
	for(i = 0; i < BPM_NUMBER_OF_SUBBANDS; i++)
	{
		SUBBAND1 *subband = &c_pSubBand[i];
		subband->BufferLoad = 0;
		subband->BPM = 0;
		memset(subband->BPMHistoryHit, 0, (BPM_DETECT_MAX_BPM + 2) * sizeof(unsigned int));
	}

	return 1;
}


int WBPMDetect1::PutSamples(short *pSamples, unsigned int nSampleNum)
{

	// create samples array

	unsigned int i;
	if(c_nChannel == 2)
	{
		// convert to mono
		for(i = 0; i < nSampleNum; i++)
			c_Samples[i] = ((BPM_DETECT1_REAL) pSamples[2 * i] +  (BPM_DETECT1_REAL) pSamples[2 * i + 1]) * c_pflWindow[i] / 2.0;

	}
	else
	{
		for(i = 0; i < nSampleNum; i++)
			c_Samples[i] = (BPM_DETECT1_REAL) pSamples[i] * c_pflWindow[i];
	}


	// make fft
	rdft(BPM_DETECT_FFT_POINTS1, 1, c_Samples, c_pnIp, c_pflw);

	BPM_DETECT1_REAL amp;
	BPM_DETECT1_REAL re = c_Samples[1] ;
	BPM_DETECT1_REAL im = 0;

	amp = sqrt(re * re + im * im) / c_sqrtFFTPoints;
	c_Amplitudes[BPM_DETECT_FFT_POINTS1 / 2 - 1] = amp * amp;


	for(i = 1; i < BPM_DETECT_FFT_POINTS1 / 2; i++)
	{
		re = c_Samples[i * 2] ;
		im = c_Samples[i * 2 + 1];
		amp = sqrt(re * re + im * im) / c_sqrtFFTPoints;
		c_Amplitudes[i - 1] = amp * amp;

	}

	unsigned int j;
	unsigned int nSubbandSize = BPM_DETECT_FFT_POINTS1 / ( 2 * BPM_NUMBER_OF_SUBBANDS);

	//for(i = 0; i < BPM_NUMBER_OF_SUBBANDS; i++)
	for(i = c_nLowLimitIndex; i < c_nHighLimitIndex; i++)
	{
		SUBBAND1 *subband = &c_pSubBand[i];

		if(subband->BPM)	// we have BPM, don't compute anymore
			continue;
	
		// === CREATE SUBBANDS =================================
		BPM_DETECT1_REAL InstantAmplitude = 0;

		for(j =  i * nSubbandSize; j < (i + 1) * nSubbandSize; j++)
			InstantAmplitude += c_Amplitudes[j];

		InstantAmplitude /= (BPM_DETECT1_REAL) nSubbandSize;

		// =====================================================

		// FILL BUFFER

		subband->Buffer[subband->BufferLoad] = InstantAmplitude;

		subband->BufferLoad++;
		if(subband->BufferLoad < c_nBufferSize)
			continue;

		unsigned int j;
		unsigned int bpm = 0;
		BPM_DETECT1_REAL max_correlation = 0;

		// calculate auto correlation of each BPM in range

		unsigned int k;

		for(j = BPM_DETECT_MIN_BPM - BPM_DETECT_MIN_MARGIN - 1; j <= BPM_DETECT_MAX_BPM + BPM_DETECT_MAX_MARGIN; j++)
		{
			BPM_DETECT1_REAL correlation = 0;
			for(k = 0; k < c_nWindowSize; k++)
				correlation += (subband->Buffer[k] * subband->Buffer[k + c_pnOffset[j]]);

			if(correlation >= max_correlation)
			{
				max_correlation = correlation;	
				bpm = j;
			}
		}

		// mark bpm in history
		if(bpm >= BPM_DETECT_MIN_BPM && bpm <= BPM_DETECT_MAX_BPM)
		{	
			
			unsigned int HistoryHit = subband->BPMHistoryHit[bpm] + subband->BPMHistoryHit[bpm - 1] + subband->BPMHistoryHit[bpm + 1];
			if(HistoryHit)
			{
				if(HistoryHit >= BPM_HISTORY_HIT)
				{
					
					unsigned int Avg = (bpm * subband->BPMHistoryHit[bpm]  + (bpm - 1) * subband->BPMHistoryHit[bpm - 1] + (bpm + 1) * subband->BPMHistoryHit[bpm + 1]) / HistoryHit;
					subband->BPM = Avg;
					c_nSubbandBPMDetected++;
				}
			}

			subband->BPMHistoryHit[bpm]++;


			unsigned int j;
			for(j = 0 ; j < c_nOverlapSuccessSize; j++)
				subband->Buffer[j] = subband->Buffer[j + c_nOverlapSuccessStart];

			subband->BufferLoad = c_nOverlapSuccessSize;
			
		}
		else
		{
			unsigned int j;
			for(j = 0 ; j < c_nOverlapFailSize; j++)
				subband->Buffer[j] = subband->Buffer[j + c_nOverlapFailStart];

			subband->BufferLoad = c_nOverlapFailSize;
		}
	}

	

	if(c_nSubbandBPMDetected == c_nBandSizeIndex)
		return 1;

	return 0;


}


unsigned int WBPMDetect1::GetBPM()
{
	unsigned int i;
	unsigned int j;
	
	for(i = c_nLowLimitIndex; i < c_nHighLimitIndex; i++)
	{
		printf("Subband: %02u   %u\n", i, c_pSubBand[i].BPM);

	}
	

	// get BPM by calculating mod value
	BPM_MOD_STRUCT mod_value[BPM_NUMBER_OF_SUBBANDS];
	memset(mod_value, 0, BPM_NUMBER_OF_SUBBANDS * sizeof(BPM_MOD_STRUCT));

	// search unique values
	unsigned int size = 0;
	for(i = c_nLowLimitIndex; i < c_nHighLimitIndex; i++)
	{
		unsigned int have = 0;
		// check if this value is already in 
		for(j = 0; j < size; j++)
		{
			if(c_pSubBand[i].BPM != 0 && mod_value[j].BPM == c_pSubBand[i].BPM)
			{
				have = 1;
				break;
			}
		}
		// we don't have value in, add new value if value is not 0
		if(have == 0 && c_pSubBand[i].BPM != 0)
		{
			mod_value[size].BPM = c_pSubBand[i].BPM;
			size++;
		}
	}

	// calculate hit values and get value with maximal hit
	unsigned int max_hit = 0;
	unsigned int BPM = 0;
	for(i = 0; i < size; i++)
	{
		for(j = c_nLowLimitIndex; j < c_nHighLimitIndex; j++)
		{
			if(mod_value[i].BPM == c_pSubBand[j].BPM)
			{
				mod_value[i].Hit++;
				mod_value[i].Sum += c_pSubBand[j].BPM;
			}

			if(mod_value[i].BPM + 1 == c_pSubBand[j].BPM)
			{
				mod_value[i].Hit++;
				mod_value[i].Sum += c_pSubBand[j].BPM;
			}

			if(mod_value[i].BPM - 1 == c_pSubBand[j].BPM)
			{
				mod_value[i].Hit++;
				mod_value[i].Sum += c_pSubBand[j].BPM;
			}


			if(mod_value[i].Hit > max_hit)
			{
				max_hit = mod_value[i].Hit;
				BPM = (unsigned int) (((double) mod_value[i].Sum / (double) mod_value[i].Hit) + 0.5);
			}
		}
	}

	return BPM;
}


int WBPMDetect1::AllocateInternalMemory()
{

	c_Amplitudes = (BPM_DETECT1_REAL*) malloc(BPM_DETECT_FFT_POINTS1 / 2 * sizeof(BPM_DETECT1_REAL));
	c_Samples = (BPM_DETECT1_REAL*) malloc(BPM_DETECT_FFT_POINTS1 * sizeof(BPM_DETECT1_REAL));
	c_pflWindow = (BPM_DETECT1_REAL*) malloc(BPM_DETECT_FFT_POINTS1 * sizeof(BPM_DETECT1_REAL));
	c_pnIp = (int*) malloc( ( 2 + (int) sqrt((BPM_DETECT1_REAL) BPM_DETECT_FFT_POINTS1 / 2.0) ) * sizeof(int));
	c_pflw = (BPM_DETECT1_REAL*) malloc(BPM_DETECT_FFT_POINTS1 / 2 * sizeof(BPM_DETECT1_REAL));
	c_pnOffset = (unsigned int*) malloc((BPM_DETECT_MAX_BPM + BPM_DETECT_MAX_MARGIN + 2) * sizeof(unsigned int));

	if(c_Amplitudes == 0 || c_Samples == 0 || c_pflWindow == 0 || c_pnIp == 0 || c_pflw == 0 || c_pnOffset == 0)
	{
		FreeInternalMemory();
		return 0;
	}


	c_pnIp[0] = 0;

	c_pSubBand = (SUBBAND1*) malloc(BPM_NUMBER_OF_SUBBANDS * sizeof(SUBBAND1));
	if(c_pSubBand == 0)
	{
		FreeInternalMemory();
		return 0;
	}

	memset(c_pSubBand, 0, BPM_NUMBER_OF_SUBBANDS * sizeof(SUBBAND1));

	unsigned int i;



	for(i = 0; i < BPM_NUMBER_OF_SUBBANDS; i++)
	{
		c_pSubBand[i].Buffer = (BPM_DETECT1_REAL*) malloc(c_nBufferSize * sizeof(BPM_DETECT1_REAL));
		if(c_pSubBand[i].Buffer == 0)
		{
			FreeInternalMemory();
			return 0;
		}

		memset(c_pSubBand[i].Buffer, 0, c_nBufferSize * sizeof(BPM_DETECT1_REAL));


		c_pSubBand[i].BPMHistoryHit = (unsigned int*) malloc((BPM_DETECT_MAX_BPM + 2) * sizeof(unsigned int));
		if(c_pSubBand[i].BPMHistoryHit == 0)
		{
			FreeInternalMemory();
			return 0;
		}
	}

 
	// create COSINE window

	double n;
	double N = (double) BPM_DETECT_FFT_POINTS1;
	double pi = PI;
	for(i = 0; i < BPM_DETECT_FFT_POINTS1; i++)
	{
		n = (double) i;
		c_pflWindow[i] = (BPM_DETECT1_REAL) ( sin( (pi * n) / (N - 1.0)) ); 
	}


	c_fReady = 1;
	return 1;

}


int WBPMDetect1::FreeInternalMemory()
{
	if(c_Amplitudes)
	{
		free(c_Amplitudes);
		c_Amplitudes = 0;
	}

	if(c_Samples)
	{
		free(c_Samples);
		c_Samples = 0;
	}

	if(c_pflWindow)
	{
		free(c_pflWindow);
		c_pflWindow = 0;
	}

	if(c_pnIp)
	{
		free(c_pnIp);
		c_pnIp = 0;
	}

	if(c_pflw)
	{
		free(c_pflw);
		c_pflw = 0;
	}

	if(c_pnOffset)
	{
		free(c_pnOffset);
		c_pnOffset = 0;
	}


	if(c_pSubBand)
	{

		unsigned int i;
		for(i = 0; i < BPM_NUMBER_OF_SUBBANDS; i++)
		{
			if(c_pSubBand[i].Buffer)
				free(c_pSubBand[i].Buffer);

			if(c_pSubBand[i].BPMHistoryHit)
				free(c_pSubBand[i].BPMHistoryHit);

		}

		free(c_pSubBand);
		c_pSubBand = 0;
	}

	c_fReady = 0;
	return 1;
}


void  WBPMDetect1::Release()
{
	delete this;
}

unsigned int WBPMDetect1::NumOfSamples()
{
	return BPM_DETECT_FFT_POINTS1;
}

int WBPMDetect1::SetFrequencyBand(unsigned int nLowLimit, unsigned int nHighLimit)
{
	// calculate low index
	double subband = (double) c_nSampleRate / (double) (BPM_NUMBER_OF_SUBBANDS * 2);

	c_nLowLimitIndex = (unsigned int) ((double) nLowLimit / (double) subband);
	c_nHighLimitIndex = (unsigned int) ((double) nHighLimit / (double) subband);
	if(c_nHighLimitIndex > BPM_NUMBER_OF_SUBBANDS)
		c_nHighLimitIndex = BPM_NUMBER_OF_SUBBANDS;	
	c_nBandSizeIndex = c_nHighLimitIndex - c_nLowLimitIndex;
	return 1;
}
