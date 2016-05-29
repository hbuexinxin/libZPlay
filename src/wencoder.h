/*
 *  libzplay - windows ( WIN32 ) multimedia library
 *  Audio encoder
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


#ifndef _W_ENCODER_H_
#define _W_ENCODER_H_


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "debug.h"
#include "wqueue.h"
#include "wdecoder.h"


typedef unsigned int  ( * TEncoderReadCallback)(void *data, unsigned int data_size, void *user_data);
typedef unsigned int  ( * TEncoderWriteCallback)(void *data, unsigned int data_size, void *user_data);
typedef unsigned int  ( * TEncoderSeekCallback)(unsigned int offset, void *user_data);
typedef unsigned int  ( * TEncoderTellCallback)(void *user_data);

class WAudioEncoder {
public:
//==============================================
	// initialize encoder, call before you use encoder
	virtual int  Initialize(unsigned int nSampleRate, unsigned int nNuberOfChannels, unsigned int nBitPerSample,
			unsigned int custom_value,
			TEncoderReadCallback read_callback,
			TEncoderWriteCallback write_callback,
			TEncoderSeekCallback seek_callback,
			TEncoderTellCallback tell_callback
			) = 0;


//==============================================
//	uninitialize decoder, call when you don't need encoder anymore

	virtual int Uninitialize() = 0;

	// PARAMETERS:
	//		None.
	//
	//	RETURN VALUES:
	//		1	- all OK
	//		0	- error

// ====================================================================
// send wave samples for encoding

	virtual int EncodeSamples(void *pSamples, unsigned int nNumberOfSamples) = 0;

// ======================================================================================

//	free class instance

	virtual void  Release() = 0;

// ======================================================

	// get error messages from decoder
	virtual DECODER_ERROR_MESSAGE * GetError() = 0;

	// PARAMETERS:
	//		None.
	//
	//	RETURN VALUES:
	//		Pointer to ENCODER_ERROR_MESSAGE structure.

// ======================================================

};

WAudioEncoder * CreateWaveEncoder();
WAudioEncoder * CreatePCMEncoder();
WAudioEncoder * CreateFLACEncoder();
WAudioEncoder * CreateFLACOggEncoder();
WAudioEncoder * CreateVorbisOggEncoder();

#ifndef LIBZPLAY_PF_VERSION
WAudioEncoder * CreateMp3Encoder();
WAudioEncoder * CreateAacEncoder();
#endif


#endif

