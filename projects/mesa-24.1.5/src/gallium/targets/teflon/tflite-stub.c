/*
 * Copyright (c) 2023-2024 Tomeu Vizoso <tomeu@tomeuvizoso.net>
 * SPDX-License-Identifier: MIT
 */

#include "tensorflow/lite/c/c_api.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/core/c/c_api.h"

void
TfLiteInterpreterOptionsAddDelegate(TfLiteInterpreterOptions *options, TfLiteOpaqueDelegate *delegate)
{
}

void
TfLiteInterpreterOptionsSetErrorReporter(
   TfLiteInterpreterOptions *options,
   void (*reporter)(void *user_data, const char *format, va_list args),
   void *user_data)
{
}

TfLiteInterpreter *
TfLiteInterpreterCreate(
   const TfLiteModel *model,
   const TfLiteInterpreterOptions *optional_options)
{
   return NULL;
}

TfLiteStatus
TfLiteInterpreterAllocateTensors(TfLiteInterpreter *interpreter)
{
   return 0;
}

int32_t
TfLiteInterpreterGetInputTensorCount(const TfLiteInterpreter *interpreter)
{
   return 0;
}

TfLiteTensor *
TfLiteInterpreterGetInputTensor(const TfLiteInterpreter *interpreter, int32_t input_index)
{
   return NULL;
}

TfLiteStatus
TfLiteTensorCopyFromBuffer(TfLiteTensor *tensor,
                           const void *input_data,
                           size_t input_data_size)
{
   return 0;
}

TfLiteStatus
TfLiteInterpreterInvoke(TfLiteInterpreter *interpreter)
{
   return 0;
}

int32_t
TfLiteInterpreterGetOutputTensorCount(const TfLiteInterpreter *interpreter)
{
   return 0;
}

const TfLiteTensor *
TfLiteInterpreterGetOutputTensor(const TfLiteInterpreter *interpreter, int32_t output_index)
{
   return NULL;
}

TfLiteStatus
TfLiteTensorCopyToBuffer(const TfLiteTensor *tensor,
                         void *output_data,
                         size_t output_data_size)
{
   return 0;
}

void
TfLiteInterpreterDelete(TfLiteInterpreter *interpreter)
{
}

void
TfLiteInterpreterOptionsDelete(TfLiteInterpreterOptions *options)
{
}

TfLiteModel *
TfLiteModelCreate(const void *model_data, size_t model_size)
{
   return NULL;
}

void
TfLiteModelDelete(TfLiteModel *model)
{
}

/* FIXME: Why do we need to redeclare the prototype for this one here? */
TfLiteInterpreterOptions *TfLiteInterpreterOptionsCreate(void);

TfLiteInterpreterOptions *
TfLiteInterpreterOptionsCreate(void)
{
   return NULL;
}
