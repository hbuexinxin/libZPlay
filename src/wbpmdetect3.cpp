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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <memory.h>

#include "debug.h"
#include "WBPMDetect3.h"


typedef struct {
	unsigned int BackwardWindow;
	unsigned int ForwardWindow;
	unsigned int PeakWindow;
	unsigned int ShiftWindow;
	unsigned int Up;
	unsigned int Down;
	BPM_DETECT3_REAL BackwardTreshold;
	BPM_DETECT3_REAL ForwardTreshold;
	unsigned int HistoryMatch;
} SUBBAND_PARAMETERS;


typedef struct {
	unsigned int BPM;
	unsigned int Hit;
	unsigned int Sum;
} BPM_MOD_STRUCT;

#define PI 3.1415926535897932384626433832795029L


// number of fft points for analyse
#define BPM_DETECT_FFT_POINTS3 64

// number of subbands
#define NUMBER_OF_SUBBANDS 16

#define BPM_MIN 40
#define BPM_MAX 170


const SUBBAND_PARAMETERS g_SubBandParam[NUMBER_OF_SUBBANDS] = {
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 0
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 1
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 2
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 3
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 4
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 5
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 6
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 7
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 8
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 9
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 10
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 11
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 12
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 13
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10}, // 14
													{400, 200, 100, 0, 10, 10, 1.1, 1.05, 10} // 15
													
													
};


#define BACKWARD_TRESHOLD 1.9
#define FORWARD_TRESHOLD 1.0



WBPMDetect3::WBPMDetect3()
{
// initialize
	c_fReady = 0;
	c_pSubBand = 0;
	c_Amplitudes = 0;
	c_Samples = 0;
	c_pflWindow = 0;
	c_pnIp = 0;		
	c_pflw = 0;
	c_nChannel = 0;
	c_nSampleRate = 0;
}

WBPMDetect3::~WBPMDetect3()
{
	FreeInternalMemory();
}


int WBPMDetect3::Initialize(unsigned int nSampleRate, unsigned int nChannel)
{
	if(nSampleRate == 0 || nChannel == 0)
		return 0;

	if(c_fReady == 0 || c_nSampleRate != nSampleRate || c_nChannel != nChannel)
	{
		FreeInternalMemory();
		if(AllocateInternalMemory(nSampleRate) == 0)
			return 0;

		c_nSampleRate = nSampleRate;
		c_nChannel = nChannel;
	}

	unsigned int i;
	for(i = 0; i < NUMBER_OF_SUBBANDS; i++)
	{
		SUBBAND3 *subband = &c_pSubBand[i];
		memset(subband->BackwardAmplitude, 0, subband->BackwardWindow * sizeof(BPM_DETECT3_REAL));
		subband->SumBackwardAmplitude = 0;
		subband->BackwardIndex = 0;
		subband->BackwardLoad = 0;

		memset(subband->ForwardAmplitude, 0, subband->ForwardWindow * sizeof(BPM_DETECT3_REAL));
		subband->SumForwardAmplitude = 0;
		subband->ForwardIndex = 0;
		subband->ForwardLoad = 0;

		memset(subband->PeakAmplitude, 0, subband->PeakWindow * sizeof(BPM_DETECT3_REAL));
		subband->SumPeakAmplitude = 0;
		subband->PeakIndex = 0;
		subband->PeakLoad = 0;

		subband->ShiftIndex = 0;
		subband->SamplePos = 0;
		subband->HistoryBPMPos = 0;


		subband->LastPeakAvgAmplitude = 0.0;
		subband->LastBackwardAvgAmplitude = 0.0;
		subband->LastForwardAvgAmplitude = 0.0;
		subband->Up = 0;
		subband->Down = 0;
		subband->PossiblePeak = 0;
		subband->PeakPos = 0;
		subband->LastSamplePos = 0;

		subband->ForwardTreshold = FORWARD_TRESHOLD;
		subband->BackwardTreshold = BACKWARD_TRESHOLD;


		memset(subband->BPMHistoryHit, 0, (BPM_MAX + 2) * sizeof(unsigned int));
	}


	c_nSubbandBPMDetected = 0;
	return 1;
}


