/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  AAC decoder
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

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include "debug.h"
#include "waacdecoder.h"
#include "wtags.h"





enum {
	DECODER_NO_ERROR = 0,
	DECODER_FILE_OPEN_ERROR,
	DECODER_FILE_SEEK_POSITION_OUT_OF_BOUNDS,
	DECODER_FILESIZE_OUT_OF_BOUNDS,
	DECODER_NOT_VALID_AAC_STREAM,
	DECODER_MEMORY_ALLOCATION_FAIL,
	DECODER_NO_ID3_DATA,
	DECODER_SEEK_NOT_SUPPORTED_MANAGED,
	DECODER_SEEK_POSITION_OUT_OF_BOUNDS,
	DECODER_REVERSE_NOT_SUPPORTED_MANAGED,
	DECODER_FUNCTION_NOT_SUPPORTED,

	DECODER_UNKNOWN_ERROR
};



wchar_t *g_aac_error_strW[DECODER_UNKNOWN_ERROR + 1] = {
	L"No error.",
	L"AACDecoder: File open error.",
	L"AACDecoder: File sek position is out of bounds.",
	L"AACDecoder: File size is out of bounds.",
	L"AACDecoder: This is not valid AAC stream.",
	L"AACDecoder: Memory allocation fail."
	L"AACDecoder: No ID3 data.",
	L"AACDecoder: Seek is not supported on managed stream.",
	L"AACDecoder: Seek position is out of bounds.",
	L"AACDecoder: Reverse mod is not supported on managed stream.",
	L"AACDecoder: Function not supported in this decoder.",

	L"AACDecoder: Unknown error."

};

#define ADIF_MAX_SIZE 30 /* Should be enough */
#define ADTS_MAX_SIZE 10 /* Should be enough */


#define FRAME_INDEX_ERR 0xFFFFFFFF

// frame overlay on seek and reverse playing
#define FRAME_OVERLAY_NUM 1
// frame overlay on seek and reverse playing on SBR encoding
#define FRAME_OVERLAY_SBR_NUM 10

// number of frames to decode in one pass
#define INPUT_FRAME_NUMBER 10

// SBR decoder delay
#define SBR_DECODER_DELAY 5123


#define MAX_CHANNEL_NUM 8


static int g_sample_rates[] = {96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000};




unsigned char ** reallocate_aac_ptr(unsigned char **src, unsigned int old_size, unsigned int new_size);


WAACDecoder::WAACDecoder()
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
	c_fReverse = 0;
	c_fManagedStream = 0;
	c_fEndOfStream = 0;
	c_nCurrentPosition = 0;
	c_fReady = 0;
// =================================================
//	INITIALIZE INPUT_STREAM_INFO
	memset(&c_isInfo, 0, sizeof(INPUT_STREAM_INFO));
// ==================================================


	c_pFramePtr = 0;
	c_nLastAvailableFrameIndex = 0;
	c_nAllocatedFrameNumber = 0;
	c_nCurrentFrameIndex = 0;


	c_pchBufferPos = NULL;
	c_nBufferSize = 0;

	c_pchBufferAllocSize = 0;
	c_pchBuffer = NULL;


	c_nSkipSamlesNum = 0;	// number of samples to skip within frame to get accurate seek position
	c_nFrameOverlay = 0;	// frame overlay, number of frames decoded, but skipped to output after seek function
	c_nCurrentSeekIndex = 0;

	c_nSamplesPerFrame = 1024;
	c_nDecoderDelay = 0;

	c_nFrameOverlaySet = FRAME_OVERLAY_NUM;

	memset(&c_DecoderData, 0, sizeof(DECODER_DATA));

	c_pchManagedBuffer = 0;
	c_nManagedBufferMaxSize = 0;
	c_nManagedBufferLoad = 0;

	c_nBitrate = 0;
	c_nBitrateSum = 0;
	c_nBitrateNum = 0;
}


