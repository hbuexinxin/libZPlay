/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  AC3 decoder
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
 * ver: 2.00
 * date: 24. April, 2010.
 *
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include "debug.h"
#include "wac3decoder.h"


// 6 blocks * 256 samples
#define SAMPLES_IN_ONE_FRAME 1536
#define INPUT_FRAME_NUMBER 5

// must be 1
#define FRAME_OVERLAY_NUM 1

// decoder delay in samples
#define DECODER_DELAY 256

//
#define MAX_FRAME_SIZE 1024

#define FRAME_INDEX_ERR 0xFFFFFFFF


enum {
	DECODER_NO_ERROR = 0,
	DECODER_INITIALIZATION_FAIL,
	DECODER_NOT_VALID_AC3_STREAM,


	DECODER_FILE_OPEN_ERROR,
	DECODER_FILE_SEEK_POSITION_OUT_OF_BOUNDS,
	DECODER_FILESIZE_OUT_OF_BOUNDS,
	DECODER_NOT_VALID_PCM_STREAM,
	DECODER_UNSUPPORTED_SAMPLERATE,
	DECODER_UNSUPPORTED_CHANNEL_NUMBER,
	DECODER_UNSUPPORTED_BIT_PER_SAMPLE,
	DECODER_MEMORY_ALLOCATION_FAIL,
	DECODER_NO_ID3_DATA,
	DECODER_SEEK_NOT_SUPPORTED_MANAGED,
	DECODER_SEEK_POSITION_OUT_OF_BOUNDS,
	DECODER_REVERSE_NOT_SUPPORTED_MANAGED,
	DECODER_FUNCTION_NOT_SUPPORTED,


	DECODER_UNKNOWN_ERROR
};




wchar_t *g_ac3_error_strW[DECODER_UNKNOWN_ERROR + 1] = {
	L"No error.",
	L"AC3Decoder: Initialization fail",
	L"AC3Decoder: This is not valid AC3 stream.",
	L"AC3Decoder: File open error.",
	L"AC3Decoder: File sek position is out of bounds.",
	L"AC3Decoder: File size is out of bounds.",
	L"AC3Decoder: This is not valid PCM stream.",
	L"AC3Decoder: Unsupported sample rate.",
	L"AC3Decoder: Unsupported channel number.",
	L"AC3Decoder: Unsupported bits per sample.",
	L"AC3Decoder: Memory allocation fail."
	L"AC3Decoder: No ID3 data.",
	L"AC3Decoder: Seek is not supported on managed stream.",
	L"AC3Decoder: Seek position is out of bounds.",
	L"AC3Decoder: Reverse mod is not supported on managed stream.",
	L"AC3Decoder: Function not supported in this decoder.",
	L"AC3Decoder: Unknown error."

};





// Summary:
//    Reallocate array of pointers to unsigned char.
// Parameters:
//    src		- Pointer to array you need to reallocate. If this parameter is NULL, new array will be allocated
//    old_size	- Old size of array, these elements will be moved to reallocated memory
//    new_size	- New size of array. If this value is 0, src array will be deallocated and function will return NULL.  
// Returns:
//    Pointer to reallocated array. If new_size is 0, function will free src array and return NULL. 
//    If new_size is not 0 and function fails, return value will be NULL.
// Description:
//    This function will reallocate array of pointers and return reallocated array. 
//    Function will always allocate new memory for array, even if you decrease array from previous size
//    new memory will be allocated.
unsigned char ** reallocate_ac3_ptr(unsigned char **src, unsigned int old_size, unsigned int new_size);


WAC3Decoder::WAC3Decoder()
{
	err(DECODER_NO_ERROR);

	c_Queue = NULL;

	c_state = NULL;


	c_pchBuffer = 0;
	c_pchBufferAllocSize = 0;
	c_pchBufferPos = 0;
	c_nBufferSize = 0;

	c_nBitrate = 0;


	c_pFramePtr = 0;
	c_nLastAvailableFrameIndex = 0;
	c_nAllocatedFrameNumber = 0;
	c_nCurrentFrameIndex = 0;

	c_fPreciseSongLength = 0;
	c_fPreciseSeek = 0;


	c_nSkipSamlesNum = 0;	// number of samples to skip within frame to get accurate seek position
	c_nFrameOverlay = 0;	// frame overlay, number of frames decoded, but skipped to output after seek function
	c_nCurrentSeekIndex = 0;
// =============================
//	INITIALIZE STREAM VALUES
	c_pchStreamStart = 0;
	c_pchStreamEnd = 0;
	c_nStreamSize = 0;
	c_pchStart = 0;
	c_pchEnd = 0;
	c_nSize = 0;
	c_pchPosition = 0;
	c_fReverse = 0;
	c_fManagedStream = 0;
	c_fEndOfStream = 0;
	c_nInSampleRate = 0;
	c_nCurrentPosition = 0;
	c_fReady = 0;
// =================================================
//	INITIALIZE INPUT_STREAM_INFO
	memset(&c_isInfo, 0, sizeof(INPUT_STREAM_INFO));
// ==================================================

// ==================================================

	memset(&c_DecoderData, 0, sizeof(DECODER_DATA));

	c_pchManagedBuffer = 0;
	c_nManagedBufferMaxSize = 0;
	c_nManagedBufferLoad = 0;
}


