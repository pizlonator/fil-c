/* GStreamer unit test for gstprofile
 *
 * Copyright (C) <2009> Edward Hervey <edward.hervey@collabora.co.uk>
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
#include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <gst/check/gstcheck.h>

#include <gst/pbutils/encoding-profile.h>
#include <gst/pbutils/encoding-target.h>
#include <gst/pbutils/pbutils.h>

#ifdef G_OS_UNIX
#include <unistd.h>             /* For R_OK etc. */
#endif

static inline gboolean
gst_caps_is_equal_unref (GstCaps * caps1, GstCaps * caps2)
{
  gboolean ret;

  ret = gst_caps_is_equal (caps1, caps2);
  gst_caps_unref (caps1);

  return ret;
}

#define CHECK_PROFILE(profile, name, description, format, preset, presence, restriction) \
  {									\
  fail_if(profile == NULL);						\
  fail_unless_equals_string (gst_encoding_profile_get_name (profile), name); \
  fail_unless_equals_string (gst_encoding_profile_get_description (profile), description); \
  fail_unless (gst_caps_is_equal_unref (gst_encoding_profile_get_format (profile), format)); \
  fail_unless_equals_string (gst_encoding_profile_get_preset (profile), preset); \
  fail_unless_equals_int (gst_encoding_profile_get_presence (profile), presence); \
  if (restriction) \
    fail_unless (gst_caps_is_equal_unref (gst_encoding_profile_get_restriction (profile), restriction)); \
  }

GST_START_TEST (test_profile_creation)
{
  GstEncodingProfile *encprof;
  GstEncodingAudioProfile *audioprof;
  GstEncodingVideoProfile *videoprof;
  GstCaps *ogg, *vorbis, *theora;
  GstCaps *test1, *test2;

  ogg = gst_caps_new_empty_simple ("application/ogg");
  vorbis = gst_caps_new_empty_simple ("audio/x-vorbis");
  theora = gst_caps_new_empty_simple ("video/x-theora");

  encprof = (GstEncodingProfile *) gst_encoding_container_profile_new ((gchar *)
      "ogg-theora-vorbis", "dumb-profile", ogg, (gchar *) "dumb-preset");
  CHECK_PROFILE (encprof, "ogg-theora-vorbis", "dumb-profile", ogg,
      "dumb-preset", 0, NULL);

  audioprof = gst_encoding_audio_profile_new (vorbis, (gchar *) "HQ", NULL, 0);
  CHECK_PROFILE ((GstEncodingProfile *) audioprof, NULL, NULL, vorbis, "HQ", 0,
      NULL);

  videoprof = gst_encoding_video_profile_new (theora, (gchar *) "HQ", NULL, 0);
  CHECK_PROFILE ((GstEncodingProfile *) videoprof, NULL, NULL, theora, "HQ",
      0, NULL);

  fail_unless (gst_encoding_container_profile_add_profile (
          (GstEncodingContainerProfile *) encprof,
          (GstEncodingProfile *) audioprof));
  fail_unless (gst_encoding_container_profile_add_profile (
          (GstEncodingContainerProfile *) encprof,
          (GstEncodingProfile *) videoprof));

  /* Test caps */
  test1 = gst_caps_from_string ("video/x-theora; audio/x-vorbis");
  test2 = gst_encoding_profile_get_input_caps (encprof);
  fail_unless (gst_caps_is_equal (test1, test2));
  gst_caps_unref (test1);
  gst_caps_unref (test2);

  gst_encoding_profile_unref (encprof);
  gst_caps_unref (ogg);
  gst_caps_unref (theora);
  gst_caps_unref (vorbis);
}

GST_END_TEST;


