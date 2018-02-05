

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
// OpenEXR_Premiere_Import.cpp
// 
// OpenEXR plug-in for Adobe Premiere
//
//------------------------------------------


#include "OpenEXR_Premiere_Import.h"

#include "OpenEXR_Premiere_IO.h"

#include "OpenEXR_Premiere_Dialogs.h"
#include "OpenEXR_UTF.h"

#include "ImfHybridInputFile.h"
#include <ImfRgbaFile.h>

#include <ImfStandardAttributes.h>
#include <ImfChannelList.h>
#include <ImfVersion.h>
#include <IexBaseExc.h>
#include <IlmThread.h>
#include <IlmThreadPool.h>
#include <ImfArray.h>
#include <ImfStdIO.h>

#include <stdio.h>
#include <assert.h>


#ifdef PRMAC_ENV
	#include <mach/mach.h>
#endif



using namespace std;
using namespace Imf;
using namespace Imath;
using namespace IlmThread;


static const csSDK_int32 OpenEXR_ID = 'oEXR';

extern unsigned int gNumCPUs;

typedef enum {
	COLORSPACE_LINEAR_ADOBE = 0,
	COLORSPACE_LINEAR_BYPASS,
	COLORSPACE_SRGB,
	COLORSPACE_REC709,
	COLORSPACE_CINEON,
	COLORSPACE_GAMMA22
} ColorSpace;

typedef struct
{	
	csSDK_int32				width;
	csSDK_int32				height;
	csSDK_int32				importerID;
	PlugMemoryFuncsPtr		memFuncs;
	SPBasicSuite			*BasicSuite;
	PrSDKPPixCreatorSuite	*PPixCreatorSuite;
#ifdef PREMIERE_CACHE_NOT_CLEARING
	PrSDKPPixCacheSuite		*PPixCacheSuite;
#endif
	PrSDKPPixSuite			*PPixSuite;
	PrSDKTimeSuite			*TimeSuite;
} ImporterLocalRec8, *ImporterLocalRec8Ptr, **ImporterLocalRec8H;


typedef struct
{
	char			magic[4];
	csSDK_uint8		version;
	csSDK_uint8		file_init;
	csSDK_uint8		colorSpace;
	csSDK_uint8		reserved[9];
	char			red[Name::SIZE];
	char			green[Name::SIZE];
	char			blue[Name::SIZE];
	char			alpha[Name::SIZE];
} ImporterPrefs, *ImporterPrefsPtr, **ImporterPrefsH;


static prMALError 
SDKInit(
	imStdParms		*stdParms, 
	imImportInfoRec *importInfo)
{
	PrSDKAppInfoSuite *appInfoSuite = NULL;
	stdParms->piSuites->utilFuncs->getSPBasicSuite()->AcquireSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion, (const void**)&appInfoSuite);
	
	if(appInfoSuite)
	{
		int fourCC = 0;
	
		appInfoSuite->GetAppInfo(PrSDKAppInfoSuite::kAppInfo_AppFourCC, (void *)&fourCC);
	
		stdParms->piSuites->utilFuncs->getSPBasicSuite()->ReleaseSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion);
		
		if(fourCC == kAppAfterEffects)
			return imOtherErr;
	}


	importInfo->importerType		= OpenEXR_ID;

	importInfo->canSave				= kPrFalse;		// Can 'save as' files to disk, real file only
	
	// imDeleteFile8 is broken on MacOS when renaming a file using the Save Captured Files dialog
	// So it is not recommended to set this on MacOS yet (bug 1627325)
#ifdef PRWIN_ENV
	importInfo->canDelete			= kPrFalse;		// File importers only, use if you only if you have child files
#endif
	
	importInfo->canResize			= kPrFalse;
	importInfo->canDoSubsize		= kPrFalse;
	
	
	importInfo->dontCache			= kPrFalse;		// Don't let Premiere cache these files
	importInfo->hasSetup			= kPrTrue;		// Set to kPrTrue if you have a setup dialog
	importInfo->setupOnDblClk		= kPrFalse;		// If user dbl-clicks file you imported, pop your setup dialog
	importInfo->keepLoaded			= kPrFalse;		// If you MUST stay loaded use, otherwise don't: play nice
	importInfo->priority			= 10;			// Original ProEXR plug-in had a priority of 0
	importInfo->canTrim				= kPrFalse;
	importInfo->canCalcSizes		= kPrFalse;
	

	if(gNumCPUs <= 1)
	{
	#ifdef PRMAC_ENV
		// get number of CPUs using Mach calls
		host_basic_info_data_t hostInfo;
		mach_msg_type_number_t infoCount;
		
		infoCount = HOST_BASIC_INFO_COUNT;
		host_info(mach_host_self(), HOST_BASIC_INFO, 
				  (host_info_t)&hostInfo, &infoCount);
		
		gNumCPUs = hostInfo.max_cpus;
	#else
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);

		gNumCPUs = systemInfo.dwNumberOfProcessors;
	#endif
	}
	
	
	return malNoError;
}


static prMALError
SDKShutdown()
{
	if( supportsThreads() )
		setGlobalThreadCount(0);
	
	return malNoError;
}


static prMALError 
SDKGetIndFormat(
	imStdParms		*stdParms, 
	csSDK_size_t	index, 
	imIndFormatRec	*SDKIndFormatRec)
{
	prMALError	result		= malNoError;
	
	switch(index)
	{
		//	Add a case for each filetype.
		
		case 0:
			do{
				char formatname[255]	= "OpenEXR";
				char shortname[32]		= "OpenEXR";
				char platformXten[256]	= "exr\0sxr\0mxr\0\0";

				SDKIndFormatRec->filetype			= OpenEXR_ID;

				SDKIndFormatRec->canWriteTimecode	= kPrTrue;
				SDKIndFormatRec->canWriteMetaData	= kPrFalse;

				SDKIndFormatRec->flags = xfIsStill | xfCanImport; // not used supposedly, but actually enables the numbered stills checkbox

			#ifdef PRWIN_ENV
				strcpy_s(SDKIndFormatRec->FormatName, sizeof (SDKIndFormatRec->FormatName), formatname);				// The long name of the importer
				strcpy_s(SDKIndFormatRec->FormatShortName, sizeof (SDKIndFormatRec->FormatShortName), shortname);		// The short (menu name) of the importer
			#else
				strcpy(SDKIndFormatRec->FormatName, formatname);			// The Long name of the importer
				strcpy(SDKIndFormatRec->FormatShortName, shortname);		// The short (menu name) of the importer
			#endif

				memcpy(SDKIndFormatRec->PlatformExtension, platformXten, 13);

			}while(0);
			
			break;

		default:
			result = imBadFormatIndex;
	}

	return result;
}



static bool
isEXR(imFileRef SDKfileRef)
{
	try
	{
		IStreamPr instream(SDKfileRef);

		char bytes[4];
		instream.read(bytes, sizeof(bytes));

		return isImfMagic(bytes);
	}
	catch(...)
	{
		return false;
	}
}


