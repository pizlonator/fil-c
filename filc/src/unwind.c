/*
 * Copyright (c) 2024 Epic Games, Inc. All Rights Reserved.
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

#include <unwind.h>
#include <stdfil.h>

#define NUM_REGISTERS 2

struct _Unwind_Context {
    const void* language_specific_data;
    void* registers[NUM_REGISTERS];
};

void _Unwind_Resume(_Unwind_Exception *exception_object)
{
    (void)exception_object;
    zerror("Not implemented - compiler calls filc_unwind_resume instead.");
}

void _Unwind_DeleteException(_Unwind_Exception *exception_object)
{
    if (!exception_object->exception_cleanup)
        return;
    exception_object->exception_cleanup(_URC_FOREIGN_EXCEPTION_CAUGHT, exception_object);
}

void* _Unwind_GetGR(struct _Unwind_Context *context, int index)
{
    return context->registers[index];
}

void _Unwind_SetGR(struct _Unwind_Context *context, int index,
                   void* new_value)
{
    context->registers[index] = new_value;
}

unsigned long _Unwind_GetIP(struct _Unwind_Context *context)
{
    (void)context;
    zerror("Not implemented.");
    return 0;
}

void _Unwind_SetIP(struct _Unwind_Context *context, unsigned long new_value)
{
    (void)context;
    (void)new_value;
    zerror("Not implemented.");
}

unsigned long _Unwind_GetRegionStart(struct _Unwind_Context *context)
{
    (void)context;
    zerror("Not implemented.");
    return 0;
}

const void* _Unwind_GetLanguageSpecificData(struct _Unwind_Context *context)
{
    return context->language_specific_data;
}

_Unwind_Reason_Code _Unwind_Backtrace(_Unwind_Trace_Fn fn, void *arg)
{
    (void)fn;
    (void)arg;
    zerror("Not implemented.");
    return _URC_NO_REASON;
}
