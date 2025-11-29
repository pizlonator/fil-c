use crate::api::icd::*;
use crate::core::context::*;
use crate::core::device::*;
use crate::core::kernel::*;
use crate::core::platform::Platform;
use crate::impl_cl_type_trait;

use mesa_rust::compiler::clc::spirv::SPIRVBin;
use mesa_rust::compiler::clc::*;
use mesa_rust::compiler::nir::*;
use mesa_rust::pipe::resource::*;
use mesa_rust::pipe::screen::ResourceType;
use mesa_rust::util::disk_cache::*;
use mesa_rust_gen::*;
use rusticl_llvm_gen::*;
use rusticl_opencl_gen::*;

use std::collections::HashMap;
use std::collections::HashSet;
use std::ffi::CString;
use std::mem::size_of;
use std::ptr::addr_of;
use std::slice;
use std::sync::Arc;
use std::sync::Mutex;
use std::sync::MutexGuard;
use std::sync::Once;

// 8 bytes so we don't have any padding.
const BIN_RUSTICL_MAGIC_STRING: &[u8; 8] = b"rusticl\0";

const BIN_HEADER_SIZE_BASE: usize =
    // 1. magic number
    size_of::<[u8; 8]>() +
    // 2. format version
    size_of::<u32>();

const BIN_HEADER_SIZE_V1: usize = BIN_HEADER_SIZE_BASE +
    // 3. device name length
    size_of::<u32>() +
    // 4. spirv len
    size_of::<u32>() +
    // 5. binary_type
    size_of::<cl_program_binary_type>();

const BIN_HEADER_SIZE: usize = BIN_HEADER_SIZE_V1;

// kernel cache
static mut DISK_CACHE: Option<DiskCache> = None;
static DISK_CACHE_ONCE: Once = Once::new();

fn get_disk_cache() -> &'static Option<DiskCache> {
    let func_ptrs = [
        // ourselves
        get_disk_cache as _,
        // LLVM
        llvm_LLVMContext_LLVMContext as _,
        // clang
        clang_getClangFullVersion as _,
        // SPIRV-LLVM-Translator
        llvm_writeSpirv1 as _,
    ];
    unsafe {
        DISK_CACHE_ONCE.call_once(|| {
            DISK_CACHE = DiskCache::new("rusticl", &func_ptrs, 0);
        });
        &*addr_of!(DISK_CACHE)
    }
}

fn clc_validator_options(dev: &Device) -> clc_validator_options {
    clc_validator_options {
        // has to match CL_DEVICE_MAX_PARAMETER_SIZE
        limit_max_function_arg: dev.param_max_size() as u32,
    }
}

pub enum ProgramSourceType {
    Binary,
    Linked,
    Src(CString),
    Il(spirv::SPIRVBin),
}

pub struct Program {
    pub base: CLObjectBase<CL_INVALID_PROGRAM>,
    pub context: Arc<Context>,
    pub devs: Vec<&'static Device>,
    pub src: ProgramSourceType,
    build: Mutex<ProgramBuild>,
}

impl_cl_type_trait!(cl_program, Program, CL_INVALID_PROGRAM);

pub struct NirKernelBuild {
    pub nir_or_cso: KernelDevStateVariant,
    pub constant_buffer: Option<Arc<PipeResource>>,
    pub info: pipe_compute_state_object_info,
    pub shared_size: u64,
    pub printf_info: Option<NirPrintfInfo>,
}

// SAFETY: `CSOWrapper` is only safe to use if the device supports `PIPE_CAP_SHAREABLE_SHADERS` and
//         we make sure to set `nir_or_cso` to `KernelDevStateVariant::Cso` only if that's the case.
unsafe impl Send for NirKernelBuild {}
unsafe impl Sync for NirKernelBuild {}

pub struct ProgramBuild {
    pub builds: HashMap<&'static Device, ProgramDevBuild>,
    pub kernel_info: HashMap<String, KernelInfo>,
    spec_constants: HashMap<u32, nir_const_value>,
    kernels: Vec<String>,
}