GST_START_TEST (test_profile_input_caps)
{
  GstEncodingProfile *sprof;
  GstCaps *vorbis;
  GstCaps *out, *restriction, *test1;

  vorbis = gst_caps_new_empty_simple ("audio/x-vorbis");

  /* Simple case, no restriction */
  sprof = (GstEncodingProfile *)
      gst_encoding_audio_profile_new (vorbis, NULL, NULL, 0);
  fail_if (sprof == NULL);

  out = gst_encoding_profile_get_input_caps (sprof);
  fail_if (out == NULL);
  fail_unless (gst_caps_is_equal (out, vorbis));
  gst_caps_unref (out);
  gst_encoding_profile_unref (sprof);

  /* One simple restriction */
  restriction = gst_caps_from_string ("audio/x-raw,channels=2,rate=44100");
  test1 = gst_caps_from_string ("audio/x-vorbis,channels=2,rate=44100");
  fail_if (restriction == NULL);

  sprof = (GstEncodingProfile *)
      gst_encoding_audio_profile_new (vorbis, NULL, restriction, 0);
  fail_if (sprof == NULL);

  out = gst_encoding_profile_get_input_caps (sprof);
  fail_if (out == NULL);
  GST_DEBUG ("got caps %" GST_PTR_FORMAT, out);
  fail_unless (gst_caps_is_equal (out, test1));
  gst_caps_unref (out);
  gst_caps_unref (restriction);
  gst_caps_unref (test1);
  gst_encoding_profile_unref (sprof);

  gst_caps_unref (vorbis);
}

GST_END_TEST;


GST_START_TEST (test_target_naming)
{
  GstEncodingTarget *target;

  gst_debug_set_threshold_for_name ("default", GST_LEVEL_NONE);

  /* NULL values */
  ASSERT_CRITICAL (target = gst_encoding_target_new (NULL, NULL, NULL, NULL));
  fail_if (target != NULL);
  ASSERT_CRITICAL (target =
      gst_encoding_target_new ("donkey", NULL, NULL, NULL));
  fail_if (target != NULL);
  ASSERT_CRITICAL (target =
      gst_encoding_target_new (NULL, "donkey", NULL, NULL));
  fail_if (target != NULL);
  ASSERT_CRITICAL (target =
      gst_encoding_target_new (NULL, NULL, "donkey", NULL));
  fail_if (target != NULL);

  /* Name and Category validation */

  /* empty non-NULL strings */
  fail_if (gst_encoding_target_new ("", "valid", "description", NULL) != NULL);
  fail_if (gst_encoding_target_new ("valid", "", "description", NULL) != NULL);

  /* don't start with a lower case ASCII character */
  fail_if (gst_encoding_target_new ("A", "valid", "description", NULL) != NULL);
  fail_if (gst_encoding_target_new ("3", "valid", "description", NULL) != NULL);
  fail_if (gst_encoding_target_new ("-", "valid", "description", NULL) != NULL);
  fail_if (gst_encoding_target_new ("!", "valid", "description", NULL) != NULL);
  fail_if (gst_encoding_target_new (" ", "valid", "description", NULL) != NULL);
  fail_if (gst_encoding_target_new ("valid", "A", "description", NULL) != NULL);
  fail_if (gst_encoding_target_new ("valid", "3", "description", NULL) != NULL);
  fail_if (gst_encoding_target_new ("valid", "-", "description", NULL) != NULL);
  fail_if (gst_encoding_target_new ("valid", "!", "description", NULL) != NULL);
  fail_if (gst_encoding_target_new ("valid", " ", "description", NULL) != NULL);

  /* Starting with anything else is valid */
  target = gst_encoding_target_new ("a", "valid", "description", NULL);
  fail_if (target == NULL);
  gst_encoding_target_unref (target);
  target = gst_encoding_target_new ("z", "valid", "description", NULL);
  fail_if (target == NULL);
  gst_encoding_target_unref (target);
  target = gst_encoding_target_new ("valid", "a", "description", NULL);
  fail_if (target == NULL);
  gst_encoding_target_unref (target);
  target = gst_encoding_target_new ("valid", "z", "description", NULL);
  fail_if (target == NULL);
  gst_encoding_target_unref (target);

  /* only inner valid characters are lower-case ASCII letters *OR* digits *OR* hyphens */
  fail_if (gst_encoding_target_new ("aA", "valid", "description",
          NULL) != NULL);
  fail_if (gst_encoding_target_new ("a!", "valid", "description",
          NULL) != NULL);
  fail_if (gst_encoding_target_new ("space donkeys", "valid", "description",
          NULL) != NULL);
  fail_if (gst_encoding_target_new ("howaboutùnicode", "valid", "description",
          NULL) != NULL);
  fail_if (gst_encoding_target_new ("valid", "aA", "description",
          NULL) != NULL);
  fail_if (gst_encoding_target_new ("valid", "a!", "description",
          NULL) != NULL);

  target =
      gst_encoding_target_new ("donkey-4-ever", "valid", "description", NULL);
  fail_if (target == NULL);
  gst_encoding_target_unref (target);
  target =
      gst_encoding_target_new ("valid", "donkey-4-ever", "description", NULL);
  fail_if (target == NULL);
  gst_encoding_target_unref (target);

}

