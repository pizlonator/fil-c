/*
 * Copyright (C) 2008 Jan Schmidt <thaytan@noraisin.net>
 */

#ifndef __RSN_PARSETTER_H__
#define __RSN_PARSETTER_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define RSN_TYPE_RSNPARSETTER \
  (rsn_parsetter_get_type())
#define RSN_PARSETTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),RSN_TYPE_RSNPARSETTER,RsnParSetter))
#define RSN_PARSETTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),RSN_TYPE_RSNPARSETTER,RsnParSetterClass))
#define RSN_IS_PARSETTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),RSN_TYPE_RSNPARSETTER))
#define RSN_IS_PARSETTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),RSN_TYPE_RSNPARSETTER))

typedef struct _RsnParSetter      RsnParSetter;
typedef struct _RsnParSetterClass RsnParSetterClass;

struct _RsnParSetter
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  gboolean override_outcaps;
  GstCaps *outcaps;

  gboolean is_widescreen;

  GstCaps *in_caps_last;
  gboolean in_caps_was_ok;
  GstCaps *in_caps_converted;
};

struct _RsnParSetterClass 
{
  GstElementClass parent_class;
};

GType rsn_parsetter_get_type (void);

G_END_DECLS

#endif /* __RSN_PARSETTER_H__ */
