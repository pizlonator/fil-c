# Cuda library

This library should be linked to by getting cflags and libs from
gstreamer-cuda-{{ gst_api_version.md }}.pc

> NOTE: This library API is considered *unstable*

## Environment variables

The GStreamer CUDA library inspects following environment variables

**`GST_CUDA_CRITICAL_ERRORS`. (Since: 1.24)**

This environment variable can be set to a comma-separated list of CUresult
values (see CUDA driver API documentation). GStreamer CUDA library will
abort when the user registered error is detected. This environment can be useful
when unrecoverable CUDA error happens. Thus in-process error recovery
(e.g., relaunching new pipeline) is not expected to work, and therefore
the process should be relaunched.

Example: `GST_CUDA_CRITICAL_ERRORS=2,700`

As a result of the above example, if `CUDA_ERROR_OUT_OF_MEMORY(2)` or
`CUDA_ERROR_ILLEGAL_ADDRESS(700)` error is detected in GStreamer CUDA library,
the process will be aborted.