GST_END_TEST;

static GstEncodingTarget *
create_saveload_target (const gchar * targetname)
{
  GstEncodingTarget *target;
  GstEncodingProfile *profile, *sprof;
  GstCaps *caps, *caps2;

  GST_DEBUG ("Creating target");

  target = gst_encoding_target_new (targetname, "herding",
      "Plenty of pony glitter profiles", NULL);
  caps = gst_caps_from_string ("animal/x-pony");
  profile =
      (GstEncodingProfile *) gst_encoding_container_profile_new ("pony",
      "I don't want a description !", caps, NULL);
  gst_caps_unref (caps);
  gst_encoding_target_add_profile (target, profile);

  caps = gst_caps_from_string ("audio/x-pony-song,pretty=True");
  caps2 = gst_caps_from_string ("audio/x-raw,channels=1,rate=44100");
  sprof =
      (GstEncodingProfile *) gst_encoding_audio_profile_new (caps, NULL, caps2,
      1);
  gst_encoding_container_profile_add_profile ((GstEncodingContainerProfile *)
      profile, sprof);
  gst_caps_unref (caps);
  gst_caps_unref (caps2);

  caps = gst_caps_from_string ("video/x-glitter,sparkling=True");
  caps2 =
      gst_caps_from_string ("video/x-raw,width=640,height=480,framerate=15/1");
  sprof = (GstEncodingProfile *)
      gst_encoding_video_profile_new (caps, "seriously glittery", caps2, 0);
  gst_encoding_video_profile_set_variableframerate ((GstEncodingVideoProfile *)
      sprof, TRUE);
  gst_encoding_container_profile_add_profile ((GstEncodingContainerProfile *)
      profile, sprof);
  gst_caps_unref (caps);
  gst_caps_unref (caps2);

  return target;
}

GST_START_TEST (test_target_profile)
{
  GstEncodingTarget *target;
  GstEncodingProfile *prof;

  target = create_saveload_target ("myponytarget");

  /* NULL isn't a valid profile name */
  ASSERT_CRITICAL (gst_encoding_target_get_profile (target, NULL));

  /* try finding a profile that doesn't exist */
  fail_if (gst_encoding_target_get_profile (target,
          "no-really-does-not-exist"));

  /* try finding a profile that exists */
  prof = gst_encoding_target_get_profile (target, "pony");
  fail_if (prof == NULL);

  gst_encoding_profile_unref (prof);
  gst_encoding_target_unref (target);
}

GST_END_TEST;

