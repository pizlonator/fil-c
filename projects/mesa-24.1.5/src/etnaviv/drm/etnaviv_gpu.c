/*
 * Copyright (C) 2015 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#include "etnaviv_priv.h"
#include "etnaviv_drmif.h"

#include "hwdb/etna_hwdb.h"

#include "hw/common.xml.h"

/* Enum with indices for each of the feature words */
enum viv_features_word {
	viv_chipFeatures = 0,
	viv_chipMinorFeatures0 = 1,
	viv_chipMinorFeatures1 = 2,
	viv_chipMinorFeatures2 = 3,
	viv_chipMinorFeatures3 = 4,
	viv_chipMinorFeatures4 = 5,
	viv_chipMinorFeatures5 = 6,
	viv_chipMinorFeatures6 = 7,
	viv_chipMinorFeatures7 = 8,
	viv_chipMinorFeatures8 = 9,
	viv_chipMinorFeatures9 = 10,
	viv_chipMinorFeatures10 = 11,
	viv_chipMinorFeatures11 = 12,
	viv_chipMinorFeatures12 = 13,
	VIV_FEATURES_WORD_COUNT /* Must be last */
};

#define VIV_FEATURE(word, feature) \
	((features[viv_ ## word] & (word ## _ ## feature)) != 0)

#define ETNA_FEATURE(word, feature) \
	if (VIV_FEATURE(word, feature)) \
		etna_core_enable_feature(&gpu->info, ETNA_FEATURE_## feature)

static void
query_features_from_kernel(struct etna_gpu *gpu)
{
	uint32_t features[VIV_FEATURES_WORD_COUNT];

	STATIC_ASSERT(ETNA_GPU_FEATURES_0  == 0x3);
	STATIC_ASSERT(ETNA_GPU_FEATURES_1  == 0x4);
	STATIC_ASSERT(ETNA_GPU_FEATURES_2  == 0x5);
	STATIC_ASSERT(ETNA_GPU_FEATURES_3  == 0x6);
	STATIC_ASSERT(ETNA_GPU_FEATURES_4  == 0x7);
	STATIC_ASSERT(ETNA_GPU_FEATURES_5  == 0x8);
	STATIC_ASSERT(ETNA_GPU_FEATURES_6  == 0x9);
	STATIC_ASSERT(ETNA_GPU_FEATURES_7  == 0xa);
	STATIC_ASSERT(ETNA_GPU_FEATURES_8  == 0xb);
	STATIC_ASSERT(ETNA_GPU_FEATURES_9  == 0xc);
	STATIC_ASSERT(ETNA_GPU_FEATURES_10 == 0xd);
	STATIC_ASSERT(ETNA_GPU_FEATURES_11 == 0xe);
	STATIC_ASSERT(ETNA_GPU_FEATURES_12 == 0xf);

	for (unsigned i = ETNA_GPU_FEATURES_0; i <= ETNA_GPU_FEATURES_12; i++) {
		uint64_t val;

		etna_gpu_get_param(gpu, i, &val);
		features[i - ETNA_GPU_FEATURES_0] = val;
	}

	gpu->info.type = ETNA_CORE_GPU;

	ETNA_FEATURE(chipFeatures, FAST_CLEAR);
	ETNA_FEATURE(chipFeatures, PIPE_3D);
	ETNA_FEATURE(chipFeatures, 32_BIT_INDICES);
	ETNA_FEATURE(chipFeatures, MSAA);
	ETNA_FEATURE(chipFeatures, DXT_TEXTURE_COMPRESSION);
	ETNA_FEATURE(chipFeatures, ETC1_TEXTURE_COMPRESSION);
	ETNA_FEATURE(chipFeatures, NO_EARLY_Z);

	ETNA_FEATURE(chipMinorFeatures0, MC20);
	ETNA_FEATURE(chipMinorFeatures0, RENDERTARGET_8K);
	ETNA_FEATURE(chipMinorFeatures0, TEXTURE_8K);
	ETNA_FEATURE(chipMinorFeatures0, HAS_SIGN_FLOOR_CEIL);
	ETNA_FEATURE(chipMinorFeatures0, HAS_SQRT_TRIG);
	ETNA_FEATURE(chipMinorFeatures0, 2BITPERTILE);
	ETNA_FEATURE(chipMinorFeatures0, SUPER_TILED);

	ETNA_FEATURE(chipMinorFeatures1, AUTO_DISABLE);
	ETNA_FEATURE(chipMinorFeatures1, TEXTURE_HALIGN);
	ETNA_FEATURE(chipMinorFeatures1, MMU_VERSION);
	ETNA_FEATURE(chipMinorFeatures1, HALF_FLOAT);
	ETNA_FEATURE(chipMinorFeatures1, WIDE_LINE);
	ETNA_FEATURE(chipMinorFeatures1, HALTI0);
	ETNA_FEATURE(chipMinorFeatures1, NON_POWER_OF_TWO);
	ETNA_FEATURE(chipMinorFeatures1, LINEAR_TEXTURE_SUPPORT);

	ETNA_FEATURE(chipMinorFeatures2, LINEAR_PE);
	ETNA_FEATURE(chipMinorFeatures2, SUPERTILED_TEXTURE);
	ETNA_FEATURE(chipMinorFeatures2, LOGIC_OP);
	ETNA_FEATURE(chipMinorFeatures2, HALTI1);
	ETNA_FEATURE(chipMinorFeatures2, SEAMLESS_CUBE_MAP);
	ETNA_FEATURE(chipMinorFeatures2, LINE_LOOP);
	ETNA_FEATURE(chipMinorFeatures2, TEXTURE_TILED_READ);
	ETNA_FEATURE(chipMinorFeatures2, BUG_FIXES8);

	ETNA_FEATURE(chipMinorFeatures3, PE_DITHER_FIX);
	ETNA_FEATURE(chipMinorFeatures3, INSTRUCTION_CACHE);
	ETNA_FEATURE(chipMinorFeatures3, HAS_FAST_TRANSCENDENTALS);

	ETNA_FEATURE(chipMinorFeatures4, SMALL_MSAA);
	ETNA_FEATURE(chipMinorFeatures4, BUG_FIXES18);
	ETNA_FEATURE(chipMinorFeatures4, TEXTURE_ASTC);
	ETNA_FEATURE(chipMinorFeatures4, SINGLE_BUFFER);
	ETNA_FEATURE(chipMinorFeatures4, HALTI2);

	ETNA_FEATURE(chipMinorFeatures5, BLT_ENGINE);
	ETNA_FEATURE(chipMinorFeatures5, HALTI3);
	ETNA_FEATURE(chipMinorFeatures5, HALTI4);
	ETNA_FEATURE(chipMinorFeatures5, HALTI5);
	ETNA_FEATURE(chipMinorFeatures5, RA_WRITE_DEPTH);

	ETNA_FEATURE(chipMinorFeatures6, CACHE128B256BPERLINE);
	ETNA_FEATURE(chipMinorFeatures6, NEW_GPIPE);
	ETNA_FEATURE(chipMinorFeatures6, NO_ASTC);
	ETNA_FEATURE(chipMinorFeatures6, V4_COMPRESSION);

	ETNA_FEATURE(chipMinorFeatures7, RS_NEW_BASEADDR);
	ETNA_FEATURE(chipMinorFeatures7, PE_NO_ALPHA_TEST);

	ETNA_FEATURE(chipMinorFeatures8, SH_NO_ONECONST_LIMIT);

	ETNA_FEATURE(chipMinorFeatures10, DEC400);
}

static void
query_limits_from_kernel(struct etna_gpu *gpu)
{
	struct etna_core_info *info = &gpu->info;
	uint64_t val;

	assert(info->type == ETNA_CORE_GPU);

	etna_gpu_get_param(gpu, ETNA_GPU_INSTRUCTION_COUNT, &val);
	info->gpu.max_instructions = val;

	etna_gpu_get_param(gpu, ETNA_GPU_VERTEX_OUTPUT_BUFFER_SIZE, &val);
	info->gpu.vertex_output_buffer_size = val;

	etna_gpu_get_param(gpu, ETNA_GPU_VERTEX_CACHE_SIZE, &val);
	info->gpu.vertex_cache_size = val;

	etna_gpu_get_param(gpu, ETNA_GPU_SHADER_CORE_COUNT, &val);
	info->gpu.shader_core_count = val;

	etna_gpu_get_param(gpu, ETNA_GPU_STREAM_COUNT, &val);
	info->gpu.stream_count = val;

	etna_gpu_get_param(gpu, ETNA_GPU_REGISTER_MAX, &val);
	info->gpu.max_registers = val;

	etna_gpu_get_param(gpu, ETNA_GPU_PIXEL_PIPES, &val);
	info->gpu.pixel_pipes = val;

	etna_gpu_get_param(gpu, ETNA_GPU_NUM_CONSTANTS, &val);
	info->gpu.num_constants = val;

	etna_gpu_get_param(gpu, ETNA_GPU_NUM_VARYINGS, &val);
	info->gpu.max_varyings = val;
}

static uint64_t get_param(struct etna_device *dev, uint32_t core, uint32_t param)
{
	struct drm_etnaviv_param req = {
		.pipe = core,
		.param = param,
	};
	int ret;

	ret = drmCommandWriteRead(dev->fd, DRM_ETNAVIV_GET_PARAM, &req, sizeof(req));
	if (ret) {
		ERROR_MSG("get-param (%x) failed! %d (%s)", param, ret, strerror(errno));
		return 0;
	}

	return req.value;
}

struct etna_gpu *etna_gpu_new(struct etna_device *dev, unsigned int core)
{
	struct etna_gpu *gpu;
	bool core_info_okay = false;

	gpu = calloc(1, sizeof(*gpu));
	if (!gpu) {
		ERROR_MSG("allocation failed");
		goto fail;
	}

	gpu->dev = dev;
	gpu->core = core;

	gpu->info.model    	= get_param(dev, core, ETNAVIV_PARAM_GPU_MODEL);
	gpu->info.revision 	= get_param(dev, core, ETNAVIV_PARAM_GPU_REVISION);

	if (!gpu->info.model)
		goto fail;

	DEBUG_MSG(" GPU model:          0x%x (rev %x)", gpu->info.model, gpu->info.revision);

	if (dev->drm_version >= ETNA_DRM_VERSION(1, 4)) {
		gpu->info.product_id = get_param(dev, core, ETNAVIV_PARAM_GPU_PRODUCT_ID);
		gpu->info.customer_id = get_param(dev, core, ETNAVIV_PARAM_GPU_CUSTOMER_ID);
		gpu->info.eco_id = get_param(dev, core, ETNAVIV_PARAM_GPU_ECO_ID);

		core_info_okay = etna_query_feature_db(&gpu->info);
		DEBUG_MSG(" Found entry in hwdb: %u\n", core_info_okay);
	}

	if (!core_info_okay) {
		query_features_from_kernel(gpu);
		query_limits_from_kernel(gpu);
	}

	return gpu;
fail:
	if (gpu)
		etna_gpu_del(gpu);

	return NULL;
}

void etna_gpu_del(struct etna_gpu *gpu)
{
	free(gpu);
}

int etna_gpu_get_param(struct etna_gpu *gpu, enum etna_param_id param,
		uint64_t *value)
{
	struct etna_device *dev = gpu->dev;
	unsigned int core = gpu->core;

	switch(param) {
	case ETNA_GPU_MODEL:
		*value = gpu->info.model;
		return 0;
	case ETNA_GPU_REVISION:
		*value = gpu->info.revision;
		return 0;
	case ETNA_GPU_FEATURES_0:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_0);
		return 0;
	case ETNA_GPU_FEATURES_1:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_1);
		return 0;
	case ETNA_GPU_FEATURES_2:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_2);
		return 0;
	case ETNA_GPU_FEATURES_3:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_3);
		return 0;
	case ETNA_GPU_FEATURES_4:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_4);
		return 0;
	case ETNA_GPU_FEATURES_5:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_5);
		return 0;
	case ETNA_GPU_FEATURES_6:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_6);
		return 0;
	case ETNA_GPU_FEATURES_7:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_7);
		return 0;
	case ETNA_GPU_FEATURES_8:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_8);
		return 0;
	case ETNA_GPU_FEATURES_9:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_9);
		return 0;
	case ETNA_GPU_FEATURES_10:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_10);
		return 0;
	case ETNA_GPU_FEATURES_11:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_11);
		return 0;
	case ETNA_GPU_FEATURES_12:
		*value = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_12);
		return 0;
	case ETNA_GPU_STREAM_COUNT:
		*value = get_param(dev, core, ETNA_GPU_STREAM_COUNT);
		return 0;
	case ETNA_GPU_REGISTER_MAX:
		*value = get_param(dev, core, ETNA_GPU_REGISTER_MAX);
		return 0;
	case ETNA_GPU_THREAD_COUNT:
		*value = get_param(dev, core, ETNA_GPU_THREAD_COUNT);
		return 0;
	case ETNA_GPU_VERTEX_CACHE_SIZE:
		*value = get_param(dev, core, ETNA_GPU_VERTEX_CACHE_SIZE);
		return 0;
	case ETNA_GPU_SHADER_CORE_COUNT:
		*value = get_param(dev, core, ETNA_GPU_SHADER_CORE_COUNT);
		return 0;
	case ETNA_GPU_PIXEL_PIPES:
		*value = get_param(dev, core, ETNA_GPU_PIXEL_PIPES);
		return 0;
	case ETNA_GPU_VERTEX_OUTPUT_BUFFER_SIZE:
		*value = get_param(dev, core, ETNA_GPU_VERTEX_OUTPUT_BUFFER_SIZE);
		return 0;
	case ETNA_GPU_BUFFER_SIZE:
		*value = get_param(dev, core, ETNA_GPU_BUFFER_SIZE);
		return 0;
	case ETNA_GPU_INSTRUCTION_COUNT:
		*value = get_param(dev, core, ETNA_GPU_INSTRUCTION_COUNT);
		return 0;
	case ETNA_GPU_NUM_CONSTANTS:
		*value = get_param(dev, core, ETNA_GPU_NUM_CONSTANTS);
		return 0;
	case ETNA_GPU_NUM_VARYINGS:
		*value = get_param(dev, core, ETNA_GPU_NUM_VARYINGS);
		return 0;
	case ETNA_SOFTPIN_START_ADDR:
		*value = get_param(dev, core, ETNA_SOFTPIN_START_ADDR);
		return 0;
	case ETNA_GPU_PRODUCT_ID:
		*value = gpu->info.product_id;
		return 0;
	case ETNA_GPU_CUSTOMER_ID:
		*value = gpu->info.customer_id;
		return 0;
	case ETNA_GPU_ECO_ID:
		*value = gpu->info.eco_id;
		return 0;

	default:
		ERROR_MSG("invalid param id: %d", param);
		return -1;
	}

	return 0;
}

struct etna_core_info *etna_gpu_get_core_info(struct etna_gpu *gpu)
{
	return &gpu->info;
}
