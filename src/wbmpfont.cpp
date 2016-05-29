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

#include <malloc.h>
#include <stdlib.h>

#include "wbmpfont.h"

 typedef struct
 {
 	int x; // x position in text bitmap
    int y; // y position in text bitmap
    int w; // width of char in text bitmap
    int h;	// height of char in text bitmap
 } WBMPCHAR;


WBmpFont::WBmpFont()
{
	_hbFont = 0;
	_dwHeight = 0;
	_lpCharMap = 0;
	_lpText = 0;
	_dwTextLen = 0;
	_dwCharMapLen = 0;

	c_Colored = 0;
	c_crColor = RGB(255, 255, 255);

}


WBmpFont::~WBmpFont()
{
	if(_hbFont)
		DeleteObject(_hbFont);

	_dwHeight = 0;

	if(_lpCharMap)
		free(_lpCharMap);

	if(_lpText)
		free(_lpText);

}

int WBmpFont::Colored(int fEnable)
{
	c_Colored = fEnable;
	return 1;
}

int WBmpFont::SetColor(COLORREF color)
{
	c_crColor = color;
	return 1;
}


int WBmpFont::SetText(char* lpText)
{
	if(!lpText)
		return 0;

	if(_lpText)
		free(_lpText);

	_dwTextLen = 0;

	_dwTextLen = strlen(lpText);
	_lpText = (char*) malloc(_dwTextLen + 1);
	if(!_lpText) {
		_dwTextLen = 0;
		return 0;
	}

	strcpy(_lpText, lpText);


	
	return 1;
}


int WBmpFont::Open(HBITMAP hbFont, char* lpCharMap, int dwFontHeight)
{
	if(!hbFont || !lpCharMap || dwFontHeight == 0) return 0;

// if opened close all resource, now this BmpFont is invalid


	if(_lpCharMap)
		free(_lpCharMap);

	_lpCharMap = 0;

	if(_hbFont)
		DeleteObject(_hbFont);

	_hbFont = 0;
	_dwHeight = 0;
	_lpCharMap = 0;
	_lpText = 0;
	_dwTextLen = 0;
	_dwCharMapLen = 0;
//
// open new BmpFont

	_dwCharMapLen = strlen(lpCharMap);
	_lpCharMap = (char*) malloc(_dwCharMapLen + 1);
	if(!_lpCharMap)
		return 0;

	strcpy(_lpCharMap,lpCharMap);

	

// copy bitmap to internal structure

	HDC hdcMem1, hdcMem2;
    BITMAP bm = {0};
  
	hdcMem1 = CreateCompatibleDC(0);
	hdcMem2 = CreateCompatibleDC(0);

    GetObject(hbFont, sizeof(BITMAP), &bm);
	
    _hbFont = CreateBitmap(bm.bmWidth, bm.bmHeight, bm.bmPlanes,
			bm.bmBitsPixel,NULL);

    
    HBITMAP hbmOld1 = (HBITMAP)SelectObject(hdcMem1, hbFont);
    HBITMAP hbmOld2 = (HBITMAP)SelectObject(hdcMem2, _hbFont);
    
    BitBlt(hdcMem2, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem1, 0, 0, SRCCOPY);
  
    SelectObject(hdcMem1, hbmOld1);
    SelectObject(hdcMem2, hbmOld2);

    DeleteDC(hdcMem1);
    DeleteDC(hdcMem2);

	_dwHeight = dwFontHeight;

	return 1;
}




int WBmpFont::Draw(HDC hdc, RECT* rcDest,
    		BOOL fStretch, BOOL fTransparent, COLORREF crTransparent)
{
	return _Draw(this,hdc,rcDest,fStretch,fTransparent,crTransparent);
}


