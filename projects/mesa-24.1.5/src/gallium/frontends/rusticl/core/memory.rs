use crate::api::icd::*;
use crate::api::types::*;
use crate::api::util::*;
use crate::core::context::*;
use crate::core::device::*;
use crate::core::format::*;
use crate::core::gl::*;
use crate::core::queue::*;
use crate::core::util::*;
use crate::impl_cl_type_trait;
use crate::impl_cl_type_trait_base;

use mesa_rust::pipe::context::*;
use mesa_rust::pipe::resource::*;
use mesa_rust::pipe::screen::ResourceType;
use mesa_rust::pipe::transfer::*;
use mesa_rust_gen::*;
use mesa_rust_util::math::*;
use mesa_rust_util::properties::Properties;
use mesa_rust_util::ptr::AllocSize;
use mesa_rust_util::ptr::TrackedPointers;
use rusticl_opencl_gen::*;

use std::alloc;
use std::alloc::Layout;
use std::cmp;
use std::collections::btree_map::Entry;
use std::collections::HashMap;
use std::convert::TryInto;
use std::mem;
use std::mem::size_of;
use std::ops::Deref;
use std::os::raw::c_void;
use std::ptr;
use std::sync::Arc;
use std::sync::Mutex;

struct Mapping<T> {
    layout: Layout,
    writes: bool,
    ptr: Option<MutMemoryPtr>,
    count: u32,
    inner: T,
}

impl<T> Drop for Mapping<T> {
    fn drop(&mut self) {
        if let Some(ptr) = &self.ptr {
            unsafe {
                alloc::dealloc(ptr.as_ptr().cast(), self.layout);
            }
        }
    }
}

impl<T> AllocSize<usize> for Mapping<T> {
    fn size(&self) -> usize {
        self.layout.size()
    }
}

impl<T> Deref for Mapping<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.inner
    }
}

struct BufferMapping {
    offset: usize,
}

struct ImageMapping {
    origin: CLVec<usize>,
    region: CLVec<usize>,
}

#[repr(transparent)]
#[derive(Clone, Copy)]
pub struct ConstMemoryPtr {
    ptr: *const c_void,
}
unsafe impl Send for ConstMemoryPtr {}
unsafe impl Sync for ConstMemoryPtr {}

impl ConstMemoryPtr {
    pub fn as_ptr(&self) -> *const c_void {
        self.ptr
    }

    /// # Safety
    ///
    /// Users need to ensure that `ptr` is only accessed in a thread-safe manner sufficient for
    /// [Send] and [Sync]
    pub unsafe fn from_ptr(ptr: *const c_void) -> Self {
        Self { ptr: ptr }
    }
}

impl From<MutMemoryPtr> for ConstMemoryPtr {
    fn from(value: MutMemoryPtr) -> Self {
        Self {
            ptr: value.ptr.cast(),
        }
    }
}

#[repr(transparent)]
#[derive(Clone, Copy)]
pub struct MutMemoryPtr {
    ptr: *mut c_void,
}
unsafe impl Send for MutMemoryPtr {}
unsafe impl Sync for MutMemoryPtr {}

impl MutMemoryPtr {
    pub fn as_ptr(&self) -> *mut c_void {
        self.ptr
    }

    /// # Safety
    ///
    /// Users need to ensure that `ptr` is only accessed in a thread-safe manner sufficient for
    /// [Send] and [Sync]
    pub unsafe fn from_ptr(ptr: *mut c_void) -> Self {
        Self { ptr: ptr }
    }
}

pub enum Mem {
    Buffer(Arc<Buffer>),
    Image(Arc<Image>),
}

impl Deref for Mem {
    type Target = MemBase;

    fn deref(&self) -> &Self::Target {
        match self {
            Self::Buffer(b) => &b.base,
            Self::Image(i) => &i.base,
        }
    }
}

impl Mem {
    pub fn is_mapped_ptr(&self, ptr: *mut c_void) -> bool {
        match self {
            Self::Buffer(b) => b.is_mapped_ptr(ptr),
            Self::Image(i) => i.is_mapped_ptr(ptr),
        }
    }

    pub fn unmap(&self, q: &Queue, ctx: &PipeContext, ptr: MutMemoryPtr) -> CLResult<()> {
        match self {
            Self::Buffer(b) => b.unmap(q, ctx, ptr),
            Self::Image(i) => i.unmap(q, ctx, ptr),
        }
    }
}

/// # Mapping memory
///
/// Maps the queue associated device's resource.
///
/// Mapping resources could have been quite straightforward if OpenCL wouldn't allow for so called
/// non blocking maps. Non blocking maps shall return a valid pointer to the mapped region
/// immediately, but should not synchronize data (in case of shadow buffers) until after the map
/// event is reached in the queue. This makes it not possible to simply use pipe_transfers as those
/// can't be explicitly synced by the frontend.
///
/// In order to have a compliant implementation of the mapping API we have to consider the following
/// cases:
///   1. Mapping a cl_mem object with CL_MEM_USE_HOST_PTR: We simply return the host_ptr.
///      Synchronization of shadowed host ptrs are done in `sync_shadow` on demand.
///   2. Mapping linear resources on UMA systems: We simply create the pipe_transfer with
///      `PIPE_MAP_DIRECTLY` and `PIPE_MAP_UNSYNCHRONIZED` and return the attached pointer.
///   3. On non UMA systems or when 2. fails (e.g. due to the resource being tiled) we
///      - create a shadow pipe_resource with `PIPE_USAGE_STAGING`,
///        `PIPE_RESOURCE_FLAG_MAP_PERSISTENT` and `PIPE_RESOURCE_FLAG_MAP_COHERENT`
///      - create a pipe_transfer with `PIPE_MAP_COHERENT`, `PIPE_MAP_PERSISTENT` and
///        `PIPE_MAP_UNSYNCHRONIZED`
///      - sync the shadow buffer like a host_ptr shadow buffer in 1.
///
/// Taking this approach we guarentee that we only copy when actually needed while making sure the
/// content behind the returned pointer is valid until unmapped.
pub struct MemBase {
    pub base: CLObjectBase<CL_INVALID_MEM_OBJECT>,
    pub context: Arc<Context>,
    pub parent: Option<Mem>,
    pub mem_type: cl_mem_object_type,
    pub flags: cl_mem_flags,
    pub size: usize,
    // it's a bit hacky, but storing the pointer as `usize` gives us `Send` and `Sync`. The
    // application is required to ensure no data races exist on the memory anyway.
    pub host_ptr: usize,
    pub props: Vec<cl_mem_properties>,
    pub cbs: Mutex<Vec<MemCB>>,
    pub gl_obj: Option<GLObject>,
    res: Option<HashMap<&'static Device, Arc<PipeResource>>>,
}