GST_START_TEST (test_saving_profile)
{
  GstEncodingTarget *orig, *loaded = NULL;
  GstEncodingProfile *proforig, *profloaded;
  gchar *profile_file_name;

  /* Create and store a target */
  orig = create_saveload_target ("myponytarget2");
  GST_DEBUG ("Saving target 'myponytarget2'");
  fail_unless (gst_encoding_target_save (orig, NULL));

  /* Check we can load it */
  profile_file_name =
      g_build_filename (g_get_user_data_dir (), "gstreamer-1.0",
      "encoding-profiles", "herding", "myponytarget2.gep", NULL);
  GST_DEBUG ("Loading target from '%s'", profile_file_name);
  loaded = gst_encoding_target_load_from_file (profile_file_name, NULL);
  fail_unless (loaded != NULL);
  g_free (profile_file_name);

  GST_DEBUG ("Checking targets are equal");
  /* Check targets are identical */
  /* 1. at the target level */
  fail_unless_equals_string (gst_encoding_target_get_name (orig),
      gst_encoding_target_get_name (loaded));
  fail_unless_equals_string (gst_encoding_target_get_category (orig),
      gst_encoding_target_get_category (loaded));
  fail_unless_equals_string (gst_encoding_target_get_description (orig),
      gst_encoding_target_get_description (loaded));
  fail_unless_equals_int (g_list_length ((GList *)
          gst_encoding_target_get_profiles (loaded)), 1);

  /* 2. at the profile level */
  profloaded =
      (GstEncodingProfile *) gst_encoding_target_get_profiles (loaded)->data;
  proforig =
      (GstEncodingProfile *) gst_encoding_target_get_profiles (orig)->data;

  fail_unless_equals_int (G_TYPE_FROM_INSTANCE (profloaded),
      G_TYPE_FROM_INSTANCE (proforig));
  GST_DEBUG ("Comparing loaded:%p to original:%p", profloaded, proforig);
  fail_unless (gst_encoding_profile_is_equal (profloaded, proforig));

  gst_encoding_target_unref (orig);
  gst_encoding_target_unref (loaded);
}

GST_END_TEST;

static void
test_individual_target (GstEncodingTarget * target)
{
  GstEncodingProfile *prof;
  GstCaps *tmpcaps, *tmpcaps2;
  GstEncodingProfile *sprof1, *sprof2;

  GST_DEBUG ("Checking the target properties");
  /* Check the target  */
  fail_unless_equals_string (gst_encoding_target_get_name (target),
      "myponytarget");
  fail_unless_equals_string (gst_encoding_target_get_category (target),
      "herding");
  fail_unless_equals_string (gst_encoding_target_get_description (target),
      "Plenty of pony glitter profiles");

  GST_DEBUG ("Checking the number of profiles the target contains");
  fail_unless_equals_int (g_list_length ((GList *)
          gst_encoding_target_get_profiles (target)), 1);


  GST_DEBUG ("Checking the container profile");
  /* Check the profile */
  prof = (GstEncodingProfile *) gst_encoding_target_get_profiles (target)->data;
  tmpcaps = gst_caps_from_string ("animal/x-pony");
  CHECK_PROFILE (prof, "pony", "I don't want a description !", tmpcaps, NULL, 0,
      0);
  gst_caps_unref (tmpcaps);

  GST_DEBUG ("Checking the container profile has 2 stream profiles");
  /* Check the stream profiles */
  fail_unless_equals_int (g_list_length ((GList *)
          gst_encoding_container_profile_get_profiles (
              (GstEncodingContainerProfile *) prof)), 2);

  GST_DEBUG ("Checking the container profile has the audio/x-pony-song stream");
  tmpcaps = gst_caps_from_string ("audio/x-pony-song,pretty=True");
  tmpcaps2 = gst_caps_from_string ("audio/x-raw,channels=1,rate=44100");
  sprof1 =
      (GstEncodingProfile *) gst_encoding_audio_profile_new (tmpcaps, NULL,
      tmpcaps2, 1);
  fail_unless (gst_encoding_container_profile_contains_profile (
          (GstEncodingContainerProfile *) prof, sprof1));
  gst_encoding_profile_unref (sprof1);
  gst_caps_unref (tmpcaps);
  gst_caps_unref (tmpcaps2);

  GST_DEBUG ("Checking the container profile has the video//x-glitter stream");
  tmpcaps = gst_caps_from_string ("video/x-glitter,sparkling=True");
  tmpcaps2 =
      gst_caps_from_string ("video/x-raw,width=640,height=480,framerate=15/1");
  sprof2 = (GstEncodingProfile *)
      gst_encoding_video_profile_new (tmpcaps, "seriously glittery", tmpcaps2,
      0);
  gst_encoding_video_profile_set_variableframerate ((GstEncodingVideoProfile *)
      sprof2, TRUE);
  fail_unless (gst_encoding_container_profile_contains_profile (
          (GstEncodingContainerProfile *) prof, sprof2));
  gst_encoding_profile_unref (sprof2);
  gst_caps_unref (tmpcaps);
  gst_caps_unref (tmpcaps2);
}

