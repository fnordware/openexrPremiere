
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
// OpenEXR_Premiere_IO.h
// 
// OpenEXR plug-in for Adobe Premiere
//
//------------------------------------------


#ifndef _OPENEXR_PREMIERE_IO_H_
#define _OPENEXR_PREMIERE_IO_H_


#include <ImfIO.h>

#include "PrSDKImport.h"
#include "PrSDKExportFileSuite.h"


class IStreamPr : public Imf::IStream
{
  public:
	IStreamPr(imFileRef fileRef);
	virtual ~IStreamPr() {}
	
	virtual bool read(char c[/*n*/], int n);
	virtual Imf::Int64 tellg();
	virtual void seekg(Imf::Int64 pos);
	
  private:
	imFileRef _fileRef;
};


class OStreamPr : public Imf::OStream
{
  public:
	OStreamPr(PrSDKExportFileSuite *fileSuite, csSDK_uint32 fileObject);
	virtual ~OStreamPr();

	virtual void write(const char c[/*n*/], int n);
	virtual Imf::Int64 tellp();
	virtual void seekp(Imf::Int64 pos);

  private:
	PrSDKExportFileSuite *suite;
	csSDK_uint32 _fileObject;
};


#endif // _OPENEXR_PREMIERE_IO_H_
