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

#ifndef _W_FLACENCODER_H_
#define _W_FLACENCODER_H_

#include "wencoder.h"

// need this for FLAC encoder
#include "../decoders/libvorbis/include/vorbis/codec.h"
#include "../decoders/libvorbis/include/vorbis/vorbisfile.h"

#include "../decoders/libflac/FLAC/stream_encoder.h"
#include "../decoders/libflac/FLAC/metadata.h"


class WFLACEncoder : public WAudioEncoder {
public:

	WFLACEncoder(int fOgg);
	~WFLACEncoder();

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


	FLAC__StreamEncoder *c_encoder;
	FLAC__int32 *c_pcm;
	unsigned int c_pcm_size;

	int c_fOgg;

	void *c_user_data;


	TEncoderReadCallback c_read_calllback;
	TEncoderWriteCallback c_write_callback;
	TEncoderSeekCallback c_seek_callback;
	TEncoderTellCallback c_tell_callback;



	static FLAC__StreamEncoderReadStatus f_read_callback(const FLAC__StreamEncoder *encoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
	static  FLAC__StreamEncoderWriteStatus  f_write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void *client_data);
	static FLAC__StreamEncoderSeekStatus f_seek_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data);
	static FLAC__StreamEncoderTellStatus f_tell_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data);


};




#endif
