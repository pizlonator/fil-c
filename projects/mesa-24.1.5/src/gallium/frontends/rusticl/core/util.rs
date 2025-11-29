use crate::api::{icd::CLResult, types::CLVec};

use mesa_rust_gen::*;
use rusticl_opencl_gen::*;

use super::gl::is_cube_map_face;

pub fn cl_mem_type_to_texture_target(mem_type: cl_mem_object_type) -> pipe_texture_target {
    match mem_type {
        CL_MEM_OBJECT_BUFFER => pipe_texture_target::PIPE_BUFFER,
        CL_MEM_OBJECT_IMAGE1D => pipe_texture_target::PIPE_TEXTURE_1D,
        CL_MEM_OBJECT_IMAGE2D => pipe_texture_target::PIPE_TEXTURE_2D,
        CL_MEM_OBJECT_IMAGE3D => pipe_texture_target::PIPE_TEXTURE_3D,
        CL_MEM_OBJECT_IMAGE1D_ARRAY => pipe_texture_target::PIPE_TEXTURE_1D_ARRAY,
        CL_MEM_OBJECT_IMAGE2D_ARRAY => pipe_texture_target::PIPE_TEXTURE_2D_ARRAY,
        CL_MEM_OBJECT_IMAGE1D_BUFFER => pipe_texture_target::PIPE_BUFFER,
        _ => pipe_texture_target::PIPE_TEXTURE_2D,
    }
}

pub fn cl_mem_type_to_texture_target_gl(
    mem_type: cl_mem_object_type,
    target: cl_GLenum,
) -> pipe_texture_target {
    if is_cube_map_face(target) {
        debug_assert_eq!(mem_type, CL_MEM_OBJECT_IMAGE2D);
        pipe_texture_target::PIPE_TEXTURE_CUBE
    } else {
        cl_mem_type_to_texture_target(mem_type)
    }
}

pub fn create_pipe_box(
    mut base: CLVec<usize>,
    mut region: CLVec<usize>,
    mem_type: cl_mem_object_type,
) -> CLResult<pipe_box> {
    if matches!(
        mem_type,
        CL_MEM_OBJECT_BUFFER
            | CL_MEM_OBJECT_IMAGE1D
            | CL_MEM_OBJECT_IMAGE1D_ARRAY
            | CL_MEM_OBJECT_IMAGE1D_BUFFER
            | CL_MEM_OBJECT_IMAGE2D
    ) {
        debug_assert!(region[2] == 1);
        region[2] = 1;
    }

    if matches!(
        mem_type,
        CL_MEM_OBJECT_BUFFER | CL_MEM_OBJECT_IMAGE1D | CL_MEM_OBJECT_IMAGE1D_BUFFER
    ) {
        debug_assert!(region[1] == 1);
        region[1] = 1;
    }

    if mem_type == CL_MEM_OBJECT_IMAGE1D_ARRAY {
        base.swap(1, 2);
        region.swap(1, 2);
    };

    Ok(pipe_box {
        x: base[0].try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
        y: base[1].try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
        z: base[2].try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
        width: region[0].try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
        height: region[1].try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
        depth: region[2].try_into().map_err(|_| CL_OUT_OF_HOST_MEMORY)?,
    })
}