impl NirKernelBuild {
    pub fn new(dev: &'static Device, mut nir: NirShader) -> Self {
        let cso = CSOWrapper::new(dev, &nir);
        let info = cso.get_cso_info();
        let cb = Self::create_nir_constant_buffer(dev, &nir);
        let shared_size = nir.shared_size() as u64;
        let printf_info = nir.take_printf_info();

        let nir_or_cso = if !dev.shareable_shaders() {
            KernelDevStateVariant::Nir(nir)
        } else {
            KernelDevStateVariant::Cso(cso)
        };

        NirKernelBuild {
            nir_or_cso: nir_or_cso,
            constant_buffer: cb,
            info: info,
            shared_size: shared_size,
            printf_info: printf_info,
        }
    }

    fn create_nir_constant_buffer(dev: &Device, nir: &NirShader) -> Option<Arc<PipeResource>> {
        let buf = nir.get_constant_buffer();
        let len = buf.len() as u32;

        if len > 0 {
            // TODO bind as constant buffer
            let res = dev
                .screen()
                .resource_create_buffer(len, ResourceType::Normal, PIPE_BIND_GLOBAL)
                .unwrap();

            dev.helper_ctx()
                .exec(|ctx| ctx.buffer_subdata(&res, 0, buf.as_ptr().cast(), len))
                .wait();

            Some(Arc::new(res))
        } else {
            None
        }
    }
}

impl ProgramBuild {
    pub fn attribute_str(&self, kernel: &str, d: &Device) -> String {
        let info = self.dev_build(d);

        let attributes_strings = [
            info.spirv.as_ref().unwrap().vec_type_hint(kernel),
            info.spirv.as_ref().unwrap().local_size(kernel),
            info.spirv.as_ref().unwrap().local_size_hint(kernel),
        ];

        let attributes_strings: Vec<_> = attributes_strings
            .iter()
            .flatten()
            .map(String::as_str)
            .collect();
        attributes_strings.join(",")
    }

    fn args(&self, dev: &Device, kernel: &str) -> Vec<spirv::SPIRVKernelArg> {
        self.dev_build(dev).spirv.as_ref().unwrap().args(kernel)
    }

    fn build_nirs(&mut self, is_src: bool) {
        for kernel_name in &self.kernels.clone() {
            let kernel_args: HashSet<_> = self
                .devs_with_build()
                .iter()
                .map(|d| self.args(d, kernel_name))
                .collect();

            let args = kernel_args.into_iter().next().unwrap();
            let mut kernel_info_set = HashSet::new();

            // TODO: we could run this in parallel?
            for dev in self.devs_with_build() {
                let (kernel_info, nir) = convert_spirv_to_nir(self, kernel_name, &args, dev);
                kernel_info_set.insert(kernel_info);

                self.builds
                    .get_mut(dev)
                    .unwrap()
                    .kernels
                    .insert(kernel_name.clone(), Arc::new(NirKernelBuild::new(dev, nir)));
            }

            // we want the same (internal) args for every compiled kernel, for now
            assert!(kernel_info_set.len() == 1);
            let mut kernel_info = kernel_info_set.into_iter().next().unwrap();

            // spec: For kernels not created from OpenCL C source and the clCreateProgramWithSource
            // API call the string returned from this query [CL_KERNEL_ATTRIBUTES] will be empty.
            if !is_src {
                kernel_info.attributes_string = String::new();
            }

            self.kernel_info.insert(kernel_name.clone(), kernel_info);
        }
    }

    fn dev_build(&self, dev: &Device) -> &ProgramDevBuild {
        self.builds.get(dev).unwrap()
    }

    fn dev_build_mut(&mut self, dev: &Device) -> &mut ProgramDevBuild {
        self.builds.get_mut(dev).unwrap()
    }

