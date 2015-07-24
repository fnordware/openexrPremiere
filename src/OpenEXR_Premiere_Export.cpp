/*
 *  ProEXR_Export.cpp
 *  ProEXR
 *
 *  Created by Brendan Bolles on 12/22/11.
 *  Copyright 2011 fnord. All rights reserved.
 *
 */

#include "OpenEXR_Premiere_Export.h"

#include "OpenEXR_Premiere_IO.h"

#include "OpenEXR_Premiere_Dialogs.h"

#include <ImfOutputFile.h>
#include <ImfRgbaFile.h>

#include <ImfStandardAttributes.h>
#include <ImfChannelList.h>
#include <ImfVersion.h>
#include <IexBaseExc.h>
#include <IlmThread.h>
#include <IlmThreadPool.h>
#include <ImfArray.h>


#ifdef PRMAC_ENV
	#include <mach/mach.h>
#else
	#include <assert.h>
	#include <time.h>
	#include <sys/timeb.h>
#endif

using namespace std;
using namespace Imf;
using namespace Imath;
using namespace IlmThread;


static const csSDK_int32 OpenEXR_ID = 'oEXR';
static const csSDK_int32 OpenEXR_Export_Class = 'eEXR';

unsigned int gNumCPUs = 1;

typedef struct ExportSettings
{
	SPBasicSuite				*spBasic;
	PrSDKExportParamSuite		*exportParamSuite;
	PrSDKExportInfoSuite		*exportInfoSuite;
	PrSDKExportFileSuite		*exportFileSuite;
	PrSDKPPixCreatorSuite		*ppixCreatorSuite;
	PrSDKPPixSuite				*ppixSuite;
	PrSDKTimeSuite				*timeSuite;
	PrSDKMemoryManagerSuite		*memorySuite;
	PrSDKSequenceRenderSuite	*sequenceRenderSuite;
	PrSDKWindowSuite			*windowSuite;
} ExportSettings;


static void
utf16ncpy(prUTF16Char *dest, const char *src, int max_len)
{
	prUTF16Char *d = dest;
	const char *c = src;
	
	do{
		*d++ = *c;
	}while(*c++ != '\0' && --max_len);
}


static prMALError
exSDKStartup(
	exportStdParms		*stdParmsP, 
	exExporterInfoRec	*infoRecP)
{
	PrSDKAppInfoSuite *appInfoSuite = NULL;
	stdParmsP->getSPBasicSuite()->AcquireSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion, (const void**)&appInfoSuite);
	
	if(appInfoSuite)
	{
		int fourCC = 0;
	
		appInfoSuite->GetAppInfo(PrSDKAppInfoSuite::kAppInfo_AppFourCC, (void *)&fourCC);
	
		stdParmsP->getSPBasicSuite()->ReleaseSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion);
		
		if(fourCC == kAppAfterEffects)
			return exportReturn_IterateExporterDone;
	}
	

	infoRecP->fileType			= OpenEXR_ID;
	
	utf16ncpy(infoRecP->fileTypeName, "OpenEXR", 255);
	utf16ncpy(infoRecP->fileTypeDefaultExtension, "exr", 255);
	
	infoRecP->classID = OpenEXR_Export_Class;
	
	infoRecP->exportReqIndex	= 0;
	infoRecP->wantsNoProgressBar = kPrFalse;
	infoRecP->hideInUI			= kPrFalse;
	infoRecP->doesNotSupportAudioOnly = kPrTrue;
	infoRecP->canExportVideo	= kPrTrue;
	infoRecP->canExportAudio	= kPrFalse;
	infoRecP->singleFrameOnly	= kPrTrue;
	
	infoRecP->interfaceVersion	= EXPORTMOD_VERSION;
	
	infoRecP->isCacheable		= kPrFalse;


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
exSDKShutdown()
{
	if( supportsThreads() )
		setGlobalThreadCount(0);
	
	return malNoError;
}


static prMALError
exSDKBeginInstance(
	exportStdParms			*stdParmsP, 
	exExporterInstanceRec	*instanceRecP)
{
	prMALError				result				= malNoError;
	SPErr					spError				= kSPNoError;
	ExportSettings			*mySettings;
	PrSDKMemoryManagerSuite	*memorySuite;
	csSDK_int32				exportSettingsSize	= sizeof(ExportSettings);
	SPBasicSuite			*spBasic			= stdParmsP->getSPBasicSuite();
	
	if(spBasic != NULL)
	{
		spError = spBasic->AcquireSuite(
			kPrSDKMemoryManagerSuite,
			kPrSDKMemoryManagerSuiteVersion,
			const_cast<const void**>(reinterpret_cast<void**>(&memorySuite)));
			
		mySettings = reinterpret_cast<ExportSettings *>(memorySuite->NewPtrClear(exportSettingsSize));

		if(mySettings)
		{
			mySettings->spBasic		= spBasic;
			mySettings->memorySuite	= memorySuite;
			
			spError = spBasic->AcquireSuite(
				kPrSDKExportParamSuite,
				kPrSDKExportParamSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportParamSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKExportFileSuite,
				kPrSDKExportFileSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportFileSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKExportInfoSuite,
				kPrSDKExportInfoSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportInfoSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKPPixCreatorSuite,
				kPrSDKPPixCreatorSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->ppixCreatorSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKPPixSuite,
				kPrSDKPPixSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->ppixSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKSequenceRenderSuite,
				kPrSDKSequenceRenderSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->sequenceRenderSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKTimeSuite,
				kPrSDKTimeSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->timeSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKWindowSuite,
				kPrSDKWindowSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->windowSuite))));
		}


		instanceRecP->privateData = reinterpret_cast<void*>(mySettings);
	}
	else
	{
		result = exportReturn_ErrMemory;
	}
	
	return result;
}


static prMALError
exSDKEndInstance(
	exportStdParms			*stdParmsP, 
	exExporterInstanceRec	*instanceRecP)
{
	prMALError				result		= malNoError;
	ExportSettings			*lRec		= reinterpret_cast<ExportSettings *>(instanceRecP->privateData);
	SPBasicSuite			*spBasic	= stdParmsP->getSPBasicSuite();
	PrSDKMemoryManagerSuite	*memorySuite;
	if(spBasic != NULL && lRec != NULL)
	{
		if (lRec->exportParamSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportParamSuite, kPrSDKExportParamSuiteVersion);
		}
		if (lRec->exportFileSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportFileSuite, kPrSDKExportFileSuiteVersion);
		}
		if (lRec->exportInfoSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportInfoSuite, kPrSDKExportInfoSuiteVersion);
		}
		if (lRec->ppixSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion);
		}
		if (lRec->sequenceRenderSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKSequenceRenderSuite, kPrSDKSequenceRenderSuiteVersion);
		}
		if (lRec->timeSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKTimeSuite, kPrSDKTimeSuiteVersion);
		}
		if (lRec->windowSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKWindowSuite, kPrSDKWindowSuiteVersion);
		}
		if (lRec->memorySuite)
		{
			memorySuite = lRec->memorySuite;
			memorySuite->PrDisposePtr(reinterpret_cast<PrMemoryPtr>(lRec));
			result = spBasic->ReleaseSuite(kPrSDKMemoryManagerSuite, kPrSDKMemoryManagerSuiteVersion);
		}
	}

	return result;
}


