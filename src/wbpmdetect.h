/*
 *  libzplay - windows ( WIN32 ) multimedia library for playing mp3, mp2, ogg, wav, flac and raw PCM streams
 *  "Beat per minute" detection class ( WIN32 )
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

#ifndef _W_BPM_DETECT_H_
#define _W_BPM_DETECT_H_


#define BPM_DETECT_USING_PEAKS 0
#define BPM_DETECT_USING_AUTOCORRELATION 1

// datection with wavelets isn't working
#define BPM_DETECT_USING_WAVELETS 2

class WBPMDetect {
public:

// ==============================================================================================
	// initialize class, call this before you put samples into class
	virtual int Initialize(unsigned int nSampleRate, unsigned int nChannel) = 0;
	//
	//	PARAMETERS:
	//		nSampleRate
	//			Sample rate. Optimized for sample rate 44100 Hz.
	//
	//		nChannel
	//			Number of channels. Stereo is converted to mono.
	//
	//	RETURN VALUES:
	//		1	- all OK, class initialized
	//		0	- memory allocation error
	//
	//	REMARKS:
	//		Call this function before you put samples. Also call this function before processing
	//		new song to clear internal buffers and initialize all to starting positions.

// ================================================================================================
	// set frequency band for searching beats

	virtual int SetFrequencyBand(unsigned int nLowLimit, unsigned int nHighLimit) = 0;






// ===============================================================================================

	// get number of samples needed for PutSamples function.
	virtual unsigned int NumOfSamples() = 0;
	//
	//	PARAMETERS:
	//		None:
	//
	//	RETURN VALUES:
	//		Number of samples needed by PutSamples function.
	//
	//	REMARKS:
	//		Use this function to get number of samples you need to feed with PutSamples function
// ===============================================================================================

	// put samples into processing
	virtual int PutSamples(short *pSamples, unsigned int nSampleNum) = 0;
	//
	//	PARAMETERS:
	//		pSamples
	//			Pointer to buffer with samples. Supports only 16 bit samples
	//
	//		nSampleNum
	//			Number of samples in buffer.
	//
	//	RETURN VALUES:
	//		1	- detecting is done, we have BPM, we don't need more data.
	//		0	- we need more data to detect BPM
	//
	//	REMARKS:
	//		Call this function with new samples until function returns 1 or you are out of data.
	//		If function returns 1, detection is done and we have BPM value, so we don't need more data.
// ================================================================================================	
	
	// get BPM value
	virtual unsigned int GetBPM() = 0; 
	//
	//	PARAMETERS:
	//		None.
	//
	//	RETURN VALUES:
	//		BPM value.
	//		This value can be 0 if detection fails or song has no beat.
	//
	//	REMARKS:
	//		There can be case that class can't detect beat value. Maybe is song too short, or has
	//		no beat, or is too much noise
// ====================================================================================================	
	//
	//	destroy class
	virtual void  Release() = 0;
	//
	//	PARAMETERS:
	//		None.
	//
	//	RETURN VALUES:
	//		None.
	//
	//	REMARKS:
	//		Call this function to destroy class instance.
// =====================================================================================================
};


WBPMDetect * CreateBPMDetect(unsigned int nMethod);






#endif
