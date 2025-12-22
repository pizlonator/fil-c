/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * ConfigFS Gadget device handling
 *
 * Copyright (C) 2018 Kieran Bingham
 *
 * Contact: Kieran Bingham <kieran.bingham@ideasonboard.com>
 */

/* To provide asprintf from the GNU library. */
#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "linux/videodev2.h"

#include "configfs.h"

/* ------------------------------------------------------------------------
 * GUIDs and formats
 */

#define UVC_GUID_FORMAT_MJPEG \
	{ 'M',  'J',  'P',  'G', 0x00, 0x00, 0x10, 0x00, \
	 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_YUY2 \
	{ 'Y',  'U',  'Y',  '2', 0x00, 0x00, 0x10, 0x00, \
	 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}

struct uvc_function_format_info
{
  uint8_t guid[16];
  uint32_t fcc;
};

static struct uvc_function_format_info uvc_formats[] = {
  {
        .guid = UVC_GUID_FORMAT_YUY2,
        .fcc = V4L2_PIX_FMT_YUYV,
      },
  {
        .guid = UVC_GUID_FORMAT_MJPEG,
        .fcc = V4L2_PIX_FMT_MJPEG,
      },
};

/* -----------------------------------------------------------------------------
 * Path handling and support
 */

static char *
path_join (const char *dirname, const char *name)
{
  char *path;
  int ret;

  ret = asprintf (&path, "%s/%s", dirname, name);

  /*
   * asprintf returns -1 on allocation or other errors, leaving 'path'
   * undefined. We shouldn't even call free(path) here. We want to return
   * NULL on error, so we must manually set it.
   */
  if (ret < 0)
    path = NULL;

  return path;
}

static char *
path_glob_first_match (const char *g)
{
  glob_t globbuf;
  char *match = NULL;

  glob (g, 0, NULL, &globbuf);

  if (globbuf.gl_pathc)
    match = strdup (globbuf.gl_pathv[0]);

  globfree (&globbuf);

  return match;
}

/*
 * Find and return the full path of the first directory entry that satisfies
 * the match function.
 */
static char *
dir_first_match (const char *dir, int (*match) (const struct dirent *))
{
  struct dirent **entries;
  unsigned int i;
  int n_entries;
  char *path;

  n_entries = scandir (dir, &entries, match, alphasort);
  if (n_entries < 0)
    return NULL;

  if (n_entries == 0) {
    free (entries);
    return NULL;
  }

  path = path_join (dir, entries[0]->d_name);

  for (i = 0; i < (unsigned int) n_entries; ++i)
    free (entries[i]);

  free (entries);

  return path;
}

/* -----------------------------------------------------------------------------
 * Attribute handling
 */

static int
attribute_read (const char *path, const char *file, void *buf, unsigned int len)
{
  char *f;
  int ret;
  int fd;

  f = path_join (path, file);
  if (!f)
    return -ENOMEM;

  fd = open (f, O_RDONLY);
  free (f);
  if (fd == -1) {
    printf ("Failed to open attribute %s: %s\n", file, strerror (errno));
    return -ENOENT;
  }

  ret = read (fd, buf, len);
  close (fd);

  if (ret < 0) {
    printf ("Failed to read attribute %s: %s\n", file, strerror (errno));
    return -ENODATA;
  }

  return len;
}

static int
attribute_read_uint (const char *path, const char *file, unsigned int *val)
{
  /* 4,294,967,295 */
  char buf[11];
  char *endptr;
  int ret;

  ret = attribute_read (path, file, buf, sizeof (buf) - 1);
  if (ret < 0)
    return ret;

  buf[ret] = '\0';

  errno = 0;

  /* base 0: Autodetect hex, octal, decimal. */
  *val = strtoul (buf, &endptr, 0);
  if (errno)
    return -errno;

  if (endptr == buf)
    return -ENODATA;

  return 0;
}

static char *
attribute_read_str (const char *path, const char *file)
{
  char buf[1024];
  char *p;
  int ret;

  ret = attribute_read (path, file, buf, sizeof (buf) - 1);
  if (ret < 0)
    return NULL;

  buf[ret] = '\0';

  p = strrchr (buf, '\n');
  if (p != buf)
    *p = '\0';

  return strdup (buf);
}

/* -----------------------------------------------------------------------------
 * UDC parsing
 */

/*
 * udc_find_video_device - Find the video device node for a UVC function
 * @udc: The UDC name
 * @function: The UVC function name
 *
 * This function finds the video device node corresponding to a UVC function as
 * specified by a @function name and @udc name.
 *
 * The @function parameter specifies the name of the USB function, usually in
 * the form "uvc.%u". If NULL the first function found will be used.
 *
 * The @udc parameter specifies the name of the UDC. If NULL any UDC that
 * contains a function matching the @function name will be used.
 *
 * Return a pointer to a newly allocated string containing the video device node
 * full path if the function is found. Otherwise return NULL. The returned
 * pointer must be freed by the caller with a call to free().
 */
static char *
udc_find_video_device (const char *udc, const char *function)
{
  char *vpath;
  char *video = NULL;
  glob_t globbuf;
  unsigned int i;
  int ret;

  ret = asprintf (&vpath,
      "/sys/class/udc/%s/device/gadget*/video4linux/video*", udc ? udc : "*");
  if (!ret)
    return NULL;

  glob (vpath, 0, NULL, &globbuf);
  free (vpath);

  for (i = 0; i < globbuf.gl_pathc; ++i) {
    char *config;
    bool match;

    /* Match on the first if no search string. */
    if (!function)
      break;

    config = attribute_read_str (globbuf.gl_pathv[i], "function_name");
    match = strcmp (function, config) == 0;

    free (config);

    if (match)
      break;
  }

  if (i < globbuf.gl_pathc) {
    gchar *v = g_path_get_basename (globbuf.gl_pathv[i]);

    video = path_join ("/dev", v);
    g_free (v);
  }

  globfree (&globbuf);

  return video;
}

/* -----------------------------------------------------------------------------
 * Legacy g_webcam support
 */

static const struct uvc_function_config g_webcam_config = {
  .control = {
        .intf = {
              .bInterfaceNumber = 0,
            },
      },
  .streaming = {
        .intf = {
              .bInterfaceNumber = 1,
            },
        .ep = {
              .bInterval = 1,
              .bMaxBurst = 0,
              .wMaxPacketSize = 1024,
            },
        .num_formats = 2,
        .formats = (struct uvc_function_config_format[]) {
              {
                    .index = 1,
                    .guid = UVC_GUID_FORMAT_YUY2,
                    .fcc = V4L2_PIX_FMT_YUYV,
                    .num_frames = 2,
                    .frames = (struct uvc_function_config_frame[]) {
                          {
                                .index = 1,
                                .width = 640,
                                .height = 360,
                                .num_intervals = 3,
                                .intervals = (unsigned int[]) {
                                      666666,
                                      10000000,
                                      50000000,
                                    },
                              }, {
                                .index = 2,
                                .width = 1280,
                                .height = 720,
                                .num_intervals = 1,
                                .intervals = (unsigned int[]) {
                                      50000000,
                                    },
                              },
                        },
                  }, {
                    .index = 2,
                    .guid = UVC_GUID_FORMAT_MJPEG,
                    .fcc = V4L2_PIX_FMT_MJPEG,
                    .num_frames = 2,
                    .frames = (struct uvc_function_config_frame[]) {
                          {
                                .index = 1,
                                .width = 640,
                                .height = 360,
                                .num_intervals = 3,
                                .intervals = (unsigned int[]) {
                                      666666,
                                      10000000,
                                      50000000,
                                    },
                              }, {
                                .index = 2,
                                .width = 1280,
                                .height = 720,
                                .num_intervals = 1,
                                .intervals = (unsigned int[]) {
                                      50000000,
                                    },
                              },
                        },
                  },
            },
      },
};

static void *
memdup (const void *src, size_t size)
{
  void *dst;

  dst = malloc (size);
  if (!dst)
    return NULL;
  memcpy (dst, src, size);
  return dst;
}

static int
parse_legacy_g_webcam (const char *udc, struct uvc_function_config *fc)
{
  unsigned int i, j;
  size_t size;

  *fc = g_webcam_config;

  /*
   * We need to duplicate all sub-structures as the
   * configfs_free_uvc_function() function expects them to be dynamically
   * allocated.
   */
  size = sizeof *fc->streaming.formats * fc->streaming.num_formats;
  fc->streaming.formats = memdup (fc->streaming.formats, size);

  for (i = 0; i < fc->streaming.num_formats; ++i) {
    struct uvc_function_config_format *format = &fc->streaming.formats[i];

    size = sizeof *format->frames * format->num_frames;
    format->frames = memdup (format->frames, size);

    for (j = 0; j < format->num_frames; ++j) {
      struct uvc_function_config_frame *frame = &format->frames[j];

      size = sizeof *frame->intervals * frame->num_intervals;
      frame->intervals = memdup (frame->intervals, size);
    }
  }

  fc->video = udc_find_video_device (udc, NULL);

  return fc->video ? 0 : -ENODEV;
}

/* -----------------------------------------------------------------------------
 * ConfigFS support
 */

/*
 * configfs_mount_point - Identify the ConfigFS mount location
 *
 * Return a pointer to a newly allocated string containing the fully qualified
 * path to the ConfigFS mount point if located. Returns NULL if the ConfigFS
 * mount point can not be identified.
 */
static char *
configfs_mount_point (void)
{
  FILE *mounts;
  char *line = NULL;
  char *path = NULL;
  size_t len = 0;

  mounts = fopen ("/proc/mounts", "r");
  if (mounts == NULL)
    return NULL;

  while (getline (&line, &len, mounts) != -1) {
    if (strstr (line, "configfs")) {
      char *saveptr;
      char *token;

      /* Obtain the second token. */
      token = strtok_r (line, " ", &saveptr);
      token = strtok_r (NULL, " ", &saveptr);

      if (token)
        path = strdup (token);

      break;
    }
  }

  free (line);
  fclose (mounts);

  return path;
}

/*
 * configfs_find_uvc_function - Find the ConfigFS full path for a UVC function
 * @function: The UVC function name
 *
 * Return a pointer to a newly allocated string containing the full ConfigFS
 * path to the function if the function is found. Otherwise return NULL. The
 * returned pointer must be freed by the caller with a call to free().
 */
static char *
configfs_find_uvc_function (const char *function)
{
  const char *target = function ? function : "*";
  const char *format;
  char *configfs;
  char *func_path;
  char *path;
  int ret;

  configfs = configfs_mount_point ();
  if (!configfs)
    printf ("Failed to locate configfs mount point, using default\n");

  /*
   * The function description can be provided as a path from the
   * usb_gadget root "g1/functions/uvc.0", or if there is no ambiguity
   * over the gadget name, a shortcut "uvc.0" can be provided.
   */
  if (!strchr (target, '/'))
    format = "%s/usb_gadget/*/functions/%s";
  else
    format = "%s/usb_gadget/%s";

  ret = asprintf (&path, format, configfs ? configfs : "/sys/kernel/config",
      target);
  free (configfs);
  if (!ret)
    return NULL;

  func_path = path_glob_first_match (path);
  free (path);

  return func_path;
}

/*
 * configfs_free_uvc_function - Free a uvc_function_config object
 * @fc: The uvc_function_config to be freed
 *
 * Free the given @fc function previously allocated by a call to
 * configfs_parse_uvc_function().
 */
void
configfs_free_uvc_function (struct uvc_function_config *fc)
{
  unsigned int i, j;

  free (fc->udc);
  free (fc->video);

  for (i = 0; i < fc->streaming.num_formats; ++i) {
    struct uvc_function_config_format *format = &fc->streaming.formats[i];

    for (j = 0; j < format->num_frames; ++j) {
      struct uvc_function_config_frame *frame = &format->frames[j];

      free (frame->intervals);
    }

    free (format->frames);
  }

  free (fc->streaming.formats);
  free (fc);
}

#define configfs_parse_child(parent, child, cfg, parse)			\
({									\
	char *__path;							\
	int __ret;							\
									\
	__path = path_join((parent), (child));				\
	if (__path) {							\
		__ret = parse(__path, (cfg));				\
		free(__path);						\
	} else {							\
		__ret = -ENOMEM;					\
	}								\
									\
	__ret;								\
})

static int
configfs_parse_interface (const char *path,
    struct uvc_function_config_interface *cfg)
{
  int ret;

  ret = attribute_read_uint (path, "bInterfaceNumber", &cfg->bInterfaceNumber);

  return ret;
}

static int
configfs_parse_control (const char *path,
    struct uvc_function_config_control *cfg)
{
  int ret;

  ret = configfs_parse_interface (path, &cfg->intf);

  return ret;
}

static int
configfs_parse_streaming_frame (const char *path,
    struct uvc_function_config_frame *frame)
{
  char *intervals;
  char *p;
  int ret = 0;

  ret = ret ? : attribute_read_uint (path, "bFrameIndex", &frame->index);
  ret = ret ? : attribute_read_uint (path, "wWidth", &frame->width);
  ret = ret ? : attribute_read_uint (path, "wHeight", &frame->height);
  ret = ret ? : attribute_read_uint (path, "dwMaxVideoFrameBufferSize",
      &frame->maxvideofbsize);
  if (ret)
    return ret;

  intervals = attribute_read_str (path, "dwFrameInterval");
  if (!intervals)
    return -EINVAL;

  for (p = intervals; *p;) {
    unsigned int interval;
    unsigned int *mem;
    char *endp;
    size_t size;

    interval = strtoul (p, &endp, 10);
    if (*endp != '\0' && *endp != '\n') {
      ret = -EINVAL;
      break;
    }

    p = *endp ? endp + 1 : endp;

    size = sizeof *frame->intervals * (frame->num_intervals + 1);
    mem = realloc (frame->intervals, size);
    if (!mem) {
      ret = -ENOMEM;
      break;
    }

    frame->intervals = mem;
    frame->intervals[frame->num_intervals++] = interval;
  }

  free (intervals);

  return ret;
}

static int
frame_filter (const struct dirent *ent)
{
  /* Accept all directories but "." and "..". */
  if (ent->d_type != DT_DIR)
    return 0;
  if (!strcmp (ent->d_name, "."))
    return 0;
  if (!strcmp (ent->d_name, ".."))
    return 0;
  return 1;
}

static int
frame_compare (const void *a, const void *b)
{
  const struct uvc_function_config_frame *fa = a;
  const struct uvc_function_config_frame *fb = b;

  if (fa->index < fb->index)
    return -1;
  else if (fa->index == fb->index)
    return 0;
  else
    return 1;
}

static int
configfs_parse_streaming_format (const char *path,
    struct uvc_function_config_format *format)
{
  struct dirent **entries;
  char link_target[1024];
  char *segment;
  unsigned int i;
  int n_entries;
  int ret;

  ret = attribute_read_uint (path, "bFormatIndex", &format->index);
  if (ret < 0)
    return ret;

  ret = readlink (path, link_target, sizeof (link_target) - 1);
  if (ret < 0)
    return ret;

  link_target[ret] = '\0';

  /*
   * Extract the second-to-last path component of the link target,
   * which contains the format descriptor type name as exposed by
   * the UVC function driver.
   */
  segment = strrchr (link_target, '/');
  if (!segment)
    return -EINVAL;
  *segment = '\0';
  segment = strrchr (link_target, '/');
  if (!segment)
    return -EINVAL;
  segment++;

  if (!strcmp (segment, "mjpeg")) {
    static const uint8_t guid[16] = UVC_GUID_FORMAT_MJPEG;
    memcpy (format->guid, guid, 16);
  } else if (!strcmp (segment, "uncompressed")) {
    ret = attribute_read (path, "guidFormat", format->guid,
        sizeof (format->guid));
    if (ret < 0)
      return ret;
  } else {
    return -EINVAL;
  }

  for (i = 0; i < G_N_ELEMENTS (uvc_formats); ++i) {
    if (!memcmp (uvc_formats[i].guid, format->guid, 16)) {
      format->fcc = uvc_formats[i].fcc;
      break;
    }
  }

  /* Find all entries corresponding to a frame and parse them. */
  n_entries = scandir (path, &entries, frame_filter, alphasort);
  if (n_entries < 0)
    return -errno;

  if (n_entries == 0) {
    free (entries);
    return -EINVAL;
  }

  format->num_frames = n_entries;
  format->frames = calloc (sizeof *format->frames, format->num_frames);
  if (!format->frames)
    return -ENOMEM;

  for (i = 0; i < (unsigned int) n_entries; ++i) {
    char *frame;

    frame = path_join (path, entries[i]->d_name);
    if (!frame) {
      ret = -ENOMEM;
      goto done;
    }

    ret = configfs_parse_streaming_frame (frame, &format->frames[i]);
    free (frame);
    if (ret < 0)
      goto done;
  }

  /* Sort the frames by index. */
  qsort (format->frames, format->num_frames, sizeof *format->frames,
      frame_compare);

done:
  for (i = 0; i < (unsigned int) n_entries; ++i)
    free (entries[i]);
  free (entries);

  return ret;
}

static int
format_filter (const struct dirent *ent)
{
  char *path;
  bool valid;

  /*
   * Accept all links that point to a directory containing a
   * "bFormatIndex" file.
   */
  if (ent->d_type != DT_LNK)
    return 0;

  path = path_join (ent->d_name, "bFormatIndex");
  if (!path)
    return 0;

  valid = access (path, R_OK);
  free (path);
  return valid;
}

static int
format_compare (const void *a, const void *b)
{
  const struct uvc_function_config_format *fa = a;
  const struct uvc_function_config_format *fb = b;

  if (fa->index < fb->index)
    return -1;
  else if (fa->index == fb->index)
    return 0;
  else
    return 1;
}

static int
configfs_parse_streaming_header (const char *path,
    struct uvc_function_config_streaming *cfg)
{
  struct dirent **entries;
  unsigned int i;
  int n_entries;
  int ret;

  /* Find all entries corresponding to a format and parse them. */
  n_entries = scandir (path, &entries, format_filter, alphasort);
  if (n_entries < 0)
    return -errno;

  if (n_entries == 0) {
    free (entries);
    return -EINVAL;
  }

  cfg->num_formats = n_entries;
  cfg->formats = calloc (sizeof *cfg->formats, cfg->num_formats);
  if (!cfg->formats)
    return -ENOMEM;

  for (i = 0; i < (unsigned int) n_entries; ++i) {
    char *format;

    format = path_join (path, entries[i]->d_name);
    if (!format) {
      ret = -ENOMEM;
      goto done;
    }

    ret = configfs_parse_streaming_format (format, &cfg->formats[i]);
    free (format);
    if (ret < 0)
      goto done;
  }

  /* Sort the formats by index. */
  qsort (cfg->formats, cfg->num_formats, sizeof *cfg->formats, format_compare);

done:
  for (i = 0; i < (unsigned int) n_entries; ++i)
    free (entries[i]);
  free (entries);

  return ret;
}

static int
link_filter (const struct dirent *ent)
{
  /* Accept all links. */
  return ent->d_type == DT_LNK;
}

static int
configfs_parse_streaming (const char *path,
    struct uvc_function_config_streaming *cfg)
{
  char *header;
  char *class;
  int ret;

  ret = configfs_parse_interface (path, &cfg->intf);
  if (ret < 0)
    return ret;

  /*
   * Handle the high-speed class descriptors only for now. Find the first
   * link to the class descriptors.
   */
  class = path_join (path, "class/hs");
  if (!class)
    return -ENOMEM;

  header = dir_first_match (class, link_filter);
  free (class);
  if (!header)
    return -EINVAL;

  ret = configfs_parse_streaming_header (header, cfg);
  free (header);
  return ret;
}

static int
configfs_parse_uvc (const char *fpath, struct uvc_function_config *fc)
{
  int ret = 0;

  ret = ret ? : configfs_parse_child (fpath, "control", &fc->control,
      configfs_parse_control);
  ret = ret ? : configfs_parse_child (fpath, "streaming", &fc->streaming,
      configfs_parse_streaming);

  /*
   * These parameters should be part of the streaming interface in
   * ConfigFS, but for legacy reasons they are located directly in the
   * function directory.
   */
  ret = ret ? : attribute_read_uint (fpath, "streaming_interval",
      &fc->streaming.ep.bInterval);
  ret = ret ? : attribute_read_uint (fpath, "streaming_maxburst",
      &fc->streaming.ep.bMaxBurst);
  ret = ret ? : attribute_read_uint (fpath, "streaming_maxpacket",
      &fc->streaming.ep.wMaxPacketSize);

  return ret;
}

/*
 * configfs_parse_uvc_function - Parse a UVC function configuration in ConfigFS
 * @function: The function name
 *
 * This function locates and parse the configuration of a UVC function in
 * ConfigFS as specified by the @function name argument. The function name can
 * be fully qualified with a gadget name (e.g. "g%u/functions/uvc.%u"), or as a
 * shortcut can be an unqualified function name (e.g. "uvc.%u"). When the
 * function name is unqualified, the first function matching the name in any
 * UDC will be returned.
 *
 * Return a pointer to a newly allocated UVC function configuration structure
 * that contains configuration parameters for the function, if the function is
 * found. Otherwise return NULL. The returned pointer must be freed by the
 * caller with a call to free().
 */
struct uvc_function_config *
configfs_parse_uvc_function (const char *function)
{
  struct uvc_function_config *fc;
  char *fpath;
  gchar *bname;
  int ret = 0;

  fc = malloc (sizeof *fc);
  if (fc == NULL)
    return NULL;

  memset (fc, 0, sizeof *fc);

  /* Find the function in ConfigFS. */
  fpath = configfs_find_uvc_function (function);
  if (!fpath) {
    /*
     * If the function can't be found attempt legacy parsing to
     * support the g_webcam gadget. The function parameter contains
     * a UDC name in that case.
     */
    ret = parse_legacy_g_webcam (function, fc);
    if (ret) {
      configfs_free_uvc_function (fc);
      fc = NULL;
    }

    return fc;
  }

  /*
   * Parse the function configuration. Remove the gadget name qualifier
   * from the function name, if any.
   */
  bname = g_path_get_basename (function);

  fc->udc = attribute_read_str (fpath, "../../UDC");
  fc->video = udc_find_video_device (fc->udc, bname);
  if (!fc->video) {
    ret = -ENODEV;
    goto done;
  }

  ret = configfs_parse_uvc (fpath, fc);

done:
  if (ret) {
    configfs_free_uvc_function (fc);
    fc = NULL;
  }

  free (fpath);
  g_free (bname);

  return fc;
}

static char *
video_find_config_name (const char *video)
{
  char *vpath;
  glob_t globbuf;
  char *config;
  int ret;

  ret = asprintf (&vpath,
      "/sys/class/udc/*/device/gadget*/video4linux/%s", video ? video : "*");
  if (!ret)
    return NULL;

  glob (vpath, 0, NULL, &globbuf);
  free (vpath);

  if (globbuf.gl_pathc != 1)
    return NULL;

  config = attribute_read_str (globbuf.gl_pathv[0], "function_name");

  globfree (&globbuf);

  return config;
}

struct uvc_function_config *
configfs_parse_uvc_videodev (int fd, const char *video)
{
  struct uvc_function_config *fc;
  char *function = NULL;
  char rpath[PATH_MAX];
  char *res;
  gchar *bname;

  res = realpath (video, rpath);
  if (!res)
    return NULL;

  bname = g_path_get_basename (rpath);
  function = video_find_config_name (bname);
  g_free (bname);

  if (!function)
    return NULL;

  fc = configfs_parse_uvc_function (function);

  free (function);

  return fc;
}
