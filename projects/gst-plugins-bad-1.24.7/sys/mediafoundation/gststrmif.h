/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha.yang@navercorp.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_STRMIF_H__
#define __GST_STRMIF_H__

#include <strmif.h>

/* From strmif.h.
 * ICodecAPI interface will not be exposed
 * for the !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) case
 * but MSDN said the interface should be available on both
 * desktop and UWP cases */
#ifndef __ICodecAPI_INTERFACE_DEFINED__
#define __ICodecAPI_INTERFACE_DEFINED__

/* interface ICodecAPI */
/* [unique][uuid][object][local] */


EXTERN_C const IID IID_ICodecAPI;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("901db4c7-31ce-41a2-85dc-8fa0bf41b8da")
    ICodecAPI : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE IsSupported(
            /* [in] */ const GUID *Api) = 0;

        virtual HRESULT STDMETHODCALLTYPE IsModifiable(
            /* [in] */ const GUID *Api) = 0;

        virtual HRESULT STDMETHODCALLTYPE GetParameterRange(
            /* [in] */ const GUID *Api,
            /* [annotation][out] */
            _Out_  VARIANT *ValueMin,
            /* [annotation][out] */
            _Out_  VARIANT *ValueMax,
            /* [annotation][out] */
            _Out_  VARIANT *SteppingDelta) = 0;

        virtual HRESULT STDMETHODCALLTYPE GetParameterValues(
            /* [in] */ const GUID *Api,
            /* [annotation][size_is][size_is][out] */
            _Outptr_result_buffer_all_(*ValuesCount)  VARIANT **Values,
            /* [annotation][out] */
            _Out_  ULONG *ValuesCount) = 0;

        virtual HRESULT STDMETHODCALLTYPE GetDefaultValue(
            /* [in] */ const GUID *Api,
            /* [annotation][out] */
            _Out_  VARIANT *Value) = 0;

        virtual HRESULT STDMETHODCALLTYPE GetValue(
            /* [in] */ const GUID *Api,
            /* [annotation][out] */
            _Out_  VARIANT *Value) = 0;

        virtual HRESULT STDMETHODCALLTYPE SetValue(
            /* [in] */ const GUID *Api,
            /* [annotation][in] */
            _In_  VARIANT *Value) = 0;

        virtual HRESULT STDMETHODCALLTYPE RegisterForEvent(
            /* [in] */ const GUID *Api,
            /* [in] */ LONG_PTR userData) = 0;

        virtual HRESULT STDMETHODCALLTYPE UnregisterForEvent(
            /* [in] */ const GUID *Api) = 0;

        virtual HRESULT STDMETHODCALLTYPE SetAllDefaults( void) = 0;

        virtual HRESULT STDMETHODCALLTYPE SetValueWithNotify(
            /* [in] */ const GUID *Api,
            /* [in] */ VARIANT *Value,
            /* [annotation][size_is][size_is][out] */
            _Outptr_result_buffer_all_(*ChangedParamCount)  GUID **ChangedParam,
            /* [annotation][out] */
            _Out_  ULONG *ChangedParamCount) = 0;

        virtual HRESULT STDMETHODCALLTYPE SetAllDefaultsWithNotify(
            /* [annotation][size_is][size_is][out] */
            _Outptr_result_buffer_all_(*ChangedParamCount)  GUID **ChangedParam,
            /* [annotation][out] */
            _Out_  ULONG *ChangedParamCount) = 0;

        virtual HRESULT STDMETHODCALLTYPE GetAllSettings(
            /* [in] */ IStream *__MIDL__ICodecAPI0000) = 0;

        virtual HRESULT STDMETHODCALLTYPE SetAllSettings(
            /* [in] */ IStream *__MIDL__ICodecAPI0001) = 0;

        virtual HRESULT STDMETHODCALLTYPE SetAllSettingsWithNotify(
            IStream *__MIDL__ICodecAPI0002,
            /* [annotation][size_is][size_is][out] */
            _Outptr_result_buffer_all_(*ChangedParamCount)  GUID **ChangedParam,
            /* [annotation][out] */
            _Out_  ULONG *ChangedParamCount) = 0;

    };