#define EXRSettingsGroup	"EXRSettingsGroup"
#define EXRCompression		"EXRCompression"
#define EXRCompressionLevel	"EXRCompressionLevel"
#define EXRFloat			"EXRfloat"
#define EXRBypassLinear		"EXRBypassLinear"
#define EXRLumiChrom		"EXRlumichrom"

#define ADBEStillSequence	"ADBEStillSequence"
#define ADBEVideoAlpha		"ADBEVideoAlpha"


static prMALError
exSDKQueryOutputSettings(
	exportStdParms				*stdParmsP,
	exQueryOutputSettingsRec	*outputSettingsP)
{
	prMALError result = malNoError;
	
	ExportSettings *privateData	= reinterpret_cast<ExportSettings*>(outputSettingsP->privateData);
	
	csSDK_uint32				exID			= outputSettingsP->exporterPluginID;
	exParamValues				width,
								height,
								frameRate,
								pixelAspectRatio,
								fieldType,
								alpha,
								compression,
								lumiChrom,
								floatNotHalf;
	PrSDKExportParamSuite		*paramSuite		= privateData->exportParamSuite;
	csSDK_int32					mgroupIndex		= 0;
	float						fps				= 0.0f;
	PrTime						ticksPerSecond	= 0;
	csSDK_uint32				videoBitrate	= 0;
	
	if(outputSettingsP->inExportVideo)
	{
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoWidth, &width);
		outputSettingsP->outVideoWidth = width.value.intValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoHeight, &height);
		outputSettingsP->outVideoHeight = height.value.intValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFPS, &frameRate);
		outputSettingsP->outVideoFrameRate = frameRate.value.timeValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAspect, &pixelAspectRatio);
		outputSettingsP->outVideoAspectNum = pixelAspectRatio.value.ratioValue.numerator;
		outputSettingsP->outVideoAspectDen = pixelAspectRatio.value.ratioValue.denominator;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFieldType, &fieldType);
		outputSettingsP->outVideoFieldType = fieldType.value.intValue;
		
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAlpha, &alpha);
		
		privateData->timeSuite->GetTicksPerSecond(&ticksPerSecond);
		fps = static_cast<float>(ticksPerSecond) / frameRate.value.timeValue;
		
		paramSuite->GetParamValue(exID, mgroupIndex, EXRCompression, &compression);
		paramSuite->GetParamValue(exID, mgroupIndex, EXRLumiChrom, &lumiChrom);
		paramSuite->GetParamValue(exID, mgroupIndex, EXRFloat, &floatNotHalf);
		
		
		int channels = (alpha.value.intValue ? 4 : 3);
		int pix_size = (floatNotHalf.value.intValue ? sizeof(float) : sizeof(half));
		int divisor = (compression.value.intValue == NO_COMPRESSION ? 1 : 2);
		
		if(lumiChrom.value.intValue)
		{
			pix_size = sizeof(half);
			divisor *= 2;
		}
		
		videoBitrate = static_cast<csSDK_uint32>(fps * width.value.intValue * height.value.intValue *
												pix_size * channels / divisor);
	}
	
	// return outBitratePerSecond in kbps
	outputSettingsP->outBitratePerSecond = (videoBitrate * 8) / 1024;


	return result;
}

static prMALError
exSDKFileExtension(
	exportStdParms					*stdParmsP, 
	exQueryExportFileExtensionRec	*exportFileExtensionRecP)
{
	utf16ncpy(exportFileExtensionRecP->outFileExtension, "exr", 255);
		
	return malNoError;
}


static prMALError
exSDKQueryStillSequence(
	exportStdParms			*stdParmsP,
	exQueryStillSequenceRec	*stillSequenceRecP)
{
	ExportSettings *privateData	= reinterpret_cast<ExportSettings*>(stillSequenceRecP->privateData);

	PrSDKExportParamSuite *paramSuite = privateData->exportParamSuite;
	
	csSDK_uint32 exID = stillSequenceRecP->exporterPluginID;
	csSDK_int32 gIdx = 0;
	
	exParamValues sequence, frameRate;
	
	paramSuite->GetParamValue(exID, gIdx, ADBEStillSequence, &sequence);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFPS, &frameRate);
	
	
	stillSequenceRecP->exportAsStillSequence = sequence.value.intValue;
	stillSequenceRecP->exportFrameRate = frameRate.value.timeValue;
	
	return malNoError;
}


static TimeCode
CalculateTimeCode(int frame_num, int frame_rate, bool drop_frame)
{
	// the easiest way to do this is just count!
	int h = 0,
		m = 0,
		s = 0,
		f = 0;
	
	// skip ahead quickly
	const int frames_per_ten_mins = (frame_rate * 60 * 10) - (drop_frame ? 9 * (frame_rate == 60 ? 4 : 2) : 0);
	const int frames_per_hour = 6 * frames_per_ten_mins;
	
	while(frame_num >= frames_per_hour)
	{
		h++;
		
		frame_num -= frames_per_hour;
	}
	
	while(frame_num >= frames_per_ten_mins)
	{
		m += 10;
		
		frame_num -= frames_per_ten_mins;
	}
	
	// now count out the rest
	int frame = 0;
	
	while(frame++ < frame_num)
	{
		if(f < frame_rate - 1)
		{
			f++;
		}
		else
		{
			f = 0;
			
			if(s < 59)
			{
				s++;
			}
			else
			{
				s = 0;
				
				if(m < 59)
				{
					m++;
					
					if(drop_frame && (m % 10) != 0) // http://en.wikipedia.org/wiki/SMPTE_timecode
					{
						f += (frame_rate == 60 ? 4 : 2);
					}
				}
				else
				{
					m = 0;
					
					h++;
				}
			}
		}
	}
	
	return TimeCode(h, m, s, f, drop_frame);
}


template <typename InFormat, typename OutFormat>
class ConvertBgraRowTask : public Task
{
  public:
	ConvertBgraRowTask(TaskGroup *group, InFormat *input_row, OutFormat *output_row, int length, bool premult);
	virtual ~ConvertBgraRowTask() {}
	
	virtual void execute();

  private:
	InFormat *_input_row;
	OutFormat *_output_row;
	int _length;
	bool _premult;
};


template <typename InFormat, typename OutFormat>
ConvertBgraRowTask<InFormat, OutFormat>::ConvertBgraRowTask(TaskGroup *group, InFormat *input_row, OutFormat *output_row, int length, bool premult) :
	Task(group),
	_input_row(input_row),
	_output_row(output_row),
	_length(length),
	_premult(premult)
{

}


template <typename InFormat, typename OutFormat>
void
ConvertBgraRowTask<InFormat, OutFormat>::execute()
{
	InFormat *in = _input_row;
	OutFormat *out = _output_row;
	
	for(int x=0; x < (_length * 4); x++)
	{
		*out++ = *in++;
	}

	if(_premult)
	{
		OutFormat *b = _output_row + 0;
		OutFormat *g = _output_row + 1;
		OutFormat *r = _output_row + 2;
		OutFormat *a = _output_row + 3;
		
		for(int x=0; x < _length; x++)
		{
			if(*a != 1.f)
			{
				*b *= *a;
				*g *= *a;
				*r *= *a;
			}

			b += 4;
			g += 4;
			r += 4;
			a += 4;
		}
	}
}