GST_START_TEST (test_loading_profile)
{
  GstEncodingTarget *target;
  gchar *profile_file_name;
  GstEncodingProfile *profile;
  GstCaps *tmpcaps;
  GValue strvalue = { 0, };
  GValue objectvalue = { 0, };

  gst_debug_set_threshold_for_name ("default", GST_LEVEL_NONE);

  /* Test loading using short method and all arguments */
  target = gst_encoding_target_load ("myponytarget", "herding", NULL);
  fail_unless (target != NULL);
  test_individual_target (target);
  gst_encoding_target_unref (target);

  /* Test loading using short method and no category */
  target = gst_encoding_target_load ("myponytarget", NULL, NULL);
  fail_unless (target != NULL);
  test_individual_target (target);
  gst_encoding_target_unref (target);

  /* Test loading using fully specified path */
  profile_file_name =
      g_build_filename (g_get_user_data_dir (), "gstreamer-1.0",
      "encoding-profiles", "herding", "myponytarget.gep", NULL);

  GST_DEBUG ("Loading target from '%s'", profile_file_name);
  target = gst_encoding_target_load_from_file (profile_file_name, NULL);
  g_free (profile_file_name);
  fail_unless (target != NULL);
  test_individual_target (target);
  gst_encoding_target_unref (target);

  /* Test getting the profiles directly
   * First without category */
  profile = gst_encoding_profile_find ("myponytarget", "pony", NULL);
  fail_unless (profile != NULL);
  tmpcaps = gst_caps_from_string ("animal/x-pony");
  CHECK_PROFILE (profile, "pony", "I don't want a description !", tmpcaps, NULL,
      0, 0);
  gst_caps_unref (tmpcaps);
  gst_encoding_profile_unref (profile);

  /* Then with a specific category */
  profile = gst_encoding_profile_find ("myponytarget", "pony", "herding");
  fail_unless (profile != NULL);
  tmpcaps = gst_caps_from_string ("animal/x-pony");
  CHECK_PROFILE (profile, "pony", "I don't want a description !", tmpcaps, NULL,
      0, 0);
  gst_caps_unref (tmpcaps);
  gst_encoding_profile_unref (profile);

  /* For my next trick, I will need the assistance of a GValue */
  g_value_init (&strvalue, G_TYPE_STRING);
  g_value_init (&objectvalue, GST_TYPE_ENCODING_PROFILE);
  g_value_set_static_string (&strvalue, "myponytarget/pony");
  fail_unless (g_value_transform (&strvalue, &objectvalue));
  profile = (GstEncodingProfile *) g_value_dup_object (&objectvalue);
  fail_if (profile == NULL);
  g_value_unset (&strvalue);
  g_value_unset (&objectvalue);
  tmpcaps = gst_caps_from_string ("animal/x-pony");
  CHECK_PROFILE (profile, "pony", "I don't want a description !", tmpcaps, NULL,
      0, 0);
  gst_caps_unref (tmpcaps);
  gst_encoding_profile_unref (profile);

  /* Let's go crazy for error detection */
  fail_if (gst_encoding_profile_find ("myponytarget", "whales", NULL));
  fail_if (gst_encoding_profile_find ("myponytarget", "whales", "herding"));
  fail_if (gst_encoding_profile_find ("myponytarget", "", NULL));
  fail_if (gst_encoding_profile_find ("", "pony", NULL));
}

GST_END_TEST;

