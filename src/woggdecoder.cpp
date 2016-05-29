/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  OGG decoder
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
 * SUPPORTED BY:
 * ============================================================================
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002 
 * by the Xiph.Org Foundation http://www.xiph.org/
 * BSD-STYLE SOURCE LICENSE  ( XIPH.TXT )
 * ============================================================================
*/


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include "debug.h"
#include "woggdecoder.h"
#include "wtags.h"
enum {
	DECODER_NO_ERROR = 0,
	DECODER_FILE_OPEN_ERROR,
	DECODER_FILE_SEEK_POSITION_OUT_OF_BOUNDS,
	DECODER_FILESIZE_OUT_OF_BOUNDS,
	DECODER_NOT_VALID_OGG_STREAM,
	DECODER_MEMORY_ALLOCATION_FAIL,
	DECODER_NO_ID3_DATA,
	DECODER_SEEK_NOT_SUPPORTED_MANAGED,
	DECODER_SEEK_POSITION_OUT_OF_BOUNDS,
	DECODER_REVERSE_NOT_SUPPORTED_MANAGED,
	DECODER_INVALID_ID3_PARAMETER,

	DECODER_FUNCTION_NOT_SUPPORTED,
	DECODER_UNKNOWN_ERROR
};




wchar_t *g_ogg_error_strW[DECODER_UNKNOWN_ERROR + 1] = {
	L"No error.",
	L"OggDecoder: File open error.",
	L"OggDecoder: File sek position is out of bounds.",
	L"OggDecoder: File size is out of bounds.",
	L"OggDecoder: This is not valid ogg stream.",
	L"OggDecoder: Memory allocation fail."
	L"OggDecoder: No ID3 data.",
	L"OggDecoder: Seek is not supported on managed stream.",
	L"OggDecoder: Seek position is out of bounds.",
	L"OggDecoder: Reverse mod is not supported on managed stream.",
	L"OggDecoder: Invalid ID3 version parameter.",

	L"OggDecoder: Functinon not supported.",
	L"OggDecoder: Unknown error."

};

// size of working buffer in samples
#define SIZE_OF_WORKING_BUFFER 4410

typedef struct {
	char *str_id;
	unsigned int size;
	unsigned int id; 
} OGG_ID3_TABLE;


#define METADATA_BLOCK_PICTURE 1000
#define OGG_TABLE_SIZE 19
OGG_ID3_TABLE OGG_ID3_ID[OGG_TABLE_SIZE] = {"ARTIST=", 7, ID3_INFO_ARTIST, 
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
								  "BPM=", 4, ID3_INFO_BPM,
								  "COVERARTMIME=", 13, ID3_INFO_PICTURE_MIME,
								  "COVERART=", 9, ID3_INFO_PICTURE_DATA,
								  "METADATA_BLOCK_PICTURE=", 23, METADATA_BLOCK_PICTURE
								  };


WOggDecoder::WOggDecoder()
{

	err(DECODER_NO_ERROR);

	c_Queue = NULL;

	c_LastBitrate = 0;

// =============================
	c_pchStreamStart = 0;
	c_pchStreamEnd = 0;
	c_nStreamSize = 0;
	c_fReverse = 0;
	c_fManagedStream = 0;
	c_fEndOfStream = 0;
	c_nReversePos = 0;
	c_nCurrentPosition = 0;
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

	memset(&vf, 0, sizeof(vf));
	c_nLogicalBitstreamIndex = 0;	// index of logical bitstream
	c_nNumberOfLogicalBitstreams = 0;	// number of logical bitstreams
	vi = 0;
	vcomment = NULL;
	memset(&bit_stream, 0, sizeof(USER_OGG_STREAM));

	c_nBufferDone = 0;

	c_pBufferAlloc = 0;
	c_nBufferAllocSize = 0;
	c_pchBufferPos = 0;
	c_nBufferSize = 0;

}


WOggDecoder::~WOggDecoder()
{

	Close();
	Uninitialize();
}



int WOggDecoder::Initialize(int param1, int param2, int param3, int param4)
{
	c_pBufferAlloc = (char*) malloc(SIZE_OF_WORKING_BUFFER * 4); // always use 16 bit stereo
	if(c_pBufferAlloc == 0)
	{
		err(DECODER_MEMORY_ALLOCATION_FAIL);
		return 0;
	}

	c_nBufferAllocSize = SIZE_OF_WORKING_BUFFER;


	return 1;
}