pub struct Buffer {
    base: MemBase,
    pub offset: usize,
    maps: Mutex<TrackedPointers<usize, Mapping<BufferMapping>>>,
}

pub struct Image {
    base: MemBase,
    pub image_format: cl_image_format,
    pub pipe_format: pipe_format,
    pub image_desc: cl_image_desc,
    pub image_elem_size: u8,
    maps: Mutex<TrackedPointers<usize, Mapping<ImageMapping>>>,
}

impl Deref for Buffer {
    type Target = MemBase;

    fn deref(&self) -> &Self::Target {
        &self.base
    }
}

impl Deref for Image {
    type Target = MemBase;

    fn deref(&self) -> &Self::Target {
        &self.base
    }
}

impl_cl_type_trait_base!(cl_mem, MemBase, [Buffer, Image], CL_INVALID_MEM_OBJECT);
impl_cl_type_trait!(cl_mem, Buffer, CL_INVALID_MEM_OBJECT, base.base);
impl_cl_type_trait!(cl_mem, Image, CL_INVALID_MEM_OBJECT, base.base);

pub trait CLImageDescInfo {
    fn type_info(&self) -> (u8, bool);
    fn pixels(&self) -> usize;
    fn bx(&self) -> CLResult<pipe_box>;
    fn row_pitch(&self) -> CLResult<u32>;
    fn slice_pitch(&self) -> usize;
    fn width(&self) -> CLResult<u32>;
    fn height(&self) -> CLResult<u32>;
    fn size(&self) -> CLVec<usize>;

    fn dims(&self) -> u8 {
        self.type_info().0
    }

    fn dims_with_array(&self) -> u8 {
        let array: u8 = self.is_array().into();
        self.dims() + array
    }

    fn has_slice(&self) -> bool {
        self.dims() == 3 || self.is_array()
    }

    fn is_array(&self) -> bool {
        self.type_info().1
    }
}

impl CLImageDescInfo for cl_image_desc {
    fn type_info(&self) -> (u8, bool) {
        match self.image_type {
            CL_MEM_OBJECT_IMAGE1D | CL_MEM_OBJECT_IMAGE1D_BUFFER => (1, false),
            CL_MEM_OBJECT_IMAGE1D_ARRAY => (1, true),
            CL_MEM_OBJECT_IMAGE2D => (2, false),
            CL_MEM_OBJECT_IMAGE2D_ARRAY => (2, true),
            CL_MEM_OBJECT_IMAGE3D => (3, false),
            _ => panic!("unknown image_type {:x}", self.image_type),
        }
    }

    fn pixels(&self) -> usize {
        let mut res = self.image_width;
        let dims = self.dims();

        if dims > 1 {
            res *= self.image_height;
        }

        if dims > 2 {
            res *= self.image_depth;
        }

        if self.is_array() {
            res *= self.image_array_size;
        }

        res
    }

    fn size(&self) -> CLVec<usize> {
        let mut height = cmp::max(self.image_height, 1);
        let mut depth = cmp::max(self.image_depth, 1);

        match self.image_type {
            CL_MEM_OBJECT_IMAGE1D_ARRAY => height = self.image_array_size,
            CL_MEM_OBJECT_IMAGE2D_ARRAY => depth = self.image_array_size,
            _ => {}
        }

        CLVec::new([self.image_width, height, depth])
    }

    fn bx(&self) -> CLResult<pipe_box> {
        create_pipe_box(CLVec::default(), self.size(), self.image_type)
    }

    fn row_pitch(&self) -> CLResult<u32> {
        self.image_row_pitch
            .try_into()
            .map_err(|_| CL_OUT_OF_HOST_MEMORY)
    }

    fn slice_pitch(&self) -> usize {
        self.image_slice_pitch
    }

    fn width(&self) -> CLResult<u32> {
        self.image_width
            .try_into()
            .map_err(|_| CL_OUT_OF_HOST_MEMORY)
    }

    fn height(&self) -> CLResult<u32> {
        self.image_height
            .try_into()
            .map_err(|_| CL_OUT_OF_HOST_MEMORY)
    }
}

fn sw_copy(
    src: *const c_void,
    dst: *mut c_void,
    region: &CLVec<usize>,
    src_origin: &CLVec<usize>,
    src_row_pitch: usize,
    src_slice_pitch: usize,
    dst_origin: &CLVec<usize>,
    dst_row_pitch: usize,
    dst_slice_pitch: usize,
    pixel_size: u8,
) {
    for z in 0..region[2] {
        for y in 0..region[1] {
            unsafe {
                ptr::copy(
                    src.add(
                        (*src_origin + [0, y, z])
                            * [pixel_size as usize, src_row_pitch, src_slice_pitch],
                    ),
                    dst.add(
                        (*dst_origin + [0, y, z])
                            * [pixel_size as usize, dst_row_pitch, dst_slice_pitch],
                    ),
                    region[0] * pixel_size as usize,
                )
            };
        }
    }
}

impl MemBase {
    pub fn new_buffer(
        context: Arc<Context>,
        flags: cl_mem_flags,
        size: usize,
        host_ptr: *mut c_void,
        props: Vec<cl_mem_properties>,
    ) -> CLResult<Arc<Buffer>> {
        let res_type = if bit_check(flags, CL_MEM_ALLOC_HOST_PTR) {
            ResourceType::Staging
        } else {
            ResourceType::Normal
        };

        let buffer = context.create_buffer(
            size,
            host_ptr,
            bit_check(flags, CL_MEM_COPY_HOST_PTR),
            res_type,
        )?;

        let host_ptr = if bit_check(flags, CL_MEM_USE_HOST_PTR) {
            host_ptr as usize
        } else {
            0
        };

        Ok(Arc::new(Buffer {
            base: Self {
                base: CLObjectBase::new(RusticlTypes::Buffer),
                context: context,
                parent: None,
                mem_type: CL_MEM_OBJECT_BUFFER,
                flags: flags,
                size: size,
                host_ptr: host_ptr,
                props: props,
                gl_obj: None,
                cbs: Mutex::new(Vec::new()),
                res: Some(buffer),
            },
            offset: 0,
            maps: Mutex::new(TrackedPointers::new()),
        }))
    }

