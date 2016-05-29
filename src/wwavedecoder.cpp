/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  Wave decoder
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
#include "wwavedecoder.h"
#include "wtags.h"


enum {
	DECODER_NO_ERROR = 0,
	DECODER_FILE_OPEN_ERROR,
	DECODER_FILE_SEEK_POSITION_OUT_OF_BOUNDS,
	DECODER_FILESIZE_OUT_OF_BOUNDS,
	DECODER_NOT_VALID_WAVE_STREAM,
	DECODER_COMPRESSED_WAVE_NOT_SUPPORTED,
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





wchar_t *g_wave_error_strW[DECODER_UNKNOWN_ERROR + 1] = {
	L"No error.",
	L"WaveDecoder: File open error.",
	L"WaveDecoder: File sek position is out of bounds.",
	L"WaveDecoder: File size is out of bounds.",
	L"WaveDecoder: This is not valid WAVE stream.",
	L"WaveDecoder: Compressed WAVE is not supported.",
	L"WaveDecoder: Unsupported channel number.",
	L"WaveDecoder: Unsupported bits per sample.",
	L"WaveDecoder: Memory allocation fail."
	L"WaveDecoder: No ID3 data.",
	L"WaveDecoder: Seek is not supported on managed stream.",
	L"WaveDecoder: Seek position is out of bounds.",
	L"WaveDecoder: Reverse mod is not supported on managed stream.",

	L"WaveDecoder: Function not supported.",
	L"WaveDecoder: Unknown error."

};

// number of samples to output
#define INPUT_PCM_BUFFER_SIZE 4410


typedef struct {
	unsigned int id;
	unsigned int size;
	unsigned char data;
} RIFF_CHUNK;


#define RIFF_CHUNK_SIZE 8

typedef struct {
	unsigned int ckID;
	unsigned int ckSize;
	unsigned short CompressionCode;
	unsigned short NumberOfChannels;
	unsigned int SampleRate;
	unsigned int AvgBytesPerSec;
	unsigned short BlockAlign;
	unsigned short BitPerSample;
	unsigned short ExtraFormatBytes;
} FMT_CHUNK;

RIFF_CHUNK * get_riff_subchunk(char *data, unsigned int size, char *id, unsigned int fSkipExtraCheck );
int get_riff_wave_id3_field(wchar_t **field, char *data, unsigned int size,  char *id);

wchar_t *get_riff_wave_id3_field(char *data, unsigned int size,  char *id);


WWaveDecoder::WWaveDecoder()
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
	c_nCurrentPosition = 0;
	c_nSeekPosition = 0;
	c_fReady = 0;
// =================================================
//	INITIALIZE INPUT_STREAM_INFO
	memset(&c_isInfo, 0, sizeof(INPUT_STREAM_INFO));
// ==================================================
//	INITIALIZE ID3_INFO
	unsigned int i;
	for(i = 0; i < ID3_FIELD_NUMBER_EX; i++)
	{
		c_fields[i] = 0;
	}
// ==================================================

}


WWaveDecoder::~WWaveDecoder()
{
	Close();
	Uninitialize();
}



int WWaveDecoder::Initialize(int param1, int param2, int param3, int param4)
{

	return 1;
}


int WWaveDecoder::Uninitialize()
{

	return 1;
}


void WWaveDecoder::Release()
{
	delete this;
}

DECODER_ERROR_MESSAGE *WWaveDecoder::GetError()
{
	return &c_err_msg;
}







