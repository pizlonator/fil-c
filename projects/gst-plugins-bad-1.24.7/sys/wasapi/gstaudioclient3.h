/*
 * Structure and enum definitions are from audioclient.h in the Windows 10 SDK
 *
 * These should be defined by MinGW, but they aren't yet since they're very new
 * so we keep a copy in our tree. All definitions are guarded, so it should be
 * fine to always include this even when building with MSVC.
 */
#pragma once

#ifndef __IAudioClient3_FWD_DEFINED__
#define __IAudioClient3_FWD_DEFINED__
typedef interface IAudioClient3 IAudioClient3;

#endif 	/* __IAudioClient3_FWD_DEFINED__ */

#ifndef __IAudioClient3_INTERFACE_DEFINED__
#define __IAudioClient3_INTERFACE_DEFINED__

#ifndef HAVE_AUDCLNT_STREAMOPTIONS
typedef enum AUDCLNT_STREAMOPTIONS
{
    AUDCLNT_STREAMOPTIONS_NONE	        = 0,
    AUDCLNT_STREAMOPTIONS_RAW	        = 0x1,
    AUDCLNT_STREAMOPTIONS_MATCH_FORMAT	= 0x2
} AUDCLNT_STREAMOPTIONS;
#endif

/* These should be available when the IAudioClient2 interface is defined */
#ifndef __IAudioClient2_FWD_DEFINED__
typedef enum _AUDIO_STREAM_CATEGORY {
  AudioCategory_Other                   = 0,
  AudioCategory_ForegroundOnlyMedia,
  AudioCategory_BackgroundCapableMedia,
  AudioCategory_Communications,
  AudioCategory_Alerts,
  AudioCategory_SoundEffects,
  AudioCategory_GameEffects,
  AudioCategory_GameMedia,
  AudioCategory_GameChat,
  AudioCategory_Speech,
  AudioCategory_Movie,
  AudioCategory_Media
} AUDIO_STREAM_CATEGORY;

typedef struct AudioClientProperties
{
    UINT32 cbSize;
    BOOL bIsOffload;
    AUDIO_STREAM_CATEGORY eCategory;
    AUDCLNT_STREAMOPTIONS Options;
} AudioClientProperties;
#endif /* __IAudioClient2_FWD_DEFINED__ */

EXTERN_C const IID IID_IAudioClient3;

typedef struct IAudioClient3Vtbl
{
    BEGIN_INTERFACE

    HRESULT ( STDMETHODCALLTYPE *QueryInterface )(
        IAudioClient3 * This,
        REFIID riid,
        void **ppvObject);

    ULONG ( STDMETHODCALLTYPE *AddRef )(
        IAudioClient3 * This);

    ULONG ( STDMETHODCALLTYPE *Release )(
        IAudioClient3 * This);

    HRESULT ( STDMETHODCALLTYPE *Initialize )(
        IAudioClient3 * This,
        AUDCLNT_SHAREMODE ShareMode,
        DWORD StreamFlags,
        REFERENCE_TIME hnsBufferDuration,
        REFERENCE_TIME hnsPeriodicity,
        const WAVEFORMATEX *pFormat,
        LPCGUID AudioSessionGuid);

    HRESULT ( STDMETHODCALLTYPE *GetBufferSize )(
        IAudioClient3 * This,
        UINT32 *pNumBufferFrames);

    HRESULT ( STDMETHODCALLTYPE *GetStreamLatency )(
        IAudioClient3 * This,
        REFERENCE_TIME *phnsLatency);

    HRESULT ( STDMETHODCALLTYPE *GetCurrentPadding )(
        IAudioClient3 * This,
        UINT32 *pNumPaddingFrames);

    HRESULT ( STDMETHODCALLTYPE *IsFormatSupported )(
        IAudioClient3 * This,
        AUDCLNT_SHAREMODE ShareMode,
        const WAVEFORMATEX *pFormat,
        WAVEFORMATEX **ppClosestMatch);

    HRESULT ( STDMETHODCALLTYPE *GetMixFormat )(
        IAudioClient3 * This,
        WAVEFORMATEX **ppDeviceFormat);

    HRESULT ( STDMETHODCALLTYPE *GetDevicePeriod )(
        IAudioClient3 * This,
        REFERENCE_TIME *phnsDefaultDevicePeriod,
        REFERENCE_TIME *phnsMinimumDevicePeriod);

    HRESULT ( STDMETHODCALLTYPE *Start )(
        IAudioClient3 * This);

    HRESULT ( STDMETHODCALLTYPE *Stop )(
        IAudioClient3 * This);

    HRESULT ( STDMETHODCALLTYPE *Reset )(
        IAudioClient3 * This);

    HRESULT ( STDMETHODCALLTYPE *SetEventHandle )(
        IAudioClient3 * This,
        HANDLE eventHandle);

    HRESULT ( STDMETHODCALLTYPE *GetService )(
        IAudioClient3 * This,
        REFIID riid,
        void **ppv);

    HRESULT ( STDMETHODCALLTYPE *IsOffloadCapable )(
        IAudioClient3 * This,
        AUDIO_STREAM_CATEGORY Category,
        BOOL *pbOffloadCapable);

    HRESULT ( STDMETHODCALLTYPE *SetClientProperties )(
        IAudioClient3 * This,
        const AudioClientProperties *pProperties);

    HRESULT ( STDMETHODCALLTYPE *GetBufferSizeLimits )(
        IAudioClient3 * This,
        const WAVEFORMATEX *pFormat,
        BOOL bEventDriven,
        REFERENCE_TIME *phnsMinBufferDuration,
        REFERENCE_TIME *phnsMaxBufferDuration);

    HRESULT ( STDMETHODCALLTYPE *GetSharedModeEnginePeriod )(
        IAudioClient3 * This,
        const WAVEFORMATEX *pFormat,
        UINT32 *pDefaultPeriodInFrames,
        UINT32 *pFundamentalPeriodInFrames,
        UINT32 *pMinPeriodInFrames,
        UINT32 *pMaxPeriodInFrames);

    HRESULT ( STDMETHODCALLTYPE *GetCurrentSharedModeEnginePeriod )(
        IAudioClient3 * This,
        WAVEFORMATEX **ppFormat,
        UINT32 *pCurrentPeriodInFrames);

    HRESULT ( STDMETHODCALLTYPE *InitializeSharedAudioStream )(
        IAudioClient3 * This,
        DWORD StreamFlags,
        UINT32 PeriodInFrames,
        const WAVEFORMATEX *pFormat,
        LPCGUID AudioSessionGuid);

    END_INTERFACE
} IAudioClient3Vtbl;

