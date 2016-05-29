/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  VORBIS OGG encoder
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
#include "wvorbisencoder.h"



enum {
	ENCODER_NO_ERROR = 0,
	ENCODER_FILEOPEN_ERROR,
	ENCODER_NOT_READY,
	ENCODER_MEMORY_ALLOCATION_FAIL,
	ENCODER_INITIALIZATION_ERROR,


	ENCODER_UNKNOWN_ERROR
};





wchar_t *g_vorbis_encoder_error_strW[ENCODER_UNKNOWN_ERROR + 1] = {
	L"WVorbisEncoder::No error.",

	L"WVorbisEncoder::File open error.",
	L"WVorbisEncoder::Encoder is not ready.",
	L"WVorbisEncoder::Memory allocation fail.",
	L"WVorbisEncoder::Encoder initialization error.",

	L"WVorbisEncoder::Unknown error."

};


void WVorbisEncoder::Release()
{
	delete this;
}

DECODER_ERROR_MESSAGE *WVorbisEncoder::GetError()
{
	return &c_err_msg;
}


void WVorbisEncoder::err(unsigned int error_code)
{
	if(error_code > ENCODER_UNKNOWN_ERROR)
		error_code = ENCODER_UNKNOWN_ERROR;

	c_err_msg.errorW = g_vorbis_encoder_error_strW[error_code];

}


WVorbisEncoder::WVorbisEncoder()
{
	err(ENCODER_NO_ERROR);
	c_fReady = 0;
	c_nSampleRate = 0;
	c_nNumberOfChannels = 0;
}


WVorbisEncoder::~WVorbisEncoder()
{
	Uninitialize();
}


int WVorbisEncoder::Initialize(unsigned int nSampleRate, unsigned int nNumberOfChannels, unsigned int nBitPerSample,
			unsigned int custom_value,
			TEncoderReadCallback read_callback,
			TEncoderWriteCallback write_callback,
			TEncoderSeekCallback seek_callback,
			TEncoderTellCallback tell_callback)
{

	c_nSampleRate = nSampleRate;
	c_nNumberOfChannels = nNumberOfChannels;
	c_nBitBerSample = nBitPerSample;
	c_nBlockAllign = nNumberOfChannels * nBitPerSample / 8;

	c_read_calllback = read_callback;
	c_write_callback = write_callback;
	c_seek_callback = seek_callback;
	c_tell_callback = tell_callback;

	c_user_data = (void*) custom_value;


	vorbis_info_init(&vi);

	if(vorbis_encode_init_vbr(&vi, c_nNumberOfChannels, c_nSampleRate, (float) 0.4))
	 {
		err(ENCODER_INITIALIZATION_ERROR);
		return 0;
	 }


	vorbis_comment_init(&vc);
	vorbis_comment_add_tag(&vc,"ENCODER","libZplay");

	vorbis_analysis_init(&vd,&vi);
	vorbis_block_init(&vd,&vb);

	ogg_stream_init(&os, GetTickCount());

    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    vorbis_analysis_headerout(&vd,&vc,&header,&header_comm,&header_code);
    ogg_stream_packetin(&os,&header); /* automatically placed in its ownpage */
    ogg_stream_packetin(&os,&header_comm);
    ogg_stream_packetin(&os,&header_code);

	/* This ensures the actual
	 * audio data will start on a new page, as per spec
	 */
	int eos = 0;
	while(!eos)
	{
		int result = ogg_stream_flush(&os, &og);
		if(result == 0)
			break;

		c_write_callback( og.header, og.header_len, c_user_data);
		c_write_callback( og.body, og.body_len, c_user_data);
	}


	c_fReady = 1;
	return 1;
}

int WVorbisEncoder::Uninitialize()
{
	if(c_fReady)
	{
		EncodeSamples(NULL, 0);	// flush encoder

		ogg_stream_clear(&os);
		vorbis_block_clear(&vb);
		vorbis_dsp_clear(&vd);
		vorbis_comment_clear(&vc);
		vorbis_info_clear(&vi);
	}


	c_fReady = 0;
	return 1;
}



int WVorbisEncoder::EncodeSamples(void *pSamples, unsigned int nNumberOfSamples)
{
	if(c_fReady == 0)
	{
		err(ENCODER_NOT_READY);
		return 0;
	}


	int eos = 0;


	unsigned int i;
	long bytes = nNumberOfSamples * c_nBlockAllign;
	
	/* data to encode */

	/* expose the buffer to submit data */
	float **buffer = vorbis_analysis_buffer(&vd, nNumberOfSamples);
	signed char *readbuffer  = (signed char*) pSamples;
      
	/* uninterleave samples */
	for(i = 0; i < nNumberOfSamples; i++)
	{
		buffer[0][i]=((readbuffer[i*4+1]<<8)|(0x00ff&(int)readbuffer[i*4]))/32768.f;
		buffer[1][i]=((readbuffer[i*4+3]<<8)| (0x00ff&(int)readbuffer[i*4+2]))/32768.f;
	}
    
	/* tell the library how much we actually submitted */
	vorbis_analysis_wrote(&vd,i);
	

	/* vorbis does some data preanalysis, then divvies up blocks for
	  more involved (potentially parallel) processing.  Get a single
	   block for encoding now */
	while(vorbis_analysis_blockout(&vd,&vb) == 1)
	{

		/* analysis, assume we want to use bitrate management */
		vorbis_analysis(&vb,NULL);
		vorbis_bitrate_addblock(&vb);

		while(vorbis_bitrate_flushpacket(&vd,&op))
		{
	
			/* weld the packet into the bitstream */
			ogg_stream_packetin(&os,&op);
	
			/* write out pages (if any) */
			while(!eos)
			{
				int result = ogg_stream_pageout(&os,&og);
				if(result==0)
					break;

			c_write_callback(og.header, og.header_len, c_user_data);
			c_write_callback(og.body, og.body_len, c_user_data);


			/* this could be set above, but for illustrative purposes, I do
			 it here (to show that vorbis does know where the stream ends) */
	  
			if(ogg_page_eos(&og))
				eos = 1;
		}
	}

  }

	return 1;
}

