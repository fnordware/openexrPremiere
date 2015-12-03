
//////////////////////////////////////////////////////////////////////////////
// 
// Copyright (c) 2015, Brendan Bolles
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
//////////////////////////////////////////////////////////////////////////////

//------------------------------------------
//
// OpenEXR_UTF.cpp
// 
// OpenEXR plug-in for Adobe Premiere
//
//------------------------------------------


#include "OpenEXR_UTF.h"


#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>


bool UTF8toUTF16(const std::string &str, utf16_char *buf, unsigned int max_len)
{
	bool result = false;

	CFStringRef input = CFStringCreateWithCString(kCFAllocatorDefault, 
													str.c_str(),
													kCFStringEncodingUTF8);
	
	if(input)
	{
		CFIndex len = CFStringGetLength(input);
		
		if(len < max_len)
		{
			CFRange range = {0, len};
			
			CFIndex chars = CFStringGetBytes(input, range,
												kCFStringEncodingUTF16, '?', FALSE,
												(UInt8 *)buf, max_len * sizeof(utf16_char), NULL);
			
			result = (chars == len);
			
			buf[len] = '\0';
		}
		
		
		CFRelease(input);
	}
	
	
	return result;	
}


std::string UTF16toUTF8(const utf16_char *str)
{
	std::string output;

	unsigned int len = 0;
	
	while(str[len] != '\0')
		len++;

	CFStringRef input = CFStringCreateWithBytes(kCFAllocatorDefault, 
												(const UInt8 *)str, len * sizeof(utf16_char),
												kCFStringEncodingUTF16, FALSE);
												
	if(input)
	{
		CFIndex len = CFStringGetLength(input);
		
		CFRange range = {0, len};
		
		CFIndex size = CFStringGetMaximumSizeForEncoding(len + 1, kCFStringEncodingUTF8);
		
		char buf[size];
		
		CFIndex usedBufLen = 0;
		
		CFIndex chars = CFStringGetBytes(input, range,
											kCFStringEncodingUTF8, '?', FALSE,
											(UInt8 *)buf, size, &usedBufLen);
											
		buf[usedBufLen] = '\0';
		
		output = buf;
	
		CFRelease(input);
	}
	
	return output;
}

#endif // __APPLE__


#ifdef WIN32

#include <Windows.h>

bool UTF8toUTF16(const std::string &str, utf16_char *buf, unsigned int max_len)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, (LPWSTR)buf, max_len);

	return (len != 0);
}


std::string UTF16toUTF8(const utf16_char *str)
{
	std::string output;

	// first one just returns the required length
	int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)str, -1, NULL, 0, NULL, NULL);

	char *buf = new char[len];

	int len2 = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)str, -1, buf, len, NULL, NULL);

	output = buf;

	delete [] buf;

	return output;
}


#endif // WIN32