WAC3Decoder::~WAC3Decoder()
{
	Close();
	Uninitialize();
}



int WAC3Decoder::Initialize(int param1, int param2, int param3, int param4)
{

	// Every A/52 frame is composed of 6 blocks, each with an output of 256
	// samples for each channel.
	c_pchBuffer = (int*) malloc((SAMPLES_IN_ONE_FRAME * INPUT_FRAME_NUMBER + DECODER_DELAY) * 4);	// always use 16 bit stereo
	if(c_pchBuffer == 0)
	{
		err(DECODER_MEMORY_ALLOCATION_FAIL);
		return 0;
	}

	c_pchBufferAllocSize = SAMPLES_IN_ONE_FRAME * INPUT_FRAME_NUMBER + DECODER_DELAY;

	c_pchBufferPos = c_pchBuffer;
	c_nBufferSize = 0;

	c_fPreciseSongLength = param1;
	c_fPreciseSeek = param2;

	return 1;
}


int WAC3Decoder::Uninitialize()
{
	if(c_pchBuffer)
		free(c_pchBuffer);

	c_pchBuffer = 0;
	c_pchBufferAllocSize = 0;
	c_pchBufferPos = 0;
	c_nBufferSize = 0;
	return 1;
}


void WAC3Decoder::Release()
{
	delete this;
}

DECODER_ERROR_MESSAGE *WAC3Decoder::GetError()
{
	return &c_err_msg;
}