int WOggDecoder::Uninitialize()
{
	if(c_pBufferAlloc)
		free(c_pBufferAlloc);

	c_pBufferAlloc = 0;
	c_nBufferAllocSize = 0;
	return 1;
}


void WOggDecoder::Release()
{
	delete this;
}

DECODER_ERROR_MESSAGE *WOggDecoder::GetError()
{
	return &c_err_msg;
}


int WOggDecoder::_OpenFile(unsigned int fLite)
{

	bit_stream.start = c_pchStreamStart;
	bit_stream.end = c_pchStreamStart + c_nStreamSize - 1;
	bit_stream.pos = c_pchStreamStart;
	bit_stream.size = c_nStreamSize;

	if(c_fManagedStream)
	{
		if(ov_open_callbacks(this, &vf, NULL, 0, cb) < 0)
		{
			err(DECODER_NOT_VALID_OGG_STREAM);
			return 0;
		}
	}
	else
	{
		if(ov_open_callbacks(&bit_stream, &vf, NULL, 0, cb) < 0)
		{
			err(DECODER_NOT_VALID_OGG_STREAM);
			return 0;
		}
	}

		
	if(fLite)
		return 1;

	// get number of logical streams
	c_nNumberOfLogicalBitstreams = ov_streams(&vf);

	if(c_nNumberOfLogicalBitstreams == 0)
	{
		err(DECODER_NOT_VALID_OGG_STREAM);
		return 0;
	}


	c_nLogicalBitstreamIndex = 0;

	vi = ov_info(&vf, c_nLogicalBitstreamIndex);


	if(vi == NULL)
	{
		err(DECODER_NOT_VALID_OGG_STREAM);
		return 0;	
	}


	c_isInfo.nSampleRate = vi->rate;
	c_isInfo.nChannel = vi->channels;


	c_isInfo.fVbr = 1;
	if(vi->bitrate_upper == vi->bitrate_lower)
		c_isInfo.fVbr = 0;
		
	c_isInfo.nFileBitrate = ov_bitrate(&vf, c_nLogicalBitstreamIndex);
	ogg_int64_t len = ov_pcm_total(&vf, c_nLogicalBitstreamIndex);
	if(len == OV_EINVAL)
		c_isInfo.nLength = 0;
	else	
		c_isInfo.nLength = len;


	if(c_isInfo.nChannel > 2)
		c_isInfo.pchStreamDescription = "VORBIS OGG MULTI CHANNEL";
	else if(c_isInfo.nChannel == 2)
		c_isInfo.pchStreamDescription = "VORBIS OGG STEREO";
	else
		c_isInfo.pchStreamDescription = "VORBIS OGG MONO";

	// get informations of input stream
	// WE ALWAYS USE 16 bit per sample

	// save original bit per sample

	if(c_isInfo.nChannel >= 2)
		c_nBlockAlign = 4;	
	else
		c_nBlockAlign = 2;
	
	c_isInfo.nBlockAlign = 4;		
	c_isInfo.nBitPerSample = 16;
	c_nReversePos = 0;

	c_nCurrentPosition = 0;
	c_fReady = 1;

	return 1;


}

int WOggDecoder::Close()
{

	FreeID3Fields(c_fields, ID3_FIELD_NUMBER_EX);

	ov_clear(&vf);
	memset(&vf, 0, sizeof(vf));
	vcomment = NULL;

	c_nLogicalBitstreamIndex = 0;	// index of logical bitstream
	c_nNumberOfLogicalBitstreams = 0;	// number of logical bitstreams

	if(c_fReady == 0)
		return 1;

	c_pchStreamStart = 0;
	c_pchStreamEnd = 0;
	c_nStreamSize = 0;
	c_fManagedStream = 0;
	c_fEndOfStream = 0;

	c_LastBitrate = 0;

// ================================

	//	INITIALIZE INPUT_STREAM_INFO
	memset(&c_isInfo, 0, sizeof(INPUT_STREAM_INFO));

// ==================================



	memset(&bit_stream, 0, sizeof(USER_OGG_STREAM));

	c_nBufferDone = 0;
	c_nReversePos = 0;
	c_nBufferSize = 0;
	c_nCurrentPosition = 0;
	
	c_fReady = 0;
	return 1;
}