prMALError 
SDKOpenFile8(
	imStdParms		*stdParms, 
	imFileRef		*SDKfileRef, 
	imFileOpenRec8	*SDKfileOpenRec8)
{
	prMALError			result = malNoError;
	ImporterLocalRec8H	localRecH;


	if(SDKfileOpenRec8->privatedata)
	{
		localRecH = (ImporterLocalRec8H)SDKfileOpenRec8->privatedata;
	}
	else
	{
		localRecH = (ImporterLocalRec8H)stdParms->piSuites->memFuncs->newHandle(sizeof(ImporterLocalRec8));
		SDKfileOpenRec8->privatedata = (PrivateDataPtr)localRecH;
	}
	

#ifdef PRWIN_ENV

	DWORD shareMode;

	if(SDKfileOpenRec8->inReadWrite == kPrOpenFileAccess_ReadOnly)
	{
		shareMode = GENERIC_READ;
	}
	else if(SDKfileOpenRec8->inReadWrite == kPrOpenFileAccess_ReadWrite)
	{
		shareMode = GENERIC_WRITE;
	}
	
	imFileRef fileRef = CreateFileW(SDKfileOpenRec8->fileinfo.filepath,
									shareMode,
									FILE_SHARE_READ,
									NULL,
									OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL,
									NULL);
							
	if(fileRef == imInvalidHandleValue || !isEXR(fileRef))
	{
		if(fileRef != imInvalidHandleValue)
			CloseHandle(fileRef);
		
		stdParms->piSuites->memFuncs->disposeHandle(reinterpret_cast<PrMemoryHandle>(SDKfileOpenRec8->privatedata));
		
		result = imBadFile;
	}
	else
	{
		SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = fileRef;
		SDKfileOpenRec8->fileinfo.filetype = OpenEXR_ID;
	}
	
#else

	SInt8 filePermissions;	

	if(SDKfileOpenRec8->inReadWrite == kPrOpenFileAccess_ReadOnly)
	{
		filePermissions = fsRdPerm;
	}
	else if(SDKfileOpenRec8->inReadWrite == kPrOpenFileAccess_ReadWrite)
	{
		filePermissions = fsRdWrPerm;
	}
	
	CFStringRef filePathCFSR = CFStringCreateWithCharacters(NULL,
												SDKfileOpenRec8->fileinfo.filepath,
												prUTF16CharLength(SDKfileOpenRec8->fileinfo.filepath));
												
	CFURLRef filePathURL = CFURLCreateWithFileSystemPath(NULL, filePathCFSR, kCFURLPOSIXPathStyle, false);
	
	FSRef fileRef;
	CFURLGetFSRef(filePathURL, &fileRef);
	
	HFSUniStr255 dataForkName;	
	FSGetDataForkName(&dataForkName);
	
	
	FSIORefNum refNum;
	OSErr err = FSOpenFork( &fileRef,
							dataForkName.length,
							dataForkName.unicode,
							filePermissions,
							&refNum);
							
							
	CFRelease(filePathURL);
	CFRelease(filePathCFSR);
	
	if(err || !isEXR(CAST_FILEREF(refNum)))
	{
		if(!err)
			FSCloseFork(refNum);

		stdParms->piSuites->memFuncs->disposeHandle(reinterpret_cast<PrMemoryHandle>(SDKfileOpenRec8->privatedata));
		
		result = imBadFile;
	}
	else
	{
		SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = CAST_FILEREF(refNum);
		SDKfileOpenRec8->fileinfo.filetype = OpenEXR_ID;
	}
	
#endif

	return result;
}


static prMALError 
SDKQuietFile(
	imStdParms			*stdParms, 
	imFileRef			*SDKfileRef, 
	void				*privateData)
{
	// If file has not yet been closed
	if(SDKfileRef && *SDKfileRef != imInvalidHandleValue)
	{
	#ifdef PRWIN_ENV
		CloseHandle(*SDKfileRef);
		*SDKfileRef = imInvalidHandleValue;
	#else
		FSCloseFork(CAST_REFNUM(*SDKfileRef));
		*SDKfileRef = imInvalidHandleValue;
	#endif
	}

	return malNoError; 
}


static prMALError 
SDKCloseFile(
	imStdParms			*stdParms, 
	imFileRef			*SDKfileRef,
	void				*privateData) 
{
	ImporterLocalRec8H ldataH	= reinterpret_cast<ImporterLocalRec8H>(privateData);
	ImporterLocalRec8Ptr ldataP = *ldataH;

	// If file has not yet been closed
	if (SDKfileRef && *SDKfileRef != imInvalidHandleValue)
	{
		SDKQuietFile(stdParms, SDKfileRef, privateData);
	}

	// Remove the privateData handle.
	// CLEANUP - Destroy the handle we created to avoid memory leaks
	if (ldataH && ldataP && ldataP->BasicSuite)
	{
		ldataP->BasicSuite->ReleaseSuite(kPrSDKPPixCreatorSuite, kPrSDKPPixCreatorSuiteVersion);
	#ifdef PREMIERE_CACHE_NOT_CLEARING
		ldataP->BasicSuite->ReleaseSuite(kPrSDKPPixCacheSuite, kPrSDKPPixCacheSuiteVersion);
	#endif
		ldataP->BasicSuite->ReleaseSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion);
		ldataP->BasicSuite->ReleaseSuite(kPrSDKTimeSuite, kPrSDKTimeSuiteVersion);
		stdParms->piSuites->memFuncs->disposeHandle(reinterpret_cast<char**>(ldataH));
	}

	return malNoError;
}


// Go ahead and overwrite any existing file. Premiere will have already checked and warned the user if file will be overwritten.
// Of course, if there are child files, you should check and return imSaveErr if appropriate
static prMALError 
SDKSaveFile8(
	imStdParms			*stdParms, 
	imSaveFileRec8		*SDKSaveFileRec8) 
{
	prMALError	result = malNoError;
	
#ifdef PRMAC_ENV
	CFStringRef			sourceFilePathCFSR,
						destFilePathCFSR,
						destFolderCFSR,
						destFileNameCFSR;
	CFRange				destFileNameRange,
						destFolderRange;
	CFURLRef			sourceFilePathURL,
						destFolderURL;
	FSRef				sourceFileRef,
						destFolderRef;
												
	// Convert prUTF16Char filePaths to FSRefs for paths
	sourceFilePathCFSR = CFStringCreateWithCharacters(	kCFAllocatorDefault,
														SDKSaveFileRec8->sourcePath,
														prUTF16CharLength(SDKSaveFileRec8->sourcePath));
	destFilePathCFSR = CFStringCreateWithCharacters(	kCFAllocatorDefault,
														SDKSaveFileRec8->destPath,
														prUTF16CharLength(SDKSaveFileRec8->destPath));
														
	// Separate the folder path from the file name
	destFileNameRange = CFStringFind(	destFilePathCFSR,
										CFSTR("/"),
										kCFCompareBackwards);
	destFolderRange.location = 0;
	destFolderRange.length = destFileNameRange.location;
	destFileNameRange.location += destFileNameRange.length;
	destFileNameRange.length = CFStringGetLength(destFilePathCFSR) - destFileNameRange.location;
	destFolderCFSR = CFStringCreateWithSubstring(	kCFAllocatorDefault,
													destFilePathCFSR,
													destFolderRange);
	destFileNameCFSR = CFStringCreateWithSubstring( kCFAllocatorDefault,
													destFilePathCFSR,
													destFileNameRange);
		
	// Make FSRefs
	sourceFilePathURL = CFURLCreateWithFileSystemPath(	kCFAllocatorDefault,
														sourceFilePathCFSR,
														kCFURLPOSIXPathStyle,
														false);
	destFolderURL = CFURLCreateWithFileSystemPath(	kCFAllocatorDefault,
													destFolderCFSR,
													kCFURLPOSIXPathStyle,
													true);
	CFURLGetFSRef (sourceFilePathURL, &sourceFileRef);
	CFURLGetFSRef (destFolderURL, &destFolderRef);						

	if(SDKSaveFileRec8->move)
	{
		if( FSCopyObjectSync(	&sourceFileRef,
								&destFolderRef,
								destFileNameCFSR,
								NULL,
								kFSFileOperationOverwrite))
		{
			result = imSaveErr;
		}
	}
	else
	{
		if( FSMoveObjectSync(	&sourceFileRef,
								&destFolderRef,
								destFileNameCFSR,
								NULL,
								kFSFileOperationOverwrite) )
		{
			result = imSaveErr;
		}
	}


#else

	// gotta admit, this is a lot easier on Windows

	if(SDKSaveFileRec8->move)
	{
		if( MoveFileW(SDKSaveFileRec8->sourcePath, SDKSaveFileRec8->destPath) == 0 )
		{
			result = imSaveErr;
		}
	}
	else
	{
		if( CopyFileW(SDKSaveFileRec8->sourcePath, SDKSaveFileRec8->destPath, kPrTrue) == 0 )
		{
			result = imSaveErr;
		}
	}
	
#endif
	
	return result;
}


