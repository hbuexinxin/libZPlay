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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "wencoder.h"
#include "wwaveencoder.h"
#include "wflacencoder.h"
#include "wvorbisencoder.h"
#include "wmp3encoder.h"
#include "waacencoder.h"



WAudioEncoder * CreateWaveEncoder()
{
	return (WAudioEncoder*)  new WWaveEncoder(0);
}

WAudioEncoder * CreatePCMEncoder()
{
	return (WAudioEncoder*)  new WWaveEncoder(1);
}

#ifndef LIBZPLAY_PF_VERSION

WAudioEncoder * CreateMp3Encoder()
{
	return (WAudioEncoder*)  new WMp3Encoder();
}

WAudioEncoder * CreateAacEncoder()
{
	return (WAudioEncoder*)  new WAACEncoder();
}

#endif


WAudioEncoder * CreateFLACEncoder()
{
	return (WAudioEncoder*)  new WFLACEncoder(0);
}


WAudioEncoder * CreateFLACOggEncoder()
{
	return (WAudioEncoder*)  new WFLACEncoder(1);
}


WAudioEncoder * CreateVorbisOggEncoder()
{
	return (WAudioEncoder*)  new WVorbisEncoder();
}