int WAC3Decoder::_OpenFile(unsigned int fSkipExtraChecking)
{

	
	c_state = a52_init(0); // O to disable accel

	if(c_state == NULL)
	{
		err(DECODER_INITIALIZATION_FAIL);
		a52_free(c_state);
		return 0;
	}


	c_pchStart = c_pchStreamStart;
	c_nSize = c_nStreamSize;
	c_pchEnd = c_pchStart + c_nSize - 1;
	c_pchPosition = c_pchStart;


	if(c_nStreamSize < 7)
	{
		err(DECODER_NOT_VALID_AC3_STREAM);
		a52_free(c_state);
		return 0;
	}

	int bit_rate;

	unsigned int length = _Sync(&c_nInSampleRate, &c_flags, &bit_rate);

	if(length == 0 || length >= c_nStreamSize || c_nInSampleRate == 0)
	{
		err(DECODER_NOT_VALID_AC3_STREAM);
		a52_free(c_state);
		return 0;
	}

	c_nSize -= (c_pchPosition - c_pchStart);
	c_pchStart = c_pchPosition;
	


	// calculate length
	unsigned int song_length_samples = (unsigned int) MulDiv(c_nSize,  c_nInSampleRate * 8,  bit_rate);	

	unsigned int nAssumedFrameNum = song_length_samples / SAMPLES_IN_ONE_FRAME;

	// check if bit_rate value is wrong and song length is too big
	if(nAssumedFrameNum > 100000)
		nAssumedFrameNum = 100000;	


	// accurate song length 

	if(c_fPreciseSongLength)
	{
			// scan stream
		unsigned int song_length_frames = get_frame_pointers(c_pchStart, c_nSize, c_nInSampleRate,
						&c_pFramePtr, nAssumedFrameNum);

		
		if(song_length_frames == 0)
		{
			err(DECODER_NOT_VALID_AC3_STREAM);
			return 0;
		}

		c_nLastAvailableFrameIndex = song_length_frames - 1;

		song_length_samples = song_length_frames * SAMPLES_IN_ONE_FRAME;

	}


// ==========================================================
// ENABLE PRECISE SEEK FUNCTION
// ==========================================================

	if(c_fPreciseSeek && c_pFramePtr == NULL) 
	{
		// Allocate array and this will enable accurate seek function, we will get poiners
		// to frames when seek occurs by scaning from first frame to specified frame.
		unsigned char **ptr = reallocate_ac3_ptr(0, 0, nAssumedFrameNum);
		if(ptr == 0)
		{
			err(DECODER_MEMORY_ALLOCATION_FAIL);
			a52_free(c_state);
			return 0;
		}

		c_pFramePtr = ptr;
		c_pFramePtr[0] = c_pchPosition;
		c_nLastAvailableFrameIndex = 0;
		c_nAllocatedFrameNumber = nAssumedFrameNum;
	}




	char *channel = "";

	// detect channel state
	switch(c_flags & A52_CHANNEL_MASK)
	{

		case A52_CHANNEL:
			channel = "AC3 Dual mono";
		break;

		case A52_MONO:
			channel = "AC3 Mono";
		break;

		case A52_STEREO:
			channel = "AC3 Stereo";
		break;

		case A52_3F:
			channel = "AC3 3 front channels";
		break;

		case A52_2F1R:
			channel = "AC3 2 front, 1 rear surround channel";
		break;

		case A52_3F1R:
			channel = "AC3 3 front, 1 rear surround channel";
		break;

		case A52_2F2R:
			channel = "AC3 2 front, 2 rear surround channels";
		break;

		case A52_3F2R:
			channel = "AC3 3 front, 2 rear surround channels";
		break;

		case A52_CHANNEL1:
			channel = "AC3 Mono";
		break;

		case A52_CHANNEL2:
			channel = "AC3 Mono";
		break;

		case A52_DOLBY:
			channel = "AC3 Dolby surround compatible stereo";
		break;

	}


	c_isInfo.nSampleRate = c_nInSampleRate;
	c_isInfo.nChannel = 2;	// output always stereo
	c_isInfo.nBitPerSample = 16;
	c_isInfo.nBlockAlign = 4;
	c_isInfo.nFileBitrate = bit_rate;
	c_isInfo.fVbr = 0;


	c_isInfo.nLength = song_length_samples;

	c_isInfo.pchStreamDescription = channel;

	c_nCurrentPosition = 0;

	c_nSkipSamlesNum = 0;	// number of samples to skip within frame to get accurate seek position
	c_nFrameOverlay = 0;	// frame overlay, number of frames decoded, but skipped to output after seek function
	c_nBufferSize = 0;

	c_fReady = 1;

	return 1;


}

int WAC3Decoder::Close()
{

	

	if(c_fReady == 0)
		return 1;

	c_nBitrate = 0;

	a52_free(c_state);

	c_pchStreamStart = 0;
	c_pchStreamEnd = 0;
	c_nStreamSize = 0;
	c_pchStart = 0;
	c_pchEnd = 0;
	c_nSize = 0;
	c_pchPosition = 0;
	c_fManagedStream = 0;
	c_fEndOfStream = 0;

// ================================

	//	INITIALIZE INPUT_STREAM_INFO
	memset(&c_isInfo, 0, sizeof(INPUT_STREAM_INFO));

// ==================================


	c_nCurrentPosition = 0;


// =============================================================
// free accurate seek structures and initialize values
	if(c_pFramePtr)
		free(c_pFramePtr);

	c_pFramePtr = 0;
	c_nLastAvailableFrameIndex = 0;
	c_nAllocatedFrameNumber = 0;
// ====================================================


	c_nSkipSamlesNum = 0;	// number of samples to skip within frame to get accurate seek position
	c_nFrameOverlay = 0;	// frame overlay, number of frames decoded, but skipped to output after seek function
	c_nBufferSize = 0;
	c_nCurrentSeekIndex = 0;


	if(c_pchManagedBuffer)
		free(c_pchManagedBuffer);

	c_pchManagedBuffer = 0;
	c_nManagedBufferMaxSize = 0;

	c_fReady = 0;

	return 1;
}



INPUT_STREAM_INFO *WAC3Decoder::GetStreamInfo()
{
	ASSERT_W(c_fReady);
	return &c_isInfo;
}


wchar_t **WAC3Decoder::GetID3Info(int version, char *pStream, unsigned int nStreamSize, int param1, int param2)
{
	err(DECODER_FUNCTION_NOT_SUPPORTED);
	return 0;
}