int WBPMDetect3::PutSamples(short *pSamples, unsigned int nSampleNum)
{

	// create samples array

	unsigned int i;

	if(c_nChannel == 2)
	{
		// convert to mono
		for(i = 0; i < nSampleNum; i++)
			c_Samples[i] = ((BPM_DETECT3_REAL) pSamples[2 * i] +  (BPM_DETECT3_REAL) pSamples[2 * i + 1])  * c_pflWindow[i] / 2.0;
	}
	else
	{
		for(i = 0; i < nSampleNum; i++)
			c_Samples[i] = (BPM_DETECT3_REAL) pSamples[i] * c_pflWindow[i];
	}


	// make fft
	rdft(BPM_DETECT_FFT_POINTS3, 1, c_Samples, c_pnIp, c_pflw);

	
	BPM_DETECT3_REAL re = c_Samples[1] ;
	BPM_DETECT3_REAL im = 0;
	BPM_DETECT3_REAL amp = sqrt(re * re + im * im);
	c_Amplitudes[BPM_DETECT_FFT_POINTS3 / 2 - 1] = amp;

	for(i = 1; i < BPM_DETECT_FFT_POINTS3 / 2; i++)
	{
		re = c_Samples[i * 2] ;
		im = c_Samples[i * 2 + 1];
		amp = sqrt(re * re + im * im);
		c_Amplitudes[i - 1] = amp;


	}

	unsigned int j;

	unsigned int nSubbandSize = BPM_DETECT_FFT_POINTS3 / ( 2 * NUMBER_OF_SUBBANDS);
	
	for(i = 0; i < NUMBER_OF_SUBBANDS; i++)
	{
		SUBBAND3 *subband = &c_pSubBand[i];

		if(subband->BPM)
			continue;

	
		// === CREATE SUBBAND3S =================================
		BPM_DETECT3_REAL InstantAmplitude = 0;

		for(j =  i * nSubbandSize; j < (i + 1) * nSubbandSize; j++)
			InstantAmplitude += c_Amplitudes[j];

		InstantAmplitude /= (BPM_DETECT3_REAL) nSubbandSize;
		

		// =====================================================
		
		 
		BPM_DETECT3_REAL amp;

		if(subband->PeakLoad >= subband->PeakWindow)
		{
			
			amp = subband->BackwardAmplitude[subband->BackwardIndex];
			// update backward window
			subband->SumBackwardAmplitude -= amp;
			// add new walue to backward history
			amp = subband->PeakAmplitude[subband->PeakIndex];
			subband->BackwardAmplitude[subband->BackwardIndex] = amp;
			subband->SumBackwardAmplitude += amp;
		
			subband->BackwardIndex++;
			if(subband->BackwardIndex >= subband->BackwardWindow)
				subband->BackwardIndex = 0;

			subband->BackwardLoad++;
			if(subband->BackwardLoad > subband->BackwardWindow)
				subband->BackwardLoad = subband->BackwardWindow;
		}
			

		if(subband->ForwardLoad >= subband->ForwardWindow)
		{
			// update peak window
			amp = subband->PeakAmplitude[subband->PeakIndex];
			subband->SumPeakAmplitude -= amp;
			amp = subband->ForwardAmplitude[subband->ForwardIndex];
			subband->PeakAmplitude[subband->PeakIndex] = amp;
			subband->SumPeakAmplitude += amp;
			
			subband->PeakIndex++;
			if(subband->PeakIndex >= subband->PeakWindow)
				subband->PeakIndex = 0;

			subband->PeakLoad++;
			if(subband->PeakLoad > subband->PeakWindow)
				subband->PeakLoad = subband->PeakWindow;

			subband->SamplePos += nSampleNum;
				
		}



		// update forward window
		amp = subband->ForwardAmplitude[subband->ForwardIndex];
		subband->SumForwardAmplitude -= amp;
		// add new walue to forward history
		subband->ForwardAmplitude[subband->ForwardIndex] = InstantAmplitude;
		subband->SumForwardAmplitude += InstantAmplitude;

		subband->ForwardIndex++;
		if(subband->ForwardIndex >= subband->ForwardWindow)
			subband->ForwardIndex = 0;

		subband->ForwardLoad++;
		if(subband->ForwardLoad > subband->ForwardWindow)
			subband->ForwardLoad = subband->ForwardWindow;


		if(subband->ShiftIndex > 0)
		{
			subband->ShiftIndex--;
			continue;
		}

		
		if(subband->BackwardLoad >= subband->BackwardWindow)
		{
			// calculate average values
			BPM_DETECT3_REAL AvgBackwardAmplitude = subband->SumBackwardAmplitude / (BPM_DETECT3_REAL) subband->BackwardLoad;	
			BPM_DETECT3_REAL AvgForwardAmplitude = subband->SumForwardAmplitude / (BPM_DETECT3_REAL) subband->ForwardLoad;
			BPM_DETECT3_REAL AvgPeakAmplitude = subband->SumPeakAmplitude / (BPM_DETECT3_REAL) subband->PeakLoad;

			if(AvgPeakAmplitude >= subband->LastPeakAvgAmplitude)
			{
				// UP
				subband->Up++;
				if(subband->Down != 0)
					subband->Down--;
			}
			else
			{
				// DOWN
				subband->Down++;
				if(subband->Up != 0)
					subband->Up--;
				
			
				if(subband->Up >= g_SubBandParam[i].Up)
				{
					subband->PossiblePeak = 0;

				
					

					unsigned int nSampleDiff = subband->LastSamplePos - subband->HistoryBPMPos;
					unsigned int bpm = (unsigned int) ((60.0 * (BPM_DETECT3_REAL) c_nSampleRate / (BPM_DETECT3_REAL) nSampleDiff) + 0.5);
					
					
					if(subband->LastPeakAvgAmplitude > subband->LastBackwardAvgAmplitude * subband->BackwardTreshold && 
						subband->LastPeakAvgAmplitude > subband->LastForwardAvgAmplitude * subband->ForwardTreshold)
					{
					
						if(bpm <= BPM_MAX)
						{
							subband->PossiblePeak = 1;
							subband->Down = 1;
							subband->PeakPos = 	subband->LastSamplePos;	
							subband->BPMCandidate = bpm;
						}
					}
					else
					{
						if(subband->LastPeakAvgAmplitude > subband->LastBackwardAvgAmplitude  && 
							subband->LastPeakAvgAmplitude > subband->LastForwardAvgAmplitude && bpm <= BPM_MAX)
						{

							if(subband->BPMHistoryHit[bpm] > 5 || subband->BPMHistoryHit[bpm - 1] > g_SubBandParam[i].HistoryMatch / 2 || subband->BPMHistoryHit[bpm + 1] > g_SubBandParam[i].HistoryMatch / 2)
							{
								subband->PossiblePeak = 1;
								subband->Down = 1;
								subband->PeakPos = 	subband->LastSamplePos;	
								subband->BPMCandidate = bpm;
							}
							subband->BackwardTreshold -= 0.05;
							if(subband->BackwardTreshold < 1.0)
								subband->BackwardTreshold = 1.0;

						
						}
					}
					

				
					subband->Up = g_SubBandParam[i].Up - 1;
					
				}
				
				if(subband->PossiblePeak && subband->Down >= g_SubBandParam[i].Down)
				{
					subband->Up = 0;
					subband->Down = 0;
					subband->PossiblePeak = 0;

					unsigned int bpm = subband->BPMCandidate;
					if(bpm >= BPM_MIN)
					{	
						unsigned int HistoryHit = subband->BPMHistoryHit[bpm] + subband->BPMHistoryHit[bpm - 1] + subband->BPMHistoryHit[bpm + 1];
						if(HistoryHit)
						{
							if(HistoryHit >= g_SubBandParam[i].HistoryMatch)
							{	
								unsigned int Avg = (bpm * subband->BPMHistoryHit[bpm]  + (bpm - 1) * subband->BPMHistoryHit[bpm - 1] + (bpm + 1) * subband->BPMHistoryHit[bpm + 1]) / HistoryHit ;
								subband->BPM = Avg;
								c_nSubbandBPMDetected++;
							}
						}

						subband->BPMHistoryHit[bpm]++;
						subband->HistoryBPMPos = subband->PeakPos;
						subband->ShiftIndex = subband->ShiftWindow;
					}
					else
					{
						subband->HistoryBPMPos = subband->PeakPos;
						subband->BackwardTreshold += 0.1;	
					}		
				}	
			}

			subband->LastPeakAvgAmplitude = AvgPeakAmplitude;
			subband->LastBackwardAvgAmplitude = AvgBackwardAmplitude;
			subband->LastForwardAvgAmplitude = AvgForwardAmplitude;
			subband->LastSamplePos = subband->SamplePos;
		}		
	}
	
	
	if(c_nSubbandBPMDetected == NUMBER_OF_SUBBANDS)
		return 1;
	

	return 0;
}