int WWaveDecoder::_OpenFile(unsigned int fSkipExtraChecking)
{


	// read RIFF chunk and check if this is valid file
	RIFF_CHUNK *riff = get_riff_subchunk((char*) c_pchStreamStart, c_nStreamSize, "RIFF", fSkipExtraChecking);
	if(riff == 0)
	{
		err(DECODER_NOT_VALID_WAVE_STREAM);
		return 0;
	}
	
	if(fSkipExtraChecking)
	{
		if((unsigned char*) (riff + 8) > c_pchStreamEnd)
		{
			err(DECODER_NOT_VALID_WAVE_STREAM);
			return 0;
		}
	}
	else if(riff->size + 8 != c_nStreamSize)
	{
		err(DECODER_NOT_VALID_WAVE_STREAM);
		return 0;
	}
	
	// check RIFF chunk type
	if(strncmp((char*) &riff->data, "WAVE", 4) != 0)
	{
		err(DECODER_NOT_VALID_WAVE_STREAM);
		return 0;
	}

	char *data = (char*) &riff->data + 4;
	unsigned int size = riff->size - 4;
	// get fmt subchunk
	RIFF_CHUNK *fmtck = get_riff_subchunk(data, size, "fmt ", fSkipExtraChecking);
	if(fmtck == 0)
	{
		err(DECODER_NOT_VALID_WAVE_STREAM);
		return 0;
	}


	if(fSkipExtraChecking)
	{
		if((unsigned char*) (fmtck + 8) > c_pchStreamEnd)
		{
			err(DECODER_NOT_VALID_WAVE_STREAM);
			return 0;
		}
	}

	// get data subchunk
	RIFF_CHUNK *pcm_data = get_riff_subchunk(data, size, "data", fSkipExtraChecking);
	if(pcm_data == 0)
	{
		err(DECODER_NOT_VALID_WAVE_STREAM);
		return 0;
	}

	if(fSkipExtraChecking)
	{
		if((unsigned char*) (pcm_data + 8) > c_pchStreamEnd)
		{
			err(DECODER_NOT_VALID_WAVE_STREAM);
			return 0;
		}
	}

	FMT_CHUNK *fmt = (FMT_CHUNK*) fmtck;
	// check compression code, compressed wave data isn't supported yet
	if(fmt->CompressionCode != 1)
	{
		err(DECODER_COMPRESSED_WAVE_NOT_SUPPORTED);
		return 0;
	}
	// support only mono and stereo, don't support multiple channel mode
	if(fmt->NumberOfChannels == 0 || fmt->NumberOfChannels > 2)
	{
		err(DECODER_UNSUPPORTED_CHANNEL_NUMBER);
		return 0;
	}

	// support only 8 bits and 16 bits per sample
	if(fmt->BitPerSample != 8 && fmt->BitPerSample != 16 && fmt->BitPerSample != 32)
	{
		err(DECODER_UNSUPPORTED_BIT_PER_SAMPLE);
		return 0;
	}

	c_pchStart = (unsigned char*) &pcm_data->data;
	c_nSize = pcm_data->size;
	c_pchPosition = c_pchStart;

	// fix stream size if data from RIFF header are incorrect, or for managed stream
	unsigned int RealSize = c_Queue->GetSizeSum() - ( c_pchStart - c_pchStreamStart );
	if(c_nSize > RealSize)
		c_nSize = RealSize;
			
	c_pchEnd = c_pchStart + c_nSize - 1;
	// remove riff header from managed stream



	if(c_fManagedStream)
	{
		if(c_Queue->CutDataFifo(c_pchStart - c_pchStreamStart) == 0)
		{
			err(DECODER_MEMORY_ALLOCATION_FAIL);
			return 0;
		}
	}


	// get informations of input stream
	// WE ALWAYS USE 16 bit per sample

	
	c_nBitPerSample = fmt->BitPerSample;
	c_nBlockAlign = fmt->BlockAlign;




	c_isInfo.nSampleRate = fmt->SampleRate;
	c_isInfo.nChannel = fmt->NumberOfChannels;
	c_isInfo.nBitPerSample = 16;
	c_isInfo.nBlockAlign = 4;
	c_isInfo.nFileBitrate = fmt->AvgBytesPerSec * 8;
	c_isInfo.fVbr = 0;
	c_isInfo.nLength = pcm_data->size / fmt->BlockAlign;

	char *channel = "STEREO";
	if(fmt->NumberOfChannels == 1)
		channel = "MONO";


	sprintf(c_pchDesc, "WAVE PCM %u-bit UNCOMPRESSED %s", fmt->BitPerSample, channel);
	c_isInfo.pchStreamDescription =  c_pchDesc;

	c_nCurrentPosition = 0;
	c_nSeekPosition = 0;

	c_fReady = 1;

	return 1;


}