GST_START_TEST (test_target_list)
{
  GList *categories;
  GList *targets;
  GList *tmp;

  /* Make sure we get our test category in the available categories */
  categories = gst_encoding_list_available_categories ();
  fail_if (categories == NULL);
  fail_if (g_list_find_custom (categories, "herding",
          (GCompareFunc) g_strcmp0) == NULL);
  g_list_foreach (categories, (GFunc) g_free, NULL);
  g_list_free (categories);

  /* Try getting all available targets with a specified category */
  targets = gst_encoding_list_all_targets ("herding");
  fail_if (targets == NULL);
  for (tmp = targets; tmp; tmp = tmp->next) {
    GstEncodingTarget *target = (GstEncodingTarget *) tmp->data;
    if (!g_strcmp0 (gst_encoding_target_get_name (target), "myponytarget"))
      break;
  }
  /* If tmp is NULL, it means we iterated the whole list without finding
   * our target */
  fail_if (tmp == NULL);
  g_list_foreach (targets, (GFunc) g_object_unref, NULL);
  g_list_free (targets);

  /* Try getting all available targets without a specified category */
  targets = gst_encoding_list_all_targets (NULL);
  fail_if (targets == NULL);
  for (tmp = targets; tmp; tmp = tmp->next) {
    GstEncodingTarget *target = (GstEncodingTarget *) tmp->data;
    if (!g_strcmp0 (gst_encoding_target_get_name (target), "myponytarget"))
      break;
  }
  /* If tmp is NULL, it means we iterated the whole list without finding
   * our target */
  fail_if (tmp == NULL);
  g_list_foreach (targets, (GFunc) g_object_unref, NULL);
  g_list_free (targets);
}

GST_END_TEST;


static const gchar *profile_string = "\
[GStreamer Encoding Target]\n\
name=myponytarget\n\
category=herding\n\
description=Plenty of pony glitter profiles\n\
\n\
[profile-pony1]\n\
name=pony\n\
type=container\n\
description=I don't want a description !\n\
format=animal/x-pony\n\
\n\
[streamprofile-pony11]\n\
parent=pony\n\
type=audio\n\
format=audio/x-pony-song,pretty=True\n\
restriction=audio/x-raw,channels=1,rate=44100\n\
presence=1\n\
\n\
[streamprofile-pony12]\n\
parent=pony\n\
type=video\n\
preset=seriously glittery\n\
format=video/x-glitter,sparkling=True\n\
restriction=video/x-raw,width=640,height=480,framerate=15/1\n\
presence=0\n\
variableframerate=true\n\
";

static void
remove_profile_file (void)
{
  gchar *profile_file_name;

  profile_file_name =
      g_build_filename (g_get_user_data_dir (), "gstreamer-1.0",
      "encoding-profiles", "herding", "myponytarget.gep", NULL);
  g_unlink (profile_file_name);
  g_free (profile_file_name);
  profile_file_name =
      g_build_filename (g_get_user_data_dir (), "gstreamer-1.0",
      "encoding-profiles", "herding", "myponytarget2.gep", NULL);
  g_unlink (profile_file_name);
  g_free (profile_file_name);
}

static void
create_profile_file (void)
{
  gchar *profile_file_name;
  gchar *profile_dir;
  GError *error = NULL;

  profile_dir =
      g_build_filename (g_get_user_data_dir (), "gstreamer-1.0",
      "encoding-profiles", "herding", NULL);
  profile_file_name =
      g_build_filename (g_get_user_data_dir (), "gstreamer-1.0",
      "encoding-profiles", "herding", "myponytarget.gep", NULL);

  /* on Windows it will ignore the mode anyway */
#ifdef G_OS_WIN32
  g_mkdir_with_parents (profile_dir, 0700);
#else
  g_mkdir_with_parents (profile_dir, S_IRUSR | S_IWUSR | S_IXUSR);
#endif

  if (!g_file_set_contents (profile_file_name, profile_string,
          strlen (profile_string), &error))
    GST_WARNING ("Couldn't write contents to file : %s", error->message);
  g_free (profile_dir);
  g_free (profile_file_name);
}

static void
test_setup (void)
{
  create_profile_file ();
}

static void
test_teardown (void)
{
  remove_profile_file ();
}

