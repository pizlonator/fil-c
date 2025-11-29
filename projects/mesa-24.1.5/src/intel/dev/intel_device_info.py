# Copyright © 2024 Intel Corporation

# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

from textwrap import dedent

# TYPES is an ordered list of all declarations in this file.
TYPES = []

# TYPES_BY_NAME allows the lookup of any declaration
TYPES_BY_NAME = {}

class Define:
    """Specifies a c macro definition."""
    def __init__(self, name, value, comment=None):
        self.name = name
        self.value = value
        self.comment = comment
        TYPES.append(self)

class EnumValue:
    """allows comments and setting of enum values"""
    def __init__(self, name, value=None, comment=None,
                 group_begin=None, group_end=None):
        self.name = name
        self.value = value
        self.comment = comment
        self.group_begin = group_begin
        self.group_end = group_end

    def __str__(self):
        return self.name

class Enum:
    """Stores details needed to declare and serialize an enumeration"""
    def __init__(self, name, values, external=False):
        self.name = name
        self.values = []
        for v in values:
            if isinstance(v, EnumValue):
                self.values.append(v)
            else:
                self.values.append(EnumValue(v))

        self.external = external
        TYPES.append(self)
        TYPES_BY_NAME[name] = TYPES[-1]

class Member:
    """Stores details needed to declare and serialize the member of a struct."""
    def __init__(self, member_type, name, array=None,
                 compiler_field=False, ray_tracing_field=False,
                 comment=None):
        self.member_type = member_type
        self.name = name
        self.array = array
        # indicates whether this field is used by the compiler, and whether it
        # should be included in the shader compiler cache hash function.
        self.compiler_field = compiler_field
        self.ray_tracing_field = ray_tracing_field
        self.comment=comment

class Struct:
    """Stores details needed to declare and serialize a struct"""
    def __init__(self, name, members):
        self.name = name
        self.members = members
        TYPES.append(self)
        TYPES_BY_NAME[name] = TYPES[-1]

INT_TYPES = set(["uint8_t",
                 "uint16_t",
                 "uint32_t",
                 "uint64_t",
                 "unsigned",
                 "int"])

FUNDAMENTAL_TYPES = set(["char", "bool"]).union(INT_TYPES)

Define("INTEL_DEVICE_MAX_NAME_SIZE", 64)
Define("INTEL_DEVICE_MAX_SLICES", 8)
Define("INTEL_DEVICE_MAX_SUBSLICES", 8, "Maximum on gfx11")
Define("INTEL_DEVICE_MAX_EUS_PER_SUBSLICE", 16, "Maximum on gfx11")
Define("INTEL_DEVICE_MAX_PIXEL_PIPES", 16, "Maximum on DG2")

Enum("intel_platform",
     [EnumValue("INTEL_PLATFORM_GFX3", value=1),
      "INTEL_PLATFORM_I965",
      "INTEL_PLATFORM_ILK",
      "INTEL_PLATFORM_G4X",
      "INTEL_PLATFORM_SNB",
      "INTEL_PLATFORM_IVB",
      "INTEL_PLATFORM_BYT",
      "INTEL_PLATFORM_HSW",
      "INTEL_PLATFORM_BDW",
      "INTEL_PLATFORM_CHV",
      "INTEL_PLATFORM_SKL",
      "INTEL_PLATFORM_BXT",
      "INTEL_PLATFORM_KBL",
      "INTEL_PLATFORM_GLK",
      "INTEL_PLATFORM_CFL",
      "INTEL_PLATFORM_ICL",
      "INTEL_PLATFORM_EHL",
      "INTEL_PLATFORM_TGL",
      "INTEL_PLATFORM_RKL",
      "INTEL_PLATFORM_DG1",
      "INTEL_PLATFORM_ADL",
      "INTEL_PLATFORM_RPL",
      EnumValue("INTEL_PLATFORM_DG2_G10", group_begin="DG2"),
      "INTEL_PLATFORM_DG2_G11",
      EnumValue("INTEL_PLATFORM_DG2_G12", group_end="DG2"),
      EnumValue("INTEL_PLATFORM_ATSM_G10", group_begin="ATSM"),
      EnumValue("INTEL_PLATFORM_ATSM_G11", group_end="ATSM"),
      EnumValue("INTEL_PLATFORM_MTL_U", group_begin="MTL"),
      EnumValue("INTEL_PLATFORM_MTL_H", group_end="MTL"),
      EnumValue("INTEL_PLATFORM_ARL_U", group_begin="ARL"),
      EnumValue("INTEL_PLATFORM_ARL_H", group_end="ARL"),
      "INTEL_PLATFORM_LNL"
      ])

