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


#ifndef _W_FLACDECODER_H_
#define _W_FLACDECODER_H_

#include "wdecoder.h"
#include "../decoders/libvorbis/include/vorbis/codec.h"
#include "../decoders/libvorbis/include/vorbis/vorbisfile.h"

#include "../decoders/libflac/FLAC/stream_decoder.h"
#include "../decoders/libflac/FLAC/metadata.h"


typedef struct {
	unsigned char *start;
	unsigned char *end;
	unsigned char *pos;
	unsigned int size;
} USER_FLAC_STREAM;


typedef struct {
	FLAC__StreamDecoderReadCallback read_func;
	FLAC__StreamDecoderSeekCallback seek_func;
	FLAC__StreamDecoderTellCallback tell_func;
	FLAC__StreamDecoderLengthCallback length_func;
	FLAC__StreamDecoderEofCallback eof_func;
	FLAC__StreamDecoderWriteCallback write_func;
	FLAC__StreamDecoderMetadataCallback metadata_func;
	FLAC__StreamDecoderErrorCallback error_func;

} FLAC_CALLBACK_FUNCTIONS;

class WFLACDecoder : public WAudioDecoder {
public:

	WFLACDecoder();
	WFLACDecoder(int fOgg);
	~WFLACDecoder();

	int OpenStream(WQueue *pQueue, int fDynamic, int param1, int param2);
	INPUT_STREAM_INFO *GetStreamInfo();

	wchar_t **GetID3Info(int version, char *pStream, unsigned int nStreamSize, int param1, int param2);

	int Close();

// ==================================================================
// INITIALIZE DECODER
	// PARAMETERS:
	//  param1	- 0: native FLAC, 1: OGG FLAC
	int Initialize(int param1, int param2, int param3, int param4);
// ===============================================================

	int Uninitialize();
	void  Release();

	int GetData(DECODER_DATA *pDecoderData);

	// seek current position
	int Seek(unsigned int nSamples);

	int GetBitrate(int fAverage);

	DECODER_ERROR_MESSAGE *GetError();

	int SetReverseMode(int fReverse);

private:
	FLAC__StreamDecoder *c_decoder;
	unsigned int c_ogg_transport_layer;
	

	FLAC_CALLBACK_FUNCTIONS flac_cb;

	static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);
	static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
	static FLAC__StreamDecoderReadStatus read_func(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
	static FLAC__StreamDecoderReadStatus managed_read_func(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
	static FLAC__StreamDecoderSeekStatus seek_func(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
	static FLAC__StreamDecoderTellStatus tell_func(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
	static FLAC__StreamDecoderLengthStatus length_func(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
	static FLAC__bool eof_func(const FLAC__StreamDecoder *decoder, void *client_data);
	static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);


	static size_t IOCallback_Read(void *ptr, size_t size, size_t nmemb, FLAC__IOHandle handle);
	static int IOCallback_Seek(FLAC__IOHandle handle, FLAC__int64 offset, int whence);
	static FLAC__int64 IOCallback_Tell(FLAC__IOHandle handle);


	// pointer to first byte of input  data stream
	unsigned char *c_pchStreamStart;
	// pointer to last byte of input data stream
	unsigned char *c_pchStreamEnd;
	// size of input data stream
	unsigned int c_nStreamSize;


	// indicates that stream is ready
	unsigned int c_fReady;

	// info about input stream
	INPUT_STREAM_INFO c_isInfo;

	// ID3 INFO

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

	int _OpenFile(unsigned int fSkipExtraChecking);

	WQueue *c_Queue;

// ============================================================


	USER_FLAC_STREAM bit_stream;

	// ==========================================


	void err(unsigned int error_code);
	DECODER_ERROR_MESSAGE c_err_msg;


	int *c_pSamplesAlloc;				// allocated buffer for samples
	unsigned int c_nSamplesAllocSize;	// size of allocated buffer in samples
	int *c_pSamplesBuffer;			// current position inside samples buffer
	unsigned int c_nSamplesNum;		// number of samples in samples buffer


	int *c_pReverseBufferAlloc;	// buffer for reverse playing
	int *c_pReverseBuffer;	// buffer for reverse playing

	unsigned int c_nCurrentPosition;

};




#endif