int WAC3Decoder::Seek(unsigned int nSamples)
{
	ASSERT_W(c_fReady);

	err(DECODER_NO_ERROR);

	c_nBitrate = 0;
	c_nBufferSize = 0;

	if(c_fManagedStream)
	{
		err(DECODER_SEEK_NOT_SUPPORTED_MANAGED);
		return 0;
	}

	if(nSamples >= c_isInfo.nLength)
	{
		err(DECODER_SEEK_POSITION_OUT_OF_BOUNDS);
		return 0;

	}

	c_nSkipSamlesNum = 0;
	c_nFrameOverlay = 0;

	unsigned char *nNewPosition = c_pchStart;


	if(c_pFramePtr)	// we can use precise seek
	{
		// get frame index 
		unsigned int nFrameIndex = getFrameIndex(nSamples);
		if(nFrameIndex == FRAME_INDEX_ERR)
		{
			err(DECODER_MEMORY_ALLOCATION_FAIL);
			return 0;
		}

		
		if(nFrameIndex >= FRAME_OVERLAY_NUM)
		{
			c_nFrameOverlay = FRAME_OVERLAY_NUM;
			nFrameIndex -= 	FRAME_OVERLAY_NUM;
		}

		nNewPosition = c_pFramePtr[nFrameIndex];
		// calculate number of samples to skip within frame
		c_nSkipSamlesNum = (nSamples - (nFrameIndex + c_nFrameOverlay) * SAMPLES_IN_ONE_FRAME) + DECODER_DELAY;


		if(c_nSkipSamlesNum > SAMPLES_IN_ONE_FRAME)
		{
			// because DECODER_DELAY we need to skip one whole frame and some samples in
			// next frame. So, we need one mpre frame overlay, and skip some samples in next frame
			c_nFrameOverlay++;
			c_nSkipSamlesNum -= SAMPLES_IN_ONE_FRAME;

		}




		c_nCurrentFrameIndex = nFrameIndex;
		c_nCurrentSeekIndex = nFrameIndex;

		if(c_fReverse)
			c_nSkipSamlesNum = 0;

	}
	else
	{
		nNewPosition = c_pchStart + (unsigned int) ((double) nSamples / (double) c_isInfo.nLength * (double) c_nSize); 
	}

	if(nNewPosition > c_pchEnd)
	{
		err(DECODER_SEEK_POSITION_OUT_OF_BOUNDS);
		return 0;
	}



	c_pchPosition = nNewPosition;
	c_nCurrentPosition = nSamples;

	return 1;

}


inline int16_t convert(int32_t i)
{
	if (i > 0x43c07fff)
		return 32767;
	else if (i < 0x43bf8000)
		return -32768;
	else
		return i - 0x43c00000;
}




void float2s16_2 (float * _f, int16_t * s16)
{
    int i;
    int32_t * f = (int32_t *) _f;
    for (i = 0; i < 256; i++)
	{
		s16[2*i] = convert(f[i]);
		s16[2*i+1] = convert(f[i+256]);
    }
}