    pub fn new_sub_buffer(
        parent: Arc<Buffer>,
        flags: cl_mem_flags,
        offset: usize,
        size: usize,
    ) -> Arc<Buffer> {
        let host_ptr = if parent.host_ptr().is_null() {
            0
        } else {
            unsafe { parent.host_ptr().add(offset) as usize }
        };

        Arc::new(Buffer {
            base: Self {
                base: CLObjectBase::new(RusticlTypes::Buffer),
                context: parent.context.clone(),
                parent: Some(Mem::Buffer(parent)),
                mem_type: CL_MEM_OBJECT_BUFFER,
                flags: flags,
                size: size,
                host_ptr: host_ptr,
                props: Vec::new(),
                gl_obj: None,
                cbs: Mutex::new(Vec::new()),
                res: None,
            },
            offset: offset,
            maps: Mutex::new(TrackedPointers::new()),
        })
    }

    pub fn new_image(
        context: Arc<Context>,
        parent: Option<Mem>,
        mem_type: cl_mem_object_type,
        flags: cl_mem_flags,
        image_format: &cl_image_format,
        mut image_desc: cl_image_desc,
        image_elem_size: u8,
        host_ptr: *mut c_void,
        props: Vec<cl_mem_properties>,
    ) -> CLResult<Arc<Image>> {
        // we have to sanitize the image_desc a little for internal use
        let api_image_desc = image_desc;
        let dims = image_desc.dims();
        let is_array = image_desc.is_array();
        if dims < 3 {
            image_desc.image_depth = 1;
        }
        if dims < 2 {
            image_desc.image_height = 1;
        }
        if !is_array {
            image_desc.image_array_size = 1;
        }

        let res_type = if bit_check(flags, CL_MEM_ALLOC_HOST_PTR) {
            ResourceType::Staging
        } else {
            ResourceType::Normal
        };

        let texture = if parent.is_none() {
            let mut texture = context.create_texture(
                &image_desc,
                image_format,
                host_ptr,
                bit_check(flags, CL_MEM_COPY_HOST_PTR),
                res_type,
            );

            // if we error allocating a Staging resource, just try with normal as
            // `CL_MEM_ALLOC_HOST_PTR` is just a performance hint.
            if res_type == ResourceType::Staging && texture.is_err() {
                texture = context.create_texture(
                    &image_desc,
                    image_format,
                    host_ptr,
                    bit_check(flags, CL_MEM_COPY_HOST_PTR),
                    ResourceType::Normal,
                )
            }

            Some(texture?)
        } else {
            None
        };

        let host_ptr = if bit_check(flags, CL_MEM_USE_HOST_PTR) {
            host_ptr as usize
        } else {
            0
        };

        let pipe_format = image_format.to_pipe_format().unwrap();
        Ok(Arc::new(Image {
            base: Self {
                base: CLObjectBase::new(RusticlTypes::Image),
                context: context,
                parent: parent,
                mem_type: mem_type,
                flags: flags,
                size: image_desc.pixels() * image_format.pixel_size().unwrap() as usize,
                host_ptr: host_ptr,
                props: props,
                gl_obj: None,
                cbs: Mutex::new(Vec::new()),
                res: texture,
            },
            image_format: *image_format,
            pipe_format: pipe_format,
            image_desc: api_image_desc,
            image_elem_size: image_elem_size,
            maps: Mutex::new(TrackedPointers::new()),
        }))
    }

    pub fn arc_from_raw(ptr: cl_mem) -> CLResult<Mem> {
        let mem = Self::ref_from_raw(ptr)?;
        match mem.base.get_type()? {
            RusticlTypes::Buffer => Ok(Mem::Buffer(Buffer::arc_from_raw(ptr)?)),
            RusticlTypes::Image => Ok(Mem::Image(Image::arc_from_raw(ptr)?)),
            _ => Err(CL_INVALID_MEM_OBJECT),
        }
    }

    pub fn arcs_from_arr(objs: *const cl_mem, count: u32) -> CLResult<Vec<Mem>> {
        let count = count as usize;
        let mut res = Vec::with_capacity(count);
        for i in 0..count {
            res.push(Self::arc_from_raw(unsafe { *objs.add(i) })?);
        }
        Ok(res)
    }

    pub fn from_gl(
        context: Arc<Context>,
        flags: cl_mem_flags,
        gl_export_manager: &GLExportManager,
    ) -> CLResult<cl_mem> {
        let export_in = &gl_export_manager.export_in;
        let export_out = &gl_export_manager.export_out;

        let (mem_type, gl_object_type) = target_from_gl(export_in.target)?;
        let gl_mem_props = gl_export_manager.get_gl_mem_props()?;

        // Handle Buffers
        let (image_format, pipe_format, rusticl_type) = if gl_export_manager.is_gl_buffer() {
            (
                cl_image_format::default(),
                pipe_format::PIPE_FORMAT_NONE,
                RusticlTypes::Buffer,
            )
        } else {
            let image_format =
                format_from_gl(export_out.internal_format).ok_or(CL_OUT_OF_HOST_MEMORY)?;
            (
                image_format,
                image_format.to_pipe_format().unwrap(),
                RusticlTypes::Image,
            )
        };

        let imported_gl_tex = context.import_gl_buffer(
            export_out.dmabuf_fd as u32,
            export_out.modifier,
            mem_type,
            export_in.target,
            pipe_format,
            gl_mem_props.clone(),
        )?;

        // Cube maps faces are not linear in memory, so copy all contents
        // of desired face into a 2D image and copy it back after gl release.
        let (shadow_map, texture) = if is_cube_map_face(export_in.target) {
            let shadow = create_shadow_slice(&imported_gl_tex, image_format)?;

            let mut res_map = HashMap::new();
            shadow
                .iter()
                .map(|(k, v)| {
                    let gl_res = imported_gl_tex.get(k).unwrap().clone();
                    res_map.insert(v.clone(), gl_res);
                })
                .for_each(drop);

            (Some(res_map), shadow)
        } else {
            (None, imported_gl_tex)
        };

        // it's kinda not supported, but we want to know if anything actually hits this as it's
        // certainly not tested by the CL CTS.
        if mem_type != CL_MEM_OBJECT_BUFFER {
            assert_eq!(gl_mem_props.offset, 0);
        }

        let base = Self {
            base: CLObjectBase::new(rusticl_type),
            context: context,
            parent: None,
            mem_type: mem_type,
            flags: flags,
            size: gl_mem_props.size(),
            host_ptr: 0,
            props: Vec::new(),
            gl_obj: Some(GLObject {
                gl_object_target: gl_export_manager.export_in.target,
                gl_object_type: gl_object_type,
                gl_object_name: export_in.obj,
                shadow_map: shadow_map,
            }),
            cbs: Mutex::new(Vec::new()),
            res: Some(texture),
        };

        Ok(if rusticl_type == RusticlTypes::Buffer {
            Arc::new(Buffer {
                base: base,
                offset: gl_mem_props.offset as usize,
                maps: Mutex::new(TrackedPointers::new()),
            })
            .into_cl()
        } else {
            Arc::new(Image {
                base: base,
                image_format: image_format,
                pipe_format: pipe_format,
                image_desc: cl_image_desc {
                    image_type: mem_type,
                    image_width: gl_mem_props.width as usize,
                    image_height: gl_mem_props.height as usize,
                    image_depth: gl_mem_props.depth as usize,
                    image_array_size: gl_mem_props.array_size as usize,
                    image_row_pitch: 0,
                    image_slice_pitch: 0,
                    num_mip_levels: 1,
                    num_samples: 1,
                    ..Default::default()
                },
                image_elem_size: gl_mem_props.pixel_size,
                maps: Mutex::new(TrackedPointers::new()),
            })
            .into_cl()
        })
    }

