/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  MP3 encoder
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
#include "wmp3encoder.h"



enum {
	ENCODER_NO_ERROR = 0,
	ENCODER_FILEOPEN_ERROR,
	ENCODER_NOT_READY,
	ENCODER_INIT_ERROR,
	ENCODER_MEMORY_ALLOCATION_FAIL,


	ENCODER_UNKNOWN_ERROR
};





wchar_t *g_mp3_encoder_error_strW[ENCODER_UNKNOWN_ERROR + 1] = {
	L"WMp3Encoder::No error.",

	L"WMp3Encoder::File open error.",
	L"WMp3Encoder::Encoder is not ready.",
	L"WMp3Encoder::Encoder initialization error.",
	L"WMp3Encoder::Memory allocation fail.",


	L"WMp3Encoder::Unknown error."

};


void WMp3Encoder::Release()
{
	delete this;
}

DECODER_ERROR_MESSAGE *WMp3Encoder::GetError()
{
	return &c_err_msg;
}


void WMp3Encoder::err(unsigned int error_code)
{
	if(error_code > ENCODER_UNKNOWN_ERROR)
		error_code = ENCODER_UNKNOWN_ERROR;

	c_err_msg.errorW = g_mp3_encoder_error_strW[error_code];

}


WMp3Encoder::WMp3Encoder()
{
	err(ENCODER_NO_ERROR);
	c_fReady = 0;
	c_nSampleRate = 0;
	c_nNumberOfChannels = 0;
	c_nSamplesBufferSizeInBytes = 0;
	c_pWorkingBuffer = NULL;
	gfp = NULL;
}


WMp3Encoder::~WMp3Encoder()
{
	Uninitialize();
}


int WMp3Encoder::Initialize(unsigned int nSampleRate, unsigned int nNumberOfChannels, unsigned int nBitPerSample,
			unsigned int custom_value,
			TEncoderReadCallback read_callback,
			TEncoderWriteCallback write_callback,
			TEncoderSeekCallback seek_callback,
			TEncoderTellCallback tell_callback)
{
	c_nSampleRate = nSampleRate;
	c_nNumberOfChannels = nNumberOfChannels;
	c_nBitBerSample = nBitPerSample;

	c_read_calllback = read_callback;
	c_write_callback = write_callback;
	c_seek_callback = seek_callback;
	c_tell_callback = tell_callback;

	c_user_data = (void*) custom_value;

	// init encoder
	gfp = lame_init();

	if(gfp == NULL)
	{
		err(ENCODER_INIT_ERROR);
		return 0;
	}

   lame_set_num_channels(gfp, c_nNumberOfChannels);
   lame_set_in_samplerate(gfp, c_nSampleRate);
   lame_set_brate(gfp, 128);
   lame_set_mode(gfp, JOINT_STEREO);
   lame_set_quality(gfp, 7);   /* 2=high  5 = medium  7=low */ 
   lame_set_VBR(gfp, vbr_off);

   if(lame_init_params(gfp) < 0)
   {
		lame_close(gfp);
		err(ENCODER_INIT_ERROR);
		return 0;
   }


  //LAME encoding call will accept any number of samples.  
    if ( 0 == lame_get_version( gfp ) )
    {
        // For MPEG-II, only 576 samples per frame per channel
        c_NumberOfInputSamples = 576 * lame_get_num_channels( gfp );
    }
    else
    {
        // For MPEG-I, 1152 samples per frame per channel
        c_NumberOfInputSamples = 1152 * lame_get_num_channels( gfp );
    }


    // Set MP3 buffer size, conservative estimate
    c_nSamplesBufferSizeInBytes = (DWORD)( 1.25 * ( c_NumberOfInputSamples / lame_get_num_channels( gfp ) ) + 7200 );

	// allocate  working buffer
	c_pWorkingBuffer = (unsigned char*) malloc(c_nSamplesBufferSizeInBytes);
	if(c_pWorkingBuffer == NULL)
	{
		lame_close(gfp);
		err(ENCODER_MEMORY_ALLOCATION_FAIL);
		return 0;

	}


	c_fReady = 1;
	return 1;
}

int WMp3Encoder::Uninitialize()
{

	if(c_fReady)
	{
		EncodeSamples(NULL, 0);

		lame_close(gfp);

		if(c_pWorkingBuffer)
			free(c_pWorkingBuffer);

		c_pWorkingBuffer = NULL;
		c_nSamplesBufferSizeInBytes = 0;
	}

	c_fReady = 0;
	return 1;
}


int WMp3Encoder::EncodeSamples(void *pSamples, unsigned int nNumberOfSamples)
{
	ASSERT_W(c_nBitBerSample == 16);
	
	if(c_fReady == 0)
	{
		err(ENCODER_NOT_READY);
		return 0;
	}

	int nOutputBytes;

	short *samples = (short*) pSamples;
	unsigned int nHaveSamples = nNumberOfSamples;

	if(nNumberOfSamples == 0)
	{
		nOutputBytes = lame_encode_flush(gfp, c_pWorkingBuffer, c_nSamplesBufferSizeInBytes);
		if(nOutputBytes > 0)
			c_write_callback(c_pWorkingBuffer, nOutputBytes, c_user_data); // 	Chunk ID
			
		return 1;

	}

	while(nHaveSamples)
	{
		unsigned int nFill = c_NumberOfInputSamples;
		if(nFill > nHaveSamples)
			nFill = nHaveSamples;

		

		if ( 1 == lame_get_num_channels( gfp ) )
		{
			nOutputBytes = lame_encode_buffer(gfp, samples, samples, nFill, c_pWorkingBuffer, 0);
			samples = &samples[nFill];
		}
		else
		{
			nOutputBytes = lame_encode_buffer_interleaved(gfp, samples, nFill, c_pWorkingBuffer, 0);
			samples = &samples[nFill * 2];
		}

		nHaveSamples -= nFill;

		if(nOutputBytes > 0)
			c_write_callback(c_pWorkingBuffer, nOutputBytes, c_user_data); // 	Chunk ID

	}

	return 1;
}