INPUT_STREAM_INFO *WOggDecoder::GetStreamInfo()
{
	ASSERT_W(c_fReady);
	return &c_isInfo;
}


wchar_t **WOggDecoder::GetID3Info(int version, char *pStream, unsigned int nStreamSize, int param1, int param2)
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

	vcomment = ov_comment(&vf, c_nLogicalBitstreamIndex);

	if(vcomment == NULL)
	{
		err(DECODER_NO_ID3_DATA);
		return NULL;
	}


	int i;
	int j;

	int have_picture = 0;
	for(i = 0; i < vcomment->comments; i++)
	{
		for(j = 0; j < OGG_TABLE_SIZE; j++)
		{
			if(strnicmp(vcomment->user_comments[i], OGG_ID3_ID[j].str_id, OGG_ID3_ID[j].size) == 0)
			{

				if(OGG_ID3_ID[j].id == ID3_INFO_PICTURE_DATA)
				{
					if(have_picture)
						break;
					

					char *buf = NULL;
					unsigned int len = vcomment->comment_lengths[i] - OGG_ID3_ID[j].size;
					unsigned int outlen = 0;
					bool ok = base64_decode_alloc (vcomment->user_comments[i] + OGG_ID3_ID[j].size, len, &buf, &outlen);
					if(!ok)
						break;

					if(buf == NULL)
						break;

					if(c_fields[OGG_ID3_ID[j].id])
						free(c_fields[OGG_ID3_ID[j].id]);

					c_fields[OGG_ID3_ID[j].id] = (wchar_t*) buf;

					wchar_t *tmp = (wchar_t*) malloc(10 * sizeof(wchar_t));
					if(tmp)
					{
						swprintf(tmp, L"%u", outlen); 
						if(c_fields[ID3_INFO_PICTURE_DATA_SIZE])
							free(c_fields[ID3_INFO_PICTURE_DATA_SIZE]);

						c_fields[ID3_INFO_PICTURE_DATA_SIZE] = tmp;

						have_picture = 1;


						// set picture type to cover
						tmp = (wchar_t*) malloc(10 * sizeof(wchar_t));
						if(tmp)
						{
							swprintf(tmp, L"%u", 3); 
							if(c_fields[ID3_INFO_PICTURE_TYPE])
								free(c_fields[ID3_INFO_PICTURE_TYPE]);

							c_fields[ID3_INFO_PICTURE_TYPE] = tmp;
								

						}

					}

					break;	
				}
				else if(OGG_ID3_ID[j].id == METADATA_BLOCK_PICTURE)
				{
				
					if(have_picture)
						break;

					wchar_t **field;
					char *buf = NULL;
					unsigned char *ptr = NULL;
					unsigned int len = vcomment->comment_lengths[i] - OGG_ID3_ID[j].size;
					unsigned int outlen = 0;

					
					bool ok = base64_decode_alloc (vcomment->user_comments[i] + OGG_ID3_ID[j].size, len, &buf, &outlen);
					if(!ok)
						break;

					if(buf == NULL)
						break;

					ptr = (unsigned char*) buf;

					if(outlen < 44)
					{
						free(buf);
						break;
					}

					// picture type
					unsigned int nType = ((ptr[0]<<24) | (ptr[1]<<16) | (ptr[2]<<8) | ptr[3]);


					wchar_t *tmp = (wchar_t*) malloc(10 * sizeof(wchar_t));
					if(tmp)
					{
						swprintf(tmp, L"%u", nType); 
						field = &c_fields[ID3_INFO_PICTURE_TYPE];
						if(*field)
							free(*field);

						*field = tmp;
					}


					if(outlen < 8)
					{
						free(buf);
						break;
					}

					ptr += 4;
					outlen -= 4;

					// picture MIME
					unsigned int nMIMELength = ((ptr[0]<<24) | (ptr[1]<<16) | (ptr[2]<<8) | ptr[3]);

					ptr += 4;
					outlen -= 4;
					if(outlen < nMIMELength + 4)
					{
						free(buf);
						break;
					}

					// MIME
					tmp = ANSIToUTF16((char*) ptr, nMIMELength);
					if(tmp)
					{
						field = &c_fields[ID3_INFO_PICTURE_MIME];
						if(*field)
							free(*field);

						*field = tmp;
					}

					ptr += nMIMELength;
					outlen -= nMIMELength;

					// description
					// picture MIME
					unsigned int nDescLength = ((ptr[0]<<24) | (ptr[1]<<16) | (ptr[2]<<8) | ptr[3]);
					ptr += 4;
					outlen -= 4;
					if(outlen < nDescLength + 4)
					{
						free(buf);
						break;
					}

				
					tmp = UTF8ToUTF16((char*) ptr, nDescLength);
					if(tmp)
					{
						field = &c_fields[ID3_INFO_PICTURE_DESCRIPTION];
						if(*field)
							free(*field);

						*field = tmp;
					}

					ptr += nDescLength;
					outlen -= nDescLength;

					if(outlen < 21)
					{
						free(buf);
						break;
					}				

					ptr += 16;
					outlen -= 16;

					// picture data
					unsigned int nDataLength = ((ptr[0]<<24) | (ptr[1]<<16) | (ptr[2]<<8) | ptr[3]);
					ptr += 4;
					outlen -= 4;
					if(outlen < nDataLength)
					{
						free(buf);
						break;
					}

					

					// copy picture data

					tmp = (wchar_t*) malloc(nDataLength);
					if(tmp)
					{
						memcpy(tmp, ptr, nDataLength);
						field = &c_fields[ID3_INFO_PICTURE_DATA];
						if(*field)
							free(*field);

						*field = tmp;
					}

					// set data size

					tmp = (wchar_t*) malloc(10 * sizeof(wchar_t));
					if(tmp)
					{
						swprintf(tmp, L"%u", nDataLength); 
						field = &c_fields[ID3_INFO_PICTURE_DATA_SIZE];
						if(*field)
							free(*field);

						*field = tmp;

						have_picture = 1;
					}
					
					free(buf);
					break;
						
				}
				else
				{
					unsigned int len = vcomment->comment_lengths[i] - OGG_ID3_ID[j].size;
					wchar_t *out = UTF8ToUTF16(vcomment->user_comments[i] + OGG_ID3_ID[j].size, len);
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

	return c_fields;
}




int WOggDecoder::Seek(unsigned int nSamples)
{
	ASSERT_W(c_fReady);

	err(DECODER_NO_ERROR);

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

	if(ov_seekable(&vf) == 0)
	{
		err(DECODER_UNKNOWN_ERROR);
		return 0;
	}

		
	if(ov_pcm_seek(&vf, nSamples) != 0)
	{
	
		err(DECODER_UNKNOWN_ERROR);
		return 0;
	}

	c_nCurrentPosition = nSamples;

	c_nReversePos = nSamples;
	return 1;

}


int WOggDecoder::GetData(DECODER_DATA *pDecoderData)
{
	ASSERT_W(c_fReady);
	ASSERT_W(pDecoderData);
	ASSERT_W(pDecoderData->pSamples);

	unsigned int nSamplesNeed = pDecoderData->nSamplesNum;	// number of samples requested by user
	unsigned int nUserRequest = pDecoderData->nSamplesNum;
	unsigned int nSamplesHave = 0;
	unsigned int nBufferDone = 0;

	c_fEndOfStream = pDecoderData->fEndOfStream;

	int current_section = c_nLogicalBitstreamIndex;
	int ret;

	int *pchOutBuffer = (int*) pDecoderData->pSamples;	// use int because here we are working always with 16 bit stereo samples
	
	// initialize this to 0													
	pDecoderData->nBufferDone = 0;
	pDecoderData->nBufferInQueue = 0;
	pDecoderData->nBytesInQueue = 0;
	pDecoderData->nSamplesNum = 0;

	pDecoderData->nStartPosition = 0;
	pDecoderData->nEndPosition = 0;

	c_nBufferDone = 0;


	while(nSamplesNeed)	// try to get all data that user need
	{
		if(c_nBufferSize == 0)	// we don't have any data in buffer, so get new data from decoder
		{
			unsigned int nBytesNeed = SIZE_OF_WORKING_BUFFER * c_nBlockAlign; // always use 16 bit stereo
			unsigned int nBytesHave = 0;

			if(c_fReverse)	// seek back
			{
				if(c_nReversePos == 0)
					break;

				// move position back
				if(c_nReversePos >= SIZE_OF_WORKING_BUFFER)
				{
					c_nReversePos -= SIZE_OF_WORKING_BUFFER;	
				}
				else
				{
					nSamplesNeed = c_nReversePos;
					c_nReversePos = 0;
				}

				if(ov_seekable(&vf) == 0)
					return DECODER_FATAL_ERROR;

				
				if(ov_pcm_seek(&vf, c_nReversePos) != 0)
					return DECODER_FATAL_ERROR;
					
			}
			

			// =====================================================================
			// FILL WORKING BUFFER WITH DATA

			unsigned int nTryAgain = 0;
			char *buf = c_pBufferAlloc;
			while(nBytesNeed)
			{
				ret = ov_read(&vf, buf, nBytesNeed, 0,2,1, &current_section);
				nBufferDone += c_nBufferDone;
				switch(ret)
				{
					case OV_HOLE:		// interruption in the data, try again
					{
						if(nTryAgain > 10)	// prevent continuous loop
						{
							nBytesNeed = 0;
						}
							
						nTryAgain++;	
					}
					break;

					case OV_EBADLINK:	// invalid stream section was supplied to libvorbisfile, or the requested link is corrupt
					case 0:	// end of file, stop loop
						nBytesNeed = 0;
					break;

					default:
					{
						// we have data from vorbis decoder
						nTryAgain = 0;

						if(c_nLogicalBitstreamIndex != current_section)
							continue;

						buf += ret;
						nBytesHave += ret;
						nBytesNeed -= ret;
					}
					break;
				}
			}

			if(nBytesHave == 0)
				break;

			c_nBufferSize = nBytesHave / c_nBlockAlign;
			c_pchBufferPos = (int*) c_pBufferAlloc;

			// =======================================================================
			// CONVERT WORKING BUFFER INTO STEREO AND REVERSE IF NEEDED
 
			if(c_isInfo.nChannel == 1)
				PCM16MonoToPCM16Stereo((short*) c_pBufferAlloc, c_nBufferSize , (short*) c_pBufferAlloc);

			if(c_fReverse)
				PCM16StereoReverse((int*) c_pBufferAlloc, c_nBufferSize , (int*)c_pBufferAlloc);
	
			// loop again and use this data from working buffer
		}
		else
		{
			// we have some data in buffer, copy this data to user buffer
			if(c_nBufferSize >= nSamplesNeed)
			{
				// we have enough data in  buffer, so we have all data that user needs
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
				// we don't have enough data in buffer, use what we have

				unsigned int i;
				for(i = 0; i < c_nBufferSize; i++)
					pchOutBuffer[i] = c_pchBufferPos[i];

				nSamplesHave += c_nBufferSize;
				// calculate number of samples we need
				nSamplesNeed -= c_nBufferSize;
				// move pointer of user buffer
				pchOutBuffer = &pchOutBuffer[c_nBufferSize];
				// buffer is now empty
				c_nBufferSize = 0;	
			}
		}
	}


	if(nBufferDone)
	{
		pDecoderData->nBufferDone = nBufferDone;
		pDecoderData->nBufferInQueue = c_Queue->GetCount();
		pDecoderData->nBytesInQueue = c_Queue->GetSizeSum();
	}


	if(c_fManagedStream)
	 {
		// we will not return partial data, unless user specified end of stream
		if(nSamplesHave < nUserRequest && c_fEndOfStream == 0)
		{
			// we need to save data from user buffer into internal buffer
			// check if we need to reallocate internal buffer

			if(c_nBufferAllocSize < nSamplesHave)
			{
				char *tmp = (char*) malloc(nSamplesHave * 4);	// always use 16 bit stereo
				if(tmp == 0)
					return DECODER_FATAL_ERROR;
					
				free(c_pBufferAlloc);
				c_pBufferAlloc = tmp;	
				c_nBufferAllocSize = nSamplesHave;
			}

			c_pchBufferPos = (int*) c_pBufferAlloc;
			pchOutBuffer = (int*) pDecoderData->pSamples;

			// move data from user buffer into internal buffer
			unsigned int i;
			for(i = 0; i < nSamplesHave; i++)
				c_pchBufferPos[i] = pchOutBuffer[i];
				
			c_nBufferSize = nSamplesHave;	
			
			return DECODER_MANAGED_NEED_MORE_DATA; 
		}
	 }


	if(nSamplesHave == 0)
	{
		if(c_fManagedStream)
		{
			if(c_fEndOfStream)
				return DECODER_END_OF_DATA;

			return DECODER_MANAGED_NEED_MORE_DATA;
		}

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




int WOggDecoder::GetBitrate(int fAverage)
{
	if(fAverage)
		return c_isInfo.nFileBitrate;

	int bitrate = ov_bitrate_instant(&vf);
	if(bitrate != 0 && bitrate != c_LastBitrate)
		c_LastBitrate = bitrate;

	return c_LastBitrate;
}


int WOggDecoder::SetReverseMode(int fReverse)
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



// vorbis file callback functions

size_t WOggDecoder::read_func(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	USER_OGG_STREAM *bit_stream = (USER_OGG_STREAM*) datasource;

	if(bit_stream->pos > bit_stream->end)
		return 0;	// end of stream data

	unsigned int data_size = size * nmemb;
	unsigned int have = bit_stream->end - bit_stream->pos + 1;
	if(data_size > have)
		data_size = have;
	
	ASSERT_W(ptr);
	ASSERT_W(bit_stream->pos);
	memcpy(ptr, bit_stream->pos, data_size);


	unsigned int read_block_num = data_size / size;
	bit_stream->pos = bit_stream->pos + read_block_num * size;

	return read_block_num; 

}


size_t WOggDecoder::managed_read_func(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	WOggDecoder *instance = (WOggDecoder*) datasource;

	instance->c_nBufferDone = 0;

	unsigned int need = size * nmemb;
	unsigned int have = instance->c_Queue->GetSizeSum();

	if(have == 0)
		return 0;	// end of stream

	if(need > have)
		need = have;

	unsigned int ret = instance->c_Queue->PullDataFifo(ptr, need, (int*) &instance->c_nBufferDone);
	return ret / size;
}



int WOggDecoder::seek_func(void *datasource, ogg_int64_t offset, int whence)
{
	USER_OGG_STREAM *bit_stream = (USER_OGG_STREAM*) datasource;
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

int WOggDecoder::close_func(void *datasource)
{
	return 0;
}

long  WOggDecoder::tell_func(void *datasource)
{
	USER_OGG_STREAM *bit_stream = (USER_OGG_STREAM*) datasource;
	return (bit_stream->pos - bit_stream->start);
}




void WOggDecoder::err(unsigned int error_code)
{
	if(error_code > DECODER_UNKNOWN_ERROR)
		error_code = DECODER_UNKNOWN_ERROR;
			
	c_err_msg.errorW = g_ogg_error_strW[error_code];
}




int WOggDecoder::OpenStream(WQueue *pQueue, int fDynamic, int param1, int param2)
{
	err(DECODER_NO_ERROR);


	c_Queue = pQueue;

	unsigned int nSize = pQueue->GetSizeSum();
	if(nSize == 0)
	{
		err(DECODER_NOT_VALID_OGG_STREAM);
		return 0;
	}


	unsigned char *ptr;
	unsigned int size; 
	if(pQueue->QueryFirstPointer((void**) &ptr, &size) == 0)
	{
		err(DECODER_NOT_VALID_OGG_STREAM);
		return 0;
	}

	c_fManagedStream = 0;

	cb.read_func = read_func;
	cb.close_func = close_func;
	cb.seek_func = seek_func;
	cb.tell_func = tell_func;

	if(param1 == 0)
	{
		c_pchStreamStart = ptr;
		c_nStreamSize = size;
		c_pchStreamEnd = c_pchStreamStart + c_nStreamSize - 1;


		if(fDynamic)
		{
			c_fManagedStream = 1;
			cb.read_func = managed_read_func;
			cb.close_func = NULL;
			cb.seek_func = NULL;
			cb.tell_func = NULL;

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

		if(_OpenFile(1) == 0)
		{
			Close();
			return 0;
		}

		return 1;

	}

	return 0;
}