#else 	/* C style interface */

    typedef struct ICodecAPIVtbl
    {
        BEGIN_INTERFACE

        HRESULT ( STDMETHODCALLTYPE *QueryInterface )(
            ICodecAPI * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */
            _COM_Outptr_  void **ppvObject);

        ULONG ( STDMETHODCALLTYPE *AddRef )(
            ICodecAPI * This);

        ULONG ( STDMETHODCALLTYPE *Release )(
            ICodecAPI * This);

        HRESULT ( STDMETHODCALLTYPE *IsSupported )(
            ICodecAPI * This,
            /* [in] */ const GUID *Api);

        HRESULT ( STDMETHODCALLTYPE *IsModifiable )(
            ICodecAPI * This,
            /* [in] */ const GUID *Api);

        HRESULT ( STDMETHODCALLTYPE *GetParameterRange )(
            ICodecAPI * This,
            /* [in] */ const GUID *Api,
            /* [annotation][out] */
            _Out_  VARIANT *ValueMin,
            /* [annotation][out] */
            _Out_  VARIANT *ValueMax,
            /* [annotation][out] */
            _Out_  VARIANT *SteppingDelta);

        HRESULT ( STDMETHODCALLTYPE *GetParameterValues )(
            ICodecAPI * This,
            /* [in] */ const GUID *Api,
            /* [annotation][size_is][size_is][out] */
            _Outptr_result_buffer_all_(*ValuesCount)  VARIANT **Values,
            /* [annotation][out] */
            _Out_  ULONG *ValuesCount);

        HRESULT ( STDMETHODCALLTYPE *GetDefaultValue )(
            ICodecAPI * This,
            /* [in] */ const GUID *Api,
            /* [annotation][out] */
            _Out_  VARIANT *Value);

        HRESULT ( STDMETHODCALLTYPE *GetValue )(
            ICodecAPI * This,
            /* [in] */ const GUID *Api,
            /* [annotation][out] */
            _Out_  VARIANT *Value);

        HRESULT ( STDMETHODCALLTYPE *SetValue )(
            ICodecAPI * This,
            /* [in] */ const GUID *Api,
            /* [annotation][in] */
            _In_  VARIANT *Value);

        HRESULT ( STDMETHODCALLTYPE *RegisterForEvent )(
            ICodecAPI * This,
            /* [in] */ const GUID *Api,
            /* [in] */ LONG_PTR userData);

        HRESULT ( STDMETHODCALLTYPE *UnregisterForEvent )(
            ICodecAPI * This,
            /* [in] */ const GUID *Api);

        HRESULT ( STDMETHODCALLTYPE *SetAllDefaults )(
            ICodecAPI * This);

        HRESULT ( STDMETHODCALLTYPE *SetValueWithNotify )(
            ICodecAPI * This,
            /* [in] */ const GUID *Api,
            /* [in] */ VARIANT *Value,
            /* [annotation][size_is][size_is][out] */
            _Outptr_result_buffer_all_(*ChangedParamCount)  GUID **ChangedParam,
            /* [annotation][out] */
            _Out_  ULONG *ChangedParamCount);

        HRESULT ( STDMETHODCALLTYPE *SetAllDefaultsWithNotify )(
            ICodecAPI * This,
            /* [annotation][size_is][size_is][out] */
            _Outptr_result_buffer_all_(*ChangedParamCount)  GUID **ChangedParam,
            /* [annotation][out] */
            _Out_  ULONG *ChangedParamCount);

        HRESULT ( STDMETHODCALLTYPE *GetAllSettings )(
            ICodecAPI * This,
            /* [in] */ IStream *__MIDL__ICodecAPI0000);

        HRESULT ( STDMETHODCALLTYPE *SetAllSettings )(
            ICodecAPI * This,
            /* [in] */ IStream *__MIDL__ICodecAPI0001);

        HRESULT ( STDMETHODCALLTYPE *SetAllSettingsWithNotify )(
            ICodecAPI * This,
            IStream *__MIDL__ICodecAPI0002,
            /* [annotation][size_is][size_is][out] */
            _Outptr_result_buffer_all_(*ChangedParamCount)  GUID **ChangedParam,
            /* [annotation][out] */
            _Out_  ULONG *ChangedParamCount);

        END_INTERFACE
    } ICodecAPIVtbl;

    interface ICodecAPI
    {
        CONST_VTBL struct ICodecAPIVtbl *lpVtbl;
    };



