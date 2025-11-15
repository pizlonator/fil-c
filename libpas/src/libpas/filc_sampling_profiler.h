/*
 * Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef FILC_SAMPLING_PROFILER_H
#define FILC_SAMPLING_PROFILER_H

#include "filc_runtime.h"

PAS_BEGIN_EXTERN_C;

PAS_API extern bool filc_sampling_profiler_enabled;
PAS_API extern unsigned filc_sampling_profiler_period_us;

PAS_API void filc_sampling_profiler_parse_settings(void);
PAS_API void filc_sampling_profiler_dump_settings(void);
PAS_API void filc_sampling_profiler_initialize(void);
PAS_API void filc_sampling_profiler_before_fork(void);
PAS_API void filc_sampling_profiler_after_fork_in_parent(void);
PAS_API void filc_sampling_profiler_after_fork_in_child(void);
PAS_API void filc_sampling_profiler_report(void);

PAS_END_EXTERN_C;

#endif /* FILC_SAMPLING_PROFILER_H */

