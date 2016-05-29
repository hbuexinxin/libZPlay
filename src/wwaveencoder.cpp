/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  Wave encoder
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
#include "wwaveencoder.h"



enum {
	ENCODER_NO_ERROR = 0,
	ENCODER_FILEOPEN_ERROR,
	ENCODER_NOT_READY,


	ENCODER_UNKNOWN_ERROR
};





wchar_t *g_wave_encoder_error_strW[ENCODER_UNKNOWN_ERROR + 1] = {
	L"WWaveEncoder::No error.",

	L"WWaveEncoder::File open error.",
	L"WWaveEncoder::Encoder is not ready.",

	L"WWaveEncoder::Unknown error."

};


void WWaveEncoder::Release()
{
	delete this;
}

DECODER_ERROR_MESSAGE *WWaveEncoder::GetError()
{
	return &c_err_msg;
}


void WWaveEncoder::err(unsigned int error_code)
{
	if(error_code > ENCODER_UNKNOWN_ERROR)
		error_code = ENCODER_UNKNOWN_ERROR;

	c_err_msg.errorW = g_wave_encoder_error_strW[error_code];

}


WWaveEncoder::WWaveEncoder()
{
	err(ENCODER_NO_ERROR);

	c_fReady = 0;
	c_fRaw = 0;
}

WWaveEncoder::WWaveEncoder(int fRawPCM)
{
	err(ENCODER_NO_ERROR);

	c_fReady = 0;
	c_fRaw = fRawPCM;
}


WWaveEncoder::~WWaveEncoder()
{
	Uninitialize();
}


int WWaveEncoder::Initialize(unsigned int nSampleRate, unsigned int nNumberOfChannels, unsigned int nBitPerSample,
			unsigned int custom_value,
			TEncoderReadCallback read_callback,
			TEncoderWriteCallback write_callback,
			TEncoderSeekCallback seek_callback,
			TEncoderTellCallback tell_callback)
{



	c_read_calllback = read_callback;
	c_write_callback = write_callback;
	c_seek_callback = seek_callback;
	c_tell_callback = tell_callback;

	c_user_data = (void*) custom_value;

	c_nDataSize = 0;

	c_nBlockAlign = nNumberOfChannels * nBitPerSample / 8;

	if(c_fRaw == 0)
	{

		// write wave RIFF header
		unsigned int nFileSize = 0; // unknown for now, we will set this value at the end
		unsigned int nSubChunkSize = 16; // for PCM files this is 1
		unsigned int nWavFormat = 1;	// uncompresse file
		unsigned int nChannels = nNumberOfChannels; // ZPlay will ALWAYS output 2 channels, 16 bit, even if input is different
		unsigned int nSampleRate1 = nSampleRate; // sample rate
		unsigned int nByteRate = nSampleRate * nNumberOfChannels * nBitPerSample / 8; 
		unsigned int nBlockAllign = nBitPerSample / 8 * nNumberOfChannels; // ZPlay will ALWAYS output 2 channels, 16 bit, even if input is different.
		unsigned int nBitPerSample1 = nBitPerSample; // ZPlay will ALWAYS output 2 channels, 16 bit, even if input is different.
		unsigned int nDataSize = 0;
		// write RIFF header and set empty wave file


		c_write_callback("RIFF", 4, c_user_data); // 	Chunk ID
		c_write_callback(&nFileSize, 4, c_user_data); // chunk size = File Size - 8
		c_write_callback("WAVE", 4, c_user_data); // File Format
		c_write_callback("fmt ", 4, c_user_data);	// SubChunk1 ID
		c_write_callback(&nSubChunkSize, 4, c_user_data);	// SubChunk1 Size = 16 bytes
		c_write_callback(&nWavFormat, 2, c_user_data); // Audio Format - uncompressed audio
		c_write_callback(&nChannels, 2, c_user_data);	// number of channels, ALWAYS  2 channels, 16 bit, even if input is different
		c_write_callback(&nSampleRate1, 4, c_user_data); // sample rate
		c_write_callback(&nByteRate, 4, c_user_data); // byte rate
		c_write_callback(&nBlockAllign, 2, c_user_data); // block allign, ALWAYS  2 channels, 16 bit, even if input is different
		c_write_callback(&nBitPerSample1, 2, c_user_data); // bit per sample, ALWAYS  2 channels, 16 bit, even if input is different
		c_write_callback("data", 4, c_user_data); // SubChunk2 ID
		c_write_callback( &nDataSize, 4, c_user_data); // The number of bytes following the header. The size of the data.
	}

	c_fReady = 1;
	return 1;
}

int WWaveEncoder::Uninitialize()
{
	// fix RIFF header, write correct file size and data size
	
	unsigned int nFileSize = c_nDataSize + 36;

	c_seek_callback(4, c_user_data); 	// seek to RIFF FileSize field
	c_write_callback(&nFileSize, 4, c_user_data);


	c_seek_callback(40, c_user_data); // seek to RIFF nDataSize field
	c_write_callback(&c_nDataSize, 4, c_user_data);


	c_fReady = 0;
	return 1;
}


int WWaveEncoder::EncodeSamples(void *pSamples, unsigned int nNumberOfSamples)
{
	if(c_fReady == 0)
	{
		err(ENCODER_NOT_READY);
		return 0;

	}

	c_nDataSize += c_write_callback(pSamples, nNumberOfSamples * c_nBlockAlign, c_user_data);
	

	return 1;
}


