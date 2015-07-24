/*
 *  ProEXR_PrIO.h
 *  ProEXR
 *
 *  Created by Brendan Bolles on 12/22/11.
 *  Copyright 2011 fnord. All rights reserved.
 *
 */

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
