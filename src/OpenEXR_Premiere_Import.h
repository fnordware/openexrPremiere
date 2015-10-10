



#ifndef _OPENEXR_PREMIERE_IMPORT_H_
#define _OPENEXR_PREMIERE_IMPORT_H_

#include	"PrSDKStructs.h"
#include	"PrSDKImport.h"
#include	"PrSDKClipRenderSuite.h"
#include	"PrSDKPPixCreatorSuite.h"
#include	"PrSDKPPixCacheSuite.h"
#include	"PrSDKWindowSuite.h"
#include	"PrSDKAppInfoSuite.h"
#ifdef		PRMAC_ENV
#include	<wchar.h>
#endif



#if IMPORTMOD_VERSION <= IMPORTMOD_VERSION_9
typedef long PrivateDataPtr;

typedef int csSDK_int32;
typedef int csSDK_size_t;
typedef long int RowbyteType;

#ifdef PRMAC_ENV
typedef SInt16 FSIORefNum;
#define CAST_REFNUM(REFNUM)		(REFNUM)
#define CAST_FILEREF(FILEREF)	(FILEREF)
#endif

#else
typedef void * PrivateDataPtr;
typedef csSDK_int32 RowbyteType;

#ifdef PRMAC_ENV
#define CAST_REFNUM(REFNUM)		reinterpret_cast<intptr_t>(REFNUM)
#define CAST_FILEREF(FILEREF)	reinterpret_cast<imFileRef>(FILEREF)
#else
#define CAST_REFNUM(REFNUM)		reinterpret_cast<int>(REFNUM)
#define CAST_FILEREF(FILEREF)	reinterpret_cast<imFileRef>(FILEREF)
#endif // PRMAC_ENV

#endif // version

// Declare plug-in entry point with C linkage
extern "C" {
PREMPLUGENTRY DllExport xImportEntry (csSDK_int32	selector, 
									  imStdParms	*stdParms, 
									  void			*param1, 
									  void			*param2);

}

#endif //_OPENEXR_PREMIERE_IMPORT_H_
