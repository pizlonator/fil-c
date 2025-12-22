/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#pragma once

#ifdef HLSL_PRECOMPILED
#include "PSMain_checker_luma.h"
#include "PSMain_checker_rgb.h"
#include "PSMain_checker_vuya.h"
#include "PSMain_checker.h"
#include "PSMain_color.h"
#include "PSMain_sample_premul.h"
#include "PSMain_sample.h"
#include "PSMain_snow.h"
#include "VSMain_color.h"
#include "VSMain_coord.h"
#include "VSMain_pos.h"
#else
const BYTE g_PSMain_checker_luma[] = { 0 };
const BYTE g_PSMain_checker_rgb[] = { 0 };
const BYTE g_PSMain_checker_vuya[] = { 0 };
const BYTE g_PSMain_checker[] = { 0 };
const BYTE g_PSMain_color[] = { 0 };
const BYTE g_PSMain_sample_premul[] = { 0 };
const BYTE g_PSMain_sample[] = { 0 };
const BYTE g_PSMain_snow[] = { 0 };
const BYTE g_VSMain_color[] = { 0 };
const BYTE g_VSMain_coord[] = { 0 };
const BYTE g_VSMain_pos[] = { 0 };
#endif

#include "PSMain_checker_luma.hlsl"
#include "PSMain_checker_rgb.hlsl"
#include "PSMain_checker_vuya.hlsl"
#include "PSMain_checker.hlsl"
#include "PSMain_color.hlsl"
#include "PSMain_sample_premul.hlsl"
#include "PSMain_sample.hlsl"
#include "PSMain_snow.hlsl"
#include "VSMain_color.hlsl"
#include "VSMain_coord.hlsl"
#include "VSMain_pos.hlsl"
