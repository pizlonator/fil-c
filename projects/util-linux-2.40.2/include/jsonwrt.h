/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_JSONWRT_H
#define UTIL_LINUX_JSONWRT_H

enum {
	UL_JSON_OBJECT,
	UL_JSON_ARRAY,
	UL_JSON_VALUE
};

struct ul_jsonwrt {
	FILE *out;
	int indent;

	unsigned int after_close :1;
};

void ul_jsonwrt_init(struct ul_jsonwrt *fmt, FILE *out, int indent);
int ul_jsonwrt_is_ready(struct ul_jsonwrt *fmt);
void ul_jsonwrt_indent(struct ul_jsonwrt *fmt);
void ul_jsonwrt_open(struct ul_jsonwrt *fmt, const char *name, int type);
void ul_jsonwrt_close(struct ul_jsonwrt *fmt, int type);

#define ul_jsonwrt_root_open(_f)	ul_jsonwrt_open(_f, NULL, UL_JSON_OBJECT)
#define ul_jsonwrt_root_close(_f)	ul_jsonwrt_close(_f, UL_JSON_OBJECT)

#define ul_jsonwrt_array_open(_f, _n)	ul_jsonwrt_open(_f, _n, UL_JSON_ARRAY)
#define ul_jsonwrt_array_close(_f)	ul_jsonwrt_close(_f, UL_JSON_ARRAY)

#define ul_jsonwrt_object_open(_f, _n)	ul_jsonwrt_open(_f, _n, UL_JSON_OBJECT)
#define ul_jsonwrt_object_close(_f)	ul_jsonwrt_close(_f, UL_JSON_OBJECT)

#define ul_jsonwrt_value_open(_f, _n)	ul_jsonwrt_open(_f, _n, UL_JSON_VALUE)
#define ul_jsonwrt_value_close(_f)	ul_jsonwrt_close(_f, UL_JSON_VALUE)


void ul_jsonwrt_value_raw(struct ul_jsonwrt *fmt,
			const char *name, const char *data);
void ul_jsonwrt_value_s(struct ul_jsonwrt *fmt,
			const char *name, const char *data);
void ul_jsonwrt_value_s_sized(struct ul_jsonwrt *fmt,
			      const char *name, const char *data, size_t size);
void ul_jsonwrt_value_u64(struct ul_jsonwrt *fmt,
			const char *name, uint64_t data);
void ul_jsonwrt_value_double(struct ul_jsonwrt *fmt,
			const char *name, long double data);
void ul_jsonwrt_value_boolean(struct ul_jsonwrt *fmt,
			const char *name, int data);
void ul_jsonwrt_value_null(struct ul_jsonwrt *fmt,
			const char *name);

#endif /* UTIL_LINUX_JSONWRT_H */