static void
Premultiply(Rgba &in)
{
	if(in.a != 1.f)
	{
		in.r *= in.a;
		in.g *= in.a;
		in.b *= in.a;
	}
}


static prMALError
exSDKExport(
	exportStdParms	*stdParmsP,
	exDoExportRec	*exportInfoP)
{
	prMALError					result					= malNoError;
	ExportSettings				*mySettings				= reinterpret_cast<ExportSettings*>(exportInfoP->privateData);
	PrSDKExportParamSuite		*paramSuite				= mySettings->exportParamSuite;
	PrSDKExportInfoSuite		*exportInfoSuite		= mySettings->exportInfoSuite;
	PrSDKSequenceRenderSuite	*renderSuite			= mySettings->sequenceRenderSuite;
	PrSDKMemoryManagerSuite		*memorySuite			= mySettings->memorySuite;
	PrSDKPPixCreatorSuite		*pixCreatorSuite		= mySettings->ppixCreatorSuite;
	PrSDKPPixSuite				*pixSuite				= mySettings->ppixSuite;

	if(!exportInfoP->exportVideo)
		return malNoError;


	csSDK_uint32 exID = exportInfoP->exporterPluginID;
	csSDK_int32 gIdx = 0;
	
	exParamValues widthP, heightP, pixelAspectRatioP, fieldTypeP, frameRateP, alphaP;
	
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &widthP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &heightP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoAspect, &pixelAspectRatioP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFieldType, &fieldTypeP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFPS, &frameRateP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoAlpha, &alphaP);
	
	
	exParamValues bypassLinearP;
	bypassLinearP.value.intValue = kPrFalse;
	paramSuite->GetParamValue(exID, gIdx, EXRBypassLinear, &bypassLinearP);
	
	SequenceRender_ParamsRec renderParms;
	const PrPixelFormat pixelFormat = (bypassLinearP.value.intValue ?
										PrPixelFormat_BGRA_4444_32f :
										PrPixelFormat_BGRA_4444_32f_Linear);
	
	
	renderParms.inRequestedPixelFormatArray = &pixelFormat;
	renderParms.inRequestedPixelFormatArrayCount = 1;
	renderParms.inWidth = widthP.value.intValue;
	renderParms.inHeight = heightP.value.intValue;
	renderParms.inPixelAspectRatioNumerator = pixelAspectRatioP.value.ratioValue.numerator;
	renderParms.inPixelAspectRatioDenominator = pixelAspectRatioP.value.ratioValue.denominator;
	renderParms.inRenderQuality = kPrRenderQuality_High;
	renderParms.inFieldType = fieldTypeP.value.intValue;
	renderParms.inDeinterlace = kPrFalse;
	renderParms.inDeinterlaceQuality = kPrRenderQuality_High;
	renderParms.inCompositeOnBlack = (alphaP.value.intValue ? kPrFalse: kPrTrue);
	
	
	csSDK_uint32 videoRenderID;
	renderSuite->MakeVideoRenderer(exID, &videoRenderID, frameRateP.value.timeValue);
	
	
	SequenceRender_GetFrameReturnRec renderResult;
	result = renderSuite->RenderVideoFrame(videoRenderID,
											exportInfoP->startTime,
											&renderParms,
											kRenderCacheType_None,
											&renderResult);
	
	if(result != suiteError_CompilerCompileAbort)
	{
		exParamValues compressionP, compressionLevelP, lumichromP, floatP;
		
		paramSuite->GetParamValue(exID, gIdx, EXRCompression, &compressionP);
		paramSuite->GetParamValue(exID, gIdx, EXRCompressionLevel, &compressionLevelP);
		paramSuite->GetParamValue(exID, gIdx, EXRLumiChrom, &lumichromP);
		paramSuite->GetParamValue(exID, gIdx, EXRFloat, &floatP);
		
		
		char *frameBufferP = NULL;
		csSDK_int32 rowbytes = 0;
		PrPixelFormat pixFormat;
		prRect bounds;
		csSDK_uint32 parN, parD;
		
		pixSuite->GetPixels(renderResult.outFrame, PrPPixBufferAccess_ReadOnly, &frameBufferP);
		pixSuite->GetRowBytes(renderResult.outFrame, &rowbytes);
		pixSuite->GetPixelFormat(renderResult.outFrame, &pixFormat);
		pixSuite->GetBounds(renderResult.outFrame, &bounds);
		pixSuite->GetPixelAspectRatio(renderResult.outFrame, &parN, &parD);
		
		
		if(pixFormat == PrPixelFormat_BGRA_4444_32f_Linear || pixFormat == PrPixelFormat_BGRA_4444_32f)
		{
			try
			{
				if( supportsThreads() )
					setGlobalThreadCount(gNumCPUs);
					
				
				bool floatNotHalf = floatP.value.intValue;
				bool lumiChrom = lumichromP.value.intValue;
				bool alpha = alphaP.value.intValue;
				
				
				int data_width, display_width, data_height, display_height;
				
				data_width = display_width = bounds.right - bounds.left;
				data_height = display_height = bounds.bottom - bounds.top;
				
				if(lumiChrom)
				{	// Luminance/Chroma needs even width & height
					data_width += (data_width % 2);
					data_height += (data_height % 2);
				}
						
				
				const Compression compression_type = (Compression)compressionP.value.intValue;
				
				
				// set up header
				Header header(	Box2i( V2i(0,0), V2i(display_width-1, display_height-1) ),
								Box2i( V2i(0,0), V2i(data_width-1, data_height-1) ),
								(float)parN / (float)parD,
								V2f(0, 0),
								1,
								INCREASING_Y,
								compression_type);
				
				
				if(compression_type == DWAA_COMPRESSION || compression_type == DWAB_COMPRESSION)
				{
					addDwaCompressionLevel(header, compressionLevelP.value.floatValue);
				}
				
				
				// store the actual ratio as a custom attribute
			#define PIXEL_ASPECT_RATIONAL_KEY	"pixelAspectRatioRational"
				if(parN != parD)
				{
					header.insert(PIXEL_ASPECT_RATIONAL_KEY, RationalAttribute( Rational(parN, parD) ) );
				}
				
				
				// add time attributes
				time_t the_time = time(NULL);
				tm *local_time = localtime(&the_time);
				
				if(local_time)
				{
					char date_string[256];
					sprintf(date_string, "%04d:%02d:%02d %02d:%02d:%02d", local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
																local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
					
					addCapDate(header, date_string);
			#ifdef PRWIN_ENV
					_timeb win_time;
					_ftime(&win_time);
					addUtcOffset(header, (float)( (win_time.timezone - (win_time.dstflag ? 60 : 0) ) * 60));
			#else
					addUtcOffset(header, (float)-local_time->tm_gmtoff);
			#endif
				}
				
				
				PrTime ticksPerSecond;
				mySettings->timeSuite->GetTicksPerSecond(&ticksPerSecond);
				
				// FPS
				if(ticksPerSecond < limits<int>::max() &&
					frameRateP.value.timeValue < limits<unsigned int>::max())
				{
					addFramesPerSecond(header, Rational(ticksPerSecond, frameRateP.value.timeValue) );
				}
				else
					addFramesPerSecond(header, Rational((double)ticksPerSecond / (double)frameRateP.value.timeValue) );
					
				
				// timecode
				PrTime frameRates[] = {	10, 15, 23,
										24, 25, 29,
										30, 50, 59,
										60};
																
				PrTime frameRateNumDens[][2] = {{10, 1}, {15, 1}, {24000, 1001},
												{24, 1}, {25, 1}, {30000, 1001},
												{30, 1}, {50, 1}, {60000, 1001},
												{60, 1}};
				
				int frameRateBase[] = {	10, 15, 24,
										24, 25, 30,
										30, 50, 60,
										60};
				
				bool dropFrame[] =	{	false, false, false,
										false, false, true,
										false, false, true,
										false };
				
				for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
				{
					frameRates[i] = ticksPerSecond / frameRateNumDens[i][0] * frameRateNumDens[i][1];
				}
				
				
				int frameRateIndex = -1;
				
				for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
				{
					if(frameRateP.value.timeValue == frameRates[i])
						frameRateIndex = i;
				}
				
				if(frameRateIndex >= 0)
				{
				#if EXPORTMOD_VERSION >= 4
					PrParam timecodeP;
					exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_SourceTimecode, &timecodeP);
					
					ExporterTimecodeRec *timecodeRec = (ExporterTimecodeRec *)timecodeP.mMemoryPtr;
					
					int frame_num = 0;
					int alt_timecode_base = 0;
					bool alt_drop_frame = false;
					
					if(timecodeRec)
					{
						frame_num = timecodeRec->mTimecodeTicks / timecodeRec->mTicksPerFrame;
						
						alt_timecode_base = ((float)ticksPerSecond / (float)timecodeRec->mTicksPerFrame) + 0.5f;
						
						alt_drop_frame = timecodeRec->mTimecodeStartPrefersDropFrame;
					
						memorySuite->PrDisposePtr(timecodeP.mMemoryPtr);
					}
					else
					{
						frame_num = (exportInfoP->startTime * frameRateNumDens[frameRateIndex][0]) /
										(ticksPerSecond * frameRateNumDens[frameRateIndex][1]);
					}
					
					
					const int alt_frame_num = (exportInfoP->startTime * frameRateNumDens[frameRateIndex][0]) /
										(ticksPerSecond * frameRateNumDens[frameRateIndex][1]);
										
					const int timecode_base = frameRateBase[frameRateIndex];
					
					const bool drop_frame = dropFrame[frameRateIndex];
					
					
					assert(frame_num == alt_frame_num);
					assert(timecode_base == alt_timecode_base);
					assert(drop_frame == alt_drop_frame); // this will fail with the export frame button
				#else
					const int frame_num = (exportInfoP->startTime * frameRateNumDens[frameRateIndex][0]) /
										(ticksPerSecond * frameRateNumDens[frameRateIndex][1]);
										
					const int timecode_base = frameRateBase[frameRateIndex];
					
					const bool drop_frame = dropFrame[frameRateIndex];
				#endif
					
					
					addTimeCode(header, CalculateTimeCode(frame_num, timecode_base, drop_frame));

					// we'll reset the FPS with the real values while we're at it
					addFramesPerSecond(header, Rational(frameRateNumDens[frameRateIndex][0], frameRateNumDens[frameRateIndex][1]) );
				}
				
				
				// graffiti
				header.insert("writer", StringAttribute("ProEXR for Premiere"));
				
				
				// Color space
				if(bypassLinearP.value.intValue)
					addComments(header, "Conversion to linear bypassed in Premiere");
	
	
				if(lumiChrom)
				{
					// make our Rgba buffer
					Array2D<Rgba> half_buffer(data_height, data_width);
					
					char *buf_row = frameBufferP;
					
					for(int y = display_height - 1; y >= 0; y--)
					{
						float *buf_pix = (float *)buf_row;
					
						for(int x=0; x < display_width; x++)
						{
							half_buffer[y][x].b = *buf_pix++;
							half_buffer[y][x].g = *buf_pix++;
							half_buffer[y][x].r = *buf_pix++;
							half_buffer[y][x].a = *buf_pix++;
							
							if(alpha)
								Premultiply( half_buffer[y][x] );
						}
						
						if(data_width == (display_width + 1))
						{
							half_buffer[y][data_width - 1] = half_buffer[y][display_width - 1];
						}
						
						buf_row += rowbytes;
					}
					
					if(data_height == (display_height + 1))
					{
						for(int x=0; x < data_width; x++)
						{
							half_buffer[data_height - 1][x] = half_buffer[display_height - 1][x];
						}
					}
					
					
					// write file
					const RgbaChannels rgba_channels = (alpha ? WRITE_YCA : WRITE_YC);
		
					OStreamPr outstream(mySettings->exportFileSuite, exportInfoP->fileObject);
					RgbaOutputFile outputFile(outstream, header, rgba_channels);
					
					outputFile.setFrameBuffer(&half_buffer[0][0], 1, data_width);
					outputFile.writePixels(data_height);
				}
				else
				{
					int width = data_width;
					int height = data_height;
					
					Imf::PixelType pix_type = (floatNotHalf ? Imf::FLOAT : Imf::HALF);
					size_t pix_size = (pix_type == Imf::FLOAT ? sizeof(float) : sizeof(half));

					char *buf_origin = frameBufferP;
					size_t buf_rowbytes = rowbytes;
					
					
					PPixHand tempWorld = NULL;
					
					// we have to make a copy of the world if we want to create Half pixels or just premultiply
					if(pix_type == Imf::HALF || alpha)
					{
						PrPixelFormat pix_format = (pix_type == Imf::HALF ? PrPixelFormat_BGRA_4444_16u : PrPixelFormat_BGRA_4444_32f_Linear);

						pixCreatorSuite->CreatePPix(&tempWorld, PrPPixBufferAccess_ReadWrite,
														pix_format, &bounds);
														
						char *temp_origin = NULL;
						pixSuite->GetPixels(tempWorld, PrPPixBufferAccess_ReadWrite, &temp_origin);
						
						csSDK_int32 temp_rowbytes = 0;
						pixSuite->GetRowBytes(tempWorld, &temp_rowbytes);
						
						TaskGroup taskGroup;
						
						char *buf_row = buf_origin;
						char *temp_row = temp_origin;
						
						for(int y=0; y < height; y++)
						{
							if(pix_type == Imf::HALF)
							{
								ThreadPool::addGlobalTask(new ConvertBgraRowTask<float, half>(&taskGroup,
																								(float *)buf_row,
																								(half *)temp_row,
																								width,
																								alpha) );
							}
							else
							{
								ThreadPool::addGlobalTask(new ConvertBgraRowTask<float, float>(&taskGroup,
																								(float *)buf_row,
																								(float *)temp_row,
																								width,
																								alpha) );
							}
							
							buf_row += buf_rowbytes;
							temp_row += temp_rowbytes;
						}
						
						buf_origin = temp_origin;
						buf_rowbytes = temp_rowbytes;
					}


					header.channels().insert("B", Channel(pix_type));
					header.channels().insert("G", Channel(pix_type));
					header.channels().insert("R", Channel(pix_type));
					
					if(alpha)
						header.channels().insert("A", Channel(pix_type));
					
						
					char *bgra_origin = buf_origin + (buf_rowbytes * (height - 1));
					
					FrameBuffer frameBuffer;
					
					frameBuffer.insert("B", Slice(pix_type, (char *)bgra_origin + (pix_size * 0), pix_size * 4, -buf_rowbytes) );
					frameBuffer.insert("G", Slice(pix_type, (char *)bgra_origin + (pix_size * 1), pix_size * 4, -buf_rowbytes) );
					frameBuffer.insert("R", Slice(pix_type, (char *)bgra_origin + (pix_size * 2), pix_size * 4, -buf_rowbytes) );
					
					if(alpha)
						frameBuffer.insert("A", Slice(pix_type, (char *)bgra_origin + (pix_size * 3), pix_size * 4, -buf_rowbytes) );
					
					
					OStreamPr outstream(mySettings->exportFileSuite, exportInfoP->fileObject);
					OutputFile file(outstream, header);
					
					file.setFrameBuffer(frameBuffer);
					file.writePixels(height);
					
					
					if(tempWorld)
					{
						pixSuite->Dispose(tempWorld);
					}
				}
			}
			catch(...)
			{
				result = exportReturn_ErrIo;
			}
		}
		
		
		pixSuite->Dispose(renderResult.outFrame);
	}
	
	renderSuite->ReleaseVideoRenderer(exID, videoRenderID);


	return result;
}