    pub fn is_buffer(&self) -> bool {
        self.mem_type == CL_MEM_OBJECT_BUFFER
    }

    pub fn has_same_parent(&self, other: &Self) -> bool {
        ptr::eq(self.get_parent(), other.get_parent())
    }

    // this is kinda bogus, because that won't work with system SVM, but the spec wants us to
    // implement this.
    pub fn is_svm(&self) -> bool {
        let mem = self.get_parent();
        self.context.find_svm_alloc(mem.host_ptr).is_some()
            && bit_check(mem.flags, CL_MEM_USE_HOST_PTR)
    }

    pub fn get_res_of_dev(&self, dev: &Device) -> CLResult<&Arc<PipeResource>> {
        self.get_parent()
            .res
            .as_ref()
            .and_then(|resources| resources.get(dev))
            .ok_or(CL_OUT_OF_HOST_MEMORY)
    }

    fn get_parent(&self) -> &Self {
        if let Some(parent) = &self.parent {
            parent
        } else {
            self
        }
    }

    pub fn host_ptr(&self) -> *mut c_void {
        self.host_ptr as *mut c_void
    }

    fn is_pure_user_memory(&self, d: &Device) -> CLResult<bool> {
        let r = self.get_res_of_dev(d)?;
        Ok(r.is_user)
    }

    fn map<T>(
        &self,
        offset: usize,
        layout: Layout,
        writes: bool,
        maps: &Mutex<TrackedPointers<usize, Mapping<T>>>,
        inner: T,
    ) -> CLResult<MutMemoryPtr> {
        let host_ptr = self.host_ptr();
        let ptr = unsafe {
            let ptr = if !host_ptr.is_null() {
                host_ptr.add(offset)
            } else {
                alloc::alloc(layout).cast()
            };

            MutMemoryPtr::from_ptr(ptr)
        };

        match maps.lock().unwrap().entry(ptr.as_ptr() as usize) {
            Entry::Occupied(mut e) => {
                debug_assert!(!host_ptr.is_null());
                e.get_mut().count += 1;
            }
            Entry::Vacant(e) => {
                e.insert(Mapping {
                    layout: layout,
                    writes: writes,
                    ptr: host_ptr.is_null().then_some(ptr),
                    count: 1,
                    inner: inner,
                });
            }
        }

        Ok(ptr)
    }
}

impl Drop for MemBase {
    fn drop(&mut self) {
        let cbs = mem::take(self.cbs.get_mut().unwrap());
        for cb in cbs.into_iter().rev() {
            cb.call(self);
        }
    }
}

impl Buffer {
    fn apply_offset(&self, offset: usize) -> CLResult<usize> {
        self.offset.checked_add(offset).ok_or(CL_OUT_OF_HOST_MEMORY)
    }

    pub fn copy_rect(
        &self,
        dst: &Self,
        q: &Queue,
        ctx: &PipeContext,
        region: &CLVec<usize>,
        src_origin: &CLVec<usize>,
        src_row_pitch: usize,
        src_slice_pitch: usize,
        dst_origin: &CLVec<usize>,
        dst_row_pitch: usize,
        dst_slice_pitch: usize,
    ) -> CLResult<()> {
        let (offset, size) =
            CLVec::calc_offset_size(src_origin, region, [1, src_row_pitch, src_slice_pitch]);
        let tx_src = self.tx(q, ctx, offset, size, RWFlags::RD)?;

        let (offset, size) =
            CLVec::calc_offset_size(dst_origin, region, [1, dst_row_pitch, dst_slice_pitch]);
        let tx_dst = dst.tx(q, ctx, offset, size, RWFlags::WR)?;

        // TODO check to use hw accelerated paths (e.g. resource_copy_region or blits)
        sw_copy(
            tx_src.ptr(),
            tx_dst.ptr(),
            region,
            &CLVec::default(),
            src_row_pitch,
            src_slice_pitch,
            &CLVec::default(),
            dst_row_pitch,
            dst_slice_pitch,
            1,
        );

        Ok(())
    }

    pub fn copy_to_buffer(
        &self,
        q: &Queue,
        ctx: &PipeContext,
        dst: &Buffer,
        src_offset: usize,
        dst_offset: usize,
        size: usize,
    ) -> CLResult<()> {
        let src_offset = self.apply_offset(src_offset)?;
        let dst_offset = dst.apply_offset(dst_offset)?;
        let src_res = self.get_res_of_dev(q.device)?;
        let dst_res = dst.get_res_of_dev(q.device)?;

        let bx = create_pipe_box(
            [src_offset, 0, 0].into(),
            [size, 1, 1].into(),
            CL_MEM_OBJECT_BUFFER,
        )?;
        let dst_origin: [u32; 3] = [
            dst_offset.try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
            0,
            0,
        ];

        ctx.resource_copy_region(src_res, dst_res, &dst_origin, &bx);
        Ok(())
    }