WAACDecoder::~WAACDecoder()
{
	Close();
	Uninitialize();
}



int WAACDecoder::Initialize(int param1, int param2, int param3, int param4)
{
	return 1;
}


int WAACDecoder::Uninitialize()
{
	

	return 1;
}


void WAACDecoder::Release()
{
	delete this;
}

DECODER_ERROR_MESSAGE *WAACDecoder::GetError()
{
	return &c_err_msg;
}



int WAACDecoder::_OpenFile(unsigned int fSkipExtraChecking)
{

	c_hDecoder = NeAACDecOpen();

	c_pchStart = c_pchStreamStart;
	c_nSize = c_nStreamSize;
	c_pchEnd = c_pchStart + c_nSize - 1;
	c_pchPosition = c_pchStart;



	// check for ID3v1 tag
	if(c_nSize > 128 && memcmp(c_pchEnd - 127, "TAG", 3) == 0)
	{
		// ID3v1 tag found, adjust stream pointers to exclude tag from stream
		c_pchEnd -= 128;
		c_nSize -= 128;	
	}

	// check for ID3v2 tag and skip ID3v2 tag
	if(( c_nSize  > 10 ) && (memcmp(c_pchStart, "ID3", 3) == 0) && c_pchStart[6] < 0x80 && c_pchStart[7] < 0x80 && c_pchStart[8] < 0x80 && c_pchStart[9] < 0x80)
	{ 
		// ID3v2 tag found
		// calculate ID3v2 frame size
		unsigned int id3v2_size	= DecodeSyncSafeInteger4( c_pchStart[6], c_pchStart[7], c_pchStart[8], c_pchStart[9]);
		
		// add ID3v2 header size
		id3v2_size += 10; // size of ID3V2 frame
		// check if this is valid ID3v2 tag
		if(c_nSize > id3v2_size )
		{
			// adjust stream pointers to exclude ID3v2 tag from stream
			c_pchStart += id3v2_size;
			c_nSize -= id3v2_size;	


			if(c_fManagedStream)
			{
				c_Queue->CutDataFifo(id3v2_size);

			}

		}
	}

	c_pchPosition = c_pchStart;


	if(c_nSize < ADTS_MAX_SIZE)
	{
		err(DECODER_NOT_VALID_AAC_STREAM);
		NeAACDecClose(c_hDecoder);
        return 0;
	}


	if(strncmp((char*) c_pchStart, "ADIF", 4) == 0)
	{
		err(DECODER_NOT_VALID_AAC_STREAM);
		NeAACDecClose(c_hDecoder);
        return 0;
	}




	// scan stream and determine length

	c_nLastAvailableFrameIndex = 0;



	c_pFramePtr = reallocate_aac_ptr(0, 0, 1000);
	if(c_pFramePtr == NULL)
	{
		err(DECODER_MEMORY_ALLOCATION_FAIL);
		NeAACDecClose(c_hDecoder);
        return 0;
	}

	c_nAllocatedFrameNumber = 1000;




	unsigned int frames = 0;
	int t_framelength = 0;
	int frame_length;
	int sr_idx;
	int ID;
	int type;
	int channels;
	int version;
	int sampling_rate;
	int bitrate;
	unsigned long *tmp_seek_table = NULL;

	unsigned char* pos = c_pchPosition;
	unsigned char *end = c_pchEnd - ADTS_MAX_SIZE;


	while(pos <= end)
	{
		// sync
        if (!( (pos[0] == 0xFF) && ( (pos[1] & 0xF6) == 0xF0)))
        {
			pos++;
			continue;
		}


		if(frames == 0)
        {
            ID = pos[1] & 0x08;
            type = (pos[2] & 0xC0) >> 6;
            sr_idx = (pos[2] & 0x3C) >> 2;
            channels = ((pos[2] & 0x01) << 2)|((pos[3] & 0xC0) >> 6);
			if(sr_idx > 11) // wrong sample rate, try sync to next frame
			{
				pos++;
				continue;
			}	
        }

        if (ID == 0)
            version = 4;
		else 
            version = 2;

        frame_length = ((((unsigned int)pos[3] & 0x3)) << 11) | (((unsigned int)pos[4]) << 3) | (pos[5] >> 5);
        t_framelength += frame_length;


		// save pointers to each frame for seek
		if(frames >= c_nAllocatedFrameNumber)
		{
			c_pFramePtr = reallocate_aac_ptr(c_pFramePtr, frames, c_nAllocatedFrameNumber + 1000);
			
			if(c_pFramePtr == NULL)
			{
				err(DECODER_MEMORY_ALLOCATION_FAIL);
				return 0;
			}

			c_nAllocatedFrameNumber += 1000;
		}


		c_pFramePtr[frames] = pos;

		pos += frame_length;
		frames++;
	}

	if(frames == 0)
	{
		err(DECODER_NOT_VALID_AAC_STREAM);
		NeAACDecClose(c_hDecoder);
        return 0;
	}
	

	c_nLastAvailableFrameIndex = frames - 1;

    sampling_rate = g_sample_rates[sr_idx];
    bitrate = (int)(((t_framelength / frames) * (sampling_rate / 1024.0)) + 0.5) * 8;



	c_config = NeAACDecGetCurrentConfiguration(c_hDecoder);
	c_config->defObjectType = LC;
	c_config->defSampleRate = 44100;
	c_config->outputFormat = FAAD_FMT_16BIT;
	c_config->downMatrix = 1;
	c_config->useOldADTSFormat = 0;

	if(NeAACDecSetConfiguration(c_hDecoder, c_config) == 0)
	{
		free(c_pFramePtr);
		c_pFramePtr = NULL;
		c_nAllocatedFrameNumber = 0;
		c_nLastAvailableFrameIndex = 0;
		err(DECODER_UNKNOWN_ERROR);
		NeAACDecClose(c_hDecoder);
		return 0;
	}


	unsigned long samplerate;
	unsigned char ch;
	
	int bytesconsumed = NeAACDecInit(c_hDecoder, c_pchPosition, c_nSize, &samplerate, &ch);

	if (bytesconsumed < 0 || (unsigned int) bytesconsumed >= c_nSize)
    {
		free(c_pFramePtr);
		c_pFramePtr = NULL;
		c_nAllocatedFrameNumber = 0;
		c_nLastAvailableFrameIndex = 0;
		err(DECODER_NOT_VALID_AAC_STREAM);
        return 0;
    }

	c_pchPosition += bytesconsumed;
	c_nSize -= bytesconsumed;

	
	// decode one frame
	int *tmp =  (int*) NeAACDecDecode(c_hDecoder, &c_frameInfo, c_pchPosition, c_nSize);
	if(tmp == NULL)
	{
		free(c_pFramePtr);
		c_pFramePtr = NULL;
		c_nAllocatedFrameNumber = 0;
		c_nLastAvailableFrameIndex = 0;
		err(DECODER_NOT_VALID_AAC_STREAM);
		NeAACDecClose(c_hDecoder);
        return 0;

	}


	if (c_frameInfo.error > 0)
	{
		free(c_pFramePtr);
		c_pFramePtr = NULL;
		c_nAllocatedFrameNumber = 0;
		c_nLastAvailableFrameIndex = 0;
		err(DECODER_NOT_VALID_AAC_STREAM);
		NeAACDecClose(c_hDecoder);
        return 0;	
	}



	c_nSamplesPerFrame = c_frameInfo.samples / c_frameInfo.channels;
	c_nDecoderDelay = 0;
	c_nFrameOverlaySet = FRAME_OVERLAY_NUM;
	switch(c_frameInfo.sbr)
	{
		default:
		case NO_SBR:
			c_nSamplesPerFrame = 1024;
		break;

		case SBR_UPSAMPLED:
			c_nSamplesPerFrame = 2048;
			c_nDecoderDelay = SBR_DECODER_DELAY;
			c_nFrameOverlaySet = FRAME_OVERLAY_SBR_NUM;
		break;

		case SBR_DOWNSAMPLED:
			c_nSamplesPerFrame = 512;
			c_nDecoderDelay = SBR_DECODER_DELAY;
			c_nFrameOverlaySet = FRAME_OVERLAY_SBR_NUM;
		break;

		case NO_SBR_UPSAMPLED:
			c_nSamplesPerFrame = 1024;
		break;

	}


	if(c_pchBuffer)
		free(c_pchBuffer);

	c_pchBufferAllocSize = 0;
	c_pchBuffer = NULL;

	c_pchBuffer = (int*) malloc(c_nSamplesPerFrame * INPUT_FRAME_NUMBER * 4);
	if(c_pchBuffer == NULL)
	{
		free(c_pFramePtr);
		c_pFramePtr = NULL;
		c_nAllocatedFrameNumber = 0;
		c_nLastAvailableFrameIndex = 0;
		err(DECODER_MEMORY_ALLOCATION_FAIL);
		NeAACDecClose(c_hDecoder);
        return 0;
	}

	c_pchBufferAllocSize = c_nSamplesPerFrame * INPUT_FRAME_NUMBER;


	// get informations of input stream
	// WE ALWAYS USE 16 bit per sample stereo



	c_isInfo.nSampleRate = samplerate;
	c_isInfo.nChannel = ch;
	c_isInfo.nBitPerSample = 16;
	c_isInfo.nBlockAlign = 4;
	c_isInfo.nFileBitrate = bitrate;
	c_isInfo.fVbr = 1;

	c_isInfo.nLength = frames * c_nSamplesPerFrame;

	strcpy(c_description, "AAC ADTS");

	switch(type)
	{
		case 0:
			strcat(c_description, " MAIN");	
		break;

		case 1:
			strcat(c_description, " LC");	
		break;

		case 2:
			strcat(c_description, " SSR");	
		break;

		case 3:
			strcat(c_description, " LTP");	
		break;

	}

	if(version == 2)
		strcat(c_description, " MPEG2");	
	else
		strcat(c_description, " MPEG4");	



	if(ch >= 2)
		strcat(c_description, " STEREO");
	else
		strcat(c_description, " MONO");
	

	c_isInfo.pchStreamDescription = c_description;
	c_nCurrentPosition = 0;


	c_nSkipSamlesNum = 0;	// number of samples to skip within frame to get accurate seek position
	c_nFrameOverlay = 0;	// frame overlay, number of frames decoded, but skipped to output after seek function
	c_nBufferSize = 0;

	c_fReady = 1;

	return 1;


}