    fn devs_with_build(&self) -> Vec<&'static Device> {
        self.builds
            .iter()
            .filter(|(_, build)| build.status == CL_BUILD_SUCCESS as cl_build_status)
            .map(|(&d, _)| d)
            .collect()
    }

    pub fn hash_key(&self, dev: &Device, name: &str) -> Option<cache_key> {
        if let Some(cache) = dev.screen().shader_cache() {
            let info = self.dev_build(dev);
            assert_eq!(info.status, CL_BUILD_SUCCESS as cl_build_status);

            let spirv = info.spirv.as_ref().unwrap();
            let mut bin = spirv.to_bin().to_vec();
            bin.extend_from_slice(name.as_bytes());

            for (k, v) in &self.spec_constants {
                bin.extend_from_slice(&k.to_ne_bytes());
                unsafe {
                    // SAFETY: we fully initialize this union
                    bin.extend_from_slice(&v.u64_.to_ne_bytes());
                }
            }

            Some(cache.gen_key(&bin))
        } else {
            None
        }
    }

    pub fn to_nir(&self, kernel: &str, d: &Device) -> NirShader {
        let mut spec_constants: Vec<_> = self
            .spec_constants
            .iter()
            .map(|(&id, &value)| nir_spirv_specialization {
                id: id,
                value: value,
                defined_on_module: true,
            })
            .collect();

        let info = self.dev_build(d);
        assert_eq!(info.status, CL_BUILD_SUCCESS as cl_build_status);

        let mut log = Platform::dbg().program.then(Vec::new);
        let nir = info.spirv.as_ref().unwrap().to_nir(
            kernel,
            d.screen
                .nir_shader_compiler_options(pipe_shader_type::PIPE_SHADER_COMPUTE),
            &d.lib_clc,
            &mut spec_constants,
            d.address_bits(),
            log.as_mut(),
        );

        if let Some(log) = log {
            for line in log {
                eprintln!("{}", line);
            }
        };

        nir.unwrap()
    }
}

#[derive(Default)]
pub struct ProgramDevBuild {
    spirv: Option<spirv::SPIRVBin>,
    status: cl_build_status,
    options: String,
    log: String,
    bin_type: cl_program_binary_type,
    pub kernels: HashMap<String, Arc<NirKernelBuild>>,
}

fn prepare_options(options: &str, dev: &Device) -> Vec<CString> {
    let mut options = options.to_owned();
    if !options.contains("-cl-std=") {
        options.push_str(" -cl-std=CL");
        options.push_str(dev.clc_version.api_str());
    }
    options.push_str(" -D__OPENCL_VERSION__=");
    options.push_str(dev.cl_version.clc_str());

    let mut res = Vec::new();

    // we seperate on a ' ' unless we hit a "
    let mut sep = ' ';
    let mut old = 0;
    for (i, c) in options.char_indices() {
        if c == '"' {
            if sep == ' ' {
                sep = '"';
            } else {
                sep = ' ';
            }
        }

        if c == '"' || c == sep {
            // beware of double seps
            if old != i {
                res.push(&options[old..i]);
            }
            old = i + c.len_utf8();
        }
    }
    // add end of the string
    res.push(&options[old..]);

    res.iter()
        .filter_map(|&a| match a {
            "-cl-denorms-are-zero" => Some("-fdenormal-fp-math=positive-zero"),
            // We can ignore it as long as we don't support ifp
            "-cl-no-subgroup-ifp" => None,
            _ => Some(a),
        })
        .map(CString::new)
        .map(Result::unwrap)
        .collect()
}

impl Program {
    fn create_default_builds(
        devs: &[&'static Device],
    ) -> HashMap<&'static Device, ProgramDevBuild> {
        devs.iter()
            .map(|&d| {
                (
                    d,
                    ProgramDevBuild {
                        spirv: None,
                        status: CL_BUILD_NONE,
                        log: String::from(""),
                        options: String::from(""),
                        bin_type: CL_PROGRAM_BINARY_TYPE_NONE,
                        kernels: HashMap::new(),
                    },
                )
            })
            .collect()
    }

