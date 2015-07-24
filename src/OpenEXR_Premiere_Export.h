/*
 *  ProEXR_Export.h
 *  ProEXR
 *
 *  Created by Brendan Bolles on 12/22/11.
 *  Copyright 2011 fnord. All rights reserved.
 *
 */

#ifndef _OPENEXR_PREMIERE_EXPORT_H
#define _OPENEXR_PREMIERE_EXPORT_H

#include	"PrSDKStructs.h"
#include	"PrSDKExport.h"
#include	"PrSDKExportFileSuite.h"
#include	"PrSDKExportInfoSuite.h"
#include	"PrSDKExportParamSuite.h"
#include	"PrSDKSequenceRenderSuite.h"
#include	"PrSDKPPixCreatorSuite.h"
#include	"PrSDKPPixCacheSuite.h"
#include	"PrSDKMemoryManagerSuite.h"
#include	"PrSDKWindowSuite.h"
#include	"PrSDKAppInfoSuite.h"
#ifdef		PRMAC_ENV
#include	<wchar.h>
#endif

extern "C" {
DllExport PREMPLUGENTRY xSDKExport (
	csSDK_int32		selector, 
	exportStdParms	*stdParms, 
	void			*param1, 
	void			*param2);
}


#endif // _OPENEXR_PREMIERE_EXPORT_H