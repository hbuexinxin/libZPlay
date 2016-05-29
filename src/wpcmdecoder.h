/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  PCM decoder
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
 * ver: 2.0
 * date: 24. April, 2010.
 *
*/

#ifndef _W_PCMDECODER_H_
#define _W_PCMDECODER_H_

#include "wdecoder.h"



class WPCMDecoder : public WAudioDecoder {
public:

	WPCMDecoder();
	~WPCMDecoder();

	int OpenStream(WQueue *pQueue, int fDynamic, int param1, int param2);

	INPUT_STREAM_INFO *GetStreamInfo();

	wchar_t **GetID3Info(int version, char *pStream, unsigned int nStreamSize, int param1, int param2);

	int Close();

// ======================================================================
	int  Initialize(int param1, int param2, int param3, int param4);
	//
	//			Raw uncompressed PCM, 8 or 16 bit per sample, 1 or 2 channel, little-endian and big-endian
	//
	//			param1
	//				Specifies sample rate of input data.
	//
	//			param2
	//				Specifies number of channels ( 1 or 2 ).
	//
	//			param3
	//				Specifies bit per sample ( 8 or 16 ):
	//
	//			param4
	//				Big-endian indicator. If this value is 0, little-endian is used.
	//				If this value is 1, big-endian is used.
// ===========================================================================

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


	// input data
	int c_nInSampleRate;
	int c_inChannelNum;
	int c_inBitPerSample;
	int c_fBigEndian;
	int c_fSigned;


	// indicates that stream is ready
	unsigned int c_fReady;

	// info about input stream
	INPUT_STREAM_INFO c_isInfo;

	// ID3 INFO
	char *c_id3Info[ID3_FIELD_NUMBER];
	wchar_t *c_id3InfoW[ID3_FIELD_NUMBER];


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

};




#endif
