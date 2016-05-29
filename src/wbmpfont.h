/*
 *  libzplay - windows ( WIN32 ) multimedia library for playing mp3, mp2, ogg, wav, flac and raw PCM streams
 *  Bitmap font class ( WIN32 )
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


#ifndef _WBMPFONT_
#define _WBMPFONT_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class WBmpFont {
public:
	WBmpFont();
	~WBmpFont();
	int Open(HBITMAP hbFont, char* lpCharMap, int dwFontHeight);
	int SetText(char* lpText);
	int Draw(HDC hdc, RECT* rcDest, BOOL fStretch, BOOL fTransparent, COLORREF crTransparent);
	int GetTextWidth(char *pText);
	int Colored(int fEnable);
	int SetColor(COLORREF color);

private:
	HBITMAP _hbFont;
	int _dwHeight;
	char* _lpCharMap;
	char* _lpText;
	int _dwTextLen;
	int _dwCharMapLen;

	int c_Colored;
	COLORREF c_crColor;

	static int _Draw(WBmpFont* font,HDC hdc, RECT* rcDest,
    		BOOL fStretch, BOOL fTransparent, COLORREF crTransparent);

};



#endif