Struct("intel_memory_class_instance",
       [ Member("int", "klass",
                comment = "Kernel backend specific class value, no translation needed yet"),
         Member("int", "instance")])

Enum("intel_device_info_mmap_mode",
      [EnumValue("INTEL_DEVICE_INFO_MMAP_MODE_UC", value=0),
       "INTEL_DEVICE_INFO_MMAP_MODE_WC",
       "INTEL_DEVICE_INFO_MMAP_MODE_WB"
       ])

Enum("intel_device_info_coherency_mode",
     [EnumValue("INTEL_DEVICE_INFO_COHERENCY_MODE_NONE", value=0),
      EnumValue("INTEL_DEVICE_INFO_COHERENCY_MODE_1WAY", comment="CPU caches are snooped by GPU"),
      EnumValue("INTEL_DEVICE_INFO_COHERENCY_MODE_2WAY",
                comment="Fully coherent between GPU and CPU")
      ])

Struct("intel_device_info_pat_entry",
       [Member("uint8_t", "index"),
        Member("intel_device_info_mmap_mode", "mmap"),
        Member("intel_device_info_coherency_mode", "coherency")])

Enum("intel_cmat_scope",
     [EnumValue("INTEL_CMAT_SCOPE_NONE", value=0),
     "INTEL_CMAT_SCOPE_SUBGROUP"])

Enum("intel_cooperative_matrix_component_type",
     ["INTEL_CMAT_FLOAT16",
      "INTEL_CMAT_FLOAT32",
      "INTEL_CMAT_SINT32",
      "INTEL_CMAT_SINT8",
      "INTEL_CMAT_UINT32",
      "INTEL_CMAT_UINT8"])

Enum("intel_engine_class",
     ["INTEL_ENGINE_CLASS_RENDER",
      "INTEL_ENGINE_CLASS_COPY",
      "INTEL_ENGINE_CLASS_VIDEO",
      "INTEL_ENGINE_CLASS_VIDEO_ENHANCE",
      "INTEL_ENGINE_CLASS_COMPUTE",
      "INTEL_ENGINE_CLASS_INVALID"])

Struct("intel_cooperative_matrix_configuration",
   [Member("intel_cmat_scope", "scope",
           comment=dedent("""\
           Matrix A is MxK.
           Matrix B is KxN.
           Matrix C and Matrix Result are MxN.

           Result = A * B + C;""")),
    Member("uint8_t", "m"),
    Member("uint8_t", "n"),
    Member("uint8_t", "k"),
    Member("intel_cooperative_matrix_component_type", "a"),
    Member("intel_cooperative_matrix_component_type", "b"),
    Member("intel_cooperative_matrix_component_type", "c"),
    Member("intel_cooperative_matrix_component_type", "result")])

Enum("intel_kmd_type",
     ["INTEL_KMD_TYPE_INVALID",
      "INTEL_KMD_TYPE_I915",
      "INTEL_KMD_TYPE_XE",
      "INTEL_KMD_TYPE_STUB",
      "INTEL_KMD_TYPE_LAST"
      ], external=True)

Struct("intel_device_info_mem_region",
       [Member("uint64_t", "size"),
        Member("uint64_t", "free")])

Struct("intel_device_info_ram_desc",
       [Member("intel_memory_class_instance", "mem"),
        Member("intel_device_info_mem_region", "mappable"),
        Member("intel_device_info_mem_region", "unmappable")])