int WWaveDecoder::Close()
{
	FreeID3Fields(c_fields, ID3_FIELD_NUMBER_EX);


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
	c_nSeekPosition = 0;

	c_fReady = 0;

	return 1;
}



RIFF_CHUNK * get_riff_subchunk(char *data, unsigned int size, char *id, unsigned int fSkipExtraCheck )
{
//
//
//	PARAMETERS:
//		data
//			Pointer to data field of parent chunk
//
//		size
//			Size of data field
//
//		id
//			Subchank identifier ( 4 bytes ID )
//
//		fSkipExtraCheck
//			Skip extra checking. Don't check if size of chunk is inside filesize
//
//	RETURN VLAUES:
//		0	- there is no subchunk with this ID
//		pointer to subchunk
//
//

	RIFF_CHUNK *upper_limit = (RIFF_CHUNK*) (data + size);
	RIFF_CHUNK *ck = (RIFF_CHUNK*) data;

	if(size < RIFF_CHUNK_SIZE)
		return 0;
		
	int allign = 0;
	unsigned int nId;
	char *sId = (char*) &nId;
	sId[0] = id[0];
	sId[1] = id[1];
	sId[2] = id[2];
	sId[3] = id[3];


	if(fSkipExtraCheck == 0)
	{

		while(ck <= upper_limit  && ( ck->size + RIFF_CHUNK_SIZE <= size))
		{
			allign = 0;
			if(( ck->size + 8 ) % 2)
				allign = 1;

			if(ck->id == nId)
			{
				if( (RIFF_CHUNK*) ((unsigned int) ck + ck->size) > upper_limit)
					return 0;

				return ck;
			}

			ck = (RIFF_CHUNK*) ((unsigned int) ck + ck->size + 8 + allign);
		}
	}
	else
	{
		while(ck <= upper_limit)
		{
			allign = 0;
			if(( ck->size + 8 ) % 2)
				allign = 1;

			if(ck->id == nId)
				return ck;

			ck = (RIFF_CHUNK*) ((unsigned int) ck + ck->size + 8 + allign);
		}
	}


	return 0;
}


INPUT_STREAM_INFO *WWaveDecoder::GetStreamInfo()
{
	ASSERT_W(c_fReady);
	return &c_isInfo;
}