static prMALError 
SDKDeleteFile8(
	imStdParms			*stdParms, 
	imDeleteFileRec8	*SDKDeleteFileRec8)
{
	prMALError	result = malNoError;

#ifdef PRWIN_ENV
	if( DeleteFileW(SDKDeleteFileRec8->deleteFilePath) )
	{
		result = imDeleteErr;
	}
#else
	CFStringRef filePathCFSR;
	CFURLRef	filePathURL;
	FSRef		fileRef;

	filePathCFSR = CFStringCreateWithCharacters(kCFAllocatorDefault,
												SDKDeleteFileRec8->deleteFilePath,
												prUTF16CharLength(SDKDeleteFileRec8->deleteFilePath));
	filePathURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
												filePathCFSR,
												kCFURLPOSIXPathStyle,
												false);
	CFURLGetFSRef(filePathURL, &fileRef);
					
	if( FSDeleteObject(&fileRef) )
	{
		result = imDeleteErr;
	}
#endif
	
	return result;
}


static void
InitPrefs(ImporterPrefs *prefs)
{
	memset(prefs, 0, sizeof(ImporterPrefs));

	strcpy(prefs->magic, "oEXR");
	prefs->version = 1;
	prefs->file_init = FALSE;

	strcpy(prefs->red, "R");
	strcpy(prefs->green, "G");
	strcpy(prefs->blue, "B");
	strcpy(prefs->alpha, "A");
}


static void
InitPrefs(
	const HybridInputFile &in,
	ImporterPrefs *prefs)
{
	if(prefs && prefs->file_init == FALSE)
	{
		const ChannelList &channels = in.channels(); 
	
		if(channels.findChannel("Y") && !channels.findChannel("R"))
		{
			strcpy(prefs->red, "Y");
			
			if(channels.findChannel("RY") && channels.findChannel("BY"))
			{
				strcpy(prefs->green, "RY");
				strcpy(prefs->blue, "BY");
			}
			else
			{
				strcpy(prefs->green, "Y");
				strcpy(prefs->blue, "Y");
			}
		}
		else
		{
			strcpy(prefs->red, (channels.findChannel("R") ? "R" : "(none)"));
			strcpy(prefs->green, (channels.findChannel("G") ? "G" : "(none)"));
			strcpy(prefs->blue, (channels.findChannel("B") ? "B" : "(none)"));
		}
		
		strcpy(prefs->alpha, (channels.findChannel("A") ? "A" : "(none)"));
		
		prefs->colorSpace = COLORSPACE_LINEAR_ADOBE;
		
		prefs->file_init = TRUE;
	}
}


static prMALError 
SDKGetPrefs8(
	imStdParms			*stdParms, 
	imFileAccessRec8	*fileInfo8, 
	imGetPrefsRec		*prefsRec)
{
	prMALError result = malNoError;
	
	// For a single frame (not a sequence) Premiere will call imGetPrefs8 twice as described
	// in the SDK guide, right as the file is imported.  We don't want a dialog to pop up right then
	// so we don't actually pop the dialog the second time, when prefsRec->firstTime == TRUE.
	//
	// For an image sequence imGetPrefs8 is not called immediately, so we have to create the prefs
	// in imGetInfo8.  See that call for more information.  imGetPrefs8 gets called when the user
	// does a Source Settings.
	
	if(prefsRec->prefsLength == 0)
	{
		prefsRec->prefsLength = sizeof(ImporterPrefs);
	}
	else
	{
		assert(prefsRec->prefsLength == sizeof(ImporterPrefs));
	
	#ifdef PRMAC_ENV
		const char *plugHndl = "com.adobe.PremierePro.OpenEXR";
		const void *mwnd = NULL;
	#else
		const char *plugHndl = NULL;
		const void *mwnd = NULL;

		PrSDKWindowSuite *windowSuite = NULL;
		stdParms->piSuites->utilFuncs->getSPBasicSuite()->AcquireSuite(kPrSDKWindowSuite, kPrSDKWindowSuiteVersion, (const void**)&windowSuite);
		
		if(windowSuite)
		{
			mwnd = windowSuite->GetMainWindow();
			stdParms->piSuites->utilFuncs->getSPBasicSuite()->ReleaseSuite(kPrSDKWindowSuite, kPrSDKWindowSuiteVersion);
		}
	#endif
		
		ImporterPrefsPtr prefs = reinterpret_cast<ImporterPrefsPtr>(prefsRec->prefs);

		if(prefs)
		{
			try
			{
				if(prefsRec->firstTime)
				{
					assert(prefs->file_init == FALSE);
				
					InitPrefs(prefs);
				}
				else
				{
					assert(prefs->file_init == TRUE);
				
					assert(0 == strncmp(prefs->magic, "oEXR", 4));
					assert(prefs->version == 1);
				
					auto_ptr<Imf::IStream> instream;
					
					if(fileInfo8->fileref != imInvalidHandleValue)
					{
						instream.reset(new IStreamPr(fileInfo8->fileref));
					}
					else
					{
						string path = UTF16toUTF8((const utf16_char *)fileInfo8->filepath);
						
						instream.reset(new StdIFStream(path.c_str()));
					}
					
					if(instream.get() == NULL)
						throw Iex::NullExc("instream is NULL");
					
					
					HybridInputFile in(*instream);
					
					assert(prefs->file_init == TRUE); // file_init should always happen in imGetInfo8...
					
					if(prefs->file_init == FALSE) // ...but just in case
					{
						assert(prefsRec->firstTime);
					
						InitPrefs(in, prefs);
					}
					
					
					const ChannelList &channels = in.channels();
					
					ChannelsList channels_list;
					
					for(ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i)
					{
						if(i.channel().type != Imf::UINT)
							channels_list.push_back( i.name() );
					}
					
					
					string	red = prefs->red,
							green = prefs->green,
							blue = prefs->blue,
							alpha = prefs->alpha;
							
					DialogColorSpace colorSpace = (DialogColorSpace)prefs->colorSpace;
					
					bool clicked_ok = ProEXR_Channels(channels_list, red, green, blue, alpha, colorSpace, plugHndl, mwnd);
					
					
					if(clicked_ok)
					{
						strncpy(prefs->red, red.c_str(), Name::MAX_LENGTH);
						strncpy(prefs->green, green.c_str(), Name::MAX_LENGTH);
						strncpy(prefs->blue, blue.c_str(), Name::MAX_LENGTH);
						strncpy(prefs->alpha, alpha.c_str(), Name::MAX_LENGTH);
						
						prefs->colorSpace = colorSpace;
					
					#ifdef PREMIERE_CACHE_NOT_CLEARING
						// Was previously having problems with prefs changing and the cache not clearing
					#if kPrSDKPPixCacheSuiteVersion >= 4
						PrSDKPPixCacheSuite *PPixCacheSuite = NULL;
						stdParms->piSuites->utilFuncs->getSPBasicSuite()->AcquireSuite(kPrSDKPPixCacheSuite, kPrSDKPPixCacheSuiteVersion, (const void**)&PPixCacheSuite);
						
						if(PPixCacheSuite)
						{
							PPixCacheSuite->ExpireAllPPixesFromCache();
							stdParms->piSuites->utilFuncs->getSPBasicSuite()->ReleaseSuite(kPrSDKPPixCacheSuite, kPrSDKPPixCacheSuiteVersion);
						}
					#endif
					#endif // PREMIERE_CACHE_NOT_CLEARING
			
						result = imNoErr;
					}
					else
						result = imCancel;
				}
			}
			catch(...)
			{
				result = imFileReadFailed;
			}
		}
	}

	return result;
}