Struct("intel_device_info_mem_desc",
       [Member("bool", "use_class_instance"),
        Member("intel_device_info_ram_desc", "sram"),
        Member("intel_device_info_ram_desc", "vram")])

Struct("intel_device_info_urb_desc",
       [Member("int", "size"),
        Member("int", "min_entries", array=4),
        Member("int", "max_entries", array=4)])

Struct("intel_device_info_pat_desc",
       [Member("intel_device_info_pat_entry", "cached_coherent",
               comment="To be used when CPU access is frequent, WB + 1 or 2 way coherent"),

        Member("intel_device_info_pat_entry", "scanout",
               comment="scanout and external BOs"),

        Member("intel_device_info_pat_entry", "writeback_incoherent",
               comment=("BOs without special needs, can be WB not coherent "
                        "or WC it depends on the platforms and KMD")),

        Member("intel_device_info_pat_entry", "writecombining")])

Struct("intel_device_info",
       [Member("intel_kmd_type", "kmd_type"),

        Member("int", "ver", compiler_field=True,
               comment="Driver internal numbers used to differentiate platforms."),

        Member("int", "verx10", compiler_field=True),

        Member("uint32_t", "gfx_ip_ver", compiler_field=True,
               comment=dedent("""\
               This is the run-time hardware GFX IP version that may be more specific
               than ver/verx10. ver/verx10 may be more useful for comparing a class
               of devices whereas gfx_ip_ver may be more useful for precisely
               checking for a graphics ip type. GFX_IP_VER(major, minor) should be
               used to compare IP versions.""")),

        Member("int", "revision",
               comment=dedent("""\
               This revision is queried from KMD unlike
               pci_revision_id from drm device. Its value is not always
               same as the pci_revision_id.
               For LNL+ this is the stepping of GT IP/GMD RevId.""")),

        Member("int", "gt"),
        Member("uint16_t", "pci_domain", comment="PCI info"),
        Member("uint8_t", "pci_bus"),
        Member("uint8_t", "pci_dev"),
        Member("uint8_t", "pci_func"),
        Member("uint16_t", "pci_device_id"),
        Member("uint8_t", "pci_revision_id"),
        Member("intel_platform", "platform", compiler_field=True),
        Member("bool", "has_hiz_and_separate_stencil"),
        Member("bool", "must_use_separate_stencil"),
        Member("bool", "has_sample_with_hiz"),
        Member("bool", "has_bit6_swizzle"),
        Member("bool", "has_llc"),
        Member("bool", "has_pln", compiler_field=True),
        Member("bool", "has_64bit_float", compiler_field=True),
        Member("bool", "has_64bit_float_via_math_pipe", compiler_field=True),
        Member("bool", "has_64bit_int", compiler_field=True),
        Member("bool", "has_integer_dword_mul", compiler_field=True),
        Member("bool", "has_compr4", compiler_field=True),
        Member("bool", "has_surface_tile_offset"),
        Member("bool", "supports_simd16_3src", compiler_field=True),
        Member("bool", "disable_ccs_repack"),

        Member("bool", "has_illegal_ccs_values",
               comment="True if CCS needs to be initialized before use."),

        Member("bool", "has_flat_ccs",
               comment=dedent("""\
               True if CCS uses a flat virtual address translation to a memory
               carve-out, rather than aux map translations, or additional surfaces.""")),

        Member("bool", "has_aux_map"),
        Member("bool", "has_caching_uapi"),
        Member("bool", "has_tiling_uapi"),
        Member("bool", "has_ray_tracing", compiler_field=True),
        Member("bool", "has_ray_query"),
        Member("bool", "has_local_mem"),
        Member("bool", "has_lsc", compiler_field=True),
        Member("bool", "has_mesh_shading"),
        Member("bool", "has_mmap_offset"),
        Member("bool", "has_userptr_probe"),
        Member("bool", "has_context_isolation"),
        Member("bool", "has_set_pat_uapi"),
        Member("bool", "has_indirect_unroll"),
        Member("bool", "has_negative_rhw_bug", compiler_field=True,
               comment="Intel hardware quirks"),

        Member("bool", "has_coarse_pixel_primitive_and_cb", compiler_field=True,
               comment=dedent("""\
               Whether this platform supports fragment shading rate controlled by a
               primitive in geometry shaders and by a control buffer.""")),

        Member("bool", "has_compute_engine", comment="Whether this platform has compute engine"),

        Member("bool", "needs_unlit_centroid_workaround", compiler_field=True,
               comment=dedent("""\
               Some versions of Gen hardware don't do centroid interpolation correctly
               on unlit pixels, causing incorrect values for derivatives near triangle
               edges.  Enabling this flag causes the fragment shader to use
               non-centroid interpolation for unlit pixels, at the expense of two extra
               fragment shader instructions.""")),

        Member("bool", "needs_null_push_constant_tbimr_workaround",
               comment=dedent("""\
               Whether the platform needs an undocumented workaround for a hardware bug
               that affects draw calls with a pixel shader that has 0 push constant cycles
               when TBIMR is enabled, which has been seen to lead to hangs.  To avoid the
               issue we simply pad the push constant payload to be at least 1 register.""")),

        Member("bool", "is_adl_n", comment="We need this for ADL-N specific Wa_14014966230."),

        Member("unsigned", "num_slices",
               comment=dedent("""\
               GPU hardware limits

               In general, you can find shader thread maximums by looking at the "Maximum
               Number of Threads" field in the Intel PRM description of the 3DSTATE_VS,
               3DSTATE_GS, 3DSTATE_HS, 3DSTATE_DS, and 3DSTATE_PS commands. URB entry
               limits come from the "Number of URB Entries" field in the
               3DSTATE_URB_VS command and friends.

               These fields are used to calculate the scratch space to allocate.  The
               amount of scratch space can be larger without being harmful on modern
               GPUs, however, prior to Haswell, programming the maximum number of threads
               to greater than the hardware maximum would cause GPU performance to tank.

               Total number of slices present on the device whether or not they've been
               fused off.

               XXX: CS thread counts are limited by the inability to do cross subslice
               communication. It is the effectively the number of logical threads which
               can be executed in a subslice. Fuse configurations may cause this number
               to change, so we program @max_cs_threads as the lower maximum.""")),

        Member("unsigned", "max_slices", compiler_field=True,
               comment=dedent("""\
               Maximum number of slices present on this device (can be more than
               num_slices if some slices are fused).""")),

        Member("unsigned", "num_subslices", array="INTEL_DEVICE_MAX_SLICES",
               comment="Number of subslices for each slice (used to be uniform until CNL)."),

        Member("unsigned", "max_subslices_per_slice", compiler_field=True,
               comment=dedent("""\
               Maximum number of subslices per slice present on this device (can be
               more than the maximum value in the num_subslices[] array if some
               subslices are fused).

               This is GT_SS_PER_SLICE in SKU.""")),

        Member("unsigned", "ppipe_subslices", array="INTEL_DEVICE_MAX_PIXEL_PIPES",
               comment="Number of subslices on each pixel pipe (ICL)."),

        Member("unsigned", "max_eus_per_subslice", compiler_field=True,
               comment="Maximum number of EUs per subslice (some EUs can be fused off)."),

        Member("unsigned", "num_thread_per_eu", compiler_field=True,
               comment="Number of threads per eu, varies between 4 and 8 between generations."),

        Member("uint8_t", "slice_masks",
               comment="A bit mask of the slices available."),

        Member("uint8_t", "subslice_masks",
               array="INTEL_DEVICE_MAX_SLICES * DIV_ROUND_UP(INTEL_DEVICE_MAX_SUBSLICES, 8)",
               compiler_field=True,
               ray_tracing_field=True,
               comment=dedent("""\
               An array of bit mask of the subslices available, use subslice_slice_stride
               to access this array.""")),

        Member("unsigned", "subslice_total",
               comment=dedent("""\
               The number of enabled subslices (considering fusing). For exactly which
               subslices are enabled, see subslice_masks[].""")),

        Member("uint8_t", "eu_masks",
               array=("INTEL_DEVICE_MAX_SLICES * INTEL_DEVICE_MAX_SUBSLICES * "
                      "DIV_ROUND_UP(INTEL_DEVICE_MAX_EUS_PER_SUBSLICE, 8)"),
               comment=dedent("""\
               An array of bit mask of EUs available, use eu_slice_stride &
               eu_subslice_stride to access this array.""")),

        Member("uint16_t", "subslice_slice_stride", compiler_field=True,
               comment="Stride to access subslice_masks[]."),

        Member("uint16_t", "eu_slice_stride",
               comment="Strides to access eu_masks[]."),

        Member("uint16_t", "eu_subslice_stride"),
        Member("unsigned", "l3_banks"),

        Member("unsigned", "max_vs_threads",
               comment="Maximum Vertex Shader threads"),

        Member("unsigned", "max_tcs_threads",
               comment="Maximum Hull Shader threads"),

        Member("unsigned", "max_tes_threads",
               comment="Maximum Domain Shader threads"),

        Member("unsigned", "max_gs_threads",
               comment="Maximum Geometry Shader threads"),

        Member("unsigned", "max_wm_threads",
               comment=dedent("""\
               Theoretical maximum number of Pixel Shader threads.

               PSD means Pixel Shader Dispatcher. On modern Intel GPUs, hardware will
               automatically scale pixel shader thread count, based on a single value
               programmed into 3DSTATE_PS.

               To calculate the maximum number of threads for Gfx8 beyond (which have
               multiple Pixel Shader Dispatchers):

               - Look up 3DSTATE_PS and find "Maximum Number of Threads Per PSD"
               - Usually there's only one PSD per subslice, so use the number of
                 subslices for number of PSDs.
               - For max_wm_threads, the total should be PSD threads * #PSDs.""")),

        Member("unsigned", "max_threads_per_psd"),

        Member("unsigned", "max_cs_threads",
               comment=dedent("""\
               Maximum Compute Shader threads.

               Thread count * number of EUs per subslice""")),

        Member("unsigned", "max_cs_workgroup_threads", compiler_field=True,
               comment=dedent("""\
               Maximum number of threads per workgroup supported by the GPGPU_WALKER or
               COMPUTE_WALKER command.

               This may be smaller than max_cs_threads as it takes into account added
               restrictions on the GPGPU/COMPUTE_WALKER commands.  While max_cs_threads
               expresses the total parallelism of the GPU, this expresses the maximum
               number of threads we can dispatch in a single workgroup.""")),


        Member("unsigned", "max_scratch_ids", array="MESA_SHADER_STAGES", compiler_field=True,
               comment=dedent("""\
               The maximum number of potential scratch ids. Due to hardware
               implementation details, the range of scratch ids may be larger than the
               number of subslices.""")),

        Member("intel_device_info_urb_desc", "urb"),
        Member("unsigned", "max_constant_urb_size_kb"),
        Member("unsigned", "mesh_max_constant_urb_size_kb"),
        Member("unsigned", "engine_class_prefetch", array="INTEL_ENGINE_CLASS_COMPUTE + 1"),
        Member("unsigned", "mem_alignment"),
        Member("uint64_t", "timestamp_frequency"),
        Member("uint64_t", "aperture_bytes"),
        Member("uint64_t", "gtt_size"),
        Member("int", "simulator_id"),
        Member("char", "name", array="INTEL_DEVICE_MAX_NAME_SIZE"),
        Member("bool", "no_hw"),
        Member("bool", "apply_hwconfig"),
        Member("intel_device_info_mem_desc", "mem"),
        Member("intel_device_info_pat_desc", "pat"),
        Member("intel_cooperative_matrix_configuration",
               "cooperative_matrix_configurations", array=4)]
       )