int WAC3Decoder::_FillBuffer()
{

	unsigned int output_size = 0;
	int sample_rate;
	int flags;

	sample_t level = 1;
	sample_t bias = 384;

	// reverse mode
	if(c_fReverse)
	{
		ASSERT_W(c_pFramePtr); // we need pointer to each frame

		if(c_nCurrentSeekIndex == 0) // if we are at song start, end of data
			return DECODER_END_OF_DATA;

		// seek backward specified number of frames
		unsigned int nIndex;


		// try frame overlay
		if(c_nCurrentSeekIndex < ( INPUT_FRAME_NUMBER + FRAME_OVERLAY_NUM))
		{
			// no frame overlay because we are near stream beginning
			nIndex =  c_nCurrentSeekIndex - INPUT_FRAME_NUMBER;
			c_nCurrentSeekIndex -= INPUT_FRAME_NUMBER;
			c_nFrameOverlay = 0;
		}
		else
		{
			nIndex =  c_nCurrentSeekIndex - INPUT_FRAME_NUMBER - FRAME_OVERLAY_NUM;
			c_nCurrentSeekIndex -= INPUT_FRAME_NUMBER;
			c_nFrameOverlay = FRAME_OVERLAY_NUM;
		}


		ASSERT_W(nIndex <= c_nLastAvailableFrameIndex);

		c_pchPosition = c_pFramePtr[nIndex];

	}


	if(c_fManagedStream) // managed stream
	{

		c_nSkipSamlesNum = 0;
		// load data from queue
		unsigned int have = c_Queue->GetSizeSum();

		if(have < 7)
		{
			if(c_fEndOfStream)
			{
				if(c_nBufferSize == 0)
					return DECODER_END_OF_DATA;

			}

			return DECODER_MANAGED_NEED_MORE_DATA;
		}	
		
		// try sync
		
		unsigned int nDone = 0;
		unsigned int ret = c_Queue->PullDataFifo(c_pchManagedBuffer, 7, (int*) &nDone);
		c_DecoderData.nBufferDone += nDone;

		// sync
		unsigned int length = a52_syncinfo(c_pchManagedBuffer, &flags, &sample_rate, &c_nBitrate);

		while(length == 0)
		{
			// need resync
			MoveMemory(c_pchManagedBuffer, c_pchManagedBuffer + 1, 6);
			ret = c_Queue->PullDataFifo(c_pchManagedBuffer + 6, 1, (int*) &nDone);
			c_DecoderData.nBufferDone += nDone;
			if(ret == 0)
			{
				if(c_fEndOfStream)
				{
					if(c_nBufferSize == 0)
						return DECODER_END_OF_DATA;

				}

				// return data back to queue
				c_Queue->PushFirst(c_pchManagedBuffer, 6);
				return DECODER_MANAGED_NEED_MORE_DATA;
			}
			
			length = a52_syncinfo(c_pchManagedBuffer, &flags, &sample_rate, &c_nBitrate);
		}


		// check if we have enough data
		if(c_Queue->GetSizeSum() < length - 7)
		{
			// return data back to queue
			c_Queue->PushFirst(c_pchManagedBuffer, 7);
			return DECODER_MANAGED_NEED_MORE_DATA;
		}


	
		if(length > c_nManagedBufferMaxSize)
		{
			// reallocate c_pchManagedBuffer
			unsigned char *tmp = (unsigned char*) malloc(length);
			if(tmp == NULL)
				return DECODER_FATAL_ERROR;
			
			CopyMemory(tmp, c_pchManagedBuffer, 7);
			free(c_pchManagedBuffer);
			c_pchManagedBuffer = tmp;
		}

		// load rest of data
		ret = c_Queue->PullDataFifo(c_pchManagedBuffer + 7, length - 7, (int*) &nDone);
		c_DecoderData.nBufferDone += nDone;
		 				
			
		c_DecoderData.nBufferInQueue = c_Queue->GetCount();
		c_DecoderData.nBytesInQueue = c_Queue->GetSizeSum();

		// decode data

		flags |= A52_STEREO;
		a52_frame (c_state, c_pchManagedBuffer, &flags, &level, bias);

		// initialize buffer
		c_pchBufferPos = c_pchBuffer;
		c_nBufferSize = 0;

		int16_t * s16 = (int16_t*) c_pchBuffer;

		int n;
		for (n = 0; n < 6; n++)
		{
				
			if(a52_block (c_state))
			{
				// error
				continue;
			}
			
			// get samples		
			sample_t *samples = a52_samples(c_state);
			// convert float to integer
			float2s16_2(samples, s16);
			s16 += 512; // 256 * 2 channels

			c_nBufferSize += 256;
		}
		
	}
	else // non managed stream
	{


		if(c_pchPosition + 7 > c_pchEnd)
			return DECODER_END_OF_DATA;	// end of data

		// check if we need to overlay this frame
		while(c_nFrameOverlay != 0)
		{
			int length = a52_syncinfo(c_pchPosition, &flags, &sample_rate, &c_nBitrate);
			flags |= A52_STEREO;
			a52_frame (c_state, c_pchPosition, &flags, &level, bias);
			int n;
			for (n = 0; n < 6; n++)
			{
					
				if(a52_block (c_state))
				{
					// error
					continue;
				}
			}

			c_nFrameOverlay--;
			c_pchPosition += length;
		}


		unsigned int count = 0;
		c_pchBufferPos = &c_pchBuffer[c_nSkipSamlesNum];
		c_nBufferSize = 0;


		while(count < INPUT_FRAME_NUMBER)
		{
			count++;

			// sync
			int length = _Sync(&sample_rate, &flags, &c_nBitrate);

			if(length == 0)
				return DECODER_END_OF_DATA;	// end of data, can't find next sync

			while(sample_rate != c_nInSampleRate) // search new frame with correct samplerate
			{
				c_pchPosition++;
				length = _Sync(&sample_rate, &flags, &c_nBitrate);
				if(length == 0)
					return DECODER_END_OF_DATA;	// end of data
			}




			flags |= A52_STEREO;
			a52_frame (c_state, c_pchPosition, &flags, &level, bias);

			// initialize buffer

			int16_t * s16 = (int16_t*) &c_pchBuffer[c_nBufferSize];

			int n;
			for (n = 0; n < 6; n++)
			{	
				if(a52_block (c_state))
				{
					// error
					continue;
				}
				
				// get samples		
				sample_t *samples = a52_samples(c_state);
				// convert float to integer
				float2s16_2(samples, s16);
				s16 += 512; // 256 * 2 channels

				c_nBufferSize += 256;
			}

			c_pchPosition += length;
		}
	}


	if(c_fReverse)
		PCM16StereoReverse((int*) c_pchBuffer, c_nBufferSize, (int*) c_pchBuffer);

	if(c_nSkipSamlesNum <= c_nBufferSize)
	{
		c_nBufferSize -= c_nSkipSamlesNum;
		c_nSkipSamlesNum = 0;	
	}
	else
	{
		c_nSkipSamlesNum -= c_nBufferSize;
		c_nBufferSize = 0;
	}


	return DECODER_OK;
}


