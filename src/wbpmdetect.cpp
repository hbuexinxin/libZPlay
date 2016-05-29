/*
 *  libzplay - windows ( WIN32 ) multimedia library for playing mp3, mp2, ogg, wav, flac and raw PCM streams
 *  "Beat per minute" detection class ( WIN32 )
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
 * ver: 1.14
 * date: 15. April, 2010.
 *
 *
*/

#include "wbpmdetect.h"
#include "wbpmdetect1.h"
#include "wbpmdetect2.h"
#include "wbpmdetect3.h"


WBPMDetect * CreateBPMDetect(unsigned int nMethod)
{
	switch(nMethod)
	{

		case BPM_DETECT_USING_WAVELETS:
		return (WBPMDetect*) new WBPMDetect2();

		case BPM_DETECT_USING_AUTOCORRELATION:
		return (WBPMDetect*) new WBPMDetect1();

		case BPM_DETECT_USING_PEAKS:
		default:
		return (WBPMDetect*) new WBPMDetect3();
	}
}




