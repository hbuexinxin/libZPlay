/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  FLAC decoder
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
 *	SUPPORTED BY:
 * ============================================================================
 * FLAC - Free Lossless Audio Codec
 * Copyright (C) 2001,2002,2003,2004,2005,2006,2007  Josh Coalson
 * http://flac.sourceforge.net/
 *
 * This file is part the FLAC project.  FLAC is comprised of several
 * components distributed under difference licenses.  The codec libraries
 * are distributed under Xiph.Org's BSD-like license (see the file
 * XIPH.TXT in this distribution).  All other programs, libraries, and
 * plugins are distributed under the LGPL or GPL (see LGPL.TXT and
 * GPL.TXT).  The documentation is distributed under the Gnu FDL (see
 * FDL.TXT).  Each file in the FLAC distribution contains at the top the
 * terms under which it may be distributed.
 *
 * Since this particular file is relevant to all components of FLAC,
 * it may be distributed under the Xiph.Org license, which is the least
 * restrictive of those mentioned above.  See the file XIPH.TXT in this
 * distribution.
 * ============================================================================
 *
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "debug.h"
#include "wflacdecoder.h"
#include "wtags.h"



enum {
	DECODER_NO_ERROR = 0,
	DECODER_FILE_OPEN_ERROR,
	DECODER_FILE_SEEK_POSITION_OUT_OF_BOUNDS,
	DECODER_FILESIZE_OUT_OF_BOUNDS,
	DECODER_NOT_VALID_FLAC_STREAM,
	DECODER_UNSUPPORTED_SAMPLERATE,
	DECODER_UNSUPPORTED_CHANNEL_NUMBER,
	DECODER_UNSUPPORTED_BIT_PER_SAMPLE,
	DECODER_MEMORY_ALLOCATION_FAIL,
	DECODER_NO_ID3_DATA,
	DECODER_SEEK_NOT_SUPPORTED_MANAGED,
	DECODER_SEEK_POSITION_OUT_OF_BOUNDS,
	DECODER_REVERSE_NOT_SUPPORTED_MANAGED,
	DECODER_FAIL_INITIALIZATION,

	DECODER_FUNCTION_NOT_SUPPORTED,
	DECODER_UNKNOWN_ERROR
};



wchar_t *g_flac_error_strW[DECODER_UNKNOWN_ERROR + 1] = {
	L"No error.",
	L"FLACDecoder: File open error.",
	L"FLACDecoder: File sek position is out of bounds.",
	L"FLACDecoder: File size is out of bounds.",
	L"FLACDecoder: This is not valid FLAC stream.",
	L"FLACDecoder: Unsupported sample rate.",
	L"FLACDecoder: Unsupported channel number.",
	L"FLACDecoder: Unsupported bits per sample.",
	L"FLACDecoder: Memory allocation fail."
	L"FLACDecoder: No ID3 data.",
	L"FLACDecoder: Seek is not supported on managed stream.",
	L"FLACDecoder: Seek position is out of bounds.",
	L"FLACDecoder: Reverse mod is not supported on managed stream.",
	L"FLACDecoder: Decoder initialization fail.",

	L"FLACDecoder: Function not supported.",
	L"FLACDecoder: Unknown error."

};

typedef struct {
	char *str_id;
	unsigned int size;
	unsigned int id; 
} FLAC_ID3_TABLE;


#define FLAC_GO_BACK_SAMPLES 11025

#define OGG_TABLE_SIZE 16
FLAC_ID3_TABLE OGG_ID3_ID[OGG_TABLE_SIZE] = {"ARTIST=", 7, ID3_INFO_ARTIST, 
							 "ALBUM=", 6, ID3_INFO_ALBUM,
							  "GENRE=", 6,  ID3_INFO_GENRE,
							   "TITLE=", 6, ID3_INFO_TITLE,
							    "TRACKNUMBER=", 12, ID3_INFO_TRACK,
								 "DATE=", 5, ID3_INFO_YEAR,
								 "COMMENT=", 8, ID3_INFO_COMMENT,
								  "DESCRIPTION=", 12,ID3_INFO_COMMENT,
								  
								  
								  "BAND=", 5, ID3_INFO_ALBUM_ARTIST,
								  "COMPOSER=", 9, ID3_INFO_COMPOSER,
								  "ORIGARTIST=", 11, ID3_INFO_ORIGINAL_ARTIST,
								  "COPYRIGHT=", 10, ID3_INFO_COPYRIGHT,
								  "LOCATION=", 9, ID3_INFO_URL,
								  "ENCODER=", 8, ID3_INFO_ENCODER,
								  "PUBLISHER=", 10, ID3_INFO_PUBLISHER,
								  "BPM=", 4, ID3_INFO_BPM
								  };


#define MANAGED_QUEUE_MIN_SIZE 20000

