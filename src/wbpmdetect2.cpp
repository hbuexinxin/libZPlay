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
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>

#include "debug.h"
#include "wbpmdetect2.h"


// number of fft points for analyse
#define BPM_WAVELET_POINTS 64

#define WAVELET_LEVEL 4

// number of subbands
#define BPM_NUMBER_OF_SUBBANDS 16

#define WAVELET_CHILD_SIZE 4 

// buffer size in milliseconds
#define BPM_BUFFER_SIZE 6000
// window size in millisecods
#define BPM_WINDOW_SIZE 4000

// overlap size in milliseconds
#define BPM_OVERLAP_SUCCESS_SIZE  2000
#define BPM_OVERLAP_FAIL_SIZE 5500




#define BPM_HISTORY_TOLERANCE 1
#define BPM_HISTORY_HIT 2


// left safe margim used by autocorrelation
#define BPM_DETECT_MIN_MARGIN 1
// right safe margin used by autocorrelation
#define BPM_DETECT_MAX_MARGIN 1

#define BPM_DETECT_MIN_BPM 55
#define BPM_DETECT_MAX_BPM 125


typedef struct {
	unsigned int BPM;
	unsigned int Hit;
	unsigned int Sum;
} BPM_MOD_STRUCT;



WBPMDetect2::WBPMDetect2()
{
// initialize
	c_fReady = 0;
	c_pSubBand = 0;
	c_Samples = 0;
	c_pnOffset = 0;
	c_nChannel = 0;
	c_nSampleRate = 0;

	wavelet = new WWavelet(D4_WAVELET);
}

WBPMDetect2::~WBPMDetect2()
{
	FreeInternalMemory();
	delete wavelet;
}


int WBPMDetect2::Initialize(unsigned int nSampleRate, unsigned int nChannel)
{
	if(nSampleRate == 0 || nChannel == 0)
		return 0;


	c_nBufferSize = (nSampleRate * BPM_BUFFER_SIZE) / (1000 * BPM_WAVELET_POINTS);
	if(c_nBufferSize == 0)
		c_nBufferSize = 1;

	c_nWindowSize = (nSampleRate * BPM_WINDOW_SIZE) / (1000 * BPM_WAVELET_POINTS) * WAVELET_CHILD_SIZE;
	if(c_nWindowSize == 0)
		c_nWindowSize = 1;

	c_nOverlapSuccessSize = (nSampleRate * BPM_OVERLAP_SUCCESS_SIZE) / (1000 * BPM_WAVELET_POINTS) * WAVELET_CHILD_SIZE;
	if(c_nOverlapSuccessSize >= c_nBufferSize)
		c_nOverlapSuccessSize = c_nBufferSize - 1;	

	c_nOverlapFailSize = (nSampleRate * BPM_OVERLAP_FAIL_SIZE) / (1000 * BPM_WAVELET_POINTS) * WAVELET_CHILD_SIZE;
	if(c_nOverlapFailSize >= c_nBufferSize)
		c_nOverlapFailSize = c_nBufferSize - 1;	

	c_nOverlapSuccessStart = c_nBufferSize * WAVELET_CHILD_SIZE - c_nOverlapSuccessSize;
	c_nOverlapFailStart = c_nBufferSize * WAVELET_CHILD_SIZE - c_nOverlapFailSize;
	c_nSubbandBPMDetected = 0;


	if(c_fReady == 0 || c_nSampleRate != nSampleRate || c_nChannel != nChannel)
	{
		if(AllocateInternalMemory() == 0)
			return 0;

		c_nSampleRate = nSampleRate;
		c_nChannel = nChannel;

		unsigned int i;
		for(i = BPM_DETECT_MIN_BPM - BPM_DETECT_MIN_MARGIN - 1; i <= BPM_DETECT_MAX_BPM + BPM_DETECT_MAX_MARGIN + 1; i++)
			c_pnOffset[i] = (unsigned int) (((double) (60 * c_nSampleRate) / (double) ( i * BPM_WAVELET_POINTS / WAVELET_CHILD_SIZE)) + 0.5);

	}

	unsigned int i;
	for(i = 0; i < BPM_NUMBER_OF_SUBBANDS; i++)
	{
		SUBBAND2 *subband = &c_pSubBand[i];
		subband->BufferLoad = 0;
		subband->BPM = 0;
		memset(subband->BPMHistoryHit, 0, (BPM_DETECT_MAX_BPM + 2) * sizeof(unsigned int));
	}



	return 1;
}