    pub fn copy_to_image(
        &self,
        q: &Queue,
        ctx: &PipeContext,
        dst: &Image,
        src_offset: usize,
        dst_origin: CLVec<usize>,
        region: &CLVec<usize>,
    ) -> CLResult<()> {
        let src_offset = self.apply_offset(src_offset)?;
        let bpp = dst.image_format.pixel_size().unwrap().into();
        let src_pitch = [bpp, bpp * region[0], bpp * region[0] * region[1]];
        let size = CLVec::calc_size(region, src_pitch);
        let tx_src = self.tx(q, ctx, src_offset, size, RWFlags::RD)?;

        // If image is created from a buffer, use image's slice and row pitch instead
        let tx_dst;
        let dst_pitch;
        if let Some(Mem::Buffer(buffer)) = &dst.parent {
            dst_pitch = [
                bpp,
                dst.image_desc.row_pitch()? as usize,
                dst.image_desc.slice_pitch(),
            ];

            let (offset, size) = CLVec::calc_offset_size(dst_origin, region, dst_pitch);
            tx_dst = buffer.tx(q, ctx, offset, size, RWFlags::WR)?;
        } else {
            tx_dst = dst.tx_image(
                q,
                ctx,
                &create_pipe_box(dst_origin, *region, dst.mem_type)?,
                RWFlags::WR,
            )?;

            dst_pitch = [1, tx_dst.row_pitch() as usize, tx_dst.slice_pitch()];
        }

        // Those pitch values cannot have 0 value in its coordinates
        debug_assert!(src_pitch[0] != 0 && src_pitch[1] != 0 && src_pitch[2] != 0);
        debug_assert!(dst_pitch[0] != 0 && dst_pitch[1] != 0 && dst_pitch[2] != 0);

        sw_copy(
            tx_src.ptr(),
            tx_dst.ptr(),
            region,
            &CLVec::default(),
            src_pitch[1],
            src_pitch[2],
            &CLVec::default(),
            dst_pitch[1],
            dst_pitch[2],
            bpp as u8,
        );
        Ok(())
    }

    pub fn fill(
        &self,
        q: &Queue,
        ctx: &PipeContext,
        pattern: &[u8],
        offset: usize,
        size: usize,
    ) -> CLResult<()> {
        let offset = self.apply_offset(offset)?;
        let res = self.get_res_of_dev(q.device)?;
        ctx.clear_buffer(
            res,
            pattern,
            offset.try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
            size.try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
        );
        Ok(())
    }

    fn is_mapped_ptr(&self, ptr: *mut c_void) -> bool {
        self.maps.lock().unwrap().contains_key(ptr as usize)
    }

    pub fn map(&self, size: usize, offset: usize, writes: bool) -> CLResult<MutMemoryPtr> {
        let layout =
            unsafe { Layout::from_size_align_unchecked(size, size_of::<[cl_ulong; 16]>()) };
        self.base.map(
            offset,
            layout,
            writes,
            &self.maps,
            BufferMapping { offset: offset },
        )
    }

    pub fn read(
        &self,
        q: &Queue,
        ctx: &PipeContext,
        offset: usize,
        ptr: MutMemoryPtr,
        size: usize,
    ) -> CLResult<()> {
        let ptr = ptr.as_ptr();
        let tx = self.tx(q, ctx, offset, size, RWFlags::RD)?;

        unsafe {
            ptr::copy(tx.ptr(), ptr, size);
        }

        Ok(())
    }

    pub fn read_rect(
        &self,
        dst: MutMemoryPtr,
        q: &Queue,
        ctx: &PipeContext,
        region: &CLVec<usize>,
        src_origin: &CLVec<usize>,
        src_row_pitch: usize,
        src_slice_pitch: usize,
        dst_origin: &CLVec<usize>,
        dst_row_pitch: usize,
        dst_slice_pitch: usize,
    ) -> CLResult<()> {
        let dst = dst.as_ptr();
        let (offset, size) =
            CLVec::calc_offset_size(src_origin, region, [1, src_row_pitch, src_slice_pitch]);
        let tx = self.tx(q, ctx, offset, size, RWFlags::RD)?;

        sw_copy(
            tx.ptr(),
            dst,
            region,
            &CLVec::default(),
            src_row_pitch,
            src_slice_pitch,
            dst_origin,
            dst_row_pitch,
            dst_slice_pitch,
            1,
        );

        Ok(())
    }

    pub fn sync_map(&self, q: &Queue, ctx: &PipeContext, ptr: MutMemoryPtr) -> CLResult<()> {
        // no need to update
        if self.is_pure_user_memory(q.device)? {
            return Ok(());
        }

        let maps = self.maps.lock().unwrap();
        let Some(mapping) = maps.find_alloc_precise(ptr.as_ptr() as usize) else {
            return Err(CL_INVALID_VALUE);
        };

        self.read(q, ctx, mapping.offset, ptr, mapping.size())
    }

    fn tx<'a>(
        &self,
        q: &Queue,
        ctx: &'a PipeContext,
        offset: usize,
        size: usize,
        rw: RWFlags,
    ) -> CLResult<GuardedPipeTransfer<'a>> {
        let offset = self.apply_offset(offset)?;
        let r = self.get_res_of_dev(q.device)?;

        Ok(ctx
            .buffer_map(
                r,
                offset.try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
                size.try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
                rw,
                ResourceMapType::Normal,
            )
            .ok_or(CL_OUT_OF_RESOURCES)?
            .with_ctx(ctx))
    }

    // TODO: only sync on unmap when the memory is not mapped for writing
    pub fn unmap(&self, q: &Queue, ctx: &PipeContext, ptr: MutMemoryPtr) -> CLResult<()> {
        let mapping = match self.maps.lock().unwrap().entry(ptr.as_ptr() as usize) {
            Entry::Vacant(_) => return Err(CL_INVALID_VALUE),
            Entry::Occupied(mut entry) => {
                entry.get_mut().count -= 1;
                (entry.get().count == 0).then(|| entry.remove())
            }
        };

        if let Some(mapping) = mapping {
            if mapping.writes && !self.is_pure_user_memory(q.device)? {
                self.write(q, ctx, mapping.offset, ptr.into(), mapping.size())?;
            }
        };

        Ok(())
    }

    pub fn write(
        &self,
        q: &Queue,
        ctx: &PipeContext,
        offset: usize,
        ptr: ConstMemoryPtr,
        size: usize,
    ) -> CLResult<()> {
        let ptr = ptr.as_ptr();
        let offset = self.apply_offset(offset)?;
        let r = self.get_res_of_dev(q.device)?;
        ctx.buffer_subdata(
            r,
            offset.try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
            ptr,
            size.try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
        );
        Ok(())
    }

    pub fn write_rect(
        &self,
        src: ConstMemoryPtr,
        q: &Queue,
        ctx: &PipeContext,
        region: &CLVec<usize>,
        src_origin: &CLVec<usize>,
        src_row_pitch: usize,
        src_slice_pitch: usize,
        dst_origin: &CLVec<usize>,
        dst_row_pitch: usize,
        dst_slice_pitch: usize,
    ) -> CLResult<()> {
        let src = src.as_ptr();
        let (offset, size) =
            CLVec::calc_offset_size(dst_origin, region, [1, dst_row_pitch, dst_slice_pitch]);
        let tx = self.tx(q, ctx, offset, size, RWFlags::WR)?;

        sw_copy(
            src,
            tx.ptr(),
            region,
            src_origin,
            src_row_pitch,
            src_slice_pitch,
            &CLVec::default(),
            dst_row_pitch,
            dst_slice_pitch,
            1,
        );

        Ok(())
    }
}

