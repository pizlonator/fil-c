/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include "vpl/mfx_dispatcher_vpl_log.h"

DispatcherLogVPL::DispatcherLogVPL() : m_logLevel(0), m_logFileName(), m_logFile(nullptr) {}

DispatcherLogVPL::~DispatcherLogVPL() {
    if (!m_logFileName.empty() && m_logFile)
        fclose(m_logFile);
    m_logFile = nullptr;
}

mfxStatus DispatcherLogVPL::Init(mfxU32 logLevel, const std::string &logFileName) {
    // avoid leaking file handle if Init is accidentally called more than once
    if (m_logFile)
        return MFX_ERR_UNSUPPORTED;

    m_logLevel    = logLevel;
    m_logFileName = logFileName;

    // append to file if it already exists, otherwise create a new one
    // m_logFile will be closed in dtor
    if (m_logLevel) {
        if (m_logFileName.empty()) {
            m_logFile = stdout;
        }
        else {
#if defined(_WIN32) || defined(_WIN64)
            fopen_s(&m_logFile, m_logFileName.c_str(), "a");
#else
            m_logFile = fopen(m_logFileName.c_str(), "a");
#endif
            if (!m_logFile) {
                m_logFile = stdout;
                fprintf(m_logFile,
                        "Warning - unable to create logfile %s\n",
                        m_logFileName.c_str());
                fprintf(m_logFile, "Log output will be sent to stdout\n");
                m_logFileName.clear();
            }
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus DispatcherLogVPL::LogMessage(const char *msg, ...) {
    if (!m_logLevel || !m_logFile)
        return MFX_ERR_NONE;

    va_list args;
    va_start(args, msg);
    vfprintf(m_logFile, msg, args);
    va_end(args);

    fprintf(m_logFile, "\n");

    return MFX_ERR_NONE;
}