unsigned int WBPMDetect3::GetBPM()
{
	unsigned int i;
	unsigned int j;
/*
	for(i = 0; i < NUMBER_OF_SUBBANDS; i++)
	{
		SUBBAND3 *subband = &c_pSubBand[i];
		printf("Subband: %02u  %u\n", i, subband->BPM);
	}
*/


	// get BPM by calculating mod value
	BPM_MOD_STRUCT mod_value[NUMBER_OF_SUBBANDS];
	memset(mod_value, 0, NUMBER_OF_SUBBANDS * sizeof(BPM_MOD_STRUCT));

	// search unique values
	unsigned int size = 0;
	for(i = 0; i < NUMBER_OF_SUBBANDS; i++)
	{
		unsigned int have = 0;
		for(j = 0; j < size; j++)
		{
			if(c_pSubBand[i].BPM != 0 && mod_value[j].BPM == c_pSubBand[i].BPM)
			{
				have = 1;
				break;
			}
		}

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
		for(j = 0; j < NUMBER_OF_SUBBANDS; j++)
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


int WBPMDetect3::AllocateInternalMemory(unsigned int nSampleRate)
{
	c_Amplitudes = (BPM_DETECT3_REAL*) malloc(BPM_DETECT_FFT_POINTS3 / 2 * sizeof(BPM_DETECT3_REAL));
	c_Samples = (BPM_DETECT3_REAL*) malloc(BPM_DETECT_FFT_POINTS3 * sizeof(BPM_DETECT3_REAL));
	c_pflWindow = (BPM_DETECT3_REAL*) malloc(BPM_DETECT_FFT_POINTS3 * sizeof(BPM_DETECT3_REAL));
	c_pnIp = (int*) malloc( ( 2 + (int) sqrt((BPM_DETECT3_REAL) BPM_DETECT_FFT_POINTS3 / 2.0) ) * sizeof(int));
	c_pflw = (BPM_DETECT3_REAL*) malloc(BPM_DETECT_FFT_POINTS3 / 2 * sizeof(BPM_DETECT3_REAL));

	if(c_Amplitudes == 0 || c_Samples == 0 || c_pflWindow == 0 || c_pnIp == 0 || c_pflw == 0)
	{
		FreeInternalMemory();
		return 0;
	}

	c_pnIp[0] = 0;

	memset(c_Samples, 0, BPM_DETECT_FFT_POINTS3 * sizeof(BPM_DETECT3_REAL));

	c_pSubBand = (SUBBAND3*) malloc(NUMBER_OF_SUBBANDS * sizeof(SUBBAND3));
	if(c_pSubBand == 0)
	{
		FreeInternalMemory();
		return 0;
	}

	memset(c_pSubBand, 0, NUMBER_OF_SUBBANDS * sizeof(SUBBAND3));

	unsigned int i;
	for(i = 0; i < NUMBER_OF_SUBBANDS; i++)
	{
		unsigned int size =  (g_SubBandParam[i].BackwardWindow * nSampleRate) / ( 1000 * BPM_DETECT_FFT_POINTS3);
		if(size == 0)
			size = 1;
		c_pSubBand[i].BackwardAmplitude = (BPM_DETECT3_REAL*) malloc(size * sizeof(BPM_DETECT3_REAL));
		if(c_pSubBand[i].BackwardAmplitude == 0)
		{
			FreeInternalMemory();
			return 0;
		}

		c_pSubBand[i].BackwardWindow = size;
	}

	for(i = 0; i < NUMBER_OF_SUBBANDS; i++)
	{
		unsigned int size =  (g_SubBandParam[i].ForwardWindow * nSampleRate) / ( 1000 * BPM_DETECT_FFT_POINTS3);
		if(size == 0)
			size = 1;
		c_pSubBand[i].ForwardAmplitude = (BPM_DETECT3_REAL*) malloc(size * sizeof(BPM_DETECT3_REAL));
		if(c_pSubBand[i].ForwardAmplitude == 0)
		{
			FreeInternalMemory();
			return 0;
		}

		c_pSubBand[i].ForwardWindow = size;
	}


	for(i = 0; i < NUMBER_OF_SUBBANDS; i++)
	{
		unsigned int size =  (g_SubBandParam[i].PeakWindow * nSampleRate) / ( 1000 * BPM_DETECT_FFT_POINTS3);
		if(size == 0)
			size = 1;
		c_pSubBand[i].PeakAmplitude = (BPM_DETECT3_REAL*) malloc(size * sizeof(BPM_DETECT3_REAL));
		if(c_pSubBand[i].PeakAmplitude == 0)
		{
			FreeInternalMemory();
			return 0;
		}

		c_pSubBand[i].PeakWindow = size;
	}

	for(i = 0; i < NUMBER_OF_SUBBANDS; i++)
	{
		unsigned int size =  (g_SubBandParam[i].ShiftWindow * nSampleRate) / ( 1000 * BPM_DETECT_FFT_POINTS3);
		c_pSubBand[i].ShiftWindow = size;
	}


	for(i = 0; i < NUMBER_OF_SUBBANDS; i++)
	{
		c_pSubBand[i].BPMHistoryHit = (unsigned int*) malloc((BPM_MAX + 2) * sizeof(unsigned int));
		if(c_pSubBand[i].BPMHistoryHit == 0)
		{
			FreeInternalMemory();
			return 0;
		}
	}


	// create COSINE window

	double n;
	double N = (double) BPM_DETECT_FFT_POINTS3;
	double pi = PI;
	for(i = 0; i < BPM_DETECT_FFT_POINTS3; i++)
	{
		n = (double) i;
		c_pflWindow[i] = (BPM_DETECT3_REAL) ( sin( (pi * n) / (N - 1.0)) );
	}


	c_fReady = 1;
	return 1;

}


int WBPMDetect3::FreeInternalMemory()
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


	if(c_pSubBand)
	{

		unsigned int i;
		for(i = 0; i < NUMBER_OF_SUBBANDS; i++)
		{
			if(c_pSubBand[i].BackwardAmplitude)
				free(c_pSubBand[i].BackwardAmplitude);

			if(c_pSubBand[i].ForwardAmplitude)
				free(c_pSubBand[i].ForwardAmplitude);

			if(c_pSubBand[i].PeakAmplitude)
				free(c_pSubBand[i].PeakAmplitude);

			if(c_pSubBand[i].BPMHistoryHit)
				free(c_pSubBand[i].BPMHistoryHit);
			

		}

		free(c_pSubBand);
		c_pSubBand = 0;
	}

	c_fReady = 0;
	return 1;
}

void  WBPMDetect3::Release()
{
	delete this;
}

unsigned int WBPMDetect3::NumOfSamples()
{
	return BPM_DETECT_FFT_POINTS3;
}

int WBPMDetect3::SetFrequencyBand(unsigned int nLowLimit, unsigned int nHighLimit)
{
	return 0;

}