impl Image {
    pub fn copy_to_buffer(
        &self,
        q: &Queue,
        ctx: &PipeContext,
        dst: &Buffer,
        src_origin: CLVec<usize>,
        dst_offset: usize,
        region: &CLVec<usize>,
    ) -> CLResult<()> {
        let dst_offset = dst.apply_offset(dst_offset)?;
        let bpp = self.image_format.pixel_size().unwrap().into();

        let src_pitch;
        let tx_src;
        if let Some(Mem::Buffer(buffer)) = &self.parent {
            src_pitch = [
                bpp,
                self.image_desc.row_pitch()? as usize,
                self.image_desc.slice_pitch(),
            ];
            let (offset, size) = CLVec::calc_offset_size(src_origin, region, src_pitch);
            tx_src = buffer.tx(q, ctx, offset, size, RWFlags::RD)?;
        } else {
            tx_src = self.tx_image(
                q,
                ctx,
                &create_pipe_box(src_origin, *region, self.mem_type)?,
                RWFlags::RD,
            )?;
            src_pitch = [1, tx_src.row_pitch() as usize, tx_src.slice_pitch()];
        }

        // If image is created from a buffer, use image's slice and row pitch instead
        let dst_pitch = [bpp, bpp * region[0], bpp * region[0] * region[1]];

        let dst_origin: CLVec<usize> = [dst_offset, 0, 0].into();
        let (offset, size) = CLVec::calc_offset_size(dst_origin, region, dst_pitch);
        let tx_dst = dst.tx(q, ctx, offset, size, RWFlags::WR)?;

        // Those pitch values cannot have 0 value in its coordinates
        debug_assert!(src_pitch[0] != 0 && src_pitch[1] != 0 && src_pitch[2] != 0);
        debug_assert!(dst_pitch[0] != 0 && dst_pitch[1] != 0 && dst_pitch[2] != 0);

        sw_copy(
            tx_src.ptr(),
            tx_dst.ptr(),
            region,
            &CLVec::default(),
            src_pitch[1],
            src_pitch[2],
            &CLVec::default(),
            dst_pitch[1],
            dst_pitch[2],
            bpp as u8,
        );
        Ok(())
    }

    pub fn copy_to_image(
        &self,
        q: &Queue,
        ctx: &PipeContext,
        dst: &Image,
        src_origin: CLVec<usize>,
        dst_origin: CLVec<usize>,
        region: &CLVec<usize>,
    ) -> CLResult<()> {
        let src_parent = self.get_parent();
        let dst_parent = dst.get_parent();
        let src_res = src_parent.get_res_of_dev(q.device)?;
        let dst_res = dst_parent.get_res_of_dev(q.device)?;

        // We just want to use sw_copy if mem objects have different types or if copy can have
        // custom strides (image2d from buff/images)
        if src_parent.is_buffer() || dst_parent.is_buffer() {
            let bpp = self.image_format.pixel_size().unwrap().into();

            let tx_src;
            let tx_dst;
            let dst_pitch;
            let src_pitch;
            if let Some(Mem::Buffer(buffer)) = &self.parent {
                src_pitch = [
                    bpp,
                    self.image_desc.row_pitch()? as usize,
                    self.image_desc.slice_pitch(),
                ];

                let (offset, size) = CLVec::calc_offset_size(src_origin, region, src_pitch);
                tx_src = buffer.tx(q, ctx, offset, size, RWFlags::RD)?;
            } else {
                tx_src = self.tx_image(
                    q,
                    ctx,
                    &create_pipe_box(src_origin, *region, src_parent.mem_type)?,
                    RWFlags::RD,
                )?;

                src_pitch = [1, tx_src.row_pitch() as usize, tx_src.slice_pitch()];
            }

            if let Some(Mem::Buffer(buffer)) = &dst.parent {
                // If image is created from a buffer, use image's slice and row pitch instead
                dst_pitch = [
                    bpp,
                    dst.image_desc.row_pitch()? as usize,
                    dst.image_desc.slice_pitch(),
                ];

                let (offset, size) = CLVec::calc_offset_size(dst_origin, region, dst_pitch);
                tx_dst = buffer.tx(q, ctx, offset, size, RWFlags::WR)?;
            } else {
                tx_dst = dst.tx_image(
                    q,
                    ctx,
                    &create_pipe_box(dst_origin, *region, dst_parent.mem_type)?,
                    RWFlags::WR,
                )?;

                dst_pitch = [1, tx_dst.row_pitch() as usize, tx_dst.slice_pitch()];
            }

            // Those pitch values cannot have 0 value in its coordinates
            debug_assert!(src_pitch[0] != 0 && src_pitch[1] != 0 && src_pitch[2] != 0);
            debug_assert!(dst_pitch[0] != 0 && dst_pitch[1] != 0 && dst_pitch[2] != 0);

            sw_copy(
                tx_src.ptr(),
                tx_dst.ptr(),
                region,
                &CLVec::default(),
                src_pitch[1],
                src_pitch[2],
                &CLVec::default(),
                dst_pitch[1],
                dst_pitch[2],
                bpp as u8,
            )
        } else {
            let bx = create_pipe_box(src_origin, *region, src_parent.mem_type)?;
            let mut dst_origin: [u32; 3] = dst_origin.try_into()?;

            if src_parent.mem_type == CL_MEM_OBJECT_IMAGE1D_ARRAY {
                (dst_origin[1], dst_origin[2]) = (dst_origin[2], dst_origin[1]);
            }

            ctx.resource_copy_region(src_res, dst_res, &dst_origin, &bx);
        }
        Ok(())
    }