static prMALError 
SDKGetIndPixelFormat(
	imStdParms			*stdParms,
	csSDK_size_t		idx,
	imIndPixelFormatRec *SDKIndPixelFormatRec) 
{
	prMALError result = malNoError;

	switch(idx)
	{
		case 0:
			SDKIndPixelFormatRec->outPixelFormat = PrPixelFormat_BGRA_4444_32f_Linear;
			break;
	
		default:
			result = imBadFormatIndex;
			break;
	}

	return result;	
}


static prMALError 
SDKAnalysis(
	imStdParms		*stdParms,
	imFileRef		SDKfileRef,
	imAnalysisRec	*SDKAnalysisRec)
{
	ImporterPrefs *prefs = reinterpret_cast<ImporterPrefs *>(SDKAnalysisRec->prefs);

	try
	{
		IStreamPr instream(SDKfileRef);
		HybridInputFile in(instream);
		
		const Header &head = in.header(0);
		
		string info;
		
		switch( head.compression() )
		{
			case Imf::NO_COMPRESSION:
				info += "No compression";
				break;

			case Imf::RLE_COMPRESSION:
				info += "RLE compression";
				break;

			case Imf::ZIPS_COMPRESSION:
				info += "Zip compression";
				break;

			case Imf::ZIP_COMPRESSION:
				info += "Zip16 compression";
				break;

			case Imf::PIZ_COMPRESSION:
				info += "Piz compression";
				break;

			case Imf::PXR24_COMPRESSION:
				info += "PXR24 compression";
				break;
			
			case Imf::B44_COMPRESSION:
				info += "B44 compression";
				break;

			case Imf::B44A_COMPRESSION:
				info += "B44A compression";
				break;

			case Imf::DWAA_COMPRESSION:
				info += "DWAA compression";
				break;

			case Imf::DWAB_COMPRESSION:
				info += "DWAB compression";
				break;

			default:
				info += "some weird compression";
				break;
		}
		
		assert(prefs != NULL); // should have this under control now
		
		if(prefs != NULL && prefs->colorSpace != COLORSPACE_LINEAR_ADOBE)
		{
			const std::string colorSpaceName = (prefs->colorSpace == COLORSPACE_LINEAR_ADOBE ? "Linear (Adobe)" :
												prefs->colorSpace == COLORSPACE_LINEAR_BYPASS ? "Linear" :
												prefs->colorSpace == COLORSPACE_SRGB ? "sRGB" :
												prefs->colorSpace == COLORSPACE_CINEON ? "Cineon" :
												prefs->colorSpace == COLORSPACE_GAMMA22 ? "Gamma 2.2" :
												"Unknown");
		
			info += ", " + colorSpaceName + " color space";
		}
		
		if(info.size() > SDKAnalysisRec->buffersize - 1)
		{
			info.resize(SDKAnalysisRec->buffersize - 4);
			info += "...";
		}
		
		if(SDKAnalysisRec->buffer != NULL && SDKAnalysisRec->buffersize > 3)
		{
			strcpy(SDKAnalysisRec->buffer, info.c_str());
		}
	}
	catch(...)
	{
		return imBadFile;
	}

	return malNoError;
}


prMALError 
SDKGetInfo8(
	imStdParms			*stdParms, 
	imFileAccessRec8	*fileAccessInfo8, 
	imFileInfoRec8		*SDKFileInfo8)
{
	prMALError					result				= malNoError;
	ImporterLocalRec8H			ldataH				= NULL;
	
	// Get a handle to our private data.  If it doesn't exist, allocate one
	// so we can use it to store our file instance info
	if(SDKFileInfo8->privatedata)
	{
		ldataH = reinterpret_cast<ImporterLocalRec8H>(SDKFileInfo8->privatedata);
	}
	else
	{
		ldataH						= reinterpret_cast<ImporterLocalRec8H>(stdParms->piSuites->memFuncs->newHandle(sizeof(ImporterLocalRec8)));
		SDKFileInfo8->privatedata	= reinterpret_cast<PrivateDataPtr>(ldataH);
	}
	
	ImporterLocalRec8Ptr ldataP = *ldataH;

	// Either way, lock it in memory so it doesn't move while we modify it.
	stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));

	// Acquire needed suites
	ldataP->memFuncs = stdParms->piSuites->memFuncs;
	ldataP->BasicSuite = stdParms->piSuites->utilFuncs->getSPBasicSuite();
	if(ldataP->BasicSuite)
	{
		ldataP->BasicSuite->AcquireSuite(kPrSDKPPixCreatorSuite, kPrSDKPPixCreatorSuiteVersion, (const void**)&ldataP->PPixCreatorSuite);
	#ifdef PREMIERE_CACHE_NOT_CLEARING
		ldataP->BasicSuite->AcquireSuite(kPrSDKPPixCacheSuite, kPrSDKPPixCacheSuiteVersion, (const void**)&ldataP->PPixCacheSuite);
	#endif
		ldataP->BasicSuite->AcquireSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion, (const void**)&ldataP->PPixSuite);
		ldataP->BasicSuite->AcquireSuite(kPrSDKTimeSuite, kPrSDKTimeSuiteVersion, (const void**)&ldataP->TimeSuite);
	}


	try
	{
		IStreamPr instream(fileAccessInfo8->fileref);
		HybridInputFile in(instream);
		
		
		const Box2i &dispW = in.displayWindow();
			
		const csSDK_int32 width = dispW.max.x - dispW.min.x + 1;
		const csSDK_int32 height = dispW.max.y - dispW.min.y + 1;


		const Header &head = in.header(0);
		
		Rational pixel_aspect_ratio;
		
		// if I stored a ratio in the file, let's use that
	#define PIXEL_ASPECT_RATIONAL_KEY	"pixelAspectRatioRational"
		const RationalAttribute *par = head.findTypedAttribute<RationalAttribute>(PIXEL_ASPECT_RATIONAL_KEY);
		
		if(par)
		{
			pixel_aspect_ratio = par->value();
		}
		else
		{
			// EXR stores pixel aspect ratio as a floating point number (boo!)
			pixel_aspect_ratio = Rational( head.pixelAspectRatio() );
		}
		

		const csSDK_int32 depth = (in.channels().findChannel("A") ? 128 : 96);


		SDKFileInfo8->hasVideo = kPrTrue;
		SDKFileInfo8->hasAudio = kPrFalse;
		
		
		// Video information
		SDKFileInfo8->vidInfo.subType		= PrPixelFormat_BGRA_4444_32f_Linear;
		SDKFileInfo8->vidInfo.imageWidth	= width;
		SDKFileInfo8->vidInfo.imageHeight	= height;
		SDKFileInfo8->vidInfo.depth			= depth;
		
		SDKFileInfo8->vidInfo.fieldType		= prFieldsNone;
		SDKFileInfo8->vidInfo.hasPulldown	= kPrFalse;
		SDKFileInfo8->vidInfo.isStill		= kPrTrue;
		SDKFileInfo8->vidInfo.noDuration	= imNoDurationStillDefault;

		SDKFileInfo8->vidInfo.alphaType		= (depth == 128 ? alphaBlackMatte : alphaNone);

		SDKFileInfo8->vidInfo.pixelAspectNum	= pixel_aspect_ratio.n;
		SDKFileInfo8->vidInfo.pixelAspectDen	= pixel_aspect_ratio.d;
		
		SDKFileInfo8->vidInfo.interpretationUncertain = imFieldTypeUncertain;

		SDKFileInfo8->vidInfo.supportsAsyncIO			= kPrFalse;
		SDKFileInfo8->vidInfo.supportsGetSourceVideo	= kPrTrue;


		if( hasFramesPerSecond( head ) )
		{
			const Rational &fps = framesPerSecond( head );
			
			SDKFileInfo8->vidScale = fps.n;
			SDKFileInfo8->vidSampleSize = fps.d;
		}
		
		
		SDKFileInfo8->accessModes = kRandomAccessImport;
		SDKFileInfo8->hasDataRate = kPrFalse;
		
		
		// For a still image (not a sequence), imGetPrefs8 will have already been called twice before
		// we get here (imGetInfo8).  This does not happen for a sequence, so we have to create and
		// initialize the memory here ourselves.  imGetPrefs8 will be called if the user goes to
		// Source Settings.
		
		// Oddly, Premiere will call imGetInfo8 over and over again for a sequence, each time with
		// SDKFileInfo8->prefs == NULL.
		
		if(SDKFileInfo8->prefs == NULL)
		{
			SDKFileInfo8->prefs = stdParms->piSuites->memFuncs->newPtr(sizeof(ImporterPrefs));
			
			InitPrefs((ImporterPrefs *)SDKFileInfo8->prefs);
		}
		
		assert(stdParms->piSuites->memFuncs->getPtrSize((char *)SDKFileInfo8->prefs) == sizeof(ImporterPrefs));
		
		
		ImporterPrefs *prefs = reinterpret_cast<ImporterPrefs *>(SDKFileInfo8->prefs);
		
		if(prefs && prefs->file_init == FALSE)
		{
			InitPrefs(in, prefs);
		}
		
		
		ldataP->width = SDKFileInfo8->vidInfo.imageWidth;
		ldataP->height = SDKFileInfo8->vidInfo.imageHeight;
		ldataP->importerID = SDKFileInfo8->vidInfo.importerID;

		stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));
	}
	catch(...)
	{
		result = malUnknownError;
	}
	
	return result;
}