static prMALError
exSDKGenerateDefaultParams(
	exportStdParms				*stdParms, 
	exGenerateDefaultParamRec	*generateDefaultParamRec)
{
	prMALError				result				= malNoError;

	ExportSettings			*lRec				= reinterpret_cast<ExportSettings *>(generateDefaultParamRec->privateData);
	PrSDKExportParamSuite	*exportParamSuite	= lRec->exportParamSuite;
	PrSDKExportInfoSuite	*exportInfoSuite	= lRec->exportInfoSuite;
	PrSDKTimeSuite			*timeSuite			= lRec->timeSuite;

	csSDK_int32 exID = generateDefaultParamRec->exporterPluginID;
	csSDK_int32 gIdx = 0;
	
	
	// get current settings
	PrParam widthP, heightP, parN, parD, fieldTypeP, frameRateP;
	
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoWidth, &widthP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoHeight, &heightP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_PixelAspectNumerator, &parN);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_PixelAspectDenominator, &parD);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoFieldType, &fieldTypeP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoFrameRate, &frameRateP);
	
	if(widthP.mInt32 == 0)
	{
		widthP.mInt32 = 1920;
	}
	
	if(heightP.mInt32 == 0)
	{
		heightP.mInt32 = 1080;
	}



	prUTF16Char groupString[256];
	
	// Video Tab
	exportParamSuite->AddMultiGroup(exID, &gIdx);
	
	utf16ncpy(groupString, "Video Tab", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBETopParamGroup, ADBEVideoTabGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
	

	// OpenEXR Settings group
	utf16ncpy(groupString, "OpenEXR Settings", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBEVideoTabGroup, EXRSettingsGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
	
	// compression
	exParamValues compressionValues;
	compressionValues.structVersion = 1;
	compressionValues.rangeMin.intValue = NO_COMPRESSION;
	compressionValues.rangeMax.intValue = DWAB_COMPRESSION;
	compressionValues.value.intValue = PIZ_COMPRESSION;
	compressionValues.disabled = kPrFalse;
	compressionValues.hidden = kPrFalse;
	
	exNewParamInfo compressionParam;
	compressionParam.structVersion = 1;
	strncpy(compressionParam.identifier, EXRCompression, 255);
	compressionParam.paramType = exParamType_int;
	compressionParam.flags = exParamFlag_none;
	compressionParam.paramValues = compressionValues;
	
	exportParamSuite->AddParam(exID, gIdx, EXRSettingsGroup, &compressionParam);


	// compression level
	exParamValues compressionLevelValues;
	compressionLevelValues.structVersion = 1;
	compressionLevelValues.rangeMin.floatValue = 0.f;
	compressionLevelValues.rangeMax.floatValue = 1000.f;
	compressionLevelValues.value.floatValue = 45.f;
	compressionLevelValues.disabled = kPrFalse;
	compressionLevelValues.hidden = kPrTrue;
	
	exNewParamInfo compressionLevelParam;
	compressionLevelParam.structVersion = 1;
	strncpy(compressionLevelParam.identifier, EXRCompressionLevel, 255);
	compressionLevelParam.paramType = exParamType_float;
	compressionLevelParam.flags = exParamFlag_slider;
	compressionLevelParam.paramValues = compressionLevelValues;
	
	exportParamSuite->AddParam(exID, gIdx, EXRSettingsGroup, &compressionLevelParam);


	// luminance/chroma
	exParamValues lumichromValues;
	lumichromValues.structVersion = 1;
	lumichromValues.value.intValue = kPrFalse;
	lumichromValues.disabled = kPrFalse;
	lumichromValues.hidden = kPrFalse;
	
	exNewParamInfo lumichromParam;
	lumichromParam.structVersion = 1;
	strncpy(lumichromParam.identifier, EXRLumiChrom, 255);
	lumichromParam.paramType = exParamType_bool;
	lumichromParam.flags = exParamFlag_none;
	lumichromParam.paramValues = lumichromValues;
	
	exportParamSuite->AddParam(exID, gIdx, EXRSettingsGroup, &lumichromParam);
	

	// 32-bit float
	exParamValues floatValues;
	floatValues.structVersion = 1;
	floatValues.value.intValue = kPrFalse;
	floatValues.disabled = kPrFalse;
	floatValues.hidden = kPrFalse;
	
	exNewParamInfo floatParam;
	floatParam.structVersion = 1;
	strncpy(floatParam.identifier, EXRFloat, 255);
	floatParam.paramType = exParamType_bool;
	floatParam.flags = exParamFlag_none;
	floatParam.paramValues = floatValues;
	
	exportParamSuite->AddParam(exID, gIdx, EXRSettingsGroup, &floatParam);
	
	
	// Bypass Linear Conversion
	exParamValues bypassValues;
	bypassValues.structVersion = 1;
	bypassValues.value.intValue = kPrFalse;
	bypassValues.disabled = kPrFalse;
	bypassValues.hidden = kPrFalse;
	
	exNewParamInfo bypassParam;
	bypassParam.structVersion = 1;
	strncpy(bypassParam.identifier, EXRBypassLinear, 255);
	bypassParam.paramType = exParamType_bool;
	bypassParam.flags = exParamFlag_none;
	bypassParam.paramValues = bypassValues;
	
	exportParamSuite->AddParam(exID, gIdx, EXRSettingsGroup, &bypassParam);
	
	
	// Image Settings group
	utf16ncpy(groupString, "Image Settings", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBEVideoTabGroup, ADBEBasicVideoGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
	
	// width
	exParamValues widthValues;
	widthValues.structVersion = 1;
	widthValues.rangeMin.intValue = 16;
	widthValues.rangeMax.intValue = 8192;
	widthValues.value.intValue = widthP.mInt32;
	widthValues.disabled = kPrFalse;
	widthValues.hidden = kPrFalse;
	
	exNewParamInfo widthParam;
	widthParam.structVersion = 1;
	strncpy(widthParam.identifier, ADBEVideoWidth, 255);
	widthParam.paramType = exParamType_int;
	widthParam.flags = exParamFlag_none;
	widthParam.paramValues = widthValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &widthParam);


	// height
	exParamValues heightValues;
	heightValues.structVersion = 1;
	heightValues.rangeMin.intValue = 16;
	heightValues.rangeMax.intValue = 8192;
	heightValues.value.intValue = heightP.mInt32;
	heightValues.disabled = kPrFalse;
	heightValues.hidden = kPrFalse;
	
	exNewParamInfo heightParam;
	heightParam.structVersion = 1;
	strncpy(heightParam.identifier, ADBEVideoHeight, 255);
	heightParam.paramType = exParamType_int;
	heightParam.flags = exParamFlag_none;
	heightParam.paramValues = heightValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &heightParam);


	// pixel aspect ratio
	exParamValues parValues;
	parValues.structVersion = 1;
	parValues.rangeMin.ratioValue.numerator = 10;
	parValues.rangeMin.ratioValue.denominator = 11;
	parValues.rangeMax.ratioValue.numerator = 2;
	parValues.rangeMax.ratioValue.denominator = 1;
	parValues.value.ratioValue.numerator = parN.mInt32;
	parValues.value.ratioValue.denominator = parD.mInt32;
	parValues.disabled = kPrFalse;
	parValues.hidden = kPrFalse;
	
	exNewParamInfo parParam;
	parParam.structVersion = 1;
	strncpy(parParam.identifier, ADBEVideoAspect, 255);
	parParam.paramType = exParamType_ratio;
	parParam.flags = exParamFlag_none;
	parParam.paramValues = parValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &parParam);
	
	
	// field order
	exParamValues fieldOrderValues;
	fieldOrderValues.structVersion = 1;
	fieldOrderValues.value.intValue = fieldTypeP.mInt32;
	fieldOrderValues.disabled = kPrFalse;
	fieldOrderValues.hidden = kPrFalse;
	
	exNewParamInfo fieldOrderParam;
	fieldOrderParam.structVersion = 1;
	strncpy(fieldOrderParam.identifier, ADBEVideoFieldType, 255);
	fieldOrderParam.paramType = exParamType_int;
	fieldOrderParam.flags = exParamFlag_none;
	fieldOrderParam.paramValues = fieldOrderValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &fieldOrderParam);


	// Export sequence
	exParamValues sequenceValues;
	sequenceValues.structVersion = 1;
	sequenceValues.value.intValue = kPrFalse;
	sequenceValues.disabled = kPrFalse;
	sequenceValues.hidden = kPrFalse;
	
	exNewParamInfo sequenceParam;
	sequenceParam.structVersion = 1;
	strncpy(sequenceParam.identifier, ADBEStillSequence, 255);
	sequenceParam.paramType = exParamType_bool;
	sequenceParam.flags = exParamFlag_none;
	sequenceParam.paramValues = sequenceValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &sequenceParam);
	
	
	// frame rate
	exParamValues fpsValues;
	fpsValues.structVersion = 1;
	fpsValues.rangeMin.timeValue = 1;
	timeSuite->GetTicksPerSecond(&fpsValues.rangeMax.timeValue);
	fpsValues.value.timeValue = frameRateP.mInt64;
	fpsValues.disabled = kPrTrue;
	fpsValues.hidden = kPrFalse;
	
	exNewParamInfo fpsParam;
	fpsParam.structVersion = 1;
	strncpy(fpsParam.identifier, ADBEVideoFPS, 255);
	fpsParam.paramType = exParamType_ticksFrameRate;
	fpsParam.flags = exParamFlag_none;
	fpsParam.paramValues = fpsValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &fpsParam);


	// Alpha channel
	exParamValues alphaValues;
	alphaValues.structVersion = 1;
	alphaValues.value.intValue = kPrFalse;
	alphaValues.disabled = kPrFalse;
	alphaValues.hidden = kPrFalse;
	
	exNewParamInfo alphaParam;
	alphaParam.structVersion = 1;
	strncpy(alphaParam.identifier, ADBEVideoAlpha, 255);
	alphaParam.paramType = exParamType_bool;
	alphaParam.flags = exParamFlag_none;
	alphaParam.paramValues = alphaValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &alphaParam);



	exportParamSuite->SetParamsVersion(exID, 1);
	
	
	return result;
}