    pub fn fill(
        &self,
        q: &Queue,
        ctx: &PipeContext,
        pattern: &[u32],
        origin: &CLVec<usize>,
        region: &CLVec<usize>,
    ) -> CLResult<()> {
        let res = self.get_res_of_dev(q.device)?;

        // make sure we allocate multiples of 4 bytes so drivers don't read out of bounds or
        // unaligned.
        // TODO: use div_ceil once it's available
        let pixel_size = self.image_format.pixel_size().unwrap().into();
        let mut new_pattern: Vec<u32> = vec![0; div_round_up(pixel_size, size_of::<u32>())];

        // we don't support CL_DEPTH for now
        assert!(pattern.len() == 4);

        // SAFETY: pointers have to be valid for read/writes of exactly one pixel of their
        // respective format.
        // `new_pattern` has the correct size due to the `size` above.
        // `pattern` is validated through the CL API and allows undefined behavior if not followed
        // by CL API rules. It's expected to be a 4 component array of 32 bit values, except for
        // CL_DEPTH where it's just one value.
        unsafe {
            util_format_pack_rgba(
                self.pipe_format,
                new_pattern.as_mut_ptr().cast(),
                pattern.as_ptr().cast(),
                1,
            );
        }

        // If image is created from a buffer, use clear_image_buffer instead
        if self.is_parent_buffer() {
            let strides = (
                self.image_desc.row_pitch()? as usize,
                self.image_desc.slice_pitch(),
            );
            ctx.clear_image_buffer(res, &new_pattern, origin, region, strides, pixel_size);
        } else {
            let bx = create_pipe_box(*origin, *region, self.mem_type)?;
            ctx.clear_texture(res, &new_pattern, &bx);
        }

        Ok(())
    }

    fn is_mapped_ptr(&self, ptr: *mut c_void) -> bool {
        self.maps.lock().unwrap().contains_key(ptr as usize)
    }

    pub fn is_parent_buffer(&self) -> bool {
        matches!(self.parent, Some(Mem::Buffer(_)))
    }

    pub fn map(
        &self,
        origin: CLVec<usize>,
        region: CLVec<usize>,
        row_pitch: &mut usize,
        slice_pitch: &mut usize,
        writes: bool,
    ) -> CLResult<MutMemoryPtr> {
        let pixel_size = self.image_format.pixel_size().unwrap() as usize;

        *row_pitch = self.image_desc.row_pitch()? as usize;
        *slice_pitch = self.image_desc.slice_pitch();

        let (offset, size) =
            CLVec::calc_offset_size(origin, region, [pixel_size, *row_pitch, *slice_pitch]);

        let layout;
        unsafe {
            layout = Layout::from_size_align_unchecked(size, size_of::<[u32; 4]>());
        }

        self.base.map(
            offset,
            layout,
            writes,
            &self.maps,
            ImageMapping {
                origin: origin,
                region: region,
            },
        )
    }

    pub fn pipe_image_host_access(&self) -> u16 {
        // those flags are all mutually exclusive
        (if bit_check(self.flags, CL_MEM_HOST_READ_ONLY) {
            PIPE_IMAGE_ACCESS_READ
        } else if bit_check(self.flags, CL_MEM_HOST_WRITE_ONLY) {
            PIPE_IMAGE_ACCESS_WRITE
        } else if bit_check(self.flags, CL_MEM_HOST_NO_ACCESS) {
            0
        } else {
            PIPE_IMAGE_ACCESS_READ_WRITE
        }) as u16
    }

    pub fn read(
        &self,
        dst: MutMemoryPtr,
        q: &Queue,
        ctx: &PipeContext,
        region: &CLVec<usize>,
        src_origin: &CLVec<usize>,
        dst_row_pitch: usize,
        dst_slice_pitch: usize,
    ) -> CLResult<()> {
        let dst = dst.as_ptr();
        let pixel_size = self.image_format.pixel_size().unwrap();

        let tx;
        let src_row_pitch;
        let src_slice_pitch;
        if let Some(Mem::Buffer(buffer)) = &self.parent {
            src_row_pitch = self.image_desc.image_row_pitch;
            src_slice_pitch = self.image_desc.image_slice_pitch;

            let (offset, size) = CLVec::calc_offset_size(
                src_origin,
                region,
                [pixel_size.into(), src_row_pitch, src_slice_pitch],
            );

            tx = buffer.tx(q, ctx, offset, size, RWFlags::RD)?;
        } else {
            let bx = create_pipe_box(*src_origin, *region, self.mem_type)?;
            tx = self.tx_image(q, ctx, &bx, RWFlags::RD)?;
            src_row_pitch = tx.row_pitch() as usize;
            src_slice_pitch = tx.slice_pitch();
        };

        sw_copy(
            tx.ptr(),
            dst,
            region,
            &CLVec::default(),
            src_row_pitch,
            src_slice_pitch,
            &CLVec::default(),
            dst_row_pitch,
            dst_slice_pitch,
            pixel_size,
        );

        Ok(())
    }

    // TODO: only sync on map when the memory is not mapped with discard
    pub fn sync_map(&self, q: &Queue, ctx: &PipeContext, ptr: MutMemoryPtr) -> CLResult<()> {
        // no need to update
        if self.is_pure_user_memory(q.device)? {
            return Ok(());
        }

        let maps = self.maps.lock().unwrap();
        let Some(mapping) = maps.find_alloc_precise(ptr.as_ptr() as usize) else {
            return Err(CL_INVALID_VALUE);
        };

        let row_pitch = self.image_desc.row_pitch()? as usize;
        let slice_pitch = self.image_desc.slice_pitch();

        self.read(
            ptr,
            q,
            ctx,
            &mapping.region,
            &mapping.origin,
            row_pitch,
            slice_pitch,
        )
    }

