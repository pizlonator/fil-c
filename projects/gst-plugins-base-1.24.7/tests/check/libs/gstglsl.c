/* GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/check/gstcheck.h>

#include <gst/gl/gstglsl.h>

#include <stdio.h>

static void
setup (void)
{
}

static void
teardown (void)
{
}

/* *INDENT-OFF* */
static const struct {GstGLSLVersion version; const gchar * name;} glsl_versions[] = {
  {GST_GLSL_VERSION_100, "100"},
  {GST_GLSL_VERSION_110, "110"},
  {GST_GLSL_VERSION_120, "120"},
  {GST_GLSL_VERSION_130, "130"},
  {GST_GLSL_VERSION_140, "140"},
  {GST_GLSL_VERSION_150, "150"},
  {GST_GLSL_VERSION_300, "300"},
  {GST_GLSL_VERSION_310, "310"},
  {GST_GLSL_VERSION_320, "320"},
  {GST_GLSL_VERSION_330, "330"},
  {GST_GLSL_VERSION_400, "400"},
  {GST_GLSL_VERSION_410, "410"},
  {GST_GLSL_VERSION_420, "420"},
  {GST_GLSL_VERSION_430, "430"},
  {GST_GLSL_VERSION_440, "440"},
  {GST_GLSL_VERSION_450, "450"},
};

static const struct {GstGLSLProfile profile; const gchar * name;} glsl_profiles[] = {
  {GST_GLSL_PROFILE_ES, "es"},
  {GST_GLSL_PROFILE_CORE, "core"},
  {GST_GLSL_PROFILE_COMPATIBILITY, "compatibility"},
};

struct version_profile_s
{
  GstGLSLVersion version;
  GstGLSLProfile profile;
  const gchar * name;
};

static const struct version_profile_s glsl_version_profiles_valid[] = {
  {GST_GLSL_VERSION_100, GST_GLSL_PROFILE_ES, "100"},
  {GST_GLSL_VERSION_110, GST_GLSL_PROFILE_COMPATIBILITY, "110"},
  {GST_GLSL_VERSION_120, GST_GLSL_PROFILE_COMPATIBILITY, "120"},
  {GST_GLSL_VERSION_130, GST_GLSL_PROFILE_COMPATIBILITY, "130"},
  {GST_GLSL_VERSION_140, GST_GLSL_PROFILE_COMPATIBILITY, "140"},
  {GST_GLSL_VERSION_150, GST_GLSL_PROFILE_CORE, "150 core"},
  {GST_GLSL_VERSION_150, GST_GLSL_PROFILE_COMPATIBILITY, "150 compatibility"},
  {GST_GLSL_VERSION_300, GST_GLSL_PROFILE_ES, "300 es"},
  {GST_GLSL_VERSION_310, GST_GLSL_PROFILE_ES, "310 es"},
  {GST_GLSL_VERSION_320, GST_GLSL_PROFILE_ES, "320 es"},
  {GST_GLSL_VERSION_330, GST_GLSL_PROFILE_CORE, "330 core"},
  {GST_GLSL_VERSION_330, GST_GLSL_PROFILE_COMPATIBILITY, "330 compatibility"},
  {GST_GLSL_VERSION_400, GST_GLSL_PROFILE_CORE, "400 core"},
  {GST_GLSL_VERSION_400, GST_GLSL_PROFILE_COMPATIBILITY, "400 compatibility"},
  {GST_GLSL_VERSION_410, GST_GLSL_PROFILE_CORE, "410 core"},
  {GST_GLSL_VERSION_410, GST_GLSL_PROFILE_COMPATIBILITY, "410 compatibility"},
  {GST_GLSL_VERSION_420, GST_GLSL_PROFILE_CORE, "420 core"},
  {GST_GLSL_VERSION_420, GST_GLSL_PROFILE_COMPATIBILITY, "420 compatibility"},
  {GST_GLSL_VERSION_430, GST_GLSL_PROFILE_CORE, "430 core"},
  {GST_GLSL_VERSION_430, GST_GLSL_PROFILE_COMPATIBILITY, "430 compatibility"},
  {GST_GLSL_VERSION_440, GST_GLSL_PROFILE_CORE, "440 core"},
  {GST_GLSL_VERSION_440, GST_GLSL_PROFILE_COMPATIBILITY, "440 compatibility"},
  {GST_GLSL_VERSION_450, GST_GLSL_PROFILE_CORE, "450 core"},
  {GST_GLSL_VERSION_450, GST_GLSL_PROFILE_COMPATIBILITY, "450 compatibility"},
};