int WAACDecoder::Close()
{


		// =============================================================
// free accurate seek structures and initialize values
	if(c_pFramePtr)
		free(c_pFramePtr);

	c_pFramePtr = NULL;
	c_nLastAvailableFrameIndex = 0;
	c_nAllocatedFrameNumber = 0;


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

// ================================

	//	INITIALIZE INPUT_STREAM_INFO
	memset(&c_isInfo, 0, sizeof(INPUT_STREAM_INFO));

// ==================================


	c_nCurrentPosition = 0;


// ====================================================


	c_pchBufferPos = NULL;
	c_nBufferSize = 0;

	if(c_pchBuffer)
		free(c_pchBuffer);

	c_pchBufferAllocSize = 0;
	c_pchBuffer = NULL;


	c_nSkipSamlesNum = 0;	// number of samples to skip within frame to get accurate seek position
	c_nFrameOverlay = 0;	// frame overlay, number of frames decoded, but skipped to output after seek function
	c_nBufferSize = 0;
	c_nCurrentSeekIndex = 0;

	c_nDecoderDelay = 0;

	if(c_pchManagedBuffer)
		free(c_pchManagedBuffer);

	c_pchManagedBuffer = 0;
	c_nManagedBufferMaxSize = 0;

	c_nBitrate = 0;

	NeAACDecClose(c_hDecoder);

	c_fReady = 0;

	return 1;
}



