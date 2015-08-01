/*
 *  ProEXR_PrIO.cpp
 *  ProEXR
 *
 *  Created by Brendan Bolles on 12/22/11.
 *  Copyright 2011 fnord. All rights reserved.
 *
 */

#include "OpenEXR_Premiere_IO.h"

#include <IexBaseExc.h>

#include <assert.h>


IStreamPr::IStreamPr(imFileRef fileRef) :
	IStream("Premiere Import File"),
	_fileRef(fileRef)
{
	seekg(0);
}


bool
IStreamPr::read(char c[/*n*/], int n)
{
#ifdef __APPLE__
	ByteCount count = n;
	
	OSErr result = FSReadFork(reinterpret_cast<intptr_t>(_fileRef), fsAtMark, 0, count, (void *)c, &count);
	
	return (result == noErr && count == n);
#else
	DWORD count = n, out = 0;
	
	BOOL result = ReadFile(_fileRef, (LPVOID)c, count, &out, NULL);
	
	return (result && (out == n));
#endif
}


Imf::Int64
IStreamPr::tellg()
{
#ifdef __APPLE__
	Imf::Int64 pos;
	SInt64 lpos;

	OSErr result = FSGetForkPosition(reinterpret_cast<intptr_t>(_fileRef), &lpos);
	
	if(result != noErr)
		throw Iex::IoExc("Error calling FSGetForkPosition().");

	pos = lpos;
	
	return pos;
#else
	Imf::Int64 pos;
	LARGE_INTEGER lpos, zero;

	zero.QuadPart = 0;

	BOOL result = SetFilePointerEx(_fileRef, zero, &lpos, FILE_CURRENT);

	if(!result)
		throw Iex::IoExc("Error calling SetFilePointerEx().");

	pos = lpos.QuadPart;
	
	return pos;
#endif
}


void
IStreamPr::seekg(Imf::Int64 pos)
{
#ifdef __APPLE__
	OSErr result = FSSetForkPosition(reinterpret_cast<intptr_t>(_fileRef), fsFromStart, pos);

	if(result != noErr)
		throw Iex::IoExc("Error calling FSSetForkPosition().");
#else
	LARGE_INTEGER lpos, out;

	lpos.QuadPart = pos;

	BOOL result = SetFilePointerEx(_fileRef, lpos, &out, FILE_BEGIN);

	if(!result || lpos.QuadPart != out.QuadPart)
		throw Iex::IoExc("Error calling SetFilePointerEx().");
#endif
}


OStreamPr::OStreamPr(PrSDKExportFileSuite *fileSuite, csSDK_uint32 fileObject) :
	OStream("Premiere Export File"),
	suite(fileSuite),
	_fileObject(fileObject)
{
	if(suite == NULL)
		throw Iex::ArgExc("Got NULL Export File Suite");
	
	prSuiteError err = suite->Open(_fileObject);
	
	if(err != suiteError_NoError)
		throw Iex::IoExc("Error opening file.");
}


OStreamPr::~OStreamPr()
{
	prSuiteError err = suite->Close(_fileObject);
	
	assert(err == suiteError_NoError);
}


void
OStreamPr::write(const char c[/*n*/], int n)
{
	prSuiteError err = suite->Write(_fileObject, (void *)c, n);
	
	if(err != suiteError_NoError)
		throw Iex::IoExc("Error writing file.");
}


Imf::Int64
OStreamPr::tellp()
{
	prInt64 pos = 0;
	
	prSuiteError err = suite->Seek(_fileObject, 0, pos, fileSeekMode_End); // there's a bug in Premiere - fileSeekMode_End is really fileSeekMode_Current
	
	if(err != suiteError_NoError)
		throw Iex::IoExc("Error seeking current position.");
	
	return pos;
}


void
OStreamPr::seekp(Imf::Int64 pos)
{
	prInt64 new_pos = 0;
	
	prSuiteError err = suite->Seek(_fileObject, pos, new_pos, fileSeekMode_Begin);
	
	if(err != suiteError_NoError)
		throw Iex::IoExc("Error seeking.");
}