static prMALError
exSDKPostProcessParams(
	exportStdParms			*stdParmsP, 
	exPostProcessParamsRec	*postProcessParamsRecP)
{
	prMALError		result	= malNoError;

	ExportSettings			*lRec				= reinterpret_cast<ExportSettings *>(postProcessParamsRecP->privateData);
	PrSDKExportParamSuite	*exportParamSuite	= lRec->exportParamSuite;
	//PrSDKExportInfoSuite	*exportInfoSuite	= lRec->exportInfoSuite;
	PrSDKTimeSuite			*timeSuite			= lRec->timeSuite;

	csSDK_int32 exID = postProcessParamsRecP->exporterPluginID;
	csSDK_int32 gIdx = 0;
	
	prUTF16Char paramString[256];
	
	
	// OpenEXR settings group
	utf16ncpy(paramString, "OpenEXR Settings", 255);
	exportParamSuite->SetParamName(exID, gIdx, EXRSettingsGroup, paramString);
	
	
	// compression
	utf16ncpy(paramString, "Compression", 255);
	exportParamSuite->SetParamName(exID, gIdx, EXRCompression, paramString);
	
	const char *compressionStrings[] = {"None",
										"RLE",
										"Zip",
										"Zip16",
										"Piz",
										"PXR24",
										"B44",
										"B44A",
										"DWAA",
										"DWAB" };
										
	csSDK_int32 compressionValues[] = {	NO_COMPRESSION,
										RLE_COMPRESSION,
										ZIPS_COMPRESSION,
										ZIP_COMPRESSION,
										PIZ_COMPRESSION,
										PXR24_COMPRESSION,
										B44_COMPRESSION,
										B44A_COMPRESSION,
										DWAA_COMPRESSION,
										DWAB_COMPRESSION };
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, EXRCompression);
	
	exOneParamValueRec tempCompression;
	
	for(csSDK_int32 i=0; i < sizeof(compressionValues) / sizeof (csSDK_int32); i++)
	{
		tempCompression.intValue = compressionValues[i];
		utf16ncpy(paramString, compressionStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, EXRCompression, &tempCompression, paramString);
	}
	
	
	// compression level
	utf16ncpy(paramString, "Compression Level", 255);
	exportParamSuite->SetParamName(exID, gIdx, EXRCompressionLevel, paramString);
	
	exParamValues compressionLevelValues;
	exportParamSuite->GetParamValue(exID, gIdx, EXRCompressionLevel, &compressionLevelValues);

	compressionLevelValues.rangeMin.floatValue = 0.f;
	compressionLevelValues.rangeMax.floatValue = 1000.f;

	exportParamSuite->ChangeParam(exID, gIdx, EXRCompressionLevel, &compressionLevelValues);
	
	
	// Luminance/Chroma
	utf16ncpy(paramString, "Luminance/Chroma", 255);
	exportParamSuite->SetParamName(exID, gIdx, EXRLumiChrom, paramString);
	
	
	// 32-bit float
	utf16ncpy(paramString, "32-bit float (not recommended)", 255);
	exportParamSuite->SetParamName(exID, gIdx, EXRFloat, paramString);
	
	
	// Bypass Linear Conversion
	utf16ncpy(paramString, "Bypass linear conversion", 255);
	exportParamSuite->SetParamName(exID, gIdx, EXRBypassLinear, paramString);
	
	
	// Image Settings group
	utf16ncpy(paramString, "Image Settings", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEBasicVideoGroup, paramString);
	
									
	// width
	utf16ncpy(paramString, "Width", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoWidth, paramString);
	
	exParamValues widthValues;
	exportParamSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &widthValues);

	widthValues.rangeMin.intValue = 16;
	widthValues.rangeMax.intValue = 8192;

	exportParamSuite->ChangeParam(exID, gIdx, ADBEVideoWidth, &widthValues);
	
	
	// height
	utf16ncpy(paramString, "Height", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoHeight, paramString);
	
	exParamValues heightValues;
	exportParamSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &heightValues);

	heightValues.rangeMin.intValue = 16;
	heightValues.rangeMax.intValue = 8192;
	
	exportParamSuite->ChangeParam(exID, gIdx, ADBEVideoHeight, &heightValues);
	
	
	// pixel aspect ratio
	utf16ncpy(paramString, "Pixel Aspect Ratio", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoAspect, paramString);
	
	csSDK_int32	PARs[][2] = {{1, 1}, {10, 11}, {40, 33}, {768, 702}, 
							{1024, 702}, {2, 1}, {4, 3}, {3, 2}};
							
	const char *PARStrings[] = {"Square pixels (1.0)",
								"D1/DV NTSC (0.9091)",
								"D1/DV NTSC Widescreen 16:9 (1.2121)",
								"D1/DV PAL (1.0940)", 
								"D1/DV PAL Widescreen 16:9 (1.4587)",
								"Anamorphic 2:1 (2.0)",
								"HD Anamorphic 1080 (1.3333)",
								"DVCPRO HD (1.5)"};


	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEVideoAspect);
	
	exOneParamValueRec tempPAR;
	
	for(csSDK_int32 i=0; i < sizeof (PARs) / sizeof(PARs[0]); i++)
	{
		tempPAR.ratioValue.numerator = PARs[i][0];
		tempPAR.ratioValue.denominator = PARs[i][1];
		utf16ncpy(paramString, PARStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEVideoAspect, &tempPAR, paramString);
	}
	
	
	// render to sequence
	utf16ncpy(paramString, "Render to Sequence", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEStillSequence, paramString);
	
	
	// field type
	utf16ncpy(paramString, "Field Type", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoFieldType, paramString);
	
	csSDK_int32	fieldOrders[] = {	prFieldsUpperFirst,
									prFieldsLowerFirst,
									prFieldsNone};
	
	const char *fieldOrderStrings[]	= {	"Upper First",
										"Lower First",
										"None"};

	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEVideoFieldType);
	
	exOneParamValueRec tempFieldOrder;
	for(int i=0; i < 3; i++)
	{
		tempFieldOrder.intValue = fieldOrders[i];
		utf16ncpy(paramString, fieldOrderStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEVideoFieldType, &tempFieldOrder, paramString);
	}
	
	
	// frame rate
	utf16ncpy(paramString, "Frame Rate", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoFPS, paramString);
	
	PrTime frameRates[] = {	10, 15, 23,
							24, 25, 29,
							30, 48, 48,
							50, 59, 60};
													
	static const PrTime frameRateNumDens[][2] = {	{10, 1}, {15, 1}, {24000, 1001},
													{24, 1}, {25, 1}, {30000, 1001},
													{30, 1}, {48000, 1001}, {48, 1},
													{50, 1}, {60000, 1001}, {60, 1}};
	
	static const char *frameRateStrings[] = {	"10",
												"15",
												"23.976",
												"24",
												"25 (PAL)",
												"29.97 (NTSC)",
												"30",
												"47.952",
												"48",
												"50",
												"59.94",
												"60"};
	
	PrTime ticksPerSecond = 0;
	timeSuite->GetTicksPerSecond(&ticksPerSecond);
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		frameRates[i] = ticksPerSecond / frameRateNumDens[i][0] * frameRateNumDens[i][1];
	}
	
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEVideoFPS);
	
	exOneParamValueRec tempFrameRate;
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		tempFrameRate.timeValue = frameRates[i];
		utf16ncpy(paramString, frameRateStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEVideoFPS, &tempFrameRate, paramString);
	}
	
	
	// Alpha channel
	utf16ncpy(paramString, "Include Alpha Channel", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoAlpha, paramString);
	
	
	return result;
}