INPUT_STREAM_INFO *WAACDecoder::GetStreamInfo()
{
	ASSERT_W(c_fReady);
	return &c_isInfo;
}


wchar_t **WAACDecoder::GetID3Info(int version, char *pStream, unsigned int nStreamSize, int param1, int param2)
{
	ASSERT_W(c_pchStreamStart);
	err(DECODER_NO_ERROR);
	return c_tag.LoadID3Info(c_pchStreamStart, c_nStreamSize, version);

}


int WAACDecoder::Seek(unsigned int nSamples)
{
	ASSERT_W(c_fReady);

	err(DECODER_NO_ERROR);

	c_nBufferSize = 0;

	c_nBitrate = 0;
	c_nBitrateSum = 0;
	c_nBitrateNum = 0;

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

		
		if(nFrameIndex >= c_nFrameOverlaySet)
		{
			c_nFrameOverlay = c_nFrameOverlaySet;
			nFrameIndex -= 	c_nFrameOverlaySet;
		}

		nNewPosition = c_pFramePtr[nFrameIndex];
		// calculate number of samples to skip within frame
		c_nSkipSamlesNum = (nSamples - (nFrameIndex + c_nFrameOverlay) * c_nSamplesPerFrame);
			

		c_nSkipSamlesNum += c_nDecoderDelay;
		c_nCurrentFrameIndex = nFrameIndex;
		c_nCurrentSeekIndex = nFrameIndex;

		if(c_fReverse)
			c_nSkipSamlesNum = 0;

		NeAACDecPostSeekReset(c_hDecoder, nFrameIndex);
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


int WAACDecoder::GetData(DECODER_DATA *pDecoderData)
{
	ASSERT_W(c_fReady);
	ASSERT_W(pDecoderData);
	ASSERT_W(pDecoderData->pSamples);

	c_DecoderData.nBufferDone = 0;

	
	c_fEndOfStream = pDecoderData->fEndOfStream;
	
	unsigned int nSamplesNeed = pDecoderData->nSamplesNum;
	unsigned int nSamplesHave = 0;

	int *pchOutBuffer = (int*) pDecoderData->pSamples;	// use int because here we are working always with 16 bit stereo samples



	pDecoderData->nBufferDone = 0;
	pDecoderData->nBufferInQueue = 0;
	pDecoderData->nBytesInQueue = 0;
	pDecoderData->nSamplesNum = 0;

	pDecoderData->nStartPosition = 0;
	pDecoderData->nEndPosition = 0;

		// non-managed stream

		while(nSamplesNeed)
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



int WAACDecoder::GetBitrate(int fAverage)
{
	if(fAverage && c_nBitrateNum)
		return (c_nBitrateSum / c_nBitrateNum) * 8 * c_isInfo.nSampleRate / c_nSamplesPerFrame;
	

	return c_nBitrate;
}


int WAACDecoder::SetReverseMode(int fReverse)
{
	ASSERT_W(c_fReady);

	err(DECODER_NO_ERROR);

	c_nBufferSize = 0;
	
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



void WAACDecoder::err(unsigned int error_code)
{
	if(error_code > DECODER_UNKNOWN_ERROR)
		error_code = DECODER_UNKNOWN_ERROR;
			
	c_err_msg.errorW = g_aac_error_strW[error_code];
}



unsigned char ** reallocate_aac_ptr(unsigned char **src, unsigned int old_size, unsigned int new_size)
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


int WAACDecoder::_FillBuffer()
{

	unsigned int output_size = 0;
	unsigned int nFrameCount = INPUT_FRAME_NUMBER;

	c_pchBufferPos = c_pchBuffer;
	c_nBufferSize = 0;

	// reverse mode
	if(c_fReverse)
	{
	
		ASSERT_W(c_pFramePtr); // we need pointer to each frame

		c_nSkipSamlesNum = 0;

		if(c_nCurrentSeekIndex == 0) // if we are at song start, end of data
			return DECODER_END_OF_DATA;

		// seek backward specified number of frames
		unsigned int nIndex;

		// try frame overlay
		if(c_nCurrentSeekIndex < ( nFrameCount + c_nFrameOverlaySet))
		{
			if(c_nCurrentSeekIndex < nFrameCount)
			{
				// no frame overlay because we are near stream beginning
				nFrameCount = c_nCurrentSeekIndex;
				nIndex =  0;
				c_nCurrentSeekIndex = 0;
				c_nFrameOverlay = 0;

			}
			else
			{

				// no frame overlay because we are near stream beginning
				nIndex =  c_nCurrentSeekIndex - nFrameCount;
				c_nCurrentSeekIndex -= nFrameCount;
				c_nFrameOverlay = 0;
			}
		}
		else
		{
			nIndex =  c_nCurrentSeekIndex - nFrameCount - c_nFrameOverlaySet;
			c_nCurrentSeekIndex -= nFrameCount;
			c_nFrameOverlay = c_nFrameOverlaySet;
		}


		ASSERT_W(nIndex <= c_nLastAvailableFrameIndex);

		c_pchPosition = c_pFramePtr[nIndex];

		NeAACDecPostSeekReset(c_hDecoder, nIndex);

	}


	if(c_fManagedStream) // managed stream
	{

		c_nSkipSamlesNum = 0;
		// load data from queue
		unsigned int have = c_Queue->GetSizeSum();

		if(have < FAAD_MIN_STREAMSIZE * MAX_CHANNEL_NUM) // check if we have enough data
		{
			if(c_fEndOfStream)
			{
				if(c_nBufferSize == 0 && have == 0)
					return DECODER_END_OF_DATA;

			}
			else
				return DECODER_MANAGED_NEED_MORE_DATA;
		}	
		
		
		unsigned int nDone = 0;
		unsigned int ret = c_Queue->PullDataFifo(c_pchManagedBuffer, FAAD_MIN_STREAMSIZE * MAX_CHANNEL_NUM, (int*) &nDone);
		c_DecoderData.nBufferDone += nDone;

		if(NeAACDecDecode2(c_hDecoder, &c_frameInfo, c_pchManagedBuffer, ret, (void**) &c_pchBufferPos, c_pchBufferAllocSize) == NULL)
		{
			
			if((unsigned int) c_frameInfo.bytesconsumed < ret)
			{
				c_Queue->PushFirst(c_pchManagedBuffer + c_frameInfo.bytesconsumed, ret - (unsigned int) c_frameInfo.bytesconsumed);
					
			}

			if(c_fEndOfStream == 0)
				return DECODER_MANAGED_NEED_MORE_DATA;
			
		}


		if (c_frameInfo.error > 0 || c_frameInfo.channels == 0)
		{
			
			if((unsigned int) c_frameInfo.bytesconsumed < ret)
			{
				c_Queue->PushFirst(c_pchManagedBuffer + c_frameInfo.bytesconsumed, ret - (unsigned int) c_frameInfo.bytesconsumed);
					
			}

			if(c_fEndOfStream == 0)
				return DECODER_MANAGED_NEED_MORE_DATA;
			
		}

		
		c_DecoderData.nBufferInQueue = c_Queue->GetCount();
		c_DecoderData.nBytesInQueue = c_Queue->GetSizeSum();

		c_nBufferSize = c_frameInfo.samples / c_frameInfo.channels;

		c_pchBufferPos = c_pchBuffer;

		if((unsigned int) c_frameInfo.bytesconsumed < ret)
		{
			c_Queue->PushFirst(c_pchManagedBuffer + c_frameInfo.bytesconsumed, ret - (unsigned int) c_frameInfo.bytesconsumed);
					
		}

	}
	else
	{
		while(nFrameCount) // do frame overlay
		{
			while(c_nFrameOverlay)
			{

				// check position, return if end of stream
				if(c_pchPosition + ADTS_MAX_SIZE > c_pchEnd)
					return DECODER_END_OF_DATA;

			
				c_pchBufferPos =  (int*) NeAACDecDecode2(c_hDecoder, &c_frameInfo, c_pchPosition, c_pchEnd + 1 - c_pchPosition, (void**) &c_pchBuffer, c_pchBufferAllocSize);
			
				if(c_pchBufferPos == NULL)
					return DECODER_END_OF_DATA;	

				if (c_frameInfo.error > 0)
				{
					return DECODER_END_OF_DATA;	
				}

				if(c_frameInfo.bytesconsumed == 0)
					c_frameInfo.bytesconsumed++;

				c_pchPosition += c_frameInfo.bytesconsumed;

				c_nFrameOverlay--;
			}


			// check position, return if end of stream
			if(c_pchPosition + ADTS_MAX_SIZE > c_pchEnd)
				return DECODER_END_OF_DATA;

			
			if(NeAACDecDecode2(c_hDecoder, &c_frameInfo, c_pchPosition, c_pchEnd + 1 - c_pchPosition, (void**) &c_pchBufferPos, c_pchBufferAllocSize) == NULL)
			{
				if(c_nBufferSize == 0)
					return DECODER_END_OF_DATA;	

			}


			if (c_frameInfo.error > 0)
			{
				if(c_nBufferSize == 0)
					return DECODER_END_OF_DATA;	
			}

			if(c_frameInfo.bytesconsumed == 0)
				c_frameInfo.bytesconsumed++;

			

			c_pchPosition += c_frameInfo.bytesconsumed;

			if(c_frameInfo.samples != 0 && c_frameInfo.channels != 0)
			{
				c_nBufferSize += (c_frameInfo.samples / c_frameInfo.channels);

				c_pchBufferPos = &c_pchBuffer[c_nBufferSize];
			}

			c_nBitrate = c_frameInfo.bytesconsumed * 8 * c_frameInfo.samplerate / c_nSamplesPerFrame;
			c_nBitrateSum += c_frameInfo.bytesconsumed;
			c_nBitrateNum++;
			
			nFrameCount--;
		}	
	}


	if(c_fReverse)
		PCM16StereoReverse((int*) c_pchBuffer, c_nBufferSize, (int*) c_pchBuffer);


	c_pchBufferPos = c_pchBuffer;
	// initialize buffer
	if(c_nBufferSize > c_nSkipSamlesNum)
	{
		c_pchBufferPos = &c_pchBuffer[c_nSkipSamlesNum];
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

unsigned int WAACDecoder::getFrameIndex(unsigned int nSamples)
{
	ASSERT_W(c_pFramePtr);
	// calculate number of frames 
	unsigned int nNeededFrameIndex = nSamples / c_nSamplesPerFrame;
	if(nNeededFrameIndex > c_nLastAvailableFrameIndex)
		return 0;

	return nNeededFrameIndex;
}

int WAACDecoder::OpenStream(WQueue *pQueue, int fDynamic, int param1, int param2)
{
	err(DECODER_NO_ERROR);

	c_Queue = pQueue;

	unsigned int nSize = pQueue->GetSizeSum();
	if(nSize == 0)
	{
		err(DECODER_NOT_VALID_AAC_STREAM);
		return 0;
	}


	unsigned char *ptr;
	unsigned int size; 
	if(pQueue->QueryFirstPointer((void**) &ptr, &size) == 0)
	{
		err(DECODER_NOT_VALID_AAC_STREAM);
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


			c_nManagedBufferMaxSize = FAAD_MIN_STREAMSIZE * 8;
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