static prMALError 
SDKPreferredFrameSize(
	imStdParms					*stdparms, 
	imPreferredFrameSizeRec		*preferredFrameSizeRec)
{
	prMALError			result	= malNoError;
	ImporterLocalRec8H	ldataH	= reinterpret_cast<ImporterLocalRec8H>(preferredFrameSizeRec->inPrivateData);
	ImporterLocalRec8Ptr ldataP = *ldataH;

	// Enumerate formats in order of priority, starting from the most preferred format
	switch(preferredFrameSizeRec->inIndex)
	{
		case 0:
			preferredFrameSizeRec->outWidth = ldataP->width;
			preferredFrameSizeRec->outHeight = ldataP->height;
			// If we supported more formats, we'd return imIterateFrameSizes to request to be called again
			result = malNoError;
			break;
	
		default:
			// We shouldn't be called for anything other than the case above
			result = imOtherErr;
	}

	return result;
}


static prMALError 
SDKGetTimeInfo8(
	imStdParms			*stdParms, 
	imFileRef			SDKfileRef, 
	imTimeInfoRec8		*SDKtimeInfoRec8)
{
	prMALError err = malNoError;

	try
	{
		IStreamPr instream(SDKfileRef);
		HybridInputFile in(instream);

		const Header &head = in.header(0);

		if(hasTimeCode( head ) && SDKtimeInfoRec8->dataType == 1)
		{
			const TimeCode &time_code = timeCode( head );
			
			const string sep = (time_code.dropFrame() ? ";" : ":");
			
			stringstream s;
			
			s << setfill('0') << setw(2) << time_code.hours() << sep;
			s << setfill('0') << setw(2) << time_code.minutes() << sep;
			s << setfill('0') << setw(2) << time_code.seconds() << sep;
			s << setfill('0') << setw(2) << time_code.frame();
					
			strncpy(SDKtimeInfoRec8->orgtime, s.str().c_str(), 17);
			
			
			if( hasFramesPerSecond( head ) )
			{
				const Rational &fps = framesPerSecond( head );
				
				SDKtimeInfoRec8->orgScale = fps.n;
				SDKtimeInfoRec8->orgSampleSize = fps.d;
			}
		}
		else
			err = imNoTimecode;
	}
	catch(...)
	{
		err = malUnknownError;
	}

	return err;
}


static prMALError	
SDKSetTimeInfo(
	imStdParms			*stdParms, 
	imFileRef			SDKfileRef, 
	imTimeInfoRec8		*SDKtimeInfoRec8)
{
	prMALError err = malNoError;

	// When I set SDKIndFormatRec->canWriteTimecode = kPrTrue, I can set the
	// timecode in Premiere, but this never actually gets called.
	// Not that I'd know what to do if it did.	OpenEXR doesn't like
	// modifying files in place.
	assert(FALSE);
	
	try
	{
		IStreamPr instream(SDKfileRef);
		HybridInputFile in(instream);
		
		if(hasTimeCode( in.header(0) ) && SDKtimeInfoRec8->dataType == 1)
		{
		
		}
	}
	catch(...)
	{
		err = malUnknownError;
	}

	return err;
}


// rarely you might get a channel that has a subsampling factor (mostly if you foolishly grab a Y/C channel)
// this will expand those channels to the full buffer size
static void FixSubsampling(const FrameBuffer &framebuffer, const Box2i &dw)
{
	for(FrameBuffer::ConstIterator i = framebuffer.begin(); i != framebuffer.end(); i++)
	{
		const Slice & slice = i.slice();
		
		if(slice.xSampling != 1 || slice.ySampling != 1)
		{
			char *row_subsampled = (char *)slice.base + (slice.yStride * (dw.min.y + ((1 + dw.max.y - dw.min.y) / slice.ySampling) - 1)) + (slice.xStride * (dw.min.x + ((1 + dw.max.x - dw.min.x) / slice.ySampling) - 1));
			char *row_expanded = (char *)slice.base + (slice.yStride * dw.max.y) + (slice.xStride * dw.max.x);
			
			for(int y = dw.max.y; y >= dw.min.y; y--)
			{
				char *pix_subsampled = row_subsampled;
				char *pix_expanded = row_expanded;
				
				for(int x = dw.max.x; x >= dw.min.x; x--)
				{
					if(slice.type == Imf::FLOAT)
					{
						*((float *)pix_expanded) = *((float *)pix_subsampled);
					}
					else if(slice.type == Imf::UINT)
					{
						*((unsigned int *)pix_expanded) = *((unsigned int *)pix_subsampled);
					}
					else
						assert(false);
					
					if(x % slice.xSampling == 0)
						pix_subsampled -= slice.xStride;
					
					pix_expanded -= slice.xStride;
				}
				
				if(y % slice.ySampling == 0)
					row_subsampled -= slice.yStride;
				
				row_expanded -= slice.yStride;
			}
		}
	}
}


typedef struct DupInfo {
	Slice src;
	Slice dst;
	
	DupInfo(const Slice &s, const Slice &d) : src(s), dst(d) {}
} DupInfo;