interface IAudioClient3
{
    CONST_VTBL struct IAudioClient3Vtbl *lpVtbl;
};

#define IAudioClient3_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) )

#define IAudioClient3_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) )

#define IAudioClient3_Release(This)	\
    ( (This)->lpVtbl -> Release(This) )


#define IAudioClient3_Initialize(This,ShareMode,StreamFlags,hnsBufferDuration,hnsPeriodicity,pFormat,AudioSessionGuid)	\
    ( (This)->lpVtbl -> Initialize(This,ShareMode,StreamFlags,hnsBufferDuration,hnsPeriodicity,pFormat,AudioSessionGuid) )

#define IAudioClient3_GetBufferSize(This,pNumBufferFrames)	\
    ( (This)->lpVtbl -> GetBufferSize(This,pNumBufferFrames) )

#define IAudioClient3_GetStreamLatency(This,phnsLatency)	\
    ( (This)->lpVtbl -> GetStreamLatency(This,phnsLatency) )

#define IAudioClient3_GetCurrentPadding(This,pNumPaddingFrames)	\
    ( (This)->lpVtbl -> GetCurrentPadding(This,pNumPaddingFrames) )

#define IAudioClient3_IsFormatSupported(This,ShareMode,pFormat,ppClosestMatch)	\
    ( (This)->lpVtbl -> IsFormatSupported(This,ShareMode,pFormat,ppClosestMatch) )

#define IAudioClient3_GetMixFormat(This,ppDeviceFormat)	\
    ( (This)->lpVtbl -> GetMixFormat(This,ppDeviceFormat) )

#define IAudioClient3_GetDevicePeriod(This,phnsDefaultDevicePeriod,phnsMinimumDevicePeriod)	\
    ( (This)->lpVtbl -> GetDevicePeriod(This,phnsDefaultDevicePeriod,phnsMinimumDevicePeriod) )

#define IAudioClient3_Start(This)	\
    ( (This)->lpVtbl -> Start(This) )

#define IAudioClient3_Stop(This)	\
    ( (This)->lpVtbl -> Stop(This) )

#define IAudioClient3_Reset(This)	\
    ( (This)->lpVtbl -> Reset(This) )

#define IAudioClient3_SetEventHandle(This,eventHandle)	\
    ( (This)->lpVtbl -> SetEventHandle(This,eventHandle) )

#define IAudioClient3_GetService(This,riid,ppv)	\
    ( (This)->lpVtbl -> GetService(This,riid,ppv) )


#define IAudioClient3_IsOffloadCapable(This,Category,pbOffloadCapable)	\
    ( (This)->lpVtbl -> IsOffloadCapable(This,Category,pbOffloadCapable) )

#define IAudioClient3_SetClientProperties(This,pProperties)	\
    ( (This)->lpVtbl -> SetClientProperties(This,pProperties) )

#define IAudioClient3_GetBufferSizeLimits(This,pFormat,bEventDriven,phnsMinBufferDuration,phnsMaxBufferDuration)	\
    ( (This)->lpVtbl -> GetBufferSizeLimits(This,pFormat,bEventDriven,phnsMinBufferDuration,phnsMaxBufferDuration) )


#define IAudioClient3_GetSharedModeEnginePeriod(This,pFormat,pDefaultPeriodInFrames,pFundamentalPeriodInFrames,pMinPeriodInFrames,pMaxPeriodInFrames)	\
    ( (This)->lpVtbl -> GetSharedModeEnginePeriod(This,pFormat,pDefaultPeriodInFrames,pFundamentalPeriodInFrames,pMinPeriodInFrames,pMaxPeriodInFrames) )

#define IAudioClient3_GetCurrentSharedModeEnginePeriod(This,ppFormat,pCurrentPeriodInFrames)	\
    ( (This)->lpVtbl -> GetCurrentSharedModeEnginePeriod(This,ppFormat,pCurrentPeriodInFrames) )

#define IAudioClient3_InitializeSharedAudioStream(This,StreamFlags,PeriodInFrames,pFormat,AudioSessionGuid)	\
    ( (This)->lpVtbl -> InitializeSharedAudioStream(This,StreamFlags,PeriodInFrames,pFormat,AudioSessionGuid) )


#endif 	/* __IAudioClient3_INTERFACE_DEFINED__ */