int WBPMDetect2::PutSamples(short *pSamples, unsigned int nSampleNum)
{

	// create samples array
	unsigned int i;


	if(c_nChannel == 2)
	{
		// convert to mono
		for(i = 0; i < nSampleNum; i++)
			c_Samples[i] = ((BPM_DETECT2_REAL) pSamples[2 * i] +  (BPM_DETECT2_REAL) pSamples[2 * i + 1]) / 2.0;

	}
	else
	{
		for(i = 0; i < nSampleNum; i++)
			c_Samples[i] = (BPM_DETECT2_REAL) pSamples[i] ;
	}



	wavelet->ForwardTrans(c_Samples, BPM_WAVELET_POINTS, WAVELET_LEVEL);

	unsigned int j;

	for(i = 0; i < BPM_NUMBER_OF_SUBBANDS; i++)
	{
		SUBBAND2 *subband = &c_pSubBand[i];

		if(subband->BPM)	// we have BPM, don't compute anymore
			continue;
	
		BPM_DETECT2_REAL amp;
		for(j = 0; j < WAVELET_CHILD_SIZE; j++)
		{
			amp = c_Samples[j + i * WAVELET_CHILD_SIZE];
			subband->Buffer[j + subband->BufferLoad * WAVELET_CHILD_SIZE] = amp * amp;

		}

		subband->BufferLoad++;
		if(subband->BufferLoad < c_nBufferSize)
			continue;

		unsigned int j;
		unsigned int bpm = 0;
		BPM_DETECT2_REAL max_correlation = 0;

		// calculate auto correlation of each BPM in range

		unsigned int k;

		for(j = BPM_DETECT_MIN_BPM - BPM_DETECT_MIN_MARGIN - 1; j <= BPM_DETECT_MAX_BPM + BPM_DETECT_MAX_MARGIN; j++)
		{
			BPM_DETECT2_REAL correlation = 0;
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

	

	if(c_nSubbandBPMDetected == BPM_NUMBER_OF_SUBBANDS)
		return 1;

	return 0;


}


unsigned int WBPMDetect2::GetBPM()
{
	unsigned int i;
	unsigned int j;
	for(i = 0; i < BPM_NUMBER_OF_SUBBANDS; i++)
	{
		printf("Subband: %02u   %u\n", i, c_pSubBand[i].BPM);

	}

	// get BPM by calculating mod value
	BPM_MOD_STRUCT mod_value[BPM_NUMBER_OF_SUBBANDS];
	memset(mod_value, 0, BPM_NUMBER_OF_SUBBANDS * sizeof(BPM_MOD_STRUCT));

	// search unique values
	unsigned int size = 0;
	for(i = 0; i < BPM_NUMBER_OF_SUBBANDS; i++)
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
		for(j = 0; j < BPM_NUMBER_OF_SUBBANDS; j++)
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


int WBPMDetect2::AllocateInternalMemory()
{

	c_Samples = (BPM_DETECT2_REAL*) malloc(BPM_WAVELET_POINTS * sizeof(BPM_DETECT2_REAL));
	c_pnOffset = (unsigned int*) malloc((BPM_DETECT_MAX_BPM + BPM_DETECT_MAX_MARGIN + 2) * sizeof(unsigned int));

	if(c_Samples == 0 || c_pnOffset == 0)
	{
		FreeInternalMemory();
		return 0;
	}

	c_pSubBand = (SUBBAND2*) malloc(BPM_NUMBER_OF_SUBBANDS * sizeof(SUBBAND2));
	if(c_pSubBand == 0)
	{
		FreeInternalMemory();
		return 0;
	}

	memset(c_pSubBand, 0, BPM_NUMBER_OF_SUBBANDS * sizeof(SUBBAND2));

	unsigned int i;



	for(i = 0; i < BPM_NUMBER_OF_SUBBANDS; i++)
	{
		c_pSubBand[i].Buffer = (BPM_DETECT2_REAL*) malloc(c_nBufferSize * WAVELET_CHILD_SIZE * sizeof(BPM_DETECT2_REAL));
		if(c_pSubBand[i].Buffer == 0)
		{
			FreeInternalMemory();
			return 0;
		}

		memset(c_pSubBand[i].Buffer, 0, c_nBufferSize * sizeof(BPM_DETECT2_REAL));


		c_pSubBand[i].BPMHistoryHit = (unsigned int*) malloc((BPM_DETECT_MAX_BPM + 2) * sizeof(unsigned int));
		if(c_pSubBand[i].BPMHistoryHit == 0)
		{
			FreeInternalMemory();
			return 0;
		}
	}

 

	c_fReady = 1;
	return 1;

}


int WBPMDetect2::FreeInternalMemory()
{

	if(c_Samples)
	{
		free(c_Samples);
		c_Samples = 0;
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


void  WBPMDetect2::Release()
{
	delete this;
}

unsigned int WBPMDetect2::NumOfSamples()
{
	return BPM_WAVELET_POINTS;
}


int WBPMDetect2::SetFrequencyBand(unsigned int nLowLimit, unsigned int nHighLimit)
{
	return 0;

}