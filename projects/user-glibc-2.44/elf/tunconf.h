#define TUNCONF_SIGNATURE		0x7c3ba94f
#define TUNCONF_VERSION			0x01000000

#define TUNCONF_FLAG_PARSED		0x00000001
#define TUNCONF_FLAG_NEGATIVE		0x00000002

#define TUNCONF_FLAG_OVERRIDABLE	0x0000000C
#define TUNCONF_OVERRIDE_DENY		0x00000004
#define TUNCONF_OVERRIDE_ALLOW		0x00000000

#define TUNCONF_EXCLUDE_SECURE		0x00000010
#define TUNCONF_EXCLUDE_UNSECURE	0x00000020

#define TUNCONF_FLAG_FILTER		0x0000ff00
#define TUNCONF_FILTER_NONE		0x00000000
#define TUNCONF_FILTER_PERPROC		0x00000100

/* An array of [num_tunables] of these follows the below.  */
struct tunable_entry_cached {
  uint32_t flags;
  uint32_t tunable_id;
  uint32_t name_offset;
  uint32_t value_offset;
  uint32_t flag_offset;
  uint32_t unused_1; /* for alignment */
  uint64_t parsed_value;
};

/* One of these is at the beginning of the tunable data block.  */
struct tunable_header_cached {
  uint32_t signature;
  uint32_t version;
  uint32_t num_tunables;
  uint32_t unused_1; /* for alignment */
  struct tunable_entry_cached tunables[0 /* num_tunables */];
};

void parse_tunconf (const char *filename, char *opt_chroot);

struct tunable_header_cached * get_tunconf_ext (uint32_t str_offset);
#define TUNCONF_SIZE(thc_p) (sizeof(struct tunable_header_cached)		\
		     + thc_p->num_tunables * sizeof (struct tunable_entry_cached))

extern const struct tunable_header_cached *
_dl_load_cache_tunables (const char **data);