    pub fn new(context: Arc<Context>, src: CString) -> Arc<Program> {
        Arc::new(Self {
            base: CLObjectBase::new(RusticlTypes::Program),
            build: Mutex::new(ProgramBuild {
                builds: Self::create_default_builds(&context.devs),
                spec_constants: HashMap::new(),
                kernels: Vec::new(),
                kernel_info: HashMap::new(),
            }),
            devs: context.devs.to_vec(),
            context: context,
            src: ProgramSourceType::Src(src),
        })
    }

    fn spirv_from_bin_for_dev(
        dev: &Device,
        bin: &[u8],
    ) -> CLResult<(SPIRVBin, cl_program_binary_type)> {
        if bin.is_empty() {
            return Err(CL_INVALID_VALUE);
        }

        if bin.len() < BIN_HEADER_SIZE_BASE {
            return Err(CL_INVALID_BINARY);
        }

        unsafe {
            let mut blob = blob_reader::default();
            blob_reader_init(&mut blob, bin.as_ptr().cast(), bin.len());

            let read_magic: &[u8] = slice::from_raw_parts(
                blob_read_bytes(&mut blob, BIN_RUSTICL_MAGIC_STRING.len()).cast(),
                BIN_RUSTICL_MAGIC_STRING.len(),
            );
            if read_magic != *BIN_RUSTICL_MAGIC_STRING {
                return Err(CL_INVALID_BINARY);
            }

            let version = u32::from_le(blob_read_uint32(&mut blob));
            match version {
                1 => {
                    let name_length = u32::from_le(blob_read_uint32(&mut blob)) as usize;
                    let spirv_size = u32::from_le(blob_read_uint32(&mut blob)) as usize;
                    let bin_type = u32::from_le(blob_read_uint32(&mut blob));

                    debug_assert!(
                        // `blob_read_*` doesn't advance the pointer on failure to read
                        blob.current.offset_from(blob.data) == BIN_HEADER_SIZE_V1 as isize
                            || blob.overrun,
                    );

                    let name = blob_read_bytes(&mut blob, name_length);
                    let spirv_data = blob_read_bytes(&mut blob, spirv_size);

                    // check that all the reads are valid before accessing the data, which might
                    // be uninitialized otherwise.
                    if blob.overrun {
                        return Err(CL_INVALID_BINARY);
                    }

                    let name: &[u8] = slice::from_raw_parts(name.cast(), name_length);
                    if dev.screen().name().as_bytes() != name {
                        return Err(CL_INVALID_BINARY);
                    }

                    let spirv = spirv::SPIRVBin::from_bin(slice::from_raw_parts(
                        spirv_data.cast(),
                        spirv_size,
                    ));

                    Ok((spirv, bin_type))
                }
                _ => Err(CL_INVALID_BINARY),
            }
        }
    }

    pub fn from_bins(
        context: Arc<Context>,
        devs: Vec<&'static Device>,
        bins: &[&[u8]],
    ) -> Result<Arc<Program>, Vec<cl_int>> {
        let mut builds = HashMap::new();
        let mut kernels = HashSet::new();

        let mut errors = vec![CL_SUCCESS as cl_int; devs.len()];
        for (idx, (&d, b)) in devs.iter().zip(bins).enumerate() {
            let build = match Self::spirv_from_bin_for_dev(d, b) {
                Ok((spirv, bin_type)) => {
                    for k in spirv.kernels() {
                        kernels.insert(k);
                    }

                    ProgramDevBuild {
                        spirv: Some(spirv),
                        bin_type: bin_type,
                        ..Default::default()
                    }
                }
                Err(err) => {
                    errors[idx] = err;
                    ProgramDevBuild {
                        status: CL_BUILD_ERROR,
                        ..Default::default()
                    }
                }
            };

            builds.insert(d, build);
        }

        if errors.iter().any(|&e| e != CL_SUCCESS as cl_int) {
            return Err(errors);
        }

        let mut build = ProgramBuild {
            builds: builds,
            spec_constants: HashMap::new(),
            kernels: kernels.into_iter().collect(),
            kernel_info: HashMap::new(),
        };
        build.build_nirs(false);

        Ok(Arc::new(Self {
            base: CLObjectBase::new(RusticlTypes::Program),
            context: context,
            devs: devs,
            src: ProgramSourceType::Binary,
            build: Mutex::new(build),
        }))
    }