static prMALError
exSDKGetParamSummary(
	exportStdParms			*stdParmsP, 
	exParamSummaryRec		*summaryRecP)
{
	ExportSettings			*privateData	= reinterpret_cast<ExportSettings*>(summaryRecP->privateData);
	PrSDKExportParamSuite	*paramSuite		= privateData->exportParamSuite;
	
	string videoSummary, audioSummary, bitrateSummary;

	csSDK_uint32				exID			= summaryRecP->exporterPluginID;
	csSDK_int32					mgroupIndex		= 0;
	
	// Standard settings
	exParamValues width, height, frameRate, sequence, alpha;
	
	paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoWidth, &width);
	paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoHeight, &height);
	paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFPS, &frameRate);
	paramSuite->GetParamValue(exID, mgroupIndex, ADBEStillSequence, &sequence);
	paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAlpha, &alpha);
	

	// oh boy, figure out frame rate
	PrTime frameRates[] = {	10, 15, 23,
							24, 25, 29,
							30, 50, 59,
							60};
													
	PrTime frameRateNumDens[][2] = {{10, 1}, {15, 1}, {24000, 1001},
									{24, 1}, {25, 1}, {30000, 1001},
									{30, 1}, {50, 1}, {60000, 1001},
									{60, 1}};
	
	const char *frameRateStrings[] = {	"10",
										"15",
										"23.976",
										"24",
										"25",
										"29.97",
										"30",
										"50",
										"59.94",
										"60"};
	
	PrTime ticksPerSecond = 0;
	privateData->timeSuite->GetTicksPerSecond (&ticksPerSecond);
	
	csSDK_int32 frame_rate_index = -1;
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		frameRates[i] = ticksPerSecond / frameRateNumDens[i][0] * frameRateNumDens[i][1];
		
		if(frameRates[i] == frameRate.value.timeValue)
			frame_rate_index = i;
	}


	stringstream videoStream;
	
	videoStream << width.value.intValue << "x" << height.value.intValue;
	
	if(frame_rate_index >= 0 && sequence.value.intValue)
	{
		videoStream << ", " << frameRateStrings[frame_rate_index] << " fps";
	}
	else if(!sequence.value.intValue)
	{
		videoStream << ", Single Frame";
	}
	
	videoStream << ", " << (alpha.value.intValue ? "Alpha" : "No Alpha");
	
	
	videoSummary = videoStream.str();
	
	
	// EXR settings
	exParamValues compression, compressionLevel, floatNotHalf, lumiChrom, bypassLinear;
	
	bypassLinear.value.intValue = kPrFalse;
	
	paramSuite->GetParamValue(exID, mgroupIndex, EXRCompression, &compression);
	paramSuite->GetParamValue(exID, mgroupIndex, EXRCompressionLevel, &compressionLevel);
	paramSuite->GetParamValue(exID, mgroupIndex, EXRLumiChrom, &lumiChrom);
	paramSuite->GetParamValue(exID, mgroupIndex, EXRFloat, &floatNotHalf);
	paramSuite->GetParamValue(exID, mgroupIndex, EXRBypassLinear, &bypassLinear);
	
	switch(compression.value.intValue)
	{
		case Imf::NO_COMPRESSION:
			bitrateSummary = "No compression";
			break;

		case Imf::RLE_COMPRESSION:
			bitrateSummary = "RLE compression";
			break;

		case Imf::ZIPS_COMPRESSION:
			bitrateSummary = "Zip compression";
			break;

		case Imf::ZIP_COMPRESSION:
			bitrateSummary = "Zip16 compression";
			break;

		case Imf::PIZ_COMPRESSION:
			bitrateSummary = "Piz compression";
			break;

		case Imf::PXR24_COMPRESSION:
			bitrateSummary = "PXR24 compression";
			break;

		case Imf::B44_COMPRESSION:
			bitrateSummary = "B44 compression";
			break;
			
		case Imf::B44A_COMPRESSION:
			bitrateSummary = "B44A compression";
			break;

		case Imf::DWAA_COMPRESSION:
			bitrateSummary = "DWAA compression";
			break;

		case Imf::DWAB_COMPRESSION:
			bitrateSummary = "DWAB compression";
			break;

		default:
			bitrateSummary = "unknown compression!";
			break;
	}
	
	if(compression.value.intValue == Imf::DWAA_COMPRESSION ||
		compression.value.intValue == Imf::DWAB_COMPRESSION)
	{
		stringstream s;
		
		s << compressionLevel.value.floatValue;
		
		bitrateSummary += " (level " + s.str() + ")";
	}
	
	if(lumiChrom.value.intValue)
		bitrateSummary += ", Luminance/Chroma";
	else if(floatNotHalf.value.intValue)
		bitrateSummary += ", 32-bit float";

	if(bypassLinear.value.intValue)
		bitrateSummary += ", Bypass";

