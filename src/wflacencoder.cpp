/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  FLAC encoder
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
#include "wtools.h"
#include "WFLACEncoder.h"





enum {
	ENCODER_NO_ERROR = 0,
	ENCODER_FILEOPEN_ERROR,
	ENCODER_NOT_READY,
	ENCODER_INITIALIZATION_ERROR,
	ENCODER_MEMORY_ALLOCATION_FAIL,


	ENCODER_UNKNOWN_ERROR
};





wchar_t *g_flac_encoder_error_strW[ENCODER_UNKNOWN_ERROR + 1] = {
	L"WFLACEncoder::No error.",
	L"WFLACEncoder::File open error.",
	L"WFLACEncoder::Encoder is not ready.",
	L"WFLACEncoder::Encoder initialization error",
	L"WFLACEncoder::Memory allocation fail.",

	L"WFLACEncoder::Unknown error."

};



void WFLACEncoder::Release()
{
	delete this;
}

DECODER_ERROR_MESSAGE *WFLACEncoder::GetError()
{
	return &c_err_msg;
}


void WFLACEncoder::err(unsigned int error_code)
{
	if(error_code > ENCODER_UNKNOWN_ERROR)
		error_code = ENCODER_UNKNOWN_ERROR;

	c_err_msg.errorW = g_flac_encoder_error_strW[error_code];

}


WFLACEncoder::WFLACEncoder(int fOgg)
{
	err(ENCODER_NO_ERROR);
	c_fReady = 0;
	c_nSampleRate = 0;
	c_nNumberOfChannels = 0;
	c_encoder = NULL;
	c_pcm = NULL;
	c_pcm_size = 0;
	c_fOgg = fOgg;
	
}


WFLACEncoder::~WFLACEncoder()
{
	Uninitialize();
}


int WFLACEncoder::Initialize(unsigned int nSampleRate, unsigned int nNumberOfChannels, unsigned int nBitPerSample,
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


	if((c_encoder = FLAC__stream_encoder_new()) == NULL)
	{
		err(ENCODER_INITIALIZATION_ERROR);
		return 0;
	}


	FLAC__bool ok = true;

	ok &= FLAC__stream_encoder_set_verify(c_encoder, false);
	ok &= FLAC__stream_encoder_set_compression_level(c_encoder, 5);
	ok &= FLAC__stream_encoder_set_channels(c_encoder, c_nNumberOfChannels);
	ok &= FLAC__stream_encoder_set_bits_per_sample(c_encoder, c_nBitBerSample);
	ok &= FLAC__stream_encoder_set_sample_rate(c_encoder, c_nSampleRate);
	ok &= FLAC__stream_encoder_set_total_samples_estimate(c_encoder, 0);

	if(!ok)
	{
		FLAC__stream_encoder_delete(c_encoder);
		c_encoder = NULL;
		err(ENCODER_INITIALIZATION_ERROR);
		return 0;
	}


	if(c_fOgg)
	{
		if(FLAC__stream_encoder_init_ogg_stream(c_encoder, f_read_callback, f_write_callback, f_seek_callback, f_tell_callback, NULL, this) != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
		{
			FLAC__stream_encoder_delete(c_encoder);
			c_encoder = NULL;
			err(ENCODER_FILEOPEN_ERROR);
			return 0;			
		}

	}
	else
	{
		if(FLAC__stream_encoder_init_stream(c_encoder, f_write_callback, f_seek_callback, f_tell_callback, NULL, this) != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
		{
			FLAC__stream_encoder_delete(c_encoder);
			c_encoder = NULL;
			err(ENCODER_FILEOPEN_ERROR);
			return 0;			
		}
	}

	c_fReady = 1;
	return 1;
}

int WFLACEncoder::Uninitialize()
{
	if(c_fReady)
	{
		FLAC__stream_encoder_finish(c_encoder);
		FLAC__stream_encoder_delete(c_encoder);
		c_encoder = NULL;

	}

	if(c_pcm)
		free(c_pcm);

	c_pcm = NULL;
	c_pcm_size = 0;

	c_fReady = 0;
	return 1;
}


int WFLACEncoder::EncodeSamples(void *pSamples, unsigned int nNumberOfSamples)
{
	if(c_fReady == 0)
	{
		err(ENCODER_NOT_READY);
		return 0;

	}


	if(c_pcm_size < nNumberOfSamples)
	{
		FLAC__int32 *tmp = (FLAC__int32*) malloc(nNumberOfSamples * c_nNumberOfChannels * sizeof(FLAC__int32));
		if(tmp == 0)
		{
			err(ENCODER_MEMORY_ALLOCATION_FAIL);
			return 0;
		}

		if(c_pcm)
			free(c_pcm);

		c_pcm = tmp;
		c_pcm_size = nNumberOfSamples;
	}



	unsigned int i;


	switch(c_nBitBerSample)
	{

		case 16:
		{

			short *samples = (short*) pSamples;
			for(i = 0; i < nNumberOfSamples * c_nNumberOfChannels; i++)		
				c_pcm[i] = samples[i];
			
		}
		break;

		default:
		return 1;


	}

			
	FLAC__stream_encoder_process_interleaved(c_encoder, c_pcm, nNumberOfSamples);
	return 1;
}



FLAC__StreamEncoderReadStatus WFLACEncoder::f_read_callback(const FLAC__StreamEncoder *encoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	WFLACEncoder *instance = (WFLACEncoder*) client_data;
	if(*bytes > 0)
	{
		unsigned int ret = instance->c_read_calllback(buffer, *bytes, instance->c_user_data);
		*bytes = ret;
		if(ret == 0)
			return FLAC__STREAM_ENCODER_READ_STATUS_END_OF_STREAM;

		return FLAC__STREAM_ENCODER_READ_STATUS_CONTINUE;
	}
	else
		return FLAC__STREAM_ENCODER_READ_STATUS_ABORT;

}



FLAC__StreamEncoderWriteStatus WFLACEncoder::f_write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void *client_data)
{
	WFLACEncoder *instance = (WFLACEncoder*) client_data;
	unsigned int ret = instance->c_write_callback((void*) buffer, bytes, instance->c_user_data);
	if(ret == 0)
		return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;

	return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}


FLAC__StreamEncoderSeekStatus WFLACEncoder::f_seek_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	WFLACEncoder *instance = (WFLACEncoder*) client_data;
	unsigned int ret = instance->c_seek_callback(absolute_byte_offset, instance->c_user_data);
	if(ret == 0)
		return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;

	return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;



}


FLAC__StreamEncoderTellStatus WFLACEncoder::f_tell_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	WFLACEncoder *instance = (WFLACEncoder*) client_data;

	unsigned int ret = instance->c_tell_callback(instance->c_user_data);

	*absolute_byte_offset = ret;
	return FLAC__STREAM_ENCODER_TELL_STATUS_OK;

}

