/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  AAC encoder
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
#include "waacencoder.h"



enum {
	ENCODER_NO_ERROR = 0,
	ENCODER_FILEOPEN_ERROR,
	ENCODER_NOT_READY,
	ENCODER_INITIALIZATION_ERROR,
	ENCODER_MEMORY_ALLOC_ERROR,


	ENCODER_UNKNOWN_ERROR
};





wchar_t *g_aac_encoder_error_strW[ENCODER_UNKNOWN_ERROR + 1] = {
	L"WAACEncoder::No error.",
	L"WAACEncoder::File open error.",
	L"WAACEncoder::Encoder is not ready.",
	L"WAACEncoder::Encoder initialization error.",
	L"WAACEncoder::Memory allocation fail.",

	L"WAACEncoder::Unknown error."

};


void WAACEncoder::Release()
{
	delete this;
}

DECODER_ERROR_MESSAGE *WAACEncoder::GetError()
{
	return &c_err_msg;
}


void WAACEncoder::err(unsigned int error_code)
{
	if(error_code > ENCODER_UNKNOWN_ERROR)
		error_code = ENCODER_UNKNOWN_ERROR;

	c_err_msg.errorW = g_aac_encoder_error_strW[error_code];

}


WAACEncoder::WAACEncoder()
{
	err(ENCODER_NO_ERROR);

	c_encoder = NULL;
	c_out_buffer = NULL;
	c_in_buffer = NULL;
	c_fReady = 0;

}


WAACEncoder::~WAACEncoder()
{
	Uninitialize();
}


int WAACEncoder::Initialize(unsigned int nSampleRate, unsigned int nNumberOfChannels, unsigned int nBitPerSample,
			unsigned int custom_value,
			TEncoderReadCallback read_callback,
			TEncoderWriteCallback write_callback,
			TEncoderSeekCallback seek_callback,
			TEncoderTellCallback tell_callback)
{



	c_nSamplerate = nSampleRate;
	c_nNumberOfChannels = nNumberOfChannels;
	c_nBitPerSample = nBitPerSample;
	c_nBlockAlling = nNumberOfChannels * nBitPerSample / 8;

	c_read_calllback = read_callback;
	c_write_callback = write_callback;
	c_seek_callback = seek_callback;
	c_tell_callback = tell_callback;

	c_user_data = (void*) custom_value;

	// open encoder
	c_encoder = faacEncOpen(nSampleRate, nNumberOfChannels, &c_samplesInput, &c_maxBytesOutput);
	if(c_encoder == NULL)
	{
		err(ENCODER_INITIALIZATION_ERROR);
		return 0;
	}


	c_nNeedBytes = c_samplesInput * c_nBlockAlling / 2;

	// set configuration

	faacEncConfigurationPtr config = faacEncGetCurrentConfiguration(c_encoder);


	config->aacObjectType = LOW;
	config->mpegVersion = MPEG2;
	config->shortctl = SHORTCTL_NORMAL;
	config->allowMidside = 1;
	config->useTns = 0;
	config->outputFormat = 1;	// ADTS
	config->inputFormat = FAAC_INPUT_16BIT;
	config->useLfe = 0;
	config->quantqual = 100;
	config->bitRate = 34;


	if(faacEncSetConfiguration(c_encoder, config) == 0)
	{
		err(ENCODER_INITIALIZATION_ERROR);
		faacEncClose(c_encoder);
		c_encoder = NULL;
		return 0;
	}
	

	// allocate memory for output buffer
	c_out_buffer = (unsigned char*) malloc(c_maxBytesOutput);
	if(c_out_buffer == NULL)
	{
		err(ENCODER_MEMORY_ALLOC_ERROR);
		faacEncClose(c_encoder);
		c_encoder = NULL;
		return 0;
	}


	c_in_buffer = (int32_t*) malloc(c_samplesInput * 4); // 16 bit STEREO


	queue.SetMemoryMode(1);
	c_fReady = 1;
	return 1;
}

int WAACEncoder::Uninitialize()
{
	if(c_fReady)
	{
		// flush encoder
		EncodeSamples(NULL, 0);
		faacEncClose(c_encoder);
		free(c_out_buffer);
		c_out_buffer = NULL;
		free(c_in_buffer);
		c_in_buffer = NULL;
		c_encoder = NULL;
		queue.Clear();
	}

	c_fReady = 0;
	return 1;
}



int WAACEncoder::EncodeSamples(void *pSamples, unsigned int nNumberOfSamples)
{
	if(c_fReady == 0)
	{
		err(ENCODER_NOT_READY);
		return 0;

	}


	if(nNumberOfSamples > 0)
	{
		queue.PushLast(pSamples, nNumberOfSamples * c_nBlockAlling);
		while(queue.GetSizeSum() >= c_nNeedBytes)
		{
			unsigned int nRead = queue.PullDataFifo(c_in_buffer, c_nNeedBytes, 0);
				int bytesWritten = faacEncEncode(c_encoder, (int32_t *) c_in_buffer, c_samplesInput,
					c_out_buffer,
					c_maxBytesOutput);

				if(bytesWritten > 0)
					c_write_callback(c_out_buffer, bytesWritten, c_user_data);
				

		}
	}
	else
	{
		// flush data
		while(queue.GetSizeSum() >= c_nNeedBytes)
		{
			unsigned int nRead = queue.PullDataFifo(c_in_buffer, c_nNeedBytes, 0);
				int bytesWritten = faacEncEncode(c_encoder, (int32_t *) c_in_buffer, c_samplesInput,
					c_out_buffer,
					c_maxBytesOutput);

				if(bytesWritten > 0)
					c_write_callback(c_out_buffer, bytesWritten, c_user_data);

		}

		unsigned int nHaveBytes = queue.GetSizeSum();
		if(nHaveBytes) // use rest of data from queue
		{
			unsigned int nRead = queue.PullDataFifo(c_in_buffer, nHaveBytes, 0);
			unsigned int nHaveSamples = nRead / (c_nBlockAlling / 2);


				int bytesWritten = faacEncEncode(c_encoder, (int32_t *) c_in_buffer, nHaveSamples,
					c_out_buffer,
					c_maxBytesOutput);

				if(bytesWritten > 0)
					c_write_callback(c_out_buffer, bytesWritten, c_user_data);
		}

		// flush decoder

		int bytesWritten = faacEncEncode(c_encoder, NULL, 0,
					c_out_buffer,
					c_maxBytesOutput);

		if(bytesWritten > 0)
			c_write_callback(c_out_buffer, bytesWritten, c_user_data);


	}



	return 1;
}