#if EXPORTMOD_VERSION >= 5
	utf16ncpy(summaryRecP->videoSummary, videoSummary.c_str(), 255);
	utf16ncpy(summaryRecP->audioSummary, audioSummary.c_str(), 255);
	utf16ncpy(summaryRecP->bitrateSummary, bitrateSummary.c_str(), 255);
#else
	utf16ncpy(summaryRecP->Summary1, videoSummary.c_str(), 255);
	utf16ncpy(summaryRecP->Summary2, audioSummary.c_str(), 255);
	utf16ncpy(summaryRecP->Summary3, bitrateSummary.c_str(), 255);
#endif
	
	return malNoError;
}


static prMALError
exSDKValidateParamChanged (
	exportStdParms		*stdParmsP, 
	exParamChangedRec	*validateParamChangedRecP)
{
	ExportSettings			*privateData	= reinterpret_cast<ExportSettings*>(validateParamChangedRecP->privateData);
	PrSDKExportParamSuite	*paramSuite		= privateData->exportParamSuite;
	
	csSDK_int32 exID = validateParamChangedRecP->exporterPluginID;
	csSDK_int32 gIdx = validateParamChangedRecP->multiGroupIndex;
	
	string param = validateParamChangedRecP->changedParamIdentifier;
	
	if(param == EXRCompression)
	{
		exParamValues compressionValue, compressionLevelValue;
		
		paramSuite->GetParamValue(exID, gIdx, EXRCompression, &compressionValue);
		paramSuite->GetParamValue(exID, gIdx, EXRCompressionLevel, &compressionLevelValue);
		
		const bool is_dwa = (compressionValue.value.intValue == Imf::DWAA_COMPRESSION ||
								compressionValue.value.intValue == Imf::DWAB_COMPRESSION);
		
		compressionLevelValue.hidden = (is_dwa ? kPrFalse : kPrTrue);
		
		paramSuite->ChangeParam(exID, gIdx, EXRCompressionLevel, &compressionLevelValue);
	}
	if(param == EXRLumiChrom)
	{
		exParamValues lumiChromValue, floatNotHalfValue;
		
		paramSuite->GetParamValue(exID, gIdx, EXRLumiChrom, &lumiChromValue);
		paramSuite->GetParamValue(exID, gIdx, EXRFloat, &floatNotHalfValue);
									
		floatNotHalfValue.disabled = (lumiChromValue.value.intValue ? kPrTrue : kPrFalse);
		
		paramSuite->ChangeParam(exID, gIdx, EXRFloat, &floatNotHalfValue);
	}
	else if(param == ADBEStillSequence)
	{
		exParamValues sequenceValue, frameRateValue;
		
		paramSuite->GetParamValue(exID, gIdx, ADBEStillSequence, &sequenceValue);
		paramSuite->GetParamValue(exID, gIdx, ADBEVideoFPS, &frameRateValue);
		
		frameRateValue.disabled = (sequenceValue.value.intValue ? kPrFalse : kPrTrue);
		
		paramSuite->ChangeParam(exID, gIdx, ADBEVideoFPS, &frameRateValue);
	}

	return malNoError;
}


