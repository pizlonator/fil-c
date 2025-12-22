/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include "vpl/mfx_dispatcher_vpl.h"

// exported functions for API >= 2.0

// create unique loader context
mfxLoader MFXLoad() {
    LoaderCtxVPL *loaderCtx;

    try {
        std::unique_ptr<LoaderCtxVPL> pLoaderCtx;
        pLoaderCtx.reset(new LoaderCtxVPL{});
        loaderCtx = (LoaderCtxVPL *)pLoaderCtx.release();
    }
    catch (...) {
        return nullptr;
    }

    // initialize logging if appropriate environment variables are set
    loaderCtx->InitDispatcherLog();

    return (mfxLoader)loaderCtx;
}

// unload libraries, destroy all created mfxConfig objects, free other memory
void MFXUnload(mfxLoader loader) {
    if (loader) {
        LoaderCtxVPL *loaderCtx = (LoaderCtxVPL *)loader;

        loaderCtx->UnloadAllLibraries();

        loaderCtx->FreeConfigFilters();

        delete loaderCtx;
    }

    return;
}

// create config context
// each loader may have more than one config context
mfxConfig MFXCreateConfig(mfxLoader loader) {
    if (!loader)
        return nullptr;

    LoaderCtxVPL *loaderCtx = (LoaderCtxVPL *)loader;
    ConfigCtxVPL *configCtx;

    DispatcherLogVPL *dispLog = loaderCtx->GetLogger();
    DISP_LOG_FUNCTION(dispLog);

    try {
        configCtx = loaderCtx->AddConfigFilter();
    }
    catch (...) {
        return nullptr;
    }

    return (mfxConfig)(configCtx);
}

// set a config proprerty to use in enumerating implementations
mfxStatus MFXSetConfigFilterProperty(mfxConfig config, const mfxU8 *name, mfxVariant value) {
    if (!config)
        return MFX_ERR_NULL_PTR;

    ConfigCtxVPL *configCtx = (ConfigCtxVPL *)config;
    LoaderCtxVPL *loaderCtx = configCtx->m_parentLoader;

    DispatcherLogVPL *dispLog = loaderCtx->GetLogger();
    DISP_LOG_FUNCTION(dispLog);

    mfxStatus sts = configCtx->SetFilterProperty(name, value);
    if (sts)
        return sts;

    loaderCtx->m_bNeedUpdateValidImpls = true;

    sts = loaderCtx->UpdateLowLatency();

    return sts;
}

// iterate over available implementations
// capabilities are returned in idesc
mfxStatus MFXEnumImplementations(mfxLoader loader,
                                 mfxU32 i,
                                 mfxImplCapsDeliveryFormat format,
                                 mfxHDL *idesc) {
    if (!loader || !idesc)
        return MFX_ERR_NULL_PTR;

    LoaderCtxVPL *loaderCtx = (LoaderCtxVPL *)loader;

    DispatcherLogVPL *dispLog = loaderCtx->GetLogger();
    DISP_LOG_FUNCTION(dispLog);

    mfxStatus sts = MFX_ERR_NONE;

    // load and query all libraries
    if (loaderCtx->m_bNeedFullQuery) {
        // if a session was already created in low-latency mode, unload all implementations
        //   before running full load and query
        if (loaderCtx->m_bLowLatency && !loaderCtx->m_bNeedLowLatencyQuery) {
            loaderCtx->UnloadAllLibraries();
        }

        sts = loaderCtx->FullLoadAndQuery();
        if (sts)
            return MFX_ERR_NOT_FOUND;
    }

    // update list of valid libraries based on updated set of
    //   mfxConfig properties
    if (loaderCtx->m_bNeedUpdateValidImpls) {
        sts = loaderCtx->UpdateValidImplList();
        if (sts)
            return MFX_ERR_NOT_FOUND;
    }

    sts = loaderCtx->QueryImpl(i, format, idesc);

    return sts;
}

// create a new session with implementation i
mfxStatus MFXCreateSession(mfxLoader loader, mfxU32 i, mfxSession *session) {
    if (!loader || !session)
        return MFX_ERR_NULL_PTR;

    LoaderCtxVPL *loaderCtx = (LoaderCtxVPL *)loader;

    DispatcherLogVPL *dispLog = loaderCtx->GetLogger();
    DISP_LOG_FUNCTION(dispLog);

    mfxStatus sts = MFX_ERR_NONE;

    if (loaderCtx->m_bLowLatency) {
        DISP_LOG_MESSAGE(dispLog, "message:  low latency mode enabled");

        if (loaderCtx->m_bNeedLowLatencyQuery) {
            // load low latency libraries
            sts = loaderCtx->LoadLibsLowLatency();
            if (sts != MFX_ERR_NONE)
                return MFX_ERR_NOT_FOUND;

            // run limited query operations for low latency init
            sts = loaderCtx->QueryLibraryCaps();
            if (sts != MFX_ERR_NONE)
                return MFX_ERR_NOT_FOUND;
        }
    }
    else {
        DISP_LOG_MESSAGE(dispLog, "message:  low latency mode disabled");

        // load and query all libraries
        if (loaderCtx->m_bNeedFullQuery) {
            sts = loaderCtx->FullLoadAndQuery();
            if (sts)
                return MFX_ERR_NOT_FOUND;
        }

        // update list of valid libraries based on updated set of
        //   mfxConfig properties
        if (loaderCtx->m_bNeedUpdateValidImpls) {
            sts = loaderCtx->UpdateValidImplList();
            if (sts)
                return MFX_ERR_NOT_FOUND;
        }
    }

    sts = loaderCtx->CreateSession(i, session);

    return sts;
}

// release memory associated with implementation description hdl
mfxStatus MFXDispReleaseImplDescription(mfxLoader loader, mfxHDL hdl) {
    if (!loader)
        return MFX_ERR_NULL_PTR;

    LoaderCtxVPL *loaderCtx = (LoaderCtxVPL *)loader;

    DispatcherLogVPL *dispLog = loaderCtx->GetLogger();
    DISP_LOG_FUNCTION(dispLog);

    mfxStatus sts = loaderCtx->ReleaseImpl(hdl);

    return sts;
}