typedef std::vector<DupInfo> DupSet;

static void FixDuplicates(const DupSet &dupSet, const Box2i &dw)
{
	for(DupSet::const_iterator i = dupSet.begin(); i != dupSet.end(); ++i)
	{
		const DupInfo &dupInfo = *i;
		
		if(dupInfo.src.type != dupInfo.dst.type)
			continue;
	
		char *src_row = (char *)dupInfo.src.base + (dupInfo.src.yStride * dw.min.y) + (dupInfo.src.xStride * dw.min.x);
		char *dst_row = (char *)dupInfo.dst.base + (dupInfo.dst.yStride * dw.min.y) + (dupInfo.dst.xStride * dw.min.x);
		
		for(int y = dw.min.y; y <= dw.max.y; y++)
		{
			if(dupInfo.src.type == Imf::FLOAT)
			{
				float *src = (float *)src_row;
				float *dst = (float *)dst_row;
				
				const int src_step = dupInfo.src.xStride / sizeof(float);
				const int dst_step = dupInfo.dst.xStride / sizeof(float);
				
				for(int x = dw.min.x; x <= dw.max.x; x++)
				{
					*dst = *src;
					
					src += src_step;
					dst += dst_step;
				}
			}
			else
				assert(false);
		
			src_row += dupInfo.src.yStride;
			dst_row += dupInfo.dst.yStride;
		}
	}
}


class FillRowTask : public Task
{
public:
	FillRowTask(TaskGroup *group, char *pixel_origin, RowbyteType rowbytes, float value, int width, int row);
	virtual ~FillRowTask() {}
	
	virtual void execute();
	
private:
	float *_pixel_row;
	const float _value;
	const int _width;
};


FillRowTask::FillRowTask(TaskGroup *group, char *pixel_origin, RowbyteType rowbytes, float value, int width, int row) :
	Task(group),
	_value(value),
	_width(width)
{
	_pixel_row = (float *)(pixel_origin + (rowbytes * row));
}


void
FillRowTask::execute()
{
	float *pix = _pixel_row;
	
	for(int x=0; x < _width; x++)
	{
		*pix++ = _value;
	}
}


class ConvertRgbaRowTask : public Task
{
  public:
	ConvertRgbaRowTask(TaskGroup *group, Rgba *input_row, float *output_row, int witdh);
	virtual ~ConvertRgbaRowTask() {}
	
	virtual void execute();

  private:
	const Rgba *_input_row;
	float *_output_row;
	const int _width;
};


ConvertRgbaRowTask::ConvertRgbaRowTask(TaskGroup *group, Rgba *input_row, float *output_row, int width) :
	Task(group),
	_input_row(input_row),
	_output_row(output_row),
	_width(width)
{

}


void
ConvertRgbaRowTask::execute()
{
	const Rgba *in = _input_row;
	float *out = _output_row;
	
	for(int x=0; x < _width; x++)
	{
		*out++ = in->b;
		*out++ = in->g;
		*out++ = in->r;
		*out++ = in->a;
		
		in++;
	}
}


class CopyPPixRowTask : public Task
{
  public:
	CopyPPixRowTask(TaskGroup *group,
					const char *input_origin, RowbyteType input_rowbytes,
					char *output_origin, RowbyteType output_rowbytes,
					int width, int row);
	virtual ~CopyPPixRowTask() {}
	
	virtual void execute();

  private:
	const float *_input_row;
	float *_output_row;
	const int _width;
};


CopyPPixRowTask::CopyPPixRowTask(TaskGroup *group,
									const char *input_origin, RowbyteType input_rowbytes,
									char *output_origin, RowbyteType output_rowbytes,
									int width, int row) :
	Task(group),
	_width(width)
{
	_input_row = (float *)(input_origin + (input_rowbytes * row));
	_output_row = (float *)(output_origin + (output_rowbytes * row));
}


void
CopyPPixRowTask::execute()
{
	const float *in = _input_row;
	float *out = _output_row;
	
	for(int x=0; x < _width; x++)
	{
		*out++ = *in++;
	}
}


class ConvertLinearToColorSpaceTask : public Task
{
  public:
	ConvertLinearToColorSpaceTask(TaskGroup *group,
								float *bgra_origin, RowbyteType rowbytes,
								int width, int row, ColorSpace colorSpace);
	virtual ~ConvertLinearToColorSpaceTask() {}
	
	virtual void execute();

  private:
	float *_row;
	const int _width;
	const ColorSpace _colorSpace;
	
	float _logGain;
	float _logOffset;
	const int _logRefBlack;
	const int _logRefWhite;
	const float _logGamma;
	
	typedef struct BRGApixel
	{
		float b;
		float g;
		float r;
		float a;
	} BGRApixel;
	
	static inline float Linear2sRGB(const float &in);
	static inline float Linear2Rec709(const float &in);
	inline float Linear2Cineon(const float &in);
	static inline float Linear2Gamma22(const float &in);
};


ConvertLinearToColorSpaceTask::ConvertLinearToColorSpaceTask(TaskGroup *group,
																float *bgra_origin, RowbyteType rowbytes,
																int width, int row, ColorSpace colorSpace) :
	Task(group),
	_width(width),
	_colorSpace(colorSpace),
	_logRefBlack(95),
	_logRefWhite(685),
	_logGamma(1.0)
{
	_row = (float *)((char *)bgra_origin + (rowbytes * row));
	
	if(colorSpace == COLORSPACE_CINEON)
	{
		_logGain = 1.0f / (1.0f - pow(pow(10.0f, (_logRefBlack - _logRefWhite) * (0.002f/0.6f) ), 1.0f/_logGamma ) );
		_logOffset = _logGain - 1.0f;
	}
}
												

void
ConvertLinearToColorSpaceTask::execute()
{
	BRGApixel *pix = (BRGApixel *)_row;
	
	for(int x=0; x < _width; x++)
	{
		const bool unmult = (pix->a > 0.f && pix->a < 1.f);
		
		if(unmult)
		{
			pix->b /= pix->a;
			pix->g /= pix->a;
			pix->r /= pix->a;
		}
		
		pix++;
	}
	
	
	pix = (BRGApixel *)_row;
	
	if(_colorSpace == COLORSPACE_SRGB)
	{
		for(int x=0; x < _width; x++)
		{
			pix->b = Linear2sRGB(pix->b);
			pix->g = Linear2sRGB(pix->g);
			pix->r = Linear2sRGB(pix->r);
			
			pix++;
		}
	}
	else if(_colorSpace == COLORSPACE_REC709)
	{
		for(int x=0; x < _width; x++)
		{
			pix->b = Linear2Rec709(pix->b);
			pix->g = Linear2Rec709(pix->g);
			pix->r = Linear2Rec709(pix->r);
			
			pix++;
		}
	}
	else if(_colorSpace == COLORSPACE_CINEON)
	{
		for(int x=0; x < _width; x++)
		{
			pix->b = Linear2Cineon(pix->b);
			pix->g = Linear2Cineon(pix->g);
			pix->r = Linear2Cineon(pix->r);
			
			pix++;
		}
	}
	else if(_colorSpace == COLORSPACE_GAMMA22)
	{
		for(int x=0; x < _width; x++)
		{
			pix->b = Linear2Gamma22(pix->b);
			pix->g = Linear2Gamma22(pix->g);
			pix->r = Linear2Gamma22(pix->r);
			
			pix++;
		}
	}
	

	pix = (BRGApixel *)_row;
	
	for(int x=0; x < _width; x++)
	{
		const bool remult = (pix->a > 0.f && pix->a < 1.f);
		
		if(remult)
		{
			pix->b *= pix->a;
			pix->g *= pix->a;
			pix->r *= pix->a;
		}
		
		pix++;
	}
}


