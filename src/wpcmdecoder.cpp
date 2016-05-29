/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  PCM decoder
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
 * ver: 2.0
 * date: 24. April, 2010.
 *
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include "debug.h"
#include "wpcmdecoder.h"




enum {
	DECODER_NO_ERROR = 0,
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



wchar_t *g_pcm_error_strW[DECODER_UNKNOWN_ERROR + 1] = {
	L"No error.",
	L"PCMDecoder: File open error.",
	L"PCMDecoder: File sek position is out of bounds.",
	L"PCMDecoder: File size is out of bounds.",
	L"PCMDecoder: This is not valid PCM stream.",
	L"PCMDecoder: Unsupported sample rate.",
	L"PCMDecoder: Unsupported channel number.",
	L"PCMDecoder: Unsupported bits per sample.",
	L"PCMDecoder: Memory allocation fail."
	L"PCMDecoder: No ID3 data.",
	L"PCMDecoder: Seek is not supported on managed stream.",
	L"PCMDecoder: Seek position is out of bounds.",
	L"PCMDecoder: Reverse mod is not supported on managed stream.",
	L"PCMDecoder: Function not supported in this decoder.",

	L"PCMDecoder: Unknown error."

};

// number of samples to output
#define INPUT_PCM_BUFFER_SIZE 4410




WPCMDecoder::WPCMDecoder()
{
	err(DECODER_NO_ERROR);

	c_Queue = NULL;

// =============================
//	INITIALIZE STREAM VALUES
	c_pchStreamStart = 0;
	c_pchStreamEnd = 0;
	c_nStreamSize = 0;
	c_pchStart = 0;
	c_pchEnd = 0;
	c_nSize = 0;
	c_pchPosition = 0;
	c_nBitPerSample = 0;
	c_nBlockAlign = 0;
	c_fReverse = 0;
	c_fManagedStream = 0;
	c_fEndOfStream = 0;
	c_nInSampleRate = 0;
	c_inChannelNum = 0;
	c_inBitPerSample = 0;
	c_fBigEndian = 0;
	c_fSigned = 0;
	c_nCurrentPosition = 0;
	c_fReady = 0;
// =================================================
//	INITIALIZE INPUT_STREAM_INFO
	memset(&c_isInfo, 0, sizeof(INPUT_STREAM_INFO));
// ==================================================
//	INITIALIZE ID3_INFO
	unsigned int i;
	for(i = 0; i < ID3_FIELD_NUMBER; i++)
	{
		c_id3Info[i] = 0;
		c_id3InfoW[i] = 0;
	}
// ==================================================

}


WPCMDecoder::~WPCMDecoder()
{
	Close();
	Uninitialize();
}



int WPCMDecoder::Initialize(int param1, int param2, int param3, int param4)
{
	if(param1 == 0)
	{
		err(DECODER_UNSUPPORTED_SAMPLERATE);
		return 0;
	}

	if(param2 == 0 || param2 > 2)
	{
		err(DECODER_UNSUPPORTED_CHANNEL_NUMBER);
		return 0;
	}

	if(param3 != 8 && param3 != 16)
	{
		err(DECODER_UNSUPPORTED_BIT_PER_SAMPLE);
		return 0;
	}

	c_fBigEndian = 0;
	c_fSigned = 0;
	if(param4)
	{
		if(param3 == 16)
			c_fBigEndian = 1;

		if(param3 == 8)
			c_fSigned = 1;
	}

	c_nInSampleRate = param1;
	c_inChannelNum = param2;
	c_inBitPerSample = param3;



	return 1;
}


int WPCMDecoder::Uninitialize()
{
	return 1;
}


void WPCMDecoder::Release()
{
	delete this;
}

DECODER_ERROR_MESSAGE *WPCMDecoder::GetError()
{
	return &c_err_msg;
}




int WPCMDecoder::_OpenFile(unsigned int fSkipExtraChecking)
{


	c_pchStart = c_pchStreamStart;
	c_nSize = c_nStreamSize;
	c_pchEnd = c_pchStart + c_nSize - 1;
	c_pchPosition = c_pchStart;

	// get informations of input stream
	// WE ALWAYS USE 16 bit per sample stereo

	// internal bit per sample
	c_nBitPerSample = c_inBitPerSample;
	// internal block align
	c_nBlockAlign = c_inBitPerSample / 8 * c_inChannelNum;


	c_isInfo.nSampleRate = c_nInSampleRate;
	c_isInfo.nChannel = c_inChannelNum;
	c_isInfo.nBitPerSample = 16;
	c_isInfo.nBlockAlign = 4;
	c_isInfo.nFileBitrate = c_nInSampleRate * c_nBlockAlign * 8;
	c_isInfo.fVbr = 0;
	c_isInfo.nLength = c_nSize / c_nBlockAlign;

	if(c_inChannelNum == 2)
		c_isInfo.pchStreamDescription = "RAW PCM UNCOMPRESSED STEREO";
	else
		c_isInfo.pchStreamDescription = "RAW PCM UNCOMPRESSED MONO";
	

	c_nCurrentPosition = 0;

	c_fReady = 1;

	return 1;


}

int WPCMDecoder::Close()
{



	if(c_fReady == 0)
		return 1;

	c_pchStreamStart = 0;
	c_pchStreamEnd = 0;
	c_nStreamSize = 0;
	c_pchStart = 0;
	c_pchEnd = 0;
	c_nSize = 0;
	c_pchPosition = 0;
	c_fManagedStream = 0;
	c_fEndOfStream = 0;
// ===================================
// free ID3 fields
	unsigned int i;
	for(i = 0; i < ID3_FIELD_NUMBER; i++)
	{	
		if(c_id3Info[i] != 0)
			free(c_id3Info[i]);

		c_id3Info[i] = 0;

		if(c_id3InfoW[i] != 0)
			free(c_id3InfoW[i]);

		c_id3InfoW[i] = 0;
	}
// ================================

	//	INITIALIZE INPUT_STREAM_INFO
	memset(&c_isInfo, 0, sizeof(INPUT_STREAM_INFO));

// ==================================

	c_nCurrentPosition = 0;

	c_fReady = 0;

	return 1;
}



INPUT_STREAM_INFO *WPCMDecoder::GetStreamInfo()
{
	ASSERT_W(c_fReady);
	return &c_isInfo;
}


wchar_t **WPCMDecoder::GetID3Info(int version, char *pStream, unsigned int nStreamSize, int param1, int param2)
{
	ASSERT_W(c_fReady);
	err(DECODER_FUNCTION_NOT_SUPPORTED);
	return NULL;

}




int WPCMDecoder::Seek(unsigned int nSamples)
{
	ASSERT_W(c_fReady);

	err(DECODER_NO_ERROR);

	if(c_fManagedStream)
	{
		err(DECODER_SEEK_NOT_SUPPORTED_MANAGED);
		return 0;
	}


	unsigned char *nNewPosition = c_pchStart + nSamples * c_nBlockAlign;

	if(nNewPosition > c_pchEnd)
	{
		err(DECODER_SEEK_POSITION_OUT_OF_BOUNDS);
		return 0;
	}

	c_pchPosition = nNewPosition;


	c_nCurrentPosition = nSamples;

	return 1;

}


int WPCMDecoder::GetData(DECODER_DATA *pDecoderData)
{
	ASSERT_W(c_fReady);
	ASSERT_W(pDecoderData);
	ASSERT_W(pDecoderData->pSamples);

	
	
	unsigned int nSamplesNeed = pDecoderData->nSamplesNum;
	unsigned int nBytesNeed = pDecoderData->nSamplesNum * c_nBlockAlign;

	c_fEndOfStream = pDecoderData->fEndOfStream;

	pDecoderData->nBufferDone = 0;
	pDecoderData->nBufferInQueue = 0;
	pDecoderData->nBytesInQueue = 0;
	pDecoderData->nSamplesNum = 0;

	pDecoderData->nStartPosition = 0;
	pDecoderData->nEndPosition = 0;

	unsigned char *src;

	if(c_fReverse == 0)
	{
		// normal playing
		if(c_fManagedStream == 0)
		{
			// non-managed stream

			// check position, return if end of stream
			if(c_pchPosition > c_pchEnd)
				return DECODER_END_OF_DATA;
					
			// calculate number of samples we have
			unsigned int nSamplesHave = (c_pchEnd - c_pchPosition + 1) / c_nBlockAlign;
			if(nSamplesHave == 0)
				return DECODER_END_OF_DATA;
			// calculate number of bytes we need 
			
			// check if we have enough data
			if(nSamplesNeed > nSamplesHave)
				nSamplesNeed = nSamplesHave;

			nBytesNeed = nSamplesNeed * c_nBlockAlign;

			// set source
			src = c_pchPosition;
			// adjust position
			c_pchPosition += nBytesNeed;

		}
		else
		{
			// managed stream

			unsigned int size = c_Queue->GetSizeSum();
			// check if we have data in queue
			unsigned int nSamplesHave = c_Queue->GetSizeSum() / c_nBlockAlign;
			if(nSamplesHave == 0)
			{
				if(c_fEndOfStream)
					return DECODER_END_OF_DATA;
						
				return DECODER_MANAGED_NEED_MORE_DATA;
			}

	
			// check if we have enough data
			if(nSamplesNeed > nSamplesHave)
			{
			
				if(c_fEndOfStream)
					nSamplesNeed = nSamplesHave;
				else
					return DECODER_MANAGED_NEED_MORE_DATA;	
			}
			

			nBytesNeed = c_Queue->PullDataFifo((short*) pDecoderData->pSamples, nBytesNeed, (int*) &pDecoderData->nBufferDone);
			src = (unsigned char*) pDecoderData->pSamples;
			nSamplesNeed = nBytesNeed / c_nBlockAlign;
			pDecoderData->nBufferInQueue = c_Queue->GetCount();
			pDecoderData->nBytesInQueue = c_Queue->GetSizeSum();
		}
	}
	else
	{

		// reverse playing

		if(c_fManagedStream == 0)
		{
			if(c_pchPosition <= c_pchStart)
				return DECODER_END_OF_DATA;

			unsigned int nSamplesHave = (c_pchPosition - c_pchStart) / c_nBlockAlign;
			if(nSamplesHave == 0)
				return DECODER_END_OF_DATA;

			if(nSamplesNeed > nSamplesHave)
				nSamplesNeed = nSamplesHave;

			nBytesNeed = nSamplesNeed * c_nBlockAlign;

			c_pchPosition -= nBytesNeed;
			src = c_pchPosition;		
		}
		else
		{
			// can't play backward managed stream
		
			return DECODER_FATAL_ERROR;	
		}	
	}


	switch(c_nBitPerSample)
	{
		case 32:
		{
			if(c_isInfo.nChannel == 2)
			{
				PCM32StereoToPCM16Stereo((int*) src, nSamplesNeed, pDecoderData->pSamples);
				pDecoderData->nSamplesNum = nSamplesNeed;
				break;
			}
			
			// mono to stereo
			PCM32MonoToPCM16Stereo((int*)src, nSamplesNeed, pDecoderData->pSamples);
			pDecoderData->nSamplesNum = nSamplesNeed;
		}
		break;

		case 16:
		{
			if(c_isInfo.nChannel == 2)
			{
				// copy data
				memcpy(pDecoderData->pSamples, src, nBytesNeed);
				// set number of bytes copied into output buffer
				pDecoderData->nSamplesNum = nSamplesNeed;
				break;
			}

			// convert from mono to stereo
			PCM16MonoToPCM16Stereo((short*) src, nSamplesNeed, pDecoderData->pSamples);
			pDecoderData->nSamplesNum = nSamplesNeed;
		}
		break;

		case 8:
		{
			if(c_isInfo.nChannel == 2)
			{
				PCM8StereoUnsignedToPCM16Stereo(src, nSamplesNeed, pDecoderData->pSamples);
				pDecoderData->nSamplesNum = nSamplesNeed;
				break;
			}
			// mono to stereo
			PCM8MonoUnsignedToPCM16Stereo(src, nSamplesNeed, pDecoderData->pSamples);
			pDecoderData->nSamplesNum = nSamplesNeed;
		}
		break;
		
	}


	if(c_fReverse)
		PCM16StereoReverse((int*) pDecoderData->pSamples, pDecoderData->nSamplesNum, (int*) pDecoderData->pSamples);
	

	if(c_fBigEndian)
		PCM16StereoBEToPCM16StereoLE(pDecoderData->pSamples, pDecoderData->nSamplesNum , pDecoderData->pSamples);


	pDecoderData->nStartPosition = c_nCurrentPosition;

	if(c_fReverse)
		c_nCurrentPosition -= pDecoderData->nSamplesNum;
	else
		c_nCurrentPosition += pDecoderData->nSamplesNum;

	pDecoderData->nEndPosition = c_nCurrentPosition;

	
	return DECODER_OK;

}



int WPCMDecoder::GetBitrate(int fAverage)
{
	return c_isInfo.nFileBitrate;
}


int WPCMDecoder::SetReverseMode(int fReverse)
{
	ASSERT_W(c_fReady);

	err(DECODER_NO_ERROR);
	
	if(fReverse)
	{
		if(c_fManagedStream)
		{
			err(DECODER_REVERSE_NOT_SUPPORTED_MANAGED);
			return 0;
		}

		c_fReverse = 1;
	}
	else
		c_fReverse = 0;


	return 1;
}








void WPCMDecoder::err(unsigned int error_code)
{
	if(error_code > DECODER_UNKNOWN_ERROR)
		error_code = DECODER_UNKNOWN_ERROR;
			
	c_err_msg.errorW = g_pcm_error_strW[error_code];
}




int WPCMDecoder::OpenStream(WQueue *pQueue, int fDynamic, int param1, int param2)
{
	err(DECODER_NO_ERROR);

	c_Queue = pQueue;

	unsigned int nSize = pQueue->GetSizeSum();
	if(nSize == 0)
	{
		err(DECODER_NOT_VALID_PCM_STREAM);
		return 0;
	}


	unsigned char *ptr;
	unsigned int size; 
	if(pQueue->QueryFirstPointer((void**) &ptr, &size) == 0)
	{
		err(DECODER_NOT_VALID_PCM_STREAM);
		return 0;
	}

	c_fManagedStream = 0;

	if(param1 == 0)
	{

		c_pchStreamStart = ptr;
		c_nStreamSize = size;
		c_pchStreamEnd = c_pchStreamStart + c_nStreamSize - 1;


		if(fDynamic)
			c_fManagedStream = 1;
		
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