DllExport PREMPLUGENTRY xSDKExport (
	csSDK_int32		selector, 
	exportStdParms	*stdParmsP, 
	void			*param1, 
	void			*param2)
{
	prMALError result = exportReturn_Unsupported;
	
	switch (selector)
	{
		case exSelStartup:
			result = exSDKStartup(	stdParmsP, 
									reinterpret_cast<exExporterInfoRec*>(param1));
			break;

		case exSelShutdown:
			result = exSDKShutdown();
			break;

		case exSelBeginInstance:
			result = exSDKBeginInstance(stdParmsP,
										reinterpret_cast<exExporterInstanceRec*>(param1));
			break;

		case exSelEndInstance:
			result = exSDKEndInstance(	stdParmsP,
										reinterpret_cast<exExporterInstanceRec*>(param1));
			break;

		case exSelGenerateDefaultParams:
			result = exSDKGenerateDefaultParams(stdParmsP,
												reinterpret_cast<exGenerateDefaultParamRec*>(param1));
			break;

		case exSelPostProcessParams:
			result = exSDKPostProcessParams(stdParmsP,
											reinterpret_cast<exPostProcessParamsRec*>(param1));
			break;

		case exSelGetParamSummary:
			result = exSDKGetParamSummary(	stdParmsP,
											reinterpret_cast<exParamSummaryRec*>(param1));
			break;

		case exSelQueryOutputSettings:
			result = exSDKQueryOutputSettings(	stdParmsP,
												reinterpret_cast<exQueryOutputSettingsRec*>(param1));
			break;

		case exSelQueryExportFileExtension:
			result = exSDKFileExtension(stdParmsP,
										reinterpret_cast<exQueryExportFileExtensionRec*>(param1));
			break;

		case exSelValidateParamChanged:
			result = exSDKValidateParamChanged(	stdParmsP,
												reinterpret_cast<exParamChangedRec*>(param1));
			break;

		case exSelValidateOutputSettings:
			result = malNoError;
			break;

		case exSelQueryStillSequence:
			result = exSDKQueryStillSequence(	stdParmsP,
												reinterpret_cast<exQueryStillSequenceRec*>(param1));
			break;

		case exSelExport:
			result = exSDKExport(	stdParmsP,
									reinterpret_cast<exDoExportRec*>(param1));
			break;
	}
	
	return result;
}