inline float
ConvertLinearToColorSpaceTask::Linear2sRGB(const float &in)
{
	return (in <= 0.0031308f ? (in * 12.92f) : 1.055f * powf(in, 1.f / 2.4f) - 0.055f);
}

inline float
ConvertLinearToColorSpaceTask::Linear2Rec709(const float &in)
{
	return (in <= 0.018f ? (in * 4.5f) : 1.099f * powf(in, 0.45f) - 0.099f);
}

inline float
ConvertLinearToColorSpaceTask::Linear2Cineon(const float &in)
{
	const float val = _logRefWhite + (log10( pow( (in + _logOffset)/_logGain, _logGamma ) ) / (0.002f/0.6f) );

	return (val / 1023.f);
}

inline float
ConvertLinearToColorSpaceTask::Linear2Gamma22(const float &in)
{
	return (in < 0.f ? -powf(-in, 1.f / 2.2f) : powf(in, 1.f / 2.2f) );
}


static prMALError 
SDKGetSourceVideo(
	imStdParms			*stdparms, 
	imFileRef			fileRef, 
	imSourceVideoRec	*sourceVideoRec)
{
	prMALError		result		= malNoError;

	// Get the privateData handle you stored in imGetInfo
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(sourceVideoRec->inPrivateData);
	ImporterLocalRec8Ptr ldataP = *ldataH;
	ImporterPrefs *prefs = reinterpret_cast<ImporterPrefs *>(sourceVideoRec->prefs);
	

	PPixHand temp_ppix = NULL;
	
	try
	{
		if( supportsThreads() )
			setGlobalThreadCount(gNumCPUs);
			
			
		// read the file
		IStreamPr instream(fileRef);
		HybridInputFile in(instream);
		
		
		const char *red = "R", *green = "G", *blue = "B", *alpha = "A";
		const char *y = "Y", *ry = "RY", *by = "BY";
		
		ColorSpace colorSpace = COLORSPACE_LINEAR_ADOBE;
		
		assert(prefs != NULL); // should have been created by imGetPrefs8 or imGetInfo8
		
		if(prefs && prefs->file_init)
		{
			assert(0 == strncmp(prefs->magic, "oEXR", 4));
			assert(prefs->version == 1);
		
			red = prefs->red;
			green = prefs->green;
			blue = prefs->blue;
			alpha = prefs->alpha;
			
			colorSpace = (ColorSpace)prefs->colorSpace;
		}
		else if(in.channels().findChannel("Y") && !in.channels().findChannel("R"))
		{
			if(in.channels().findChannel("RY") && in.channels().findChannel("BY"))
			{
				red = y;
				green = ry;
				blue = by;
			}
			else
			{
				red = green = blue = y;
			}
		}
		
		
		// make the Premiere buffer
		if(sourceVideoRec->inFrameFormats == NULL)
			throw Iex::NullExc("inFrameFormats is NULL");
		
		assert(sourceVideoRec->inNumFrameFormats == 1);
		assert(sourceVideoRec->inFrameFormats[0].inPixelFormat == PrPixelFormat_BGRA_4444_32f_Linear);
		
		imFrameFormat frameFormat = sourceVideoRec->inFrameFormats[0];
		
		frameFormat.inPixelFormat = (colorSpace == COLORSPACE_LINEAR_ADOBE ? PrPixelFormat_BGRA_4444_32f_Linear : PrPixelFormat_BGRA_4444_32f);
				
		const Box2i &dispW = in.displayWindow();
		
		const int width = dispW.max.x - dispW.min.x + 1;
		const int height = dispW.max.y - dispW.min.y + 1;
		
		assert(frameFormat.inFrameWidth == width);
		assert(frameFormat.inFrameHeight == height);
		
		prRect theRect;
		prSetRect(&theRect, 0, 0, width, height);
		
		RowbyteType rowBytes = 0;
		char *buf = NULL;
		
		ldataP->PPixCreatorSuite->CreatePPix(sourceVideoRec->outFrame, PrPPixBufferAccess_ReadWrite, frameFormat.inPixelFormat, &theRect);
		ldataP->PPixSuite->GetPixels(*sourceVideoRec->outFrame, PrPPixBufferAccess_WriteOnly, &buf);
		ldataP->PPixSuite->GetRowBytes(*sourceVideoRec->outFrame, &rowBytes);
		


		char *dataW_origin = buf;
		RowbyteType dataW_rowbytes = rowBytes;
		csSDK_int32 dataW_width = width;
		csSDK_int32 dataW_height = height;
		
		
		const Box2i &dataW = in.dataWindow();
		
		if((dataW != dispW) || (in.parts() > 1))
		{
			// if some pixels will not be written to,
			// clear the PPixHand out first
			if( (in.parts() > 1) ||
				(dataW.min.x > dispW.min.x) ||
				(dataW.min.y > dispW.min.y) ||
				(dataW.max.x < dispW.max.x) ||
				(dataW.max.y < dispW.max.y) )
			{
				TaskGroup taskGroup;
				
				for(int y=0; y < height; y++)
				{
					ThreadPool::addGlobalTask(new FillRowTask(&taskGroup, buf, rowBytes, 0.f,
																width * 4, y) );
				}
				
				// if the dataWindow does not actually intersect the displayWindow,
				// no need to continue further
				if( !dataW.intersects(dispW) )
				{
					return result;
				}
			}
			
			
			dataW_width = dataW.max.x - dataW.min.x + 1;
			dataW_height = dataW.max.y - dataW.min.y + 1;
			
			// if dataWindow is completely inside displayWindow, we can use the
			// existing PPixHand, otherwise have to create a new one
			if( (dataW.min.x >= dispW.min.x) &&
				(dataW.min.y >= dispW.min.y) &&
				(dataW.max.x <= dispW.max.x) &&
				(dataW.max.y <= dispW.max.y) )
			{
				dataW_origin = buf + (rowBytes * (dispW.max.y - dataW.max.y)) + (sizeof(float) * 4 * (dataW.min.x - dispW.min.x));
			}
			else
			{
				prRect tempRect;
				prSetRect(&tempRect, 0, 0, dataW_width, dataW_height);
				
				ldataP->PPixCreatorSuite->CreatePPix(&temp_ppix, PrPPixBufferAccess_ReadWrite, frameFormat.inPixelFormat, &tempRect);
				ldataP->PPixSuite->GetPixels(temp_ppix, PrPPixBufferAccess_ReadWrite, &dataW_origin);
				ldataP->PPixSuite->GetRowBytes(temp_ppix, &dataW_rowbytes);
			}
		}
		
		
		if(frameFormat.inPixelFormat == PrPixelFormat_BGRA_4444_32f_Linear || frameFormat.inPixelFormat == PrPixelFormat_BGRA_4444_32f)
		{
			if(string(red) == "Y" &&
				(string(green) == "RY" || string(green) == "Y") &&
				(string(blue) == "BY" || string(blue) == "Y") )
			{
				instream.seekg(0);
				
				Array2D<Rgba> half_buffer(dataW_height, dataW_width);
				
				RgbaInputFile inputFile(instream);
				
				inputFile.setFrameBuffer(&half_buffer[-dataW.min.y][-dataW.min.x], 1, dataW_width);
				inputFile.readPixels(dataW.min.y, dataW.max.y);
				
				
				TaskGroup taskGroup;
				
				char *buf_row = dataW_origin;
				
				for(int y = dataW_height - 1; y >= 0; y--)
				{
					float *buf_pix = (float *)buf_row;
					
					ThreadPool::addGlobalTask(new ConvertRgbaRowTask(&taskGroup,
																		&half_buffer[y][0],
																		buf_pix,
																		dataW_width) );
					
					buf_row += dataW_rowbytes;
				}
			}
			else
			{
				FrameBuffer frameBuffer;
				
				char *exr_BGRA_origin = (char *)dataW_origin - (sizeof(float) * 4 * dataW.min.x) + (dataW_rowbytes * dataW.max.y);
				
				
				DupSet dupSet;
				
				const char *chan[4] = { blue, green, red, alpha };
				
				for(int c=0; c < 4; c++)
				{
					int xSampling = 1,
						ySampling = 1;
					
					const Channel *channel = in.channels().findChannel(chan[c]);
					
					if(channel)
					{
						xSampling = channel->xSampling;
						ySampling = channel->ySampling;
					}
					
					const float fill = (c == 3 ? 1.f : 0.f);
				
					Slice slice(Imf::FLOAT,
								exr_BGRA_origin + (sizeof(float) * c),
								sizeof(float) * 4,
								-dataW_rowbytes,
								xSampling, ySampling, fill);
					
					const Slice *dup_slice = frameBuffer.findSlice(chan[c]);
					
					if(dup_slice == NULL)
					{
						frameBuffer.insert(chan[c], slice);
					}
					else
					{
						// The OpenEXR FrameBuffer can only hold one slice per channel name
						// because it uses a std::map.  If the user uses the same channel name
						// more than once, we have to duplicate it ourselves.
						
						dupSet.push_back( DupInfo(*dup_slice, slice) );
					}
				}
	

				in.setFrameBuffer(frameBuffer);
				
				in.readPixels(dataW.min.y, dataW.max.y);
				
				
				FixSubsampling(frameBuffer, dataW);
				
				FixDuplicates(dupSet, dataW);
			}
			
			
			if(temp_ppix)
			{
				// have to draw dataWindow pixels inside the displayWindow
				prPoint disp_origin, data_origin;
				
				if(dispW.min.x < dataW.min.x)
				{
					disp_origin.x = dataW.min.x - dispW.min.x;
					data_origin.x = 0;
				}
				else
				{
					disp_origin.x = 0;
					data_origin.x = dispW.min.x - dataW.min.x;
				}
				
				
				if(dispW.max.y > dataW.max.y)
				{
					disp_origin.y = dispW.max.y - dataW.max.y;
					data_origin.y = 0;
				}
				else
				{
					disp_origin.y = 0;
					data_origin.y = dataW.max.y - dispW.max.y;
				}
				
				
				char *display_pixel_origin = buf + (disp_origin.y * rowBytes) + (disp_origin.x * sizeof(float) * 4);
				
				char *data_pixel_origin = dataW_origin + (data_origin.y * dataW_rowbytes) + (data_origin.x * sizeof(float) * 4);
				
				const int copy_width = min(dispW.max.x, dataW.max.x) - max(dispW.min.x, dataW.min.x) + 1;
				const int copy_height = min(dispW.max.y, dataW.max.y) - max(dispW.min.y, dataW.min.y) + 1;
				
				
				TaskGroup taskGroup;
				
				for(int y=0; y < copy_height; y++)
				{
					ThreadPool::addGlobalTask(new CopyPPixRowTask(&taskGroup,
														data_pixel_origin, dataW_rowbytes,
														display_pixel_origin, rowBytes,
														copy_width * 4, y) );
				}
			}
			
			if(colorSpace == COLORSPACE_SRGB || colorSpace == COLORSPACE_REC709 ||
				colorSpace == COLORSPACE_CINEON || colorSpace == COLORSPACE_GAMMA22)
			{
				TaskGroup taskGroup;
				
				for(int y=0; y < height; y++)
				{
					ThreadPool::addGlobalTask(new ConvertLinearToColorSpaceTask(&taskGroup,
													(float *)buf, rowBytes,
													width, y, colorSpace));
				}
			}
		}
		else
			assert(false);
	}
	catch(...)
	{
		result = malUnknownError;
	}
	
	
	if(temp_ppix)
		ldataP->PPixSuite->Dispose(temp_ppix);
	

	return result;
}