#ifdef COBJMACROS


#define ICodecAPI_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) )

#define ICodecAPI_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) )

#define ICodecAPI_Release(This)	\
    ( (This)->lpVtbl -> Release(This) )


#define ICodecAPI_IsSupported(This,Api)	\
    ( (This)->lpVtbl -> IsSupported(This,Api) )

#define ICodecAPI_IsModifiable(This,Api)	\
    ( (This)->lpVtbl -> IsModifiable(This,Api) )

#define ICodecAPI_GetParameterRange(This,Api,ValueMin,ValueMax,SteppingDelta)	\
    ( (This)->lpVtbl -> GetParameterRange(This,Api,ValueMin,ValueMax,SteppingDelta) )

#define ICodecAPI_GetParameterValues(This,Api,Values,ValuesCount)	\
    ( (This)->lpVtbl -> GetParameterValues(This,Api,Values,ValuesCount) )

#define ICodecAPI_GetDefaultValue(This,Api,Value)	\
    ( (This)->lpVtbl -> GetDefaultValue(This,Api,Value) )

#define ICodecAPI_GetValue(This,Api,Value)	\
    ( (This)->lpVtbl -> GetValue(This,Api,Value) )

#define ICodecAPI_SetValue(This,Api,Value)	\
    ( (This)->lpVtbl -> SetValue(This,Api,Value) )

#define ICodecAPI_RegisterForEvent(This,Api,userData)	\
    ( (This)->lpVtbl -> RegisterForEvent(This,Api,userData) )

#define ICodecAPI_UnregisterForEvent(This,Api)	\
    ( (This)->lpVtbl -> UnregisterForEvent(This,Api) )

#define ICodecAPI_SetAllDefaults(This)	\
    ( (This)->lpVtbl -> SetAllDefaults(This) )

#define ICodecAPI_SetValueWithNotify(This,Api,Value,ChangedParam,ChangedParamCount)	\
    ( (This)->lpVtbl -> SetValueWithNotify(This,Api,Value,ChangedParam,ChangedParamCount) )

#define ICodecAPI_SetAllDefaultsWithNotify(This,ChangedParam,ChangedParamCount)	\
    ( (This)->lpVtbl -> SetAllDefaultsWithNotify(This,ChangedParam,ChangedParamCount) )

#define ICodecAPI_GetAllSettings(This,__MIDL__ICodecAPI0000)	\
    ( (This)->lpVtbl -> GetAllSettings(This,__MIDL__ICodecAPI0000) )

#define ICodecAPI_SetAllSettings(This,__MIDL__ICodecAPI0001)	\
    ( (This)->lpVtbl -> SetAllSettings(This,__MIDL__ICodecAPI0001) )

#define ICodecAPI_SetAllSettingsWithNotify(This,__MIDL__ICodecAPI0002,ChangedParam,ChangedParamCount)	\
    ( (This)->lpVtbl -> SetAllSettingsWithNotify(This,__MIDL__ICodecAPI0002,ChangedParam,ChangedParamCount) )

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ICodecAPI_INTERFACE_DEFINED__ */

#endif /* __GST_STRMIF_H__ */