int WAC3Decoder::GetData(DECODER_DATA *pDecoderData)
{
	ASSERT_W(c_fReady);
	ASSERT_W(pDecoderData);
	ASSERT_W(pDecoderData->pSamples);

	
	
	unsigned int nSamplesNeed = pDecoderData->nSamplesNum;
	unsigned int nSamplesHave = 0;

	c_fEndOfStream = pDecoderData->fEndOfStream;


	pDecoderData->nBufferDone = 0;
	pDecoderData->nBufferInQueue = 0;
	pDecoderData->nBytesInQueue = 0;
	pDecoderData->nSamplesNum = 0;

	pDecoderData->nStartPosition = 0;
	pDecoderData->nEndPosition = 0;

	c_DecoderData.nBufferDone = 0;


	int *pchOutBuffer = (int*) pDecoderData->pSamples;	// use int because here we are working always with 16 bit stereo samples

	
	
	
	// non-managed stream

	while(nSamplesNeed)	// try to get all data that user need
	{

		if(c_nBufferSize == 0)
		{
			int ret = _FillBuffer();

			// data for managed stream
			pDecoderData->nBufferDone += c_DecoderData.nBufferDone;
			pDecoderData->nBufferInQueue = c_DecoderData.nBufferInQueue;
			pDecoderData->nBytesInQueue = c_DecoderData.nBytesInQueue;

			switch(ret)
			{
				case DECODER_OK: // buffer is loaded, all is OK by decoder, so try again with buffer
				continue;


				case DECODER_END_OF_DATA:	// decoder has no data anymore, end of stream or fatal error
				case DECODER_FATAL_ERROR:
				{
					if(nSamplesHave)	// if we have some data already in user buffer, give this to user
					{
						pDecoderData->nSamplesNum = nSamplesHave;
						return DECODER_OK;
					}
				}
				return ret;


				case DECODER_MANAGED_NEED_MORE_DATA:
				{
					// we can't get more data from decoder and we don't have enough data in user buffer
					// to fulfill user request

					if(c_fEndOfStream)
					{
						// user specifies that there will be no data, so give user samples we have 
						if(nSamplesHave)
						{
							pDecoderData->nSamplesNum = nSamplesHave;
							return DECODER_OK;
						}	

						// we don't have any data, user specifies that there will be no more data, return end of data.
						return DECODER_END_OF_DATA;
					}

					// we can expect more data from user so we will not return partial data to user,
					// instead we will save this partial data into internal buffer and indicate user that we need
					// more input data for decoder

					if(nSamplesHave)	// we need to save samples from user buffer
					{
						// check if we need to reallocate working buffer
						if(c_pchBufferAllocSize < nSamplesHave)	// reallocate buffer
						{
							int *tmp = (int*) malloc(nSamplesHave * 4);	// always use 16 bit stereo
							if(tmp == 0)
								return DECODER_FATAL_ERROR;

							free(c_pchBuffer);
							c_pchBuffer = tmp;
							// new allocated size
							c_pchBufferAllocSize = nSamplesHave;		
						}

						// copy back data from user buffer to our internal buffer
						unsigned int i;
						pchOutBuffer = (int*) pDecoderData->pSamples;
						for(i = 0; i < nSamplesHave; i++)
							c_pchBuffer[i] = pchOutBuffer[i];
								
						c_nBufferSize = nSamplesHave;
						c_pchBufferPos = c_pchBuffer;	
					}
							
				}

				return DECODER_MANAGED_NEED_MORE_DATA;

			}
		}


		// we have all data we need
		if(c_nBufferSize >= nSamplesNeed)
		{
			unsigned int i;
			for(i = 0; i < nSamplesNeed; i++)
				pchOutBuffer[i] = c_pchBufferPos[i];

			nSamplesHave += nSamplesNeed;

			// move pointer to next sample in buffer, need this for next call to GetData
			c_pchBufferPos = &c_pchBufferPos[nSamplesNeed];
			// calculate number of remaining samples in buffer
			c_nBufferSize -= nSamplesNeed;
			// we have all samples we need, don't loop anymore
			nSamplesNeed = 0;
		}
		else
		{
			unsigned int i;
			for(i = 0; i < c_nBufferSize; i++)
				pchOutBuffer[i] = c_pchBufferPos[i];

			pchOutBuffer = &pchOutBuffer[c_nBufferSize];

			nSamplesHave += c_nBufferSize;
			nSamplesNeed -= c_nBufferSize;
			c_nBufferSize = 0;
		}
	}
		
	
		
	pDecoderData->nStartPosition = c_nCurrentPosition;

	if(c_fReverse)
		c_nCurrentPosition -= nSamplesHave;
	else
		c_nCurrentPosition += nSamplesHave;

	pDecoderData->nEndPosition = c_nCurrentPosition;


	pDecoderData->nSamplesNum = nSamplesHave;

	if(nSamplesHave == 0)
		return DECODER_END_OF_DATA;
	
	return DECODER_OK;

}



