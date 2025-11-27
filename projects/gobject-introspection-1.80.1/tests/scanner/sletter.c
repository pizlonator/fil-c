/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#include "config.h"

#include "sletter.h"

void
s_hello (void)
{
  g_print ("hello world\n");
}

GQuark
s_spawn_error_quark (void)
{
  return g_quark_from_static_string ("s-spawn-error");
}

GQuark
s_dbus_error_quark (void)
{
  return g_quark_from_static_string ("s-dbus-error");
}