    fn tx_image<'a>(
        &self,
        q: &Queue,
        ctx: &'a PipeContext,
        bx: &pipe_box,
        rw: RWFlags,
    ) -> CLResult<GuardedPipeTransfer<'a>> {
        let r = self.get_res_of_dev(q.device)?;
        Ok(ctx
            .texture_map(r, bx, rw, ResourceMapType::Normal)
            .ok_or(CL_OUT_OF_RESOURCES)?
            .with_ctx(ctx))
    }

    // TODO: only sync on unmap when the memory is not mapped for writing
    pub fn unmap(&self, q: &Queue, ctx: &PipeContext, ptr: MutMemoryPtr) -> CLResult<()> {
        let mapping = match self.maps.lock().unwrap().entry(ptr.as_ptr() as usize) {
            Entry::Vacant(_) => return Err(CL_INVALID_VALUE),
            Entry::Occupied(mut entry) => {
                entry.get_mut().count -= 1;
                (entry.get().count == 0).then(|| entry.remove())
            }
        };

        let row_pitch = self.image_desc.row_pitch()? as usize;
        let slice_pitch = self.image_desc.slice_pitch();

        if let Some(mapping) = mapping {
            if mapping.writes && !self.is_pure_user_memory(q.device)? {
                self.write(
                    ptr.into(),
                    q,
                    ctx,
                    &mapping.region,
                    row_pitch,
                    slice_pitch,
                    &mapping.origin,
                )?;
            }
        }

        Ok(())
    }

    pub fn write(
        &self,
        src: ConstMemoryPtr,
        q: &Queue,
        ctx: &PipeContext,
        region: &CLVec<usize>,
        src_row_pitch: usize,
        mut src_slice_pitch: usize,
        dst_origin: &CLVec<usize>,
    ) -> CLResult<()> {
        let src = src.as_ptr();
        let dst_row_pitch = self.image_desc.image_row_pitch;
        let dst_slice_pitch = self.image_desc.image_slice_pitch;

        if let Some(Mem::Buffer(buffer)) = &self.parent {
            let pixel_size = self.image_format.pixel_size().unwrap();
            let (offset, size) = CLVec::calc_offset_size(
                dst_origin,
                region,
                [pixel_size.into(), dst_row_pitch, dst_slice_pitch],
            );
            let tx = buffer.tx(q, ctx, offset, size, RWFlags::WR)?;

            sw_copy(
                src,
                tx.ptr(),
                region,
                &CLVec::default(),
                src_row_pitch,
                src_slice_pitch,
                &CLVec::default(),
                dst_row_pitch,
                dst_slice_pitch,
                pixel_size,
            );
        } else {
            let res = self.get_res_of_dev(q.device)?;
            let bx = create_pipe_box(*dst_origin, *region, self.mem_type)?;

            if self.mem_type == CL_MEM_OBJECT_IMAGE1D_ARRAY {
                src_slice_pitch = src_row_pitch;
            }

            ctx.texture_subdata(
                res,
                &bx,
                src,
                src_row_pitch
                    .try_into()
                    .map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
                src_slice_pitch,
            );
        }
        Ok(())
    }
}

pub struct Sampler {
    pub base: CLObjectBase<CL_INVALID_SAMPLER>,
    pub context: Arc<Context>,
    pub normalized_coords: bool,
    pub addressing_mode: cl_addressing_mode,
    pub filter_mode: cl_filter_mode,
    pub props: Option<Properties<cl_sampler_properties>>,
}

impl_cl_type_trait!(cl_sampler, Sampler, CL_INVALID_SAMPLER);

impl Sampler {
    pub fn new(
        context: Arc<Context>,
        normalized_coords: bool,
        addressing_mode: cl_addressing_mode,
        filter_mode: cl_filter_mode,
        props: Option<Properties<cl_sampler_properties>>,
    ) -> Arc<Sampler> {
        Arc::new(Self {
            base: CLObjectBase::new(RusticlTypes::Sampler),
            context: context,
            normalized_coords: normalized_coords,
            addressing_mode: addressing_mode,
            filter_mode: filter_mode,
            props: props,
        })
    }

    pub fn nir_to_cl(
        addressing_mode: u32,
        filter_mode: u32,
        normalized_coords: u32,
    ) -> (cl_addressing_mode, cl_filter_mode, bool) {
        let addr_mode = match addressing_mode {
            cl_sampler_addressing_mode::SAMPLER_ADDRESSING_MODE_NONE => CL_ADDRESS_NONE,
            cl_sampler_addressing_mode::SAMPLER_ADDRESSING_MODE_CLAMP_TO_EDGE => {
                CL_ADDRESS_CLAMP_TO_EDGE
            }
            cl_sampler_addressing_mode::SAMPLER_ADDRESSING_MODE_CLAMP => CL_ADDRESS_CLAMP,
            cl_sampler_addressing_mode::SAMPLER_ADDRESSING_MODE_REPEAT => CL_ADDRESS_REPEAT,
            cl_sampler_addressing_mode::SAMPLER_ADDRESSING_MODE_REPEAT_MIRRORED => {
                CL_ADDRESS_MIRRORED_REPEAT
            }
            _ => panic!("unknown addressing_mode"),
        };

        let filter = match filter_mode {
            cl_sampler_filter_mode::SAMPLER_FILTER_MODE_NEAREST => CL_FILTER_NEAREST,
            cl_sampler_filter_mode::SAMPLER_FILTER_MODE_LINEAR => CL_FILTER_LINEAR,
            _ => panic!("unknown filter_mode"),
        };

        (addr_mode, filter, normalized_coords != 0)
    }

    pub fn cl_to_pipe(
        (addressing_mode, filter_mode, normalized_coords): (
            cl_addressing_mode,
            cl_filter_mode,
            bool,
        ),
    ) -> pipe_sampler_state {
        let mut res = pipe_sampler_state::default();

        let wrap = match addressing_mode {
            CL_ADDRESS_CLAMP_TO_EDGE => pipe_tex_wrap::PIPE_TEX_WRAP_CLAMP_TO_EDGE,
            CL_ADDRESS_CLAMP => pipe_tex_wrap::PIPE_TEX_WRAP_CLAMP_TO_BORDER,
            CL_ADDRESS_REPEAT => pipe_tex_wrap::PIPE_TEX_WRAP_REPEAT,
            CL_ADDRESS_MIRRORED_REPEAT => pipe_tex_wrap::PIPE_TEX_WRAP_MIRROR_REPEAT,
            // TODO: what's a reasonable default?
            _ => pipe_tex_wrap::PIPE_TEX_WRAP_CLAMP_TO_EDGE,
        };

        let img_filter = match filter_mode {
            CL_FILTER_NEAREST => pipe_tex_filter::PIPE_TEX_FILTER_NEAREST,
            CL_FILTER_LINEAR => pipe_tex_filter::PIPE_TEX_FILTER_LINEAR,
            _ => panic!("unknown filter_mode"),
        };

        res.set_min_img_filter(img_filter);
        res.set_mag_img_filter(img_filter);
        res.set_unnormalized_coords((!normalized_coords).into());
        res.set_wrap_r(wrap);
        res.set_wrap_s(wrap);
        res.set_wrap_t(wrap);

        res
    }

    pub fn pipe(&self) -> pipe_sampler_state {
        Self::cl_to_pipe((
            self.addressing_mode,
            self.filter_mode,
            self.normalized_coords,
        ))
    }
}