    pub fn from_spirv(context: Arc<Context>, spirv: &[u8]) -> Arc<Program> {
        let builds = Self::create_default_builds(&context.devs);
        Arc::new(Self {
            base: CLObjectBase::new(RusticlTypes::Program),
            devs: context.devs.clone(),
            context: context,
            src: ProgramSourceType::Il(SPIRVBin::from_bin(spirv)),
            build: Mutex::new(ProgramBuild {
                builds: builds,
                spec_constants: HashMap::new(),
                kernels: Vec::new(),
                kernel_info: HashMap::new(),
            }),
        })
    }

    pub fn build_info(&self) -> MutexGuard<ProgramBuild> {
        self.build.lock().unwrap()
    }

    pub fn status(&self, dev: &Device) -> cl_build_status {
        self.build_info().dev_build(dev).status
    }

    pub fn log(&self, dev: &Device) -> String {
        self.build_info().dev_build(dev).log.clone()
    }

    pub fn bin_type(&self, dev: &Device) -> cl_program_binary_type {
        self.build_info().dev_build(dev).bin_type
    }

    pub fn options(&self, dev: &Device) -> String {
        self.build_info().dev_build(dev).options.clone()
    }

    // we need to precalculate the size
    pub fn bin_sizes(&self) -> Vec<usize> {
        let lock = self.build_info();
        let mut res = Vec::new();
        for d in &self.devs {
            let info = lock.dev_build(d);

            res.push(info.spirv.as_ref().map_or(0, |s| {
                s.to_bin().len() + d.screen().name().as_bytes().len() + BIN_HEADER_SIZE
            }));
        }
        res
    }

    pub fn binaries(&self, vals: &[u8]) -> CLResult<Vec<*mut u8>> {
        // if the application didn't provide any pointers, just return the length of devices
        if vals.is_empty() {
            return Ok(vec![std::ptr::null_mut(); self.devs.len()]);
        }

        // vals is an array of pointers where we should write the device binaries into
        if vals.len() != self.devs.len() * size_of::<*const u8>() {
            return Err(CL_INVALID_VALUE);
        }

        let ptrs: &[*mut u8] = unsafe {
            slice::from_raw_parts(vals.as_ptr().cast(), vals.len() / size_of::<*mut u8>())
        };

        let lock = self.build_info();
        for (i, d) in self.devs.iter().enumerate() {
            let info = lock.dev_build(d);

            // no spirv means nothing to write
            let Some(spirv) = info.spirv.as_ref() else {
                continue;
            };
            let spirv = spirv.to_bin();

            unsafe {
                let mut blob = blob::default();

                // sadly we have to trust the buffer to be correctly sized...
                blob_init_fixed(&mut blob, ptrs[i].cast(), usize::MAX);

                blob_write_bytes(
                    &mut blob,
                    BIN_RUSTICL_MAGIC_STRING.as_ptr().cast(),
                    BIN_RUSTICL_MAGIC_STRING.len(),
                );

                // binary format version
                blob_write_uint32(&mut blob, 1_u32.to_le());

                let device_name = d.screen().name();
                let device_name = device_name.as_bytes();

                blob_write_uint32(&mut blob, (device_name.len() as u32).to_le());
                blob_write_uint32(&mut blob, (spirv.len() as u32).to_le());
                blob_write_uint32(&mut blob, info.bin_type.to_le());
                debug_assert!(blob.size == BIN_HEADER_SIZE);

                blob_write_bytes(&mut blob, device_name.as_ptr().cast(), device_name.len());
                blob_write_bytes(&mut blob, spirv.as_ptr().cast(), spirv.len());
                blob_finish(&mut blob);
            }
        }

        Ok(ptrs.to_vec())
    }

