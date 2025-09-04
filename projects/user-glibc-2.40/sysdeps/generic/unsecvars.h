/* Environment variable to be removed for SUID programs.  The names are
   all stuffed in a single string which means they have to be terminated
   with a '\0' explicitly.  */
#define UNSECURE_ENVVARS \
  "GCONV_PATH\0"							      \
  "GETCONF_DIR\0"							      \
  "GLIBC_TUNABLES\0"							      \
  "HOSTALIASES\0"							      \
  "LD_AUDIT\0"								      \
  "LD_BIND_NOT\0"							      \
  "LD_BIND_NOW\0"							      \
  "LD_DEBUG\0"								      \
  "LD_DEBUG_OUTPUT\0"							      \
  "LD_DYNAMIC_WEAK\0"							      \
  "LD_LIBRARY_PATH\0"							      \
  "LD_ORIGIN_PATH\0"							      \
  "LD_PRELOAD\0"							      \
  "LD_PROFILE\0"							      \
  "LD_SHOW_AUXV\0"							      \
  "LD_VERBOSE\0"							      \
  "LD_WARN\0"								      \
  "LOCALDOMAIN\0"							      \
  "LOCPATH\0"								      \
  "MALLOC_ARENA_MAX\0"							      \
  "MALLOC_ARENA_TEST\0"							      \
  "MALLOC_MMAP_MAX_\0"							      \
  "MALLOC_MMAP_THRESHOLD_\0"						      \
  "MALLOC_PERTURB_\0"							      \
  "MALLOC_TOP_PAD_\0"							      \
  "MALLOC_TRACE\0"							      \
  "MALLOC_TRIM_THRESHOLD_\0"						      \
  "NIS_PATH\0"								      \
  "NLSPATH\0"								      \
  "RESOLV_HOST_CONF\0"							      \
  "RES_OPTIONS\0"							      \
  "TMPDIR\0"								      \
  "TZDIR\0"