GST_START_TEST (test_file_extension)
{
  GstEncodingContainerProfile *cprof;
  GstCaps *ogg, *speex, *vorbis, *theora, *id3, *mp3;

  /* 1 - ogg variants */
  ogg = gst_caps_new_empty_simple ("application/ogg");
  cprof = gst_encoding_container_profile_new ("myprofile", NULL, ogg, NULL);
  gst_caps_unref (ogg);

  fail_unless_equals_string (gst_encoding_profile_get_file_extension
      (GST_ENCODING_PROFILE (cprof)), "ogg");

  speex = gst_caps_new_empty_simple ("audio/x-speex");
  gst_encoding_container_profile_add_profile (cprof,
      (GstEncodingProfile *) gst_encoding_audio_profile_new (speex, NULL,
          NULL, 1));
  gst_caps_unref (speex);

  fail_unless_equals_string (gst_encoding_profile_get_file_extension
      (GST_ENCODING_PROFILE (cprof)), "spx");

  vorbis = gst_caps_new_empty_simple ("audio/x-vorbis");
  gst_encoding_container_profile_add_profile (cprof,
      (GstEncodingProfile *) gst_encoding_audio_profile_new (vorbis, NULL,
          NULL, 1));
  gst_caps_unref (vorbis);

  fail_unless_equals_string (gst_encoding_profile_get_file_extension
      (GST_ENCODING_PROFILE (cprof)), "ogg");

  theora = gst_caps_new_empty_simple ("video/x-theora");
  gst_encoding_container_profile_add_profile (cprof,
      (GstEncodingProfile *) gst_encoding_video_profile_new (theora, NULL,
          NULL, 1));
  gst_caps_unref (theora);

  fail_unless_equals_string (gst_encoding_profile_get_file_extension
      (GST_ENCODING_PROFILE (cprof)), "ogv");

  gst_encoding_profile_unref (cprof);

  /* 2 - tag container */
  id3 = gst_caps_new_empty_simple ("application/x-id3");
  cprof = gst_encoding_container_profile_new ("myprofile", NULL, id3, NULL);
  gst_caps_unref (id3);

  fail_unless (gst_encoding_profile_get_file_extension (GST_ENCODING_PROFILE
          (cprof)) == NULL);

  mp3 = gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
      "layer", G_TYPE_INT, 3, NULL);
  gst_encoding_container_profile_add_profile (cprof,
      (GstEncodingProfile *) gst_encoding_audio_profile_new (mp3, NULL,
          NULL, 1));
  gst_caps_unref (mp3);

  fail_unless_equals_string (gst_encoding_profile_get_file_extension
      (GST_ENCODING_PROFILE (cprof)), "mp3");

  gst_encoding_profile_unref (cprof);
}

GST_END_TEST;

static Suite *
profile_suite (void)
{
  Suite *s = suite_create ("profile support library");
  TCase *tc_chain = tcase_create ("general");
  gboolean can_write;

  /* check if we can create profiles */
#ifdef G_OS_UNIX
  {
    gchar *gst_dir =
        g_build_filename (g_get_user_data_dir (), "gstreamer-1.0", NULL);
    can_write = (g_access (gst_dir, R_OK | W_OK | X_OK) == 0);
    g_free (gst_dir);
  }
#else
  can_write = FALSE;            /* FIXME: fix can_write test on Windows */
#endif

  gst_pb_utils_init ();

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_profile_creation);
  tcase_add_test (tc_chain, test_profile_input_caps);
  tcase_add_test (tc_chain, test_target_naming);
  tcase_add_test (tc_chain, test_target_profile);
  tcase_add_test (tc_chain, test_file_extension);
  if (can_write) {
    tcase_add_test (tc_chain, test_loading_profile);
    tcase_add_test (tc_chain, test_saving_profile);
    tcase_add_test (tc_chain, test_target_list);
  }

  tcase_add_unchecked_fixture (tc_chain, test_setup, test_teardown);

  return s;
}

GST_CHECK_MAIN (profile);
