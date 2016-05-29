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


#ifndef _W_OGGDECODER_H_
#define _W_OGGDECODER_H_

#include "wdecoder.h"
#include "../decoders/libvorbis/include/vorbis/codec.h"
#include "../decoders/libvorbis/include/vorbis/vorbisfile.h"


typedef struct {
	unsigned char *start;
	unsigned char *end;
	unsigned char *pos;
	unsigned int size;
} USER_OGG_STREAM;

class WOggDecoder : public WAudioDecoder {
public:

	WOggDecoder();
	~WOggDecoder();

	int OpenStream(WQueue *pQueue, int fDynamic, int param1, int param2);
	INPUT_STREAM_INFO *GetStreamInfo();

	wchar_t **GetID3Info(int version, char *pStream, unsigned int nStreamSize, int param1, int param2);

	int Close();
	int Initialize(int param1, int param2, int param3, int param4);
	int Uninitialize();
	void  Release();
	int GetData(DECODER_DATA *pDecoderData);

	// seek current position
	int Seek(unsigned int nSamples);

	int GetBitrate(int fAverage);

	DECODER_ERROR_MESSAGE *GetError();

	int SetReverseMode(int fReverse);

private:

	// pointer to first byte of input  data stream
	unsigned char *c_pchStreamStart;
	// pointer to last byte of input data stream
	unsigned char *c_pchStreamEnd;
	// size of input data stream
	unsigned int c_nStreamSize;

	unsigned int c_nCurrentPosition;	// current position



	// indicates that stream is ready
	unsigned int c_fReady;

	// info about input stream
	INPUT_STREAM_INFO c_isInfo;


	wchar_t *c_fields[ID3_FIELD_NUMBER_EX];


	// indicates reverse mode
	unsigned int c_fReverse;
	// reverse position
	unsigned int c_nReversePos;

	// managed stream
	unsigned int c_fManagedStream;
	// end of stream indicator
	unsigned int c_fEndOfStream;

	// buffers done
	unsigned int c_nBufferDone;

	// working buffer
	char *c_pBufferAlloc;

	// buffer allocated size in samples
	unsigned int c_nBufferAllocSize;
	int *c_pchBufferPos;
	unsigned int c_nBufferSize;

	// block align
	unsigned int c_nBlockAlign;

	int _OpenFile(unsigned int fLite);

	WQueue *c_Queue;

// ============================================================
	ov_callbacks cb;	// callback functions for reading data from stream
	OggVorbis_File vf;
	vorbis_info *vi;
	vorbis_comment *vcomment;

	// IO callback functions
	//
	// mapped memory or memory buffer
	static size_t read_func(void *ptr, size_t size, size_t nmemb, void *datasource);
	static int seek_func(void *datasource, ogg_int64_t offset, int whence);
	static int close_func(void *datasource);
	static long  tell_func(void *datasource);
	static size_t managed_read_func(void *ptr, size_t size, size_t nmemb, void *datasource);


	int c_nLogicalBitstreamIndex;	// index of logical bitstream
	int c_nNumberOfLogicalBitstreams;	// number of logical bitstreams

	USER_OGG_STREAM bit_stream;

	// ==========================================

	void err(unsigned int error_code);
	DECODER_ERROR_MESSAGE c_err_msg;

	int c_LastBitrate;
};




#endif