WFLACDecoder::WFLACDecoder()
{
	c_decoder = 0;
	c_ogg_transport_layer = 0;

	c_Queue = NULL;

	err(DECODER_NO_ERROR);

// =============================
//	INITIALIZE STREAM VALUES
	c_pchStreamStart = 0;
	c_pchStreamEnd = 0;
	c_nStreamSize = 0;
	c_fReverse = 0;
	c_fManagedStream = 0;
	c_nReversePos = 0;
	c_fReady = 0;
// =================================================
//	INITIALIZE INPUT_STREAM_INFO
	memset(&c_isInfo, 0, sizeof(INPUT_STREAM_INFO));
// ==================================================
//	INITIALIZE ID3_INFO
	unsigned int i;
	for(i = 0; i < ID3_FIELD_NUMBER_EX; i++)
		c_fields[i] = NULL;
// ==================================================

	memset(&bit_stream, 0, sizeof(USER_FLAC_STREAM));
	c_nBufferDone = 0;
	c_pSamplesAlloc = 0;				// allocated buffer for samples
	c_nSamplesAllocSize = 0;	// size of allocated buffer in samples
	c_pSamplesBuffer = 0;			// current position inside samples buffer
	c_nSamplesNum = 0;		// number of samples in samples buffer
	c_pReverseBufferAlloc = 0;
	c_nCurrentPosition = 0;
}


WFLACDecoder::WFLACDecoder(int fOgg)
{

	c_decoder = 0;

	c_ogg_transport_layer = fOgg;

	c_Queue = NULL;

	err(DECODER_NO_ERROR);

// =============================
//	INITIALIZE STREAM VALUES
	c_pchStreamStart = 0;
	c_pchStreamEnd = 0;
	c_nStreamSize = 0;
	c_fReverse = 0;
	c_fManagedStream = 0;
	c_nReversePos = 0;
	c_fReady = 0;
// =================================================
//	INITIALIZE INPUT_STREAM_INFO
	memset(&c_isInfo, 0, sizeof(INPUT_STREAM_INFO));
// ==================================================
//	INITIALIZE ID3_INFO
	unsigned int i;
	for(i = 0; i < ID3_FIELD_NUMBER_EX; i++)
		c_fields[i] = NULL;
// ==================================================

	memset(&bit_stream, 0, sizeof(USER_FLAC_STREAM));
	c_nBufferDone = 0;
	c_pSamplesAlloc = 0;				// allocated buffer for samples
	c_nSamplesAllocSize = 0;	// size of allocated buffer in samples
	c_pSamplesBuffer = 0;			// current position inside samples buffer
	c_nSamplesNum = 0;		// number of samples in samples buffer
	c_pReverseBufferAlloc = 0;
	c_nCurrentPosition = 0;
}


WFLACDecoder::~WFLACDecoder()
{
	Close();
	Uninitialize();
}



int WFLACDecoder::Initialize(int param1, int param2, int param3, int param4)
{
	c_decoder = FLAC__stream_decoder_new();
	if(c_decoder == NULL)
	{
		err(DECODER_MEMORY_ALLOCATION_FAIL);
		return 0;
	}


	// set error callback function
	flac_cb.error_func = error_callback;
	// set metadata callback
	flac_cb.metadata_func = metadata_callback;
	// write callback
	flac_cb.write_func = write_callback;
	

	return 1;
}


int WFLACDecoder::Uninitialize()
{
	if(c_decoder)
	{
		FLAC__stream_decoder_delete(c_decoder);
		c_decoder = 0;
	}

	return 1;
}


void WFLACDecoder::Release()
{
	delete this;
}

DECODER_ERROR_MESSAGE *WFLACDecoder::GetError()
{
	return &c_err_msg;
}