    pub fn kernel_signatures(&self, kernel_name: &str) -> HashSet<Vec<spirv::SPIRVKernelArg>> {
        let build = self.build_info();
        let devs = build.devs_with_build();

        if devs.is_empty() {
            return HashSet::new();
        }

        devs.iter().map(|d| build.args(d, kernel_name)).collect()
    }

    pub fn kernels(&self) -> Vec<String> {
        self.build_info().kernels.clone()
    }

    pub fn active_kernels(&self) -> bool {
        self.build_info()
            .builds
            .values()
            .any(|b| b.kernels.values().any(|b| Arc::strong_count(b) > 1))
    }

    pub fn build(&self, dev: &Device, options: String) -> bool {
        let lib = options.contains("-create-library");
        let mut info = self.build_info();
        if !self.do_compile(dev, options, &Vec::new(), &mut info) {
            return false;
        }

        let d = info.dev_build_mut(dev);

        // skip compilation if we already have the right thing.
        if self.is_bin() {
            if d.bin_type == CL_PROGRAM_BINARY_TYPE_EXECUTABLE && !lib
                || d.bin_type == CL_PROGRAM_BINARY_TYPE_LIBRARY && lib
            {
                return true;
            }
        }

        let spirvs = [d.spirv.as_ref().unwrap()];
        let (spirv, log) = spirv::SPIRVBin::link(&spirvs, lib);

        d.log.push_str(&log);
        d.spirv = spirv;
        if let Some(spirv) = &d.spirv {
            d.bin_type = if lib {
                CL_PROGRAM_BINARY_TYPE_LIBRARY
            } else {
                CL_PROGRAM_BINARY_TYPE_EXECUTABLE
            };
            d.status = CL_BUILD_SUCCESS as cl_build_status;
            let mut kernels = spirv.kernels();
            info.kernels.append(&mut kernels);
            info.build_nirs(self.is_src());
            true
        } else {
            d.status = CL_BUILD_ERROR;
            d.bin_type = CL_PROGRAM_BINARY_TYPE_NONE;
            false
        }
    }

    fn do_compile(
        &self,
        dev: &Device,
        options: String,
        headers: &[spirv::CLCHeader],
        info: &mut MutexGuard<ProgramBuild>,
    ) -> bool {
        let d = info.dev_build_mut(dev);

        let val_options = clc_validator_options(dev);
        let (spirv, log) = match &self.src {
            ProgramSourceType::Il(spirv) => {
                if Platform::dbg().allow_invalid_spirv {
                    (Some(spirv.clone()), String::new())
                } else {
                    spirv.clone_on_validate(&val_options)
                }
            }
            ProgramSourceType::Src(src) => {
                let args = prepare_options(&options, dev);

                if Platform::dbg().clc {
                    let src = src.to_string_lossy();
                    eprintln!("dumping compilation inputs:");
                    eprintln!("compilation arguments: {args:?}");
                    if !headers.is_empty() {
                        eprintln!("headers: {headers:#?}");
                    }
                    eprintln!("source code:\n{src}");
                }

                let (spirv, msgs) = spirv::SPIRVBin::from_clc(
                    src,
                    &args,
                    headers,
                    get_disk_cache(),
                    dev.cl_features(),
                    &dev.spirv_extensions,
                    dev.address_bits(),
                );

                if Platform::dbg().validate_spirv {
                    if let Some(spirv) = spirv {
                        let (res, spirv_msgs) = spirv.validate(&val_options);
                        (res.then_some(spirv), format!("{}\n{}", msgs, spirv_msgs))
                    } else {
                        (None, msgs)
                    }
                } else {
                    (spirv, msgs)
                }
            }
            // do nothing if we got a library or binary
            _ => {
                return true;
            }
        };

        d.spirv = spirv;
        d.log = log;
        d.options = options;

        if d.spirv.is_some() {
            d.status = CL_BUILD_SUCCESS as cl_build_status;
            d.bin_type = CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT;
            true
        } else {
            d.status = CL_BUILD_ERROR;
            false
        }
    }

