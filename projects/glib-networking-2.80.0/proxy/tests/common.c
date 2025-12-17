/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GProxyResolver tests
 *
 * Copyright 2011-2013 Red Hat, Inc.
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

static void
test_proxy_uri_common (void)
{
  GProxyResolver *resolver;
  gchar **proxies;
  GError *error = NULL;

  resolver = g_proxy_resolver_get_default ();

  proxies = g_proxy_resolver_lookup (resolver, "http://one.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "http://proxy.example.com:8080");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "HTTPS://uppercase.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "http://proxy-s.example.com:7070");
  g_strfreev (proxies);

  /* Unknown protocols will use the http proxy by default in this configuration. */
  proxies = g_proxy_resolver_lookup (resolver, "htt://missing-letter.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "http://proxy.example.com:8080");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "ftps://extra-letter.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "http://proxy.example.com:8080");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "ftp://five.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "ftp://proxy-f.example.com:6060");
  g_strfreev (proxies);
}

static void
test_proxy_socks_common (void)
{
  GProxyResolver *resolver;
  gchar **proxies;
  GError *error = NULL;

  resolver = g_proxy_resolver_get_default ();

  proxies = g_proxy_resolver_lookup (resolver, "http://one.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 3);
  g_assert_cmpstr (proxies[0], ==, "socks5://proxy.example.com:1234");
  g_assert_cmpstr (proxies[1], ==, "socks4a://proxy.example.com:1234");
  g_assert_cmpstr (proxies[2], ==, "socks4://proxy.example.com:1234");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "wednesday://two.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 3);
  g_assert_cmpstr (proxies[0], ==, "socks5://proxy.example.com:1234");
  g_assert_cmpstr (proxies[1], ==, "socks4a://proxy.example.com:1234");
  g_assert_cmpstr (proxies[2], ==, "socks4://proxy.example.com:1234");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "http://127.0.0.1/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "direct://");
  g_strfreev (proxies);
}

static const char *ignore_hosts[] = {
  ".bbb.xx",
  "*.ccc.xx",
  "ddd.xx",
  "*.eee.xx:8000",
  "127.0.0.0/24",
  "10.0.0.1:8000",
  "::1",
  "fe80::/10",
  NULL
};
static const int n_ignore_hosts = G_N_ELEMENTS (ignore_hosts) - 1;

static const struct {
  const char *uri;
  const char *proxy;
  gboolean libproxy_fails;
} ignore_tests[] = {
  { "http://aaa.xx/",                   "http://localhost:8080" },
  { "http://aaa.xx:8000/",              "http://localhost:8080" },
  { "http://www.aaa.xx/",               "http://localhost:8080" },
  { "http://www.aaa.xx:8000/",          "http://localhost:8080" },
  { "https://aaa.xx/",                  "http://localhost:8080" },
  { "http://bbb.xx/",                   "direct://", TRUE },
  { "http://www.bbb.xx/",               "direct://" },
  { "http://bbb.xx:8000/",              "direct://", TRUE },
  { "http://www.bbb.xx:8000/",          "direct://" },
  { "https://bbb.xx/",                  "direct://", TRUE },
  { "http://nobbb.xx/",          "http://localhost:8080" },
  { "http://www.nobbb.xx/",      "http://localhost:8080" },
  { "http://nobbb.xx:8000/",     "http://localhost:8080" },
  { "http://www.nobbb.xx:8000/", "http://localhost:8080" },
  { "https://nobbb.xx/",         "http://localhost:8080" },
  { "http://ccc.xx/",                   "direct://", TRUE },
  { "http://www.ccc.xx/",               "direct://" },
  { "http://ccc.xx:8000/",              "direct://", TRUE },
  { "http://www.ccc.xx:8000/",          "direct://" },
  { "https://ccc.xx/",                  "direct://", TRUE },
  { "http://ddd.xx/",                   "direct://" },
  { "http://ddd.xx:8000/",              "direct://" },
  { "http://www.ddd.xx/",               "direct://", TRUE },
  { "http://www.ddd.xx:8000/",          "direct://", TRUE },
  { "https://ddd.xx/",                  "direct://" },
  { "http://eee.xx/",                   "http://localhost:8080", TRUE },
  { "http://eee.xx:8000/",              "direct://", TRUE },
  { "http://www.eee.xx/",               "http://localhost:8080" },
  { "http://www.eee.xx:8000/",          "direct://" },
  { "https://eee.xx/",                  "http://localhost:8080", TRUE },
  { "http://1.2.3.4/",                  "http://localhost:8080" },
  { "http://127.0.0.1/",                "direct://" },
  { "http://127.0.0.2/",                "direct://" },
  { "http://127.0.0.255/",              "direct://" },
  { "http://127.0.1.0/",                "http://localhost:8080" },
  { "http://10.0.0.1/",                 "http://localhost:8080" },
  { "http://10.0.0.1:8000/",            "direct://" },
  { "http://[::1]/",                    "direct://", TRUE },
  { "http://[::1]:80/",                 "direct://", TRUE },
  { "http://[::1:1]/",                  "http://localhost:8080" },
  { "http://[::1:1]:80/",               "http://localhost:8080" },
  { "http://[fe80::1]/",                "direct://", TRUE },
  { "http://[fe80::1]:80/",             "direct://", TRUE },
  { "http://[fec0::1]/",                "http://localhost:8080" },
  { "http://[fec0::1]:80/",             "http://localhost:8080" }
};
static const int n_ignore_tests = G_N_ELEMENTS (ignore_tests);

static void
test_proxy_ignore_common (void)
{
  GProxyResolver *resolver;
  GError *error = NULL;
  char **proxies;
  int i;

#ifndef LIBPROXY_0_5
  gboolean is_libproxy = g_strcmp0 (g_getenv ("GIO_PROXY_TEST_NAME"), "libproxy") == 0;
#endif

  resolver = g_proxy_resolver_get_default ();

  for (i = 0; i < n_ignore_tests; i++)
    {
      proxies = g_proxy_resolver_lookup (resolver, ignore_tests[i].uri,
                                         NULL, &error);
      g_assert_no_error (error);

#ifndef LIBPROXY_0_5
      if (is_libproxy && ignore_tests[i].libproxy_fails)
        g_assert_cmpstr (proxies[0], ==, "http://localhost:8080");
      else
#endif
        g_assert_cmpstr (proxies[0], ==, ignore_tests[i].proxy);

      g_strfreev (proxies);
    }
}