int WFLACDecoder::_OpenFile(unsigned int fSkipExtraChecking)
{
	ASSERT_W(c_decoder);

	bit_stream.start = c_pchStreamStart;
	bit_stream.end = c_pchStreamStart + c_nStreamSize - 1;
	bit_stream.pos = c_pchStreamStart;
	bit_stream.size = c_nStreamSize;


	if(c_ogg_transport_layer == 0)
	{
		if(FLAC__stream_decoder_init_stream(c_decoder, flac_cb.read_func, flac_cb.seek_func,
				flac_cb.tell_func, flac_cb.length_func, flac_cb.eof_func, flac_cb.write_func,
				flac_cb.metadata_func, flac_cb.error_func, this) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		{
			err(DECODER_FAIL_INITIALIZATION);
			return 0;
		}
	}
	else
	{
		if(FLAC__stream_decoder_init_ogg_stream(c_decoder, flac_cb.read_func, flac_cb.seek_func,
				flac_cb.tell_func, flac_cb.length_func, flac_cb.eof_func, flac_cb.write_func,
				flac_cb.metadata_func, flac_cb.error_func, this) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		{
			err(DECODER_FAIL_INITIALIZATION);
			return 0;
		}
	}



	

	// decode metadata
	if(FLAC__stream_decoder_process_until_end_of_metadata(c_decoder) == 0)
	{
		// decode metadata
		if(FLAC__stream_decoder_process_until_end_of_metadata(c_decoder) == 0)
		{
			err(DECODER_NOT_VALID_FLAC_STREAM);
			return 0;
		}	
	}



	if(c_fReady == 0)
	{
		err(DECODER_NOT_VALID_FLAC_STREAM);
		return 0;
	}

	
	c_isInfo.nFileBitrate = 0;
	if(c_isInfo.nLength)
	{
		unsigned int sec = c_isInfo.nLength / c_isInfo.nSampleRate;
		c_isInfo.nFileBitrate = bit_stream.size * 8 / sec;
	}

	c_nReversePos = 0;

	c_nCurrentPosition = 0;


	return 1;


}

int WFLACDecoder::Close()
{
	FreeID3Fields(c_fields, ID3_FIELD_NUMBER_EX);

	if(c_pSamplesAlloc)
		free(c_pSamplesAlloc);

	c_pSamplesAlloc = 0;				// allocated buffer for samples
	c_nSamplesAllocSize = 0;	// size of allocated buffer in samples
	c_pSamplesBuffer = 0;			// current position inside samples buffer
	c_nSamplesNum = 0;		// number of samples in samples buffer

	if(c_pReverseBufferAlloc)
		free(c_pReverseBufferAlloc);

	c_pReverseBufferAlloc = 0;

	if(c_fReady == 0)
		return 1;

	FLAC__stream_decoder_finish(c_decoder);


	c_pchStreamStart = 0;
	c_pchStreamEnd = 0;
	c_nStreamSize = 0;
	c_fManagedStream = 0;
// ===================================

// ================================

	//	INITIALIZE INPUT_STREAM_INFO
	memset(&c_isInfo, 0, sizeof(INPUT_STREAM_INFO));

// ==================================


	memset(&bit_stream, 0, sizeof(USER_FLAC_STREAM));


	c_nBufferDone = 0;
	c_nReversePos = 0;


	c_nCurrentPosition = 0;


// ===================================
	
	c_fReady = 0;
	return 1;
}



INPUT_STREAM_INFO *WFLACDecoder::GetStreamInfo()
{
	ASSERT_W(c_fReady);
	return &c_isInfo;
}


wchar_t **WFLACDecoder::GetID3Info(int version, char *pStream, unsigned int nStreamSize, int param1, int param2)
{

	err(DECODER_NO_ERROR);

// free previous fields
	FreeID3Fields(c_fields, ID3_FIELD_NUMBER_EX);

	// allocate and initialize new fields
	if(AllocateID3Fields(c_fields, ID3_FIELD_NUMBER_EX, 1) == 0)
	{
		err(DECODER_MEMORY_ALLOCATION_FAIL);
		return 0;
	}

	FLAC__IOCallbacks IOCallbacks;
	memset(&IOCallbacks, 0, sizeof(FLAC__IOCallbacks));

	IOCallbacks.read = IOCallback_Read;
	IOCallbacks.seek = IOCallback_Seek;
	IOCallbacks.tell = IOCallback_Tell;

	// make locak bit_stream
	USER_FLAC_STREAM bit_stream;
	bit_stream.start = c_pchStreamStart;
	bit_stream.end = c_pchStreamStart + c_nStreamSize - 1;
	bit_stream.pos = c_pchStreamStart;
	bit_stream.size = c_nStreamSize;

	FLAC__Metadata_Chain * chain = FLAC__metadata_chain_new(); 
	if(chain == NULL)
	{
		err(DECODER_MEMORY_ALLOCATION_FAIL);
		return 0;
	}


	if(c_ogg_transport_layer)
	{
		if(FLAC__metadata_chain_read_ogg_with_callbacks(chain, (FLAC__IOHandle) &bit_stream, IOCallbacks) == false)
		{
			FLAC__metadata_chain_delete(chain);
			err(DECODER_NO_ID3_DATA);
			return 0;
		}

	}
	else
	{
		if(FLAC__metadata_chain_read_with_callbacks(chain, (FLAC__IOHandle) &bit_stream, IOCallbacks) == false)
		{
			FLAC__metadata_chain_delete(chain);
			err(DECODER_NO_ID3_DATA);
			return 0;
		}
	}

	FLAC__Metadata_Iterator* iterator = FLAC__metadata_iterator_new(); 
	FLAC__metadata_iterator_init(iterator, chain);


	FLAC__StreamMetadata* data;
	FLAC__StreamMetadata *tags = 0;
	FLAC__StreamMetadata *picture_block = 0;


	do
	{	
		data = FLAC__metadata_iterator_get_block (iterator);
		if(data->type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
		{
			tags = data;
			if(picture_block)
				break;

		}
		else if(data->type == FLAC__METADATA_TYPE_PICTURE)
		{
			picture_block = data;
			if(tags)
				break;

		}

	}
	while(FLAC__metadata_iterator_next(iterator));


	if(picture_block)
	{
		FLAC__StreamMetadata_Picture picture = picture_block->data.picture; 

		wchar_t *buf = ANSIToUTF16(picture.mime_type, -1);
		if(buf)
		{
			if(c_fields[ID3_INFO_PICTURE_MIME])
				free(c_fields[ID3_INFO_PICTURE_MIME]);

			c_fields[ID3_INFO_PICTURE_MIME] = buf;
		}


		buf = UTF8ToUTF16((char*) picture.description, -1);

		if(buf)
		{
			if(c_fields[ID3_INFO_PICTURE_DESCRIPTION])
				free(c_fields[ID3_INFO_PICTURE_DESCRIPTION]);

			c_fields[ID3_INFO_PICTURE_DESCRIPTION] = buf;
		}

	
		wchar_t *tmp = (wchar_t*) malloc(10 * sizeof(wchar_t));
		if(tmp)
		{
			swprintf(tmp, L"%u", picture.data_length); 
			if(c_fields[ID3_INFO_PICTURE_DATA_SIZE])
				free(c_fields[ID3_INFO_PICTURE_DATA_SIZE]);

			c_fields[ID3_INFO_PICTURE_DATA_SIZE] = tmp;

		}


		char *buf1 = (char*) malloc(picture.data_length);
	
		if(buf == NULL)
		{
			wcscpy(c_fields[ID3_INFO_PICTURE_DATA_SIZE], L"0");	
		}
		else
		{
	
			memcpy(buf1, picture.data, picture.data_length); 
			if(c_fields[ID3_INFO_PICTURE_DATA])
				free(c_fields[ID3_INFO_PICTURE_DATA]);

			c_fields[ID3_INFO_PICTURE_DATA] = (wchar_t*) buf1;

		}

		// picture type

		tmp = (wchar_t*) malloc(10 * sizeof(wchar_t));
		if(tmp)
		{
			swprintf(tmp, L"%u", picture.type); 
			if(c_fields[ID3_INFO_PICTURE_TYPE])
				free(c_fields[ID3_INFO_PICTURE_TYPE]);

			c_fields[ID3_INFO_PICTURE_TYPE] = tmp;

		}

	}


	if(tags)
	{
		unsigned int i;
		unsigned int j;
		char *buf = 0;
		for(i = 0; i < tags->data.vorbis_comment.num_comments; i++)
		{
			for(j = 0; j < OGG_TABLE_SIZE; j++)
			{
				if(strnicmp((char*) tags->data.vorbis_comment.comments[i].entry, OGG_ID3_ID[j].str_id, OGG_ID3_ID[j].size) == 0)
				{
					unsigned int len = tags->data.vorbis_comment.comments[i].length - OGG_ID3_ID[j].size;

			
					wchar_t *out = UTF8ToUTF16((char*) tags->data.vorbis_comment.comments[i].entry + OGG_ID3_ID[j].size, len);
					if(out)
					{
						if(c_fields[OGG_ID3_ID[j].id])
							free(c_fields[OGG_ID3_ID[j].id]);

						c_fields[OGG_ID3_ID[j].id] = out;
					}

					break;			 	
				}
			}
		}
	}


	FLAC__metadata_iterator_delete (iterator);
	FLAC__metadata_chain_delete(chain);

	return c_fields;
	
}





int WFLACDecoder::Seek(unsigned int nSamples)
{
	ASSERT_W(c_fReady);

	err(DECODER_NO_ERROR);

	// clear buffer
	c_nSamplesNum = 0;

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


	if(FLAC__stream_decoder_seek_absolute(c_decoder, nSamples) == false)
	{
		err(DECODER_UNKNOWN_ERROR);
		return 0;
	}

	if(FLAC__stream_decoder_get_state(c_decoder) == FLAC__STREAM_DECODER_SEEK_ERROR)
	{
		if(FLAC__stream_decoder_flush(c_decoder) == false)
		{
			err(DECODER_UNKNOWN_ERROR);
			return 0;
		}

		if(FLAC__stream_decoder_reset(c_decoder) == false)
		{
			err(DECODER_UNKNOWN_ERROR);
			return 0;
		}
	}


	c_nReversePos = nSamples;

	if(c_fReverse)
		c_nSamplesNum = 0; // clear buffer again because FLAC__stream_decoder_seek_absolute decodes audio data in write callback
	
	
	c_nCurrentPosition = nSamples;		
	return 1;

}


int WFLACDecoder::GetData(DECODER_DATA *pDecoderData)
{
	ASSERT_W(c_fReady);
	ASSERT_W(pDecoderData);
	ASSERT_W(pDecoderData->pSamples);


	unsigned int nSamplesNeed = pDecoderData->nSamplesNum;
	unsigned int nBytesNeed = pDecoderData->nSamplesNum * 4;
	unsigned int nSamplesHave = 0;

	c_fEndOfStream = pDecoderData->fEndOfStream;

	pDecoderData->nBufferDone = 0;
	pDecoderData->nBufferInQueue = 0;
	pDecoderData->nBytesInQueue = 0;
	pDecoderData->nSamplesNum = 0;

	pDecoderData->nStartPosition = 0;
	pDecoderData->nEndPosition = 0;

	c_nBufferDone = 0;


	int *buf = (int*) pDecoderData->pSamples;

	unsigned int ErrorCode = 0;


	while(nSamplesNeed)
	{
		if(c_nSamplesNum)	// we have some data in buffer
		{
			unsigned int i;
			if(c_nSamplesNum >= nSamplesNeed) // we have enough data
			{
				for(i = 0; i < nSamplesNeed; i++)	// copy data to user buffer
					buf[i] = c_pSamplesBuffer[i];

				nSamplesHave += nSamplesNeed;
				// move buffer pointer
				c_pSamplesBuffer = &c_pSamplesBuffer[nSamplesNeed];
				c_nSamplesNum -= nSamplesNeed;
				nSamplesNeed = 0;	// we are done
			}
			else
			{
				for(i = 0; i < c_nSamplesNum; i++)	// copy data to user buffer
					buf[i] = c_pSamplesBuffer[i];

				nSamplesHave += c_nSamplesNum;

				nSamplesNeed -= c_nSamplesNum;
				buf = &buf[c_nSamplesNum];	// move user buffer to new position because we need more data

				// buffer is empty
				c_pSamplesBuffer = c_pSamplesAlloc;
				c_nSamplesNum = 0;
			}
		}
		else
		{
			// we have no data in buffer, get data from decoder

			if(c_fReverse)
			{
				if(c_nReversePos == 0)
				{
					ErrorCode = 2;
					break;	// stop loop
				}

				// now we need to calculate number of samples we will go back in position

				unsigned int nGoBackSamples = FLAC_GO_BACK_SAMPLES;

				if(c_pReverseBufferAlloc == 0)
				{
					c_pReverseBufferAlloc = (int*) malloc(FLAC_GO_BACK_SAMPLES * 4);	// always use 16 bit stereo
					if(c_pReverseBufferAlloc == 0)
						return DECODER_FATAL_ERROR;

				}

				c_pReverseBuffer = c_pReverseBufferAlloc;


				// move position back
				if(c_nReversePos < nSamplesNeed)
					nSamplesNeed = c_nReversePos;

				// move position back
				if(c_nReversePos >= nGoBackSamples)
				{
					c_nReversePos -= nGoBackSamples;	
				}
				else
				{
					nGoBackSamples = c_nReversePos; // always use 16 bit stereo
					c_nReversePos = 0;
				}

			
				// now we need to get samples into reverse buffer

				// seek decoder
				if(FLAC__stream_decoder_seek_absolute(c_decoder, c_nReversePos) == false)
				{
					ErrorCode = 1;
					break;	// stop loop
				}
		
				if(FLAC__stream_decoder_get_state(c_decoder) == FLAC__STREAM_DECODER_SEEK_ERROR)
				{
					if(FLAC__stream_decoder_flush(c_decoder) == false)
					{
						ErrorCode = 1;
						break;	// stop loop
					}

					if(FLAC__stream_decoder_reset(c_decoder) == false)
					{
						ErrorCode = 1;
						break;	// stop loop
					}
				}	

				unsigned int nHaveSamples = 0;
				// load samples into our reverse buffer
				while(nGoBackSamples)
				{
					if(c_nSamplesNum)
					{
						// we have samples from decoder, copy this samples into reverse buffer
						unsigned int nCopySamples = nGoBackSamples < c_nSamplesNum ? nGoBackSamples : c_nSamplesNum;
						unsigned int i;
						for(i = 0; i < nCopySamples; i++)
							c_pReverseBuffer[i] = c_pSamplesBuffer[i];
							
						nGoBackSamples -= nCopySamples;	
						c_nSamplesNum = 0;
						c_pReverseBuffer = &c_pReverseBuffer[nCopySamples];
						nHaveSamples += nCopySamples;
					}
					else
					{
						// we need more data, so get data from decoder
						if(FLAC__stream_decoder_process_single(c_decoder) == false)
							break;	// stop loop
					

						if(FLAC__stream_decoder_get_state(c_decoder) == FLAC__STREAM_DECODER_END_OF_STREAM)
							break;	// stop loop

					}
				}

				if(nHaveSamples == 0)
					continue;

				// reverse buffer
				PCM16StereoReverse(c_pReverseBufferAlloc, nHaveSamples, c_pReverseBufferAlloc);

				// swap working buffer with reverse buffer
				c_pSamplesBuffer = c_pReverseBufferAlloc;
				c_nSamplesNum = nHaveSamples;
				
				
			}
			else // no reverse
			{
				if(c_fManagedStream)
				{
					unsigned int nHave = c_Queue->GetSizeSum();
					if(nHave < MANAGED_QUEUE_MIN_SIZE)
					{
						if(c_fEndOfStream == 0)
						{
							ErrorCode = 2;
							break;
						}
					}
				}

				// we need more data, so get data from decoder
				if(FLAC__stream_decoder_process_single(c_decoder) == false)
				{
					ErrorCode = 1;
					break;
				}
			
					
				if(FLAC__stream_decoder_get_state(c_decoder) == FLAC__STREAM_DECODER_END_OF_STREAM)
					break;
				
				
			}
		}
	}

	if(c_nBufferDone)
	{
		pDecoderData->nBufferDone = c_nBufferDone;
		pDecoderData->nBufferInQueue = c_Queue->GetCount();
		pDecoderData->nBytesInQueue = c_Queue->GetSizeSum();
	}



	if(nSamplesHave == 0)
	{
		if(c_fManagedStream)
		{
			if(ErrorCode == 2)
				return DECODER_MANAGED_NEED_MORE_DATA;

			if(ErrorCode == 1)
				return DECODER_FATAL_ERROR;
				
			return DECODER_END_OF_DATA;
		}

		if(ErrorCode == 1)
			return DECODER_FATAL_ERROR;
				
		return DECODER_END_OF_DATA;
	}

	pDecoderData->nSamplesNum = nSamplesHave;

	pDecoderData->nStartPosition = c_nCurrentPosition;
	if(c_fReverse)
		c_nCurrentPosition -= nSamplesHave;
	else
		c_nCurrentPosition += nSamplesHave;
	pDecoderData->nEndPosition = c_nCurrentPosition;



	return DECODER_OK;		
}




int WFLACDecoder::GetBitrate(int fAverage)
{
	return c_isInfo.nFileBitrate;
}


int WFLACDecoder::SetReverseMode(int fReverse)
{
	ASSERT_W(c_fReady);


	err(DECODER_NO_ERROR);
	// clear buffer
	c_nSamplesNum = 0;
	
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
	{
		c_fReverse = 0;
		c_nSamplesNum = 0;
	}


	return 1;

}



// flac callback func


FLAC__StreamDecoderReadStatus WFLACDecoder::read_func(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	if(*bytes > 0)
	{
		WFLACDecoder *instance = (WFLACDecoder*) client_data;
		USER_FLAC_STREAM *bit_stream = &instance->bit_stream;

		if(bit_stream->pos > bit_stream->end)
		{
			*bytes = 0;
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		}

		unsigned int data_size = *bytes;
		unsigned int have = bit_stream->end - bit_stream->pos + 1;
		if(data_size > have)
			data_size = have;
		
		memcpy(buffer, bit_stream->pos, data_size);
		*bytes = data_size;

	
		bit_stream->pos = bit_stream->pos + data_size;

		return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
	}

	return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

}


FLAC__StreamDecoderReadStatus WFLACDecoder::managed_read_func(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{

	if(*bytes > 0)
	{
		WFLACDecoder *instance = (WFLACDecoder*) client_data;
		USER_FLAC_STREAM *bit_stream = &instance->bit_stream;

		unsigned int have = instance->c_Queue->GetSizeSum();

		if(have == 0)
		{
			*bytes = 0;
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		}

		unsigned int data_size = *bytes;
		if(data_size > have)
			data_size = have;

		instance->c_nBufferDone = 0;

		unsigned int ret = instance->c_Queue->PullDataFifo(buffer, data_size, (int*) &instance->c_nBufferDone);
		*bytes = ret;

		return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
	}

	return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

}




FLAC__StreamDecoderSeekStatus WFLACDecoder::seek_func(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	WFLACDecoder *instance = (WFLACDecoder*) client_data;
	USER_FLAC_STREAM *bit_stream = &instance->bit_stream;


	unsigned char *new_pos =  bit_stream->start + absolute_byte_offset;		
	if(new_pos > bit_stream->end + 1)
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR; 
		
	bit_stream->pos = new_pos;	
	return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}


FLAC__StreamDecoderTellStatus WFLACDecoder::tell_func(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	WFLACDecoder *instance = (WFLACDecoder*) client_data;
	USER_FLAC_STREAM *bit_stream = &instance->bit_stream;

	*absolute_byte_offset = bit_stream->pos - bit_stream->start;
     return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus WFLACDecoder::length_func(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
	WFLACDecoder *instance = (WFLACDecoder*) client_data;
	USER_FLAC_STREAM *bit_stream = &instance->bit_stream;

	*stream_length = bit_stream->size;
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool WFLACDecoder::eof_func(const FLAC__StreamDecoder *decoder, void *client_data)
{
	WFLACDecoder *instance = (WFLACDecoder*) client_data;
	USER_FLAC_STREAM *bit_stream = &instance->bit_stream;

	if(bit_stream->pos > bit_stream->end)
		return true;

	return false;
}





void WFLACDecoder::err(unsigned int error_code)
{
	if(error_code > DECODER_UNKNOWN_ERROR)
		error_code = DECODER_UNKNOWN_ERROR;
			
	c_err_msg.errorW = g_flac_error_strW[error_code];
}



void WFLACDecoder::error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{

}


void WFLACDecoder::metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	WFLACDecoder *instance = (WFLACDecoder*) client_data;

	instance->c_fReady = 0;

	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
	{

		instance->c_isInfo.nSampleRate = metadata->data.stream_info.sample_rate;
		if(instance->c_isInfo.nSampleRate == 0)
		{
			instance->err(DECODER_UNSUPPORTED_SAMPLERATE);
			return;
		}

		instance->c_isInfo.nChannel = metadata->data.stream_info.channels;
		if(instance->c_isInfo.nChannel == 0 || instance->c_isInfo.nChannel > 2)
		{
			instance->err(DECODER_UNSUPPORTED_CHANNEL_NUMBER);
			return;
		}

		instance->c_isInfo.nBitPerSample = metadata->data.stream_info.bits_per_sample;
		if(instance->c_isInfo.nBitPerSample != 8 && instance->c_isInfo.nBitPerSample != 16 &&
					instance->c_isInfo.nBitPerSample != 32)
		{
			instance->err(DECODER_UNSUPPORTED_BIT_PER_SAMPLE);
			return;

		}


		instance->c_isInfo.fVbr = 1;
		instance->c_isInfo.nLength = metadata->data.stream_info.total_samples;
		
		if(instance->c_ogg_transport_layer == 0)
		{
			if(instance->c_isInfo.nChannel > 2)
				instance->c_isInfo.pchStreamDescription = "FLAC MULTI CHANNEL";
			else if(instance->c_isInfo.nChannel == 2)
				instance->c_isInfo.pchStreamDescription = "FLAC STEREO";
			else
				instance->c_isInfo.pchStreamDescription = "FLAC MONO";
		}
		else
		{
			if(instance->c_isInfo.nChannel > 2)
				instance->c_isInfo.pchStreamDescription = "FLAC OGG MULTI CHANNEL";
			else if(instance->c_isInfo.nChannel == 2)
				instance->c_isInfo.pchStreamDescription = "FLAC OGG STEREO";
			else
				instance->c_isInfo.pchStreamDescription = "FLAC OGG MONO";


		}

		instance->c_isInfo.nBlockAlign = 4;

		instance->c_fReady = 1;

	}
	
}



FLAC__StreamDecoderWriteStatus WFLACDecoder::write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	// here we will take data from decoder and copy this data into buffer
	WFLACDecoder *instance = (WFLACDecoder*) client_data;


	ASSERT_W(instance->c_nSamplesNum == 0);

	// some checking and initializations
	if(instance->c_isInfo.nBitPerSample)
	{
		if(instance->c_isInfo.nBitPerSample != frame->header.bits_per_sample)
			return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;	// skip this frame	
	}
	else
		instance->c_isInfo.nBitPerSample = frame->header.bits_per_sample;	

	if(instance->c_isInfo.nSampleRate)
	{
		if(instance->c_isInfo.nSampleRate != frame->header.sample_rate)
			return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;	// skip this frame	
	}
	else
		instance->c_isInfo.nSampleRate = frame->header.sample_rate;


	if(instance->c_isInfo.nChannel)
	{
		if(instance->c_isInfo.nChannel != frame->header.channels)
			return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;	// skip this frame	
	}
	else
		instance->c_isInfo.nChannel = frame->header.channels;
		
	// calculate number of bytes from decoder
	unsigned int nHaveBytesFromDecoder = frame->header.blocksize * 4; // always use 16 bit stereo

	if(nHaveBytesFromDecoder > instance->c_nSamplesAllocSize) // reallocate samples buffer
	{
		if(instance->c_pSamplesAlloc)
			free(instance->c_pSamplesAlloc);

		instance->c_nSamplesAllocSize = 0;
		instance->c_pSamplesAlloc = (int*) malloc(nHaveBytesFromDecoder);
		if(instance->c_pSamplesAlloc == 0)
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

		instance->c_nSamplesAllocSize = nHaveBytesFromDecoder;
	}


	// detect if stereo or mono
	unsigned int right = 0;
	switch(frame->header.channels)
	{
		case 1:	// mono
			right = 0;
		break;
		
		case 2:	// stereo
			right = 1;
		break;	

		default:
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT; // multiple channels are not supported

	}

	// do work here
	switch(frame->header.bits_per_sample)
	{
		case 24:
		{
			// copy data from decoder into buffer
			unsigned int i;
			unsigned int j;
			short *sample = (short*) instance->c_pSamplesAlloc;
			for(i = 0, j = 0; i < frame->header.blocksize; i++, j += 2)
			{
				sample[j] = buffer[0][i] >> 8;
				sample[j + 1] = buffer[right][i] >> 8;
			}
		}
		break;

		case 16:
		{
			// copy data from decoder into buffer
			unsigned int i;
			unsigned int j;
			short *sample = (short*) instance->c_pSamplesAlloc;

			for(i = 0, j = 0; i < frame->header.blocksize; i++, j += 2)
			{
				sample[j] = buffer[0][i];
				sample[j + 1] = buffer[right][i];
			}
		}
		break;

		case 8:
		{
			// copy data from decoder into buffer
			unsigned int i;
			unsigned int j;
			short *sample = (short*) instance->c_pSamplesAlloc;
			for(i = 0, j = 0; i < frame->header.blocksize; i++, j += 2)
			{
				sample[j] = buffer[0][i] << 8;
				sample[j + 1] = buffer[right][i] << 8;
			}
		}
		break;

		default:
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}


	instance->c_pSamplesBuffer = instance->c_pSamplesAlloc;
	instance->c_nSamplesNum = frame->header.blocksize;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}



size_t WFLACDecoder::IOCallback_Read(void *ptr, size_t size, size_t nmemb, FLAC__IOHandle handle)
{
	USER_FLAC_STREAM *bit_stream = (USER_FLAC_STREAM*) handle;

	if(bit_stream->pos > bit_stream->end)
		return 0;	// end of stream data

	unsigned int data_size = size * nmemb;
	unsigned int have = bit_stream->end - bit_stream->pos + 1;
	if(data_size > have)
		data_size = have;
	
	memcpy(ptr, bit_stream->pos, data_size);

	unsigned int read_block_num = data_size / size;
	bit_stream->pos = bit_stream->pos + read_block_num * size;

	return read_block_num; 

}


int WFLACDecoder::IOCallback_Seek(FLAC__IOHandle handle, FLAC__int64 offset, int whence)
{
	USER_FLAC_STREAM *bit_stream = (USER_FLAC_STREAM*) handle;


	switch(whence)
	{
		case 0: // SEEK_SET	
		{
			unsigned char *new_pos =  bit_stream->start + offset;
			
			if(new_pos > bit_stream->end + 1)
				return -1;
		
			bit_stream->pos = new_pos;	

		}
		return 0;

		case 1: // SEEK_CUR
		{
			unsigned char *new_pos = bit_stream->pos + offset;
			if(new_pos < bit_stream->start || new_pos > bit_stream->end + 1)
				return -1;
			
			bit_stream->pos = new_pos;

		}
		return 0;

		case 2: // SEEK_END
		{
			unsigned char *new_pos = bit_stream->end + 1 - offset;
			if(new_pos < bit_stream->start || new_pos > bit_stream->end + 1)
				return -1;

			bit_stream->pos = new_pos;
		}
		return 0;

		default:
		return -1;
	}

	return -1;
}

FLAC__int64 WFLACDecoder::IOCallback_Tell(FLAC__IOHandle handle)
{
	USER_FLAC_STREAM *bit_stream = (USER_FLAC_STREAM*) handle;

	return (bit_stream->pos - bit_stream->start);
}





int WFLACDecoder::OpenStream(WQueue *pQueue, int fDynamic, int param1, int param2)
{
	err(DECODER_NO_ERROR);
	c_Queue = pQueue;

	unsigned int nSize = pQueue->GetSizeSum();
	if(nSize == 0)
	{
		err(DECODER_NOT_VALID_FLAC_STREAM);
		return 0;
	}


	unsigned char *ptr;
	unsigned int size; 
	if(pQueue->QueryFirstPointer((void**) &ptr, &size) == 0)
	{
		err(DECODER_NOT_VALID_FLAC_STREAM);
		return 0;
	}

	c_fManagedStream = 0;

	// set callback functions
	flac_cb.read_func = read_func;
	flac_cb.seek_func = seek_func;
	flac_cb.tell_func = tell_func;
	flac_cb.length_func = length_func;
	flac_cb.eof_func = eof_func;

	if(param1 == 0)
	{
		c_pchStreamStart = ptr;
		c_nStreamSize = size;
		c_pchStreamEnd = c_pchStreamStart + c_nStreamSize - 1;


		if(fDynamic)
		{
			c_fManagedStream = 1;

			// set callback functions
			flac_cb.read_func = managed_read_func;
			flac_cb.seek_func = NULL;
			flac_cb.tell_func = NULL;
			flac_cb.length_func = NULL;
			flac_cb.eof_func = NULL;
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