/* combinations that produce different results between serializing/deserializing
 * due to default values being imposed */
static const struct version_profile_s glsl_version_profiles_valid_serialize[] = {
  {GST_GLSL_VERSION_100, GST_GLSL_PROFILE_NONE, "100"},
  {GST_GLSL_VERSION_110, GST_GLSL_PROFILE_NONE, "110"},
  {GST_GLSL_VERSION_120, GST_GLSL_PROFILE_NONE, "120"},
  {GST_GLSL_VERSION_130, GST_GLSL_PROFILE_NONE, "130"},
  {GST_GLSL_VERSION_140, GST_GLSL_PROFILE_NONE, "140"},
  {GST_GLSL_VERSION_150, GST_GLSL_PROFILE_NONE, "150"},
  {GST_GLSL_VERSION_330, GST_GLSL_PROFILE_NONE, "330"},
  {GST_GLSL_VERSION_400, GST_GLSL_PROFILE_NONE, "400"},
  {GST_GLSL_VERSION_410, GST_GLSL_PROFILE_NONE, "410"},
  {GST_GLSL_VERSION_420, GST_GLSL_PROFILE_NONE, "420"},
  {GST_GLSL_VERSION_430, GST_GLSL_PROFILE_NONE, "430"},
  {GST_GLSL_VERSION_440, GST_GLSL_PROFILE_NONE, "440"},
  {GST_GLSL_VERSION_450, GST_GLSL_PROFILE_NONE, "450"},
};
static const struct version_profile_s glsl_version_profiles_valid_deserialize[] = {
  {GST_GLSL_VERSION_100, GST_GLSL_PROFILE_ES, "100"},
  {GST_GLSL_VERSION_110, GST_GLSL_PROFILE_COMPATIBILITY, "110"},
  {GST_GLSL_VERSION_120, GST_GLSL_PROFILE_COMPATIBILITY, "120"},
  {GST_GLSL_VERSION_130, GST_GLSL_PROFILE_COMPATIBILITY, "130"},
  {GST_GLSL_VERSION_140, GST_GLSL_PROFILE_COMPATIBILITY, "140"},
  {GST_GLSL_VERSION_150, GST_GLSL_PROFILE_CORE, "150"},
  {GST_GLSL_VERSION_330, GST_GLSL_PROFILE_CORE, "330"},
  {GST_GLSL_VERSION_400, GST_GLSL_PROFILE_CORE, "400"},
  {GST_GLSL_VERSION_410, GST_GLSL_PROFILE_CORE, "410"},
  {GST_GLSL_VERSION_420, GST_GLSL_PROFILE_CORE, "420"},
  {GST_GLSL_VERSION_430, GST_GLSL_PROFILE_CORE, "430"},
  {GST_GLSL_VERSION_440, GST_GLSL_PROFILE_CORE, "440"},
  {GST_GLSL_VERSION_450, GST_GLSL_PROFILE_CORE, "450"},
};

static const gchar * invalid_deserialize_glsl[] = {
  "",
  " \t\r\n",
  "ael dja",
  "es",
  "core",
  "compatibility",
  "1000",
  "100 es",
  "100 core",
  "100 compatibility",
  "150 es",
  "300 core",
  "300 compatibility",
  "310 core",
  "310 compatibility",
  "320 core",
  "320 compatibility",
  "330 es",
};