PREMPLUGENTRY DllExport xImportEntry (
	csSDK_int32		selector, 
	imStdParms		*stdParms, 
	void			*param1, 
	void			*param2)
{
	prMALError	result				= imUnsupported;

	switch (selector)
	{
		case imInit:
			result =	SDKInit(stdParms, 
								reinterpret_cast<imImportInfoRec*>(param1));
			break;

		case imShutdown:
			result =	SDKShutdown();
			break;
			
		case imGetInfo8:
			result =	SDKGetInfo8(stdParms, 
									reinterpret_cast<imFileAccessRec8*>(param1), 
									reinterpret_cast<imFileInfoRec8*>(param2));
			break;

		case imGetTimeInfo8:
			result =	SDKGetTimeInfo8(stdParms, 
										reinterpret_cast<imFileRef>(param1),
										reinterpret_cast<imTimeInfoRec8*>(param2));
			break;
			
		case imSetTimeInfo8:
			result =	SDKSetTimeInfo( stdParms, 
										reinterpret_cast<imFileRef>(param1),
										reinterpret_cast<imTimeInfoRec8*>(param2));
			break;
			
		case imOpenFile8:
			result =	SDKOpenFile8(	stdParms, 
										reinterpret_cast<imFileRef*>(param1), 
										reinterpret_cast<imFileOpenRec8*>(param2));
			break;
		
		case imGetPrefs8:
			result =	SDKGetPrefs8(	stdParms, 
										reinterpret_cast<imFileAccessRec8*>(param1),
										reinterpret_cast<imGetPrefsRec*>(param2));
			break;

		case imQuietFile:
			result =	SDKQuietFile(	stdParms, 
										reinterpret_cast<imFileRef*>(param1), 
										param2); 
			break;

		case imCloseFile:
			result =	SDKCloseFile(	stdParms, 
										reinterpret_cast<imFileRef*>(param1), 
										param2);
			break;

		case imAnalysis:
			result =	SDKAnalysis(	stdParms,
										reinterpret_cast<imFileRef>(param1),
										reinterpret_cast<imAnalysisRec*>(param2));
			break;

		case imGetIndFormat:
			result =	SDKGetIndFormat(stdParms, 
										reinterpret_cast<csSDK_size_t>(param1),
										reinterpret_cast<imIndFormatRec*>(param2));
			break;

		case imSaveFile8:
			result =	SDKSaveFile8(	stdParms, 
										reinterpret_cast<imSaveFileRec8*>(param1));
			break;
			
		case imDeleteFile8:
			result =	SDKDeleteFile8( stdParms, 
										reinterpret_cast<imDeleteFileRec8*>(param1));
			break;

		case imGetIndPixelFormat:
			result = SDKGetIndPixelFormat(	stdParms,
											reinterpret_cast<csSDK_size_t>(param1),
											reinterpret_cast<imIndPixelFormatRec*>(param2));
			break;

		// Importers that support the Premiere Pro 2.0 API must return malSupports8 for this selector
		case imGetSupports8:
			result = malSupports8;
			break;

		case imGetPreferredFrameSize:
			result =	SDKPreferredFrameSize(	stdParms,
												reinterpret_cast<imPreferredFrameSizeRec*>(param1));
			break;

		case imGetSourceVideo:
			result =	SDKGetSourceVideo(	stdParms,
											reinterpret_cast<imFileRef>(param1),
											reinterpret_cast<imSourceVideoRec*>(param2));
			break;

		case imCreateAsyncImporter:
			result = imUnsupported;
			break;
	}

	return result;
}