int WAC3Decoder::GetBitrate(int fAverage)
{

	if(fAverage)
		return c_isInfo.nFileBitrate;

	return c_nBitrate;
}


int WAC3Decoder::SetReverseMode(int fReverse)
{
	ASSERT_W(c_fReady);

	err(DECODER_NO_ERROR);

	c_nBitrate = 0;
	c_nBufferSize = 0;
	
	if(fReverse)
	{
		if(c_fManagedStream)
		{
			err(DECODER_REVERSE_NOT_SUPPORTED_MANAGED);
			return 0;
		}

		if(c_pFramePtr == NULL || c_fPreciseSeek == 0 )
		{	
			// dynamic scan
			unsigned char **ptr = reallocate_ac3_ptr(0, 0, 1);
			if(ptr == 0)
			{
				err(DECODER_MEMORY_ALLOCATION_FAIL);
				return 0;
			}

			c_pFramePtr = ptr;
			c_pFramePtr[0] = c_pchStart;
			c_nLastAvailableFrameIndex = 0;
			c_nAllocatedFrameNumber = 1;
			c_fPreciseSeek = 1;
		}

		c_fReverse = 1;

		c_nBufferSize = 0;
	}
	else
		c_fReverse = 0;


	return 1;
}







void WAC3Decoder::err(unsigned int error_code)
{
	if(error_code > DECODER_UNKNOWN_ERROR)
		error_code = DECODER_UNKNOWN_ERROR;
			
	c_err_msg.errorW = g_ac3_error_strW[error_code];
}



// resync stream
int WAC3Decoder::_Sync(int *sample_rate, int *flags, int *bitrate)
{


	while(c_pchPosition + 7 < c_pchEnd)
	{
		int length = a52_syncinfo(c_pchPosition, flags, sample_rate, bitrate);
		if(length == 0 || c_pchPosition + length > c_pchEnd)
		{
			c_pchPosition++;
			continue;
		}

		return length;

	}

	return 0;
}



unsigned int WAC3Decoder::get_frame_pointers(unsigned char *buff, unsigned int size, int sample_rate,
		unsigned char*** frame_ptr, unsigned int initial_num)
{

	unsigned char *pos = buff;
	unsigned char *end = pos + size - 1;



	unsigned int count = 0;
	int br;


	unsigned char **ptr = 0;
	unsigned int alloc_num = 0;

	int sr;
	int flags;

	// preallocate
	if(initial_num)
	{
		ptr = reallocate_ac3_ptr(0, 0, initial_num);
		if(ptr == 0)
			return 0;
	}

	alloc_num = initial_num;

	
	while(pos + 7 < end)
	{

	
		int length = a52_syncinfo(pos, &flags, &sr, &br);
		if(length == 0 || pos + length > end || sr != sample_rate)
		{
			pos++;
			continue;
		}


		// check if we need to reallocate
		if(count + 1 > alloc_num)
		{
			alloc_num += 1024; 	
			ptr = reallocate_ac3_ptr(ptr, count, alloc_num);
			if(ptr == 0)
				return 0;	
		}

		ptr[count] = (unsigned char*) pos;
		count++;

		pos += length;
	}

	*frame_ptr = ptr;
	return count;
}