static const struct {GstGLSLVersion version; GstGLSLProfile profile;} invalid_serialize_glsl[] = {
  {GST_GLSL_VERSION_100, GST_GLSL_PROFILE_CORE},
  {GST_GLSL_VERSION_100, GST_GLSL_PROFILE_COMPATIBILITY},
  {GST_GLSL_VERSION_110, GST_GLSL_PROFILE_ES},
  {GST_GLSL_VERSION_110, GST_GLSL_PROFILE_CORE},
  {GST_GLSL_VERSION_120, GST_GLSL_PROFILE_ES},
  {GST_GLSL_VERSION_120, GST_GLSL_PROFILE_CORE},
  {GST_GLSL_VERSION_130, GST_GLSL_PROFILE_ES},
  {GST_GLSL_VERSION_130, GST_GLSL_PROFILE_CORE},
  {GST_GLSL_VERSION_140, GST_GLSL_PROFILE_ES},
  {GST_GLSL_VERSION_140, GST_GLSL_PROFILE_CORE},
  {GST_GLSL_VERSION_150, GST_GLSL_PROFILE_ES},
  {GST_GLSL_VERSION_300, GST_GLSL_PROFILE_NONE},
  {GST_GLSL_VERSION_300, GST_GLSL_PROFILE_CORE},
  {GST_GLSL_VERSION_300, GST_GLSL_PROFILE_COMPATIBILITY},
  {GST_GLSL_VERSION_310, GST_GLSL_PROFILE_NONE},
  {GST_GLSL_VERSION_310, GST_GLSL_PROFILE_CORE},
  {GST_GLSL_VERSION_310, GST_GLSL_PROFILE_COMPATIBILITY},
  {GST_GLSL_VERSION_320, GST_GLSL_PROFILE_NONE},
  {GST_GLSL_VERSION_320, GST_GLSL_PROFILE_CORE},
  {GST_GLSL_VERSION_320, GST_GLSL_PROFILE_COMPATIBILITY},
  {GST_GLSL_VERSION_330, GST_GLSL_PROFILE_ES},
  {GST_GLSL_VERSION_400, GST_GLSL_PROFILE_ES},
  {GST_GLSL_VERSION_410, GST_GLSL_PROFILE_ES},
  {GST_GLSL_VERSION_420, GST_GLSL_PROFILE_ES},
  {GST_GLSL_VERSION_430, GST_GLSL_PROFILE_ES},
  {GST_GLSL_VERSION_440, GST_GLSL_PROFILE_ES},
  {GST_GLSL_VERSION_450, GST_GLSL_PROFILE_ES},
};

static const struct {const gchar *name; gboolean succeed; GstGLSLVersion version; GstGLSLProfile profile;} glsl_str_map[] = {
  {"//#version 100\n", FALSE, GST_GLSL_VERSION_NONE, GST_GLSL_PROFILE_NONE},
  {"/*\n#version 100*/\n", FALSE, GST_GLSL_VERSION_NONE, GST_GLSL_PROFILE_NONE},
  {"/*\r#version 100*/", FALSE, GST_GLSL_VERSION_NONE, GST_GLSL_PROFILE_NONE},
  {"#\rversion 100", FALSE, GST_GLSL_VERSION_NONE, GST_GLSL_PROFILE_NONE},
  {"#\nversion 100", FALSE, GST_GLSL_VERSION_NONE, GST_GLSL_PROFILE_NONE},
  {"\t#version 100", FALSE, GST_GLSL_VERSION_NONE, GST_GLSL_PROFILE_NONE},
  {"//\r#version 100", TRUE, GST_GLSL_VERSION_100, GST_GLSL_PROFILE_ES},
  {"//\n#version 100", TRUE, GST_GLSL_VERSION_100, GST_GLSL_PROFILE_ES},
  {"# \tversion 100", TRUE, GST_GLSL_VERSION_100, GST_GLSL_PROFILE_ES},
  {"\n#version 100", TRUE, GST_GLSL_VERSION_100, GST_GLSL_PROFILE_ES},
  {"\r#version 100", TRUE, GST_GLSL_VERSION_100, GST_GLSL_PROFILE_ES},
};
/* *INDENT-ON* */