    pub fn compile(&self, dev: &Device, options: String, headers: &[spirv::CLCHeader]) -> bool {
        self.do_compile(dev, options, headers, &mut self.build_info())
    }

    pub fn link(
        context: Arc<Context>,
        devs: &[&'static Device],
        progs: &[Arc<Program>],
        options: String,
    ) -> Arc<Program> {
        let mut builds = HashMap::new();
        let mut kernels = HashSet::new();
        let mut locks: Vec<_> = progs.iter().map(|p| p.build_info()).collect();
        let lib = options.contains("-create-library");

        for &d in devs {
            let bins: Vec<_> = locks
                .iter_mut()
                .map(|l| l.dev_build(d).spirv.as_ref().unwrap())
                .collect();

            let (spirv, log) = spirv::SPIRVBin::link(&bins, lib);
            let (spirv, log) = if Platform::dbg().validate_spirv {
                if let Some(spirv) = spirv {
                    let val_options = clc_validator_options(d);
                    let (res, spirv_msgs) = spirv.validate(&val_options);
                    (res.then_some(spirv), format!("{}\n{}", log, spirv_msgs))
                } else {
                    (None, log)
                }
            } else {
                (spirv, log)
            };

            let status;
            let bin_type;
            if let Some(spirv) = &spirv {
                for k in spirv.kernels() {
                    kernels.insert(k);
                }
                status = CL_BUILD_SUCCESS as cl_build_status;
                bin_type = if lib {
                    CL_PROGRAM_BINARY_TYPE_LIBRARY
                } else {
                    CL_PROGRAM_BINARY_TYPE_EXECUTABLE
                };
            } else {
                status = CL_BUILD_ERROR;
                bin_type = CL_PROGRAM_BINARY_TYPE_NONE;
            };

            builds.insert(
                d,
                ProgramDevBuild {
                    spirv: spirv,
                    status: status,
                    log: log,
                    options: String::from(""),
                    bin_type: bin_type,
                    kernels: HashMap::new(),
                },
            );
        }

        let mut build = ProgramBuild {
            builds: builds,
            spec_constants: HashMap::new(),
            kernels: kernels.into_iter().collect(),
            kernel_info: HashMap::new(),
        };

        // Pre build nir kernels
        build.build_nirs(false);

        Arc::new(Self {
            base: CLObjectBase::new(RusticlTypes::Program),
            context: context,
            devs: devs.to_owned(),
            src: ProgramSourceType::Linked,
            build: Mutex::new(build),
        })
    }

    pub fn is_bin(&self) -> bool {
        matches!(self.src, ProgramSourceType::Binary)
    }

    pub fn is_il(&self) -> bool {
        matches!(self.src, ProgramSourceType::Il(_))
    }

    pub fn is_src(&self) -> bool {
        matches!(self.src, ProgramSourceType::Src(_))
    }

    pub fn get_spec_constant_size(&self, spec_id: u32) -> u8 {
        match &self.src {
            ProgramSourceType::Il(il) => il
                .spec_constant(spec_id)
                .map_or(0, spirv::CLCSpecConstantType::size),
            _ => unreachable!(),
        }
    }

    pub fn set_spec_constant(&self, spec_id: u32, data: &[u8]) {
        let mut lock = self.build_info();
        let mut val = nir_const_value::default();

        match data.len() {
            1 => val.u8_ = u8::from_ne_bytes(data.try_into().unwrap()),
            2 => val.u16_ = u16::from_ne_bytes(data.try_into().unwrap()),
            4 => val.u32_ = u32::from_ne_bytes(data.try_into().unwrap()),
            8 => val.u64_ = u64::from_ne_bytes(data.try_into().unwrap()),
            _ => unreachable!("Spec constant with invalid size!"),
        };

        lock.spec_constants.insert(spec_id, val);
    }
}
