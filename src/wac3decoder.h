/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  AC3 decoder
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

#ifndef _W_AC3DECODER_H_
#define _W_AC3DECODER_H_

#include "wdecoder.h"

#define HAVE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../decoders/a52dec/include/config.h"
#include "../decoders/a52dec/include/inttypes.h"
#include "../decoders/a52dec/include/a52.h"
#include "../decoders/a52dec/include/mm_accel.h"

#ifdef __cplusplus
}
#endif


class WAC3Decoder : public WAudioDecoder {
public:

	WAC3Decoder();
	~WAC3Decoder();

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


	// input data
	int c_nInSampleRate;
	int c_flags;



	// indicates that stream is ready
	unsigned int c_fReady;

	// info about input stream
	INPUT_STREAM_INFO c_isInfo;


	// specify reverse mode
	unsigned int c_fReverse;

	// managed stream
	unsigned int c_fManagedStream;
	// end of stream indicator
	unsigned int c_fEndOfStream;


	int _OpenFile(unsigned int fSkipExtraChecking);


	// resync stream
	//
	// PARAMETERS:
	//
	// RETURNS:
	//		0 - can't find sync
	//		> 0	- frame length in bytes
	int _Sync(int *sample_rate, int *flags, int *bitrate);


	// fill internal buffer with decoded data
	//
	// RETURNS:
	//	0	- end of data
	//  number of samples filledn in internal buffer
	int _FillBuffer();


	// working buffer
	int *c_pchBuffer;
	// allocated size of buffer in stereo samples
	unsigned int c_pchBufferAllocSize;
	int *c_pchBufferPos;
	unsigned int c_nBufferSize;


	// array of pointers to  frames, need this for accurate seek function
	unsigned char **c_pFramePtr;
	// index of last available frame in c_pFramePtr array
	unsigned int c_nLastAvailableFrameIndex;
	// allocated size of c_pFramePtr array, nzmber of allocated elements
	unsigned int c_nAllocatedFrameNumber;
	// current frame index, used only with accurate seek with dynamic scann
	unsigned int c_nCurrentFrameIndex;

	// scan whole stream, count number of AC3 frames and get frame pointers
	static unsigned int get_frame_pointers(unsigned char *buff, unsigned int size, int sample_rate,
						unsigned char*** frame_ptr, unsigned int initial_num);

	// get frame index for accurate seek
	unsigned int getFrameIndex(unsigned int nSamples);


	int c_fPreciseSongLength;
	int c_fPreciseSeek;


		// buffer for data from managed streams
	unsigned char *c_pchManagedBuffer;
	// size of managed buffer in bytes
	unsigned int c_nManagedBufferMaxSize;
	// current load
	unsigned int c_nManagedBufferLoad;


	unsigned int c_nSkipSamlesNum;	// number of samples to skip within frame to get accurate seek position
	unsigned int c_nFrameOverlay;	// frame overlay, number of frames decoded, but skipped to output after seek function


	// current seek index, need this for reverse mode
	unsigned int c_nCurrentSeekIndex;

	// current bitrate
	int c_nBitrate;

	WQueue *c_Queue;


	void err(unsigned int error_code);
	DECODER_ERROR_MESSAGE c_err_msg;
	
	unsigned int c_nCurrentPosition;

	DECODER_DATA c_DecoderData;

	// ac3 

	a52_state_t * c_state;

};




#endif
