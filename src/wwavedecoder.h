/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  Wave decoder
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

#ifndef _W_WAVEDECODER_H_
#define _W_WAVEDECODER_H_

#include "wdecoder.h"



class WWaveDecoder : public WAudioDecoder {
public:

	WWaveDecoder();
	~WWaveDecoder();

	int OpenStream(WQueue *pQueue, int fDynamic, int param1, int param2);

	INPUT_STREAM_INFO *GetStreamInfo();

	wchar_t **GetID3Info(int version, char *pStream, unsigned int nStreamSize, int param1, int param2);

	int Close();
	int  Initialize(int param1, int param2, int param3, int param4);
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

	// pointer to first byte of input data
	unsigned char *c_pchStart;
	// pointer to last byte of input data
	unsigned char *c_pchEnd;
	// input data size in bytes
	unsigned int c_nSize;
	// current position within data stream, decoder
	unsigned char *c_pchPosition;


	// indicates that stream is ready
	unsigned int c_fReady;

	// info about input stream
	INPUT_STREAM_INFO c_isInfo;

	char c_pchDesc[128];

	// ID3 INFO
	wchar_t *c_fields[ID3_FIELD_NUMBER_EX];



	// bit per sample, need this to convert 8 bit to 16 bit
	unsigned int c_nBitPerSample;
	// clockalign, need this to seek
	unsigned int c_nBlockAlign;
	// specify reverse mode
	unsigned int c_fReverse;

	// managed stream
	unsigned int c_fManagedStream;
	// end of stream indicator
	unsigned int c_fEndOfStream;



	int _OpenFile(unsigned int fSkipExtraChecking);

	WQueue *c_Queue;

	void err(unsigned int error_code);
	DECODER_ERROR_MESSAGE c_err_msg;


	unsigned int c_nCurrentPosition;
	unsigned int c_nSeekPosition;


};




#endif
