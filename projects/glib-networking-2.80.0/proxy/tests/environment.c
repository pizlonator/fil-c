/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GLibProxyResolver tests
 *
 * Copyright © 2011-2013, 2022 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>

#include "common.c"

/* These tests use subprocesses in order to get a new GEnvironmentProxyResolver
 * for each test. Using a new subprocess also adds a veneer of thread safety to
 * our use of setenv()/unsetenv().
 */

static void
test_proxy_uri (void)
{
  if (!g_test_subprocess ())
    {
      g_test_trap_subprocess (NULL, 0, 0);
      g_test_trap_assert_passed ();
      return;
    }

  g_setenv ("http_proxy", "http://proxy.example.com:8080", TRUE);
  g_setenv ("https_proxy", "http://proxy-s.example.com:7070", TRUE);
  g_setenv ("ftp_proxy", "ftp://proxy-f.example.com:6060", TRUE);

  test_proxy_uri_common ();
}

static void
test_proxy_socks (void)
{
  if (!g_test_subprocess ())
    {
      g_test_trap_subprocess (NULL, 0, 0);
      g_test_trap_assert_passed ();
      return;
    }

  g_setenv ("http_proxy", "socks://proxy.example.com:1234", TRUE);
  g_setenv ("no_proxy", "127.0.0.1", TRUE);

  test_proxy_socks_common ();
}

static void
test_proxy_ignore (void)
{
  gchar *no_proxy;

  if (!g_test_subprocess ())
    {
      g_test_trap_subprocess (NULL, 0, 0);
      g_test_trap_assert_passed ();
      return;
    }

  no_proxy = g_strjoinv (",", (gchar **)ignore_hosts);

  g_setenv ("http_proxy", "http://localhost:8080", TRUE);
  g_setenv ("no_proxy", no_proxy, TRUE);
  g_free (no_proxy);

  test_proxy_ignore_common ();
}

int
main (int   argc,
      char *argv[])
{
  /* Unset variables that would make libproxy try to use gconf or ksettings */
  g_unsetenv ("GNOME_DESKTOP_SESSION_ID");
  g_unsetenv ("DESKTOP_SESSION");
  g_unsetenv ("KDE_FULL_SESSION");

  /* Unset variables that libproxy would look at if it were smarter, and which
   * it might possibly look at in the future. Just covering our bases. */
  g_unsetenv ("XDG_CURRENT_DESKTOP");

  /* Unset static proxy settings */
  g_unsetenv ("http_proxy");
  g_unsetenv ("HTTP_PROXY");
  g_unsetenv ("https_proxy");
  g_unsetenv ("HTTPS_PROXY");
  g_unsetenv ("ftp_proxy");
  g_unsetenv ("FTP_PROXY");
  g_unsetenv ("no_proxy");
  g_unsetenv ("NO_PROXY");

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/proxy/environment/uri", test_proxy_uri);
  g_test_add_func ("/proxy/environment/socks", test_proxy_socks);
  g_test_add_func ("/proxy/environment/ignore", test_proxy_ignore);

  return g_test_run();
}