GST_START_TEST (test_serialization)
{
  gint i;

  /* versions */
  for (i = 0; i < G_N_ELEMENTS (glsl_versions); i++) {
    GstGLSLVersion version;
    const gchar *version_s;

    version_s = gst_glsl_version_to_string (glsl_versions[i].version);
    fail_unless (g_strcmp0 (version_s, glsl_versions[i].name) == 0, "%s != %s",
        version_s, glsl_versions[i].name);
    version = gst_glsl_version_from_string (glsl_versions[i].name);
    fail_unless (version == glsl_versions[i].version, "%s != %s",
        gst_glsl_version_to_string (glsl_versions[i].version),
        gst_glsl_version_to_string (version));
  }

  /* profiles */
  for (i = 0; i < G_N_ELEMENTS (glsl_profiles); i++) {
    GstGLSLProfile profile;
    const gchar *profile_s;

    profile_s = gst_glsl_profile_to_string (glsl_profiles[i].profile);
    fail_unless (g_strcmp0 (profile_s, glsl_profiles[i].name) == 0, "%s != %s",
        profile_s, glsl_profiles[i].name);
    profile = gst_glsl_profile_from_string (glsl_profiles[i].name);
    fail_unless (profile == glsl_profiles[i].profile, "%s != %s",
        gst_glsl_profile_to_string (glsl_profiles[i].profile),
        gst_glsl_profile_to_string (profile));
  }

  for (i = 0; i < G_N_ELEMENTS (glsl_version_profiles_valid); i++) {
    gchar *version_profile_s;
    GstGLSLVersion version;
    GstGLSLProfile profile;

    version_profile_s =
        gst_glsl_version_profile_to_string (glsl_version_profiles_valid
        [i].version, glsl_version_profiles_valid[i].profile);
    fail_unless (g_strcmp0 (version_profile_s,
            glsl_version_profiles_valid[i].name) == 0, "%s != %s",
        version_profile_s, glsl_version_profiles_valid[i].name);
    fail_unless (gst_glsl_version_profile_from_string
        (glsl_version_profiles_valid[i].name, &version, &profile),
        "Failed to parse %s", glsl_version_profiles_valid[i].name);
    fail_unless (profile == glsl_version_profiles_valid[i].profile
        && version == glsl_version_profiles_valid[i].version, "%s != %s %s",
        glsl_version_profiles_valid[i].name,
        gst_glsl_version_to_string (version),
        gst_glsl_profile_to_string (profile));
    g_free (version_profile_s);
  }

  for (i = 0; i < G_N_ELEMENTS (glsl_version_profiles_valid_serialize); i++) {
    gchar *version_profile_s;

    version_profile_s =
        gst_glsl_version_profile_to_string
        (glsl_version_profiles_valid_serialize[i].version,
        glsl_version_profiles_valid_serialize[i].profile);
    fail_unless (g_strcmp0 (version_profile_s,
            glsl_version_profiles_valid_serialize[i].name) == 0, "%s != %s",
        version_profile_s, glsl_version_profiles_valid_serialize[i].name);
    g_free (version_profile_s);
  }

  for (i = 0; i < G_N_ELEMENTS (glsl_version_profiles_valid_deserialize); i++) {
    GstGLSLVersion version;
    GstGLSLProfile profile;

    fail_unless (gst_glsl_version_profile_from_string
        (glsl_version_profiles_valid_deserialize[i].name, &version, &profile),
        "Failed to parse %s", glsl_version_profiles_valid_deserialize[i].name);
    fail_unless (profile == glsl_version_profiles_valid_deserialize[i].profile
        && version == glsl_version_profiles_valid_deserialize[i].version,
        "%s != %s %s", glsl_version_profiles_valid_deserialize[i].name,
        gst_glsl_version_to_string (version),
        gst_glsl_profile_to_string (profile));
  }

  /* failures */
  for (i = 0; i < G_N_ELEMENTS (invalid_deserialize_glsl); i++) {
    GstGLSLVersion version;
    GstGLSLProfile profile;

    fail_if (gst_glsl_version_profile_from_string (invalid_deserialize_glsl[i],
            &version, &profile),
        "successfully deserialized %s into %s %s (should have failed)",
        invalid_deserialize_glsl[i], gst_glsl_version_to_string (version),
        gst_glsl_profile_to_string (profile));
  }

  /* failures */
  for (i = 0; i < G_N_ELEMENTS (invalid_serialize_glsl); i++) {
    gchar *version_profile_s;

    version_profile_s =
        gst_glsl_version_profile_to_string (invalid_serialize_glsl[i].version,
        invalid_serialize_glsl[i].profile);

    fail_if (version_profile_s != NULL,
        "successfully serialized %s from %s %s (should have failed)",
        version_profile_s,
        gst_glsl_version_to_string (invalid_serialize_glsl[i].version),
        gst_glsl_profile_to_string (invalid_serialize_glsl[i].profile));

    g_free (version_profile_s);
  }

  /* map strings to version/profile */
  for (i = 0; i < G_N_ELEMENTS (glsl_str_map); i++) {
    GstGLSLVersion version;
    GstGLSLProfile profile;

    fail_unless (glsl_str_map[i].succeed ==
        gst_glsl_string_get_version_profile (glsl_str_map[i].name, &version,
            &profile), "Incorrect result for parsing \'%s\': %s",
        glsl_str_map[i].name, glsl_str_map[i].succeed ? "false" : "true");
    if (glsl_str_map[i].succeed) {
      fail_unless (version == glsl_str_map[i].version, "With %s: %s != %s",
          glsl_str_map[i].name,
          gst_glsl_version_to_string (glsl_str_map[i].version),
          gst_glsl_version_to_string (version));
      fail_unless (profile == glsl_str_map[i].profile, "With %s: %s != %s",
          glsl_str_map[i].name,
          gst_glsl_profile_to_string (glsl_str_map[i].profile),
          gst_glsl_profile_to_string (profile));
    }
  }

  /* special ones */
  {
    GstGLSLVersion version;
    GstGLSLProfile profile;
    gchar *version_profile_s;

    version_profile_s =
        gst_glsl_version_profile_to_string (GST_GLSL_VERSION_100,
        GST_GLSL_PROFILE_ES);
    fail_unless (g_strcmp0 (version_profile_s, "100") == 0, "%s != 100",
        version_profile_s);
    g_free (version_profile_s);

    version_profile_s =
        gst_glsl_version_profile_to_string (GST_GLSL_VERSION_100,
        GST_GLSL_PROFILE_NONE);
    fail_unless (g_strcmp0 (version_profile_s, "100") == 0, "%s != 100",
        version_profile_s);
    g_free (version_profile_s);

    fail_unless (gst_glsl_version_profile_from_string ("100", &version,
            &profile));
    fail_unless (version == GST_GLSL_VERSION_100
        && profile == GST_GLSL_PROFILE_ES, "100 != %s %s",
        gst_glsl_version_to_string (version),
        gst_glsl_profile_to_string (profile));
  }
}

GST_END_TEST;

static Suite *
gst_gl_upload_suite (void)
{
  Suite *s = suite_create ("GstGLSL");
  TCase *tc_chain = tcase_create ("glsl");

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, setup, teardown);
  tcase_add_test (tc_chain, test_serialization);

  return s;
}

GST_CHECK_MAIN (gst_gl_upload);