wchar_t **WWaveDecoder::GetID3Info(int version, char *pStream, unsigned int nStreamSize, int param1, int param2)
{

	err(DECODER_NO_ERROR);

	FreeID3Fields(c_fields, ID3_FIELD_NUMBER_EX);

		// allocate and initialize new fields
	if(AllocateID3Fields(c_fields, ID3_FIELD_NUMBER_EX, 1) == 0)
	{
		err(DECODER_MEMORY_ALLOCATION_FAIL);
		return NULL;
	}


// search for list chunk
	RIFF_CHUNK *riff = get_riff_subchunk((char*) c_pchStreamStart, c_nStreamSize, "RIFF", 0);
	if(riff == 0)
	{
		err(DECODER_NOT_VALID_WAVE_STREAM);
		return NULL;
	}

	char *data = (char*) &riff->data;
	unsigned int size = riff->size;

		
	if(strncmp((char*) data, "WAVE", 4) != 0)
	{
		err(DECODER_NOT_VALID_WAVE_STREAM);
		return NULL;
	}

	data += 4;
	size -= 4;

	wchar_t *tmp;



	RIFF_CHUNK *disp = get_riff_subchunk( data, size, "DISP", 0);
	if(disp && disp->data == CF_TEXT && disp->size > 4)
	{
		tmp = ANSIToUTF16((char*) (&disp->data + 4), disp->size - 4);
		if(tmp)
		{
			if(c_fields[ID3_INFO_TITLE])
				free(c_fields[ID3_INFO_TITLE]);

			c_fields[ID3_INFO_TITLE] = tmp;
		}
			
	}


	RIFF_CHUNK *list = get_riff_subchunk((char*) data, size, "LIST", 0);
	if(list == 0)
	{
		err(DECODER_NO_ID3_DATA);
		return NULL;
	}

	if(strncmp((char*) &list->data, "INFO", 4) != 0)
	{
		err(DECODER_NO_ID3_DATA);
		return NULL;
	}
	
	data = (char*) &list->data + 4;
	size = list->size - 4;


	tmp = get_riff_wave_id3_field(data, size, "IART");
	if(tmp)
	{
		if(c_fields[ID3_INFO_ARTIST])
			free(c_fields[ID3_INFO_ARTIST]);

		c_fields[ID3_INFO_ARTIST] = tmp;
	}

	tmp = get_riff_wave_id3_field(data, size, "ICMT");
	if(tmp)
	{
		if(c_fields[ID3_INFO_COMMENT])
			free(c_fields[ID3_INFO_COMMENT]);

		c_fields[ID3_INFO_COMMENT] = tmp;
	}

	tmp = get_riff_wave_id3_field(data, size, "IGNR");
	if(tmp)
	{
		if(c_fields[ID3_INFO_GENRE])
			free(c_fields[ID3_INFO_GENRE]);

		c_fields[ID3_INFO_GENRE] = tmp;
	}

	tmp = get_riff_wave_id3_field(data, size, "ICRD");
	if(tmp)
	{
		if(c_fields[ID3_INFO_YEAR])
			free(c_fields[ID3_INFO_YEAR]);

		c_fields[ID3_INFO_YEAR] = tmp;
	}

	tmp = get_riff_wave_id3_field(data, size, "INAM");
	if(tmp)
	{
		if(c_fields[ID3_INFO_ALBUM])
			free(c_fields[ID3_INFO_ALBUM]);

		c_fields[ID3_INFO_ALBUM] = tmp;
	}

	tmp = get_riff_wave_id3_field(data, size, "ISBJ");
	if(tmp)
	{
		if(c_fields[ID3_INFO_TRACK])
			free(c_fields[ID3_INFO_TRACK]);

		c_fields[ID3_INFO_TRACK] = tmp;
	}

	return c_fields;

}

wchar_t *get_riff_wave_id3_field(char *data, unsigned int size,  char *id)
{


	RIFF_CHUNK *ck = get_riff_subchunk(data, size, id, 0);
	if(ck)
	{
		wchar_t *tmp = ANSIToUTF16((char*) &ck->data, ck->size);
		return tmp;
	}

	return NULL;

}



int get_riff_wave_id3_field(wchar_t **field, char *data, unsigned int size,  char *id)
{
//
//
//	RETURN:
//		0	- there is no field with this ID, field data is "\0"
//		1	- data retreived
//		-1	- memory allocation error, field is NULL
	

/*
	if(*field)
		free(*field);

	*field = 0;

	RIFF_CHUNK *ck = get_riff_subchunk(data, size, id, 0);
	if(ck)
	{
		wchar_t *tmp = ANSIToUTF16((char*) &ck->data, ck->size);
		if(tmp)
		{
			*field = tmp;
			return 1;
		}

		*field = (char*) malloc(ck->size + 1);	
		if(*field == NULL)
		{
			*field = (char*) malloc(1);
			if(*field == NULL)
				return -1;

			**field = 0;
			return 1;
		}

		memcpy(*field, &ck->data, ck->size);
		(*field)[ck->size] = 0;
		return 1;


	}

	*field = (char*) malloc(1);
	if(*field == NULL)
		return -1;

	**field = 0;
	*/
	return 0;

}


int WWaveDecoder::Seek(unsigned int nSamples)
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
	c_nSeekPosition = nSamples;

	return 1;

}


