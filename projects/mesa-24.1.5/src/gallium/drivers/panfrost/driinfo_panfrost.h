/* panfrost specific driconf options */
DRI_CONF_SECTION_PERFORMANCE
   DRI_CONF_OPT_B(pan_force_afbc_packing, false, "Use AFBC-P for textures")

   /* 2M chunks. */
   DRI_CONF_OPT_I(pan_csf_chunk_size, 2 * 1024 * 1024, 256 * 1024, 8 * 1024 * 1024, "CSF Tiler Chunk Size")
   DRI_CONF_OPT_I(pan_csf_initial_chunks, 5, 1, 65535, "CSF Tiler Initial Chunks")
   /* 64 x 2M = 128M, which matches the tiler_heap BO allocated in
    * panfrost_open_device() for pre-v10 HW.
    */
   DRI_CONF_OPT_I(pan_csf_max_chunks, 64, 1, 65535, "CSF Tiler Max Chunks")
DRI_CONF_SECTION_END