unsigned char ** reallocate_ac3_ptr(unsigned char **src, unsigned int old_size, unsigned int new_size)
{
	if(new_size == 0)	// request to free src
	{
		if(src != NULL)
			free(src);

		return NULL;
	}


	// allocate new memory
	unsigned char **ptr = (unsigned char**) malloc(new_size * sizeof(unsigned char*));
	if(ptr == NULL)
	{
		if(src)
			free(src);
		return NULL;
	}

	// copy old data into new memory
	unsigned int size = old_size < new_size ? old_size : new_size;
	if(src)
	{
		unsigned int i;
		for(i = 0; i < size; i++)
			ptr[i] = src[i];

		free(src);
	}

	return ptr;
}



unsigned int WAC3Decoder::getFrameIndex(unsigned int nSamples)
{
	ASSERT_W(c_pFramePtr);
	// calculate number of frames 
	unsigned int nNeededFrameIndex = nSamples / SAMPLES_IN_ONE_FRAME;
	if(nNeededFrameIndex > c_nLastAvailableFrameIndex)
	{
		// we don't have this index in table, scan stream for this index

		unsigned char *pos = c_pFramePtr[c_nLastAvailableFrameIndex];
		int flags;
		int sr;
		int br;
		int length = a52_syncinfo(pos, &flags, &sr, &br);
		if(length == 0 || pos + length > c_pchEnd || sr != c_nInSampleRate)
			return FRAME_INDEX_ERR;

		pos += length;
	

		unsigned char *tmp = 0;
		unsigned int count = 0;

		// search until you find index you need
		while(c_nLastAvailableFrameIndex < nNeededFrameIndex)
		{
			// check if we have allocated memory for more frames
			if(c_nLastAvailableFrameIndex + 1 >= c_nAllocatedFrameNumber)
			{
				// we don't have allocated space for more frames, need to reallocate
				// we will increase allocated space for 1024 frames
				unsigned int nAlloc = c_nAllocatedFrameNumber + 1024;
				// try to reallocate 
				unsigned char **ptr = reallocate_ac3_ptr(c_pFramePtr, c_nLastAvailableFrameIndex + 1, nAlloc);
				if(ptr == 0)
					return FRAME_INDEX_ERR;
				

				c_pFramePtr = ptr;
				c_nAllocatedFrameNumber = nAlloc;
			}
		

			while(pos + 7 < c_pchEnd)
			{
				length = a52_syncinfo(pos, &flags, &sr, &br);
				if(length == 0 || pos + length > c_pchEnd || sr != c_nInSampleRate)
				{
					pos++;
					continue;
				}
				
				break;		
			}

			c_nLastAvailableFrameIndex++;
			c_pFramePtr[c_nLastAvailableFrameIndex] = (unsigned char*) pos;

			pos += length;
		}
	}

	return nNeededFrameIndex;
}

int WAC3Decoder::OpenStream(WQueue *pQueue, int fDynamic, int param1, int param2)
{
	err(DECODER_NO_ERROR);

	c_Queue = pQueue;

	unsigned int nSize = pQueue->GetSizeSum();
	if(nSize == 0)
	{
		err(DECODER_NOT_VALID_AC3_STREAM);
		return 0;
	}


	unsigned char *ptr;
	unsigned int size; 
	if(pQueue->QueryFirstPointer((void**) &ptr, &size) == 0)
	{
		err(DECODER_NOT_VALID_AC3_STREAM);
		return 0;
	}

	c_fManagedStream = 0;

	if(param1 == 0)
	{

		c_pchStreamStart = ptr;
		c_nStreamSize = size;
		c_pchStreamEnd = c_pchStreamStart + c_nStreamSize - 1;


		if(fDynamic)
		{
			c_fManagedStream = 1;
			c_nManagedBufferMaxSize = MAX_FRAME_SIZE;
			c_pchManagedBuffer = (unsigned char*) malloc(c_nManagedBufferMaxSize);
			if(c_pchManagedBuffer == 0)
			{
				c_nManagedBufferMaxSize = 0;
				err(DECODER_MEMORY_ALLOCATION_FAIL);
				return 0;

			}
		}

		
		if(_OpenFile(0) == 0)
		{
			Close();
			return 0;
		}

		return 1;
	}
	else
	{
		c_pchStreamStart = ptr;
		c_nStreamSize = size;
		c_pchStreamEnd = c_pchStreamStart + c_nStreamSize - 1;
		return 1;

	}

	return 0;
}