int WWaveDecoder::GetData(DECODER_DATA *pDecoderData)
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
			{
				pDecoderData->nStartPosition = c_nCurrentPosition;
				pDecoderData->nEndPosition = c_nCurrentPosition;
				return DECODER_END_OF_DATA;
			}
			// calculate number of bytes we need 
			
			// check if we have enough data
			if(nSamplesNeed > nSamplesHave)
				nSamplesNeed = nSamplesHave;

			nBytesNeed = nSamplesNeed * c_nBlockAlign;

			// set source
			src = c_pchPosition;
			// adjust position
			c_pchPosition += nBytesNeed;


			pDecoderData->nStartPosition = c_nCurrentPosition;
			c_nCurrentPosition += nSamplesNeed;
			pDecoderData->nEndPosition  = c_nCurrentPosition;



		}
		else
		{
			// managed stream
		
			unsigned int nSamplesHave = c_Queue->GetSizeSum() / c_nBlockAlign;

			// if our queue is empty
			if(nSamplesHave == 0)
			{
				if(c_fEndOfStream)
				{
					pDecoderData->nStartPosition = c_nCurrentPosition;
					pDecoderData->nEndPosition = c_nCurrentPosition;
					return DECODER_END_OF_DATA;
				}
				
				pDecoderData->nStartPosition = c_nCurrentPosition;
				pDecoderData->nEndPosition = c_nCurrentPosition;		
				return DECODER_MANAGED_NEED_MORE_DATA;
			}

	
			// check if we have enough data to fill user buffer
			if(nSamplesNeed > nSamplesHave)
			{
			
				if(c_fEndOfStream)	// end of stream, give user data we have
					nSamplesNeed = nSamplesHave;
				else
				{
					pDecoderData->nStartPosition = c_nCurrentPosition;
					pDecoderData->nEndPosition = c_nCurrentPosition;
					return DECODER_MANAGED_NEED_MORE_DATA;	// ask user for more input data
				}
			}
			

			nBytesNeed = c_Queue->PullDataFifo((short*) pDecoderData->pSamples, nBytesNeed, (int*) &pDecoderData->nBufferDone);
			src = (unsigned char*) pDecoderData->pSamples;
			nSamplesNeed = nBytesNeed / c_nBlockAlign;
			pDecoderData->nBufferInQueue = c_Queue->GetCount();
			pDecoderData->nBytesInQueue = c_Queue->GetSizeSum();


			pDecoderData->nStartPosition = c_nCurrentPosition;
			c_nCurrentPosition += nSamplesNeed;
			pDecoderData->nEndPosition = c_nCurrentPosition;
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

			pDecoderData->nStartPosition = c_nCurrentPosition;
			c_nCurrentPosition -= nSamplesNeed;
			pDecoderData->nEndPosition = c_nCurrentPosition;		
		}
		else
		{
			// can't play backward managed stream
			pDecoderData->nStartPosition = c_nCurrentPosition;
			pDecoderData->nEndPosition = c_nCurrentPosition;
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
	
	pDecoderData->fReverse = c_fReverse;
	pDecoderData->nSeekPosition = c_nSeekPosition;
	return DECODER_OK;
}





int WWaveDecoder::GetBitrate(int fAverage)
{
	return c_isInfo.nFileBitrate;
}


int WWaveDecoder::SetReverseMode(int fReverse)
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





void WWaveDecoder::err(unsigned int error_code)
{
	if(error_code > DECODER_UNKNOWN_ERROR)
		error_code = DECODER_UNKNOWN_ERROR;

	c_err_msg.errorW = g_wave_error_strW[error_code];

}


int WWaveDecoder::OpenStream(WQueue *pQueue, int fDynamic, int param1, int param2)
{
	err(DECODER_NO_ERROR);

	c_Queue = pQueue;

	unsigned int nSize = pQueue->GetSizeSum();
	if(nSize == 0)
	{
		err(DECODER_NOT_VALID_WAVE_STREAM);
		return 0;
	}


	unsigned char *ptr;
	unsigned int size; 
	if(pQueue->QueryFirstPointer((void**) &ptr, &size) == 0)
	{
		err(DECODER_NOT_VALID_WAVE_STREAM);
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
		
		if(_OpenFile(c_fManagedStream) == 0)
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