int WBmpFont::GetTextWidth(char *pText)
{
	int bitmap_width = 0;

	if(pText == 0)
		pText = _lpText;

	if(pText)
	{
		int len = strlen(pText);
		int dwCharWidth = 0;
		char lpCharWidth[3];
		lpCharWidth[2] = 0;
		int i;
		for(i = 0; i < len; i++)
		{
			// get width of characters in character map row
			memcpy(lpCharWidth, _lpCharMap,2);
			dwCharWidth = atoi(lpCharWidth);
			if(dwCharWidth <= 0)
			{
			 // invalid char map
				return 0;
			}	

		// search for coordinates
			for(int j = 3; j < _dwCharMapLen; j++)
			{
				if(_lpCharMap[j] == pText[i])
				{
					bitmap_width += dwCharWidth;
					break;
				}


				if((_lpCharMap[j] == '\n') && (j < _dwCharMapLen - 2))
				{
					memcpy(lpCharWidth, _lpCharMap+j+1, 2);
					dwCharWidth = atoi(lpCharWidth);
					if(dwCharWidth <= 0)
					{
	 				// invalid char map
						return 0;
					}
					j += 3;

				}
			}
		}
	}

	return bitmap_width;
}


int WBmpFont::_Draw(WBmpFont* font,HDC hdc, RECT* rcDest,
    		BOOL fStretch, BOOL fTransparent, COLORREF crTransparent)
{
	
	if(font->_dwTextLen == 0)
		return 0;

	int height  = min((int)rcDest->bottom, font->_dwHeight);
	HDC hdcCharFont = CreateCompatibleDC(hdc);
	HBITMAP hbOldCharFont = (HBITMAP) SelectObject(hdcCharFont,font->_hbFont);
	HDC hdcText = CreateCompatibleDC(hdc);
	HBITMAP hbText = CreateCompatibleBitmap(hdc,rcDest->right, height);
	HBITMAP hbOldText =(HBITMAP) SelectObject(hdcText, hbText);



	int dwCharWidth = 0;
	char lpCharWidth[3];
	int char_index = 0;
	int row = 0;
	int offset = 0;
	lpCharWidth[2] = 0;
	
	int rest = rcDest->right;
	int right = 0;
	int bitmap_width = 0;


	for(int i = 0; i < font->_dwTextLen; i++)
	{
		memcpy(lpCharWidth,font->_lpCharMap,2);
		dwCharWidth = atoi(lpCharWidth);
		if(dwCharWidth <= 0)
		{
		 // invalid char map
			return 0;
		}	
		char_index = 0;	
		row = 0;
	// search for coordinates
		for(int j = 3; j < font->_dwCharMapLen; j++)
		{
			if(font->_lpCharMap[j] == font->_lpText[i])
			{
				if(rest < dwCharWidth)
				{
					right = rest;
					i = font->_dwTextLen;
				}	
				else
				{
					right = dwCharWidth;
				}

					BitBlt(hdcText, offset, 0, right, height,
					hdcCharFont,char_index * dwCharWidth, font->_dwHeight * row ,
					SRCCOPY);
			
			
				

				bitmap_width += right;

				offset += dwCharWidth; 
				rest -= dwCharWidth;
				break;
			}


			if((font->_lpCharMap[j] == '\n') && (j < font->_dwCharMapLen - 2))
			{
				memcpy(lpCharWidth,font->_lpCharMap+j+1,2);
				dwCharWidth = atoi(lpCharWidth);
				if(dwCharWidth <= 0)
				{
	 			// invalid char map
					return 0;
				}
				j += 3;
				row++;
				char_index = 0;
			}
			else
			 char_index++;	
		}
	}

	SelectObject(hdcCharFont, hbOldCharFont);
	DeleteDC(hdcCharFont);

	if(!fTransparent) {
	
		BitBlt(hdc,rcDest->left,rcDest->top,
				bitmap_width, height, hdcText,
				0,0 ,
				SRCCOPY);

		SelectObject(hdcText, hbOldText);
		DeleteDC(hdcText);
		DeleteObject(hbText);
	}
	else if(font->c_Colored == 0)
	{
// 1. create mask
		// get information about bitmap


		HBITMAP hbmMask = CreateBitmap(bitmap_width, height, 1, 1, NULL);
		HDC hdcMask = CreateCompatibleDC(hdcText);
		HBITMAP hbmOldMask =(HBITMAP)SelectObject(hdcMask, hbmMask);

		::SetBkColor(hdcText, crTransparent);
		BitBlt(hdcMask, 0, 0, bitmap_width, height, hdcText, 0, 0, SRCCOPY);

		HDC hdcNewText = CreateCompatibleDC(hdcText);
		HBITMAP hbmNewText = CreateCompatibleBitmap(hdcText, bitmap_width, height);
		HBITMAP hbmOldNewText =(HBITMAP) SelectObject(hdcNewText, hbmNewText);
		
		BitBlt(hdcNewText,0,0,bitmap_width,height,hdcMask,0,0,SRCCOPY);
		BitBlt(hdcNewText,0,0,bitmap_width,height,hdcText,0,0,SRCERASE);

		BitBlt(hdc,rcDest->left,rcDest->top,
				bitmap_width, height, hdcMask,
				0,0 ,
				SRCAND);

	
		BitBlt(hdc,rcDest->left,rcDest->top,
				bitmap_width, height, hdcNewText,
				0,0 ,
				SRCPAINT);


		SelectObject(hdcNewText, hbmOldNewText);
		DeleteDC(hdcNewText);
		DeleteObject(hbmNewText);

		SelectObject(hdcMask, hbmOldMask);
		DeleteDC(hdcMask);
		DeleteObject(hbmMask);

		SelectObject(hdcText, hbOldText);
		DeleteDC(hdcText);
		DeleteObject(hbText);

	
	}
	else
	{
		// create mask
		HBITMAP hbmMask = CreateBitmap(bitmap_width, height, 1, 1, NULL);
		HDC hdcMask = CreateCompatibleDC(hdcText);
		HBITMAP hbmOldMask =(HBITMAP)SelectObject(hdcMask, hbmMask);

		::SetBkColor(hdcText, crTransparent);
		BitBlt(hdcMask, 0, 0, bitmap_width, height, hdcText, 0, 0, SRCCOPY);



		// text
		HDC hdcNewText = CreateCompatibleDC(hdcText);
		HBITMAP hbmNewText = CreateCompatibleBitmap(hdcText, bitmap_width, height);
		HBITMAP hbmOldNewText =(HBITMAP) SelectObject(hdcNewText, hbmNewText);
		
		RECT rc;
		SetRect(&rc, 0, 0, bitmap_width, height);
		HBRUSH hBrush = CreateSolidBrush(font->c_crColor);
		HBRUSH oldBrush = (HBRUSH) SelectObject(hdcText, hBrush);
		FillRect(hdcText, &rc, hBrush);
		SelectObject(hdcText, oldBrush);
		DeleteObject(hBrush);
		// 
		BitBlt(hdcNewText,0,0,bitmap_width,height,hdcMask,0,0,SRCCOPY);
		BitBlt(hdcNewText,0,0,bitmap_width,height,hdcText,0,0,SRCERASE);

		BitBlt(hdc,rcDest->left,rcDest->top,
				bitmap_width, height, hdcMask,
				0,0 ,
				SRCAND);

	
		BitBlt(hdc,rcDest->left,rcDest->top,
				bitmap_width, height, hdcNewText,
				0,0 ,
				SRCPAINT);


		SelectObject(hdcNewText, hbmOldNewText);
		DeleteDC(hdcNewText);
		DeleteObject(hbmNewText);

		SelectObject(hdcMask, hbmOldMask);
		DeleteDC(hdcMask);
		DeleteObject(hbmMask);

		SelectObject(hdcText, hbOldText);
		DeleteDC(hdcText);
		DeleteObject(hbText);


	}


	return bitmap_width;
}


