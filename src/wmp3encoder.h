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

#ifndef _W_WMP3ENCODER_H_
#define _W_WMP3ENCODER_H_

#include "wencoder.h"
#include "../decoders/lame/include/lame.h"


class WMp3Encoder : public WAudioEncoder {
public:

	WMp3Encoder();
	~WMp3Encoder();

int Initialize(unsigned int nSampleRate, unsigned int nNuberOfChannels, unsigned int nBitPerSample,
			unsigned int custom_value,
			TEncoderReadCallback read_callback,
			TEncoderWriteCallback write_callback,
			TEncoderSeekCallback seek_callback,
			TEncoderTellCallback tell_callback
			);


	int Uninitialize();
	int EncodeSamples(void *pSamples, unsigned int nNumberOfSamples);
	void  Release();
	DECODER_ERROR_MESSAGE * GetError();


private:

	void err(unsigned int error_code);
	DECODER_ERROR_MESSAGE c_err_msg;

	int c_fReady;

	unsigned int c_nSampleRate;
	unsigned int c_nNumberOfChannels;
	unsigned int c_nBitBerSample;
	unsigned int c_nBlockAllign;

	unsigned int c_nSamplesBufferSizeInBytes;
	unsigned char *c_pWorkingBuffer;
	unsigned int c_NumberOfInputSamples;

	void *c_user_data;


	lame_global_flags *gfp;

	TEncoderReadCallback c_read_calllback;
	TEncoderWriteCallback c_write_callback;
	TEncoderSeekCallback c_seek_callback;
	TEncoderTellCallback c_tell_callback;



};




#endif
