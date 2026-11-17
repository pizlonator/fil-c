/*
 * Copyright © 2013-2015 Red Hat, Inc.
 * Copyright © 2022, 2023 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 *(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "uuid.hh"

#include <glib.h>

using namespace vte;

constinit auto const dummy_uuid = VTE_DEFINE_UUID(1, 2, 1003, 8004, 5);
constinit auto const gnome_uuid3 = VTE_DEFINE_UUID(eeec79ff, 4091, 3991, a17c, 1cbd847e92db);
constinit auto const gnome_uuid5 = VTE_DEFINE_UUID(362b097a, 0554, 5ee4, bb28, 6173eaf6bbef);

// The NIL UUID
constinit uuid const nil = uuid{};

// When constructing a v3 or v5 UUID using this namespace,
// the name string is a FQDN
constinit uuid const uuid_namespace_dns = VTE_DEFINE_UUID(6ba7b810, 9dad, 11d1, 80b4, 00c04fd430c8);

// When constructing a v3 or v5 UUID using this namespace,
// the name string is an URL
constinit uuid const uuid_namespace_url = VTE_DEFINE_UUID(6ba7b811, 9dad, 11d1, 80b4, 00c04fd430c8);

// When constructing a v3 or v5 UUID using this namespace,
// the name string is an ISO OID
constinit uuid const uuid_namespace_oid = VTE_DEFINE_UUID(6ba7b812, 9dad, 11d1, 80b4, 00c04fd430c8);

// When constructing a v3 or v5 UUID using this namespace,
// the name string is an X.500 DN in DER or text output format
constinit uuid const uuid_namespace_x500 = VTE_DEFINE_UUID(6ba7b814, 9dad, 11d1, 80b4, 00c04fd430c8);

static void
test_uuid_equal(void)
{
        auto const nil_uuid = uuid();
        g_assert_true(nil == nil_uuid);
        g_assert_true(nil_uuid.is_nil());
        g_assert_true(nil_uuid == nil_uuid);
        g_assert_false(nil_uuid == dummy_uuid);

        auto const copy_uuid = dummy_uuid;
        g_assert_true(copy_uuid == dummy_uuid);
}

static void
test_uuid_string(void)
{
        g_assert_false(uuid_string_is_valid("6079c6d3-ffe3-42ac-a3cf"));
        g_assert_false(uuid_string_is_valid("zzzzzzzz-zzzz-zzzz-zzzz-zzzzzzzzzzzz"));

        g_assert_false(uuid_string_is_valid("{6079c6d3-ffe3-42ac-a3cf-7137b101b6ca"));
        g_assert_false(uuid_string_is_valid("(6079c6d3-ffe3-42ac-a3cf-7137b101b6ca}"));
        g_assert_false(uuid_string_is_valid(" 6079c6d3-ffe3-42ac-a3cf-7137b101b6ca "));
        g_assert_false(uuid_string_is_valid("urn:uuid:{6079c6d3-ffe3-42ac-a3cf-7137b101b6ca}"));

        g_assert_true(uuid_string_is_valid("00000000-0000-0000-0000-000000000000"));
        g_assert_true(uuid_string_is_valid("6079c6d3-ffe3-42ac-a3cf-7137b101b6ca"));
        g_assert_true(uuid_string_is_valid("{6079c6d3-ffe3-42ac-a3cf-7137b101b6ca}"));
        g_assert_true(uuid_string_is_valid("urn:uuid:6079c6d3-ffe3-42ac-a3cf-7137b101b6ca"));

        try {
                auto u = uuid("00000001-0002-1003-8004-000000000005");
                g_assert_true(dummy_uuid == u);
        } catch (...) {
                g_assert_not_reached();
        }

        try {
                auto u = uuid("00000000-0000-0000-0000-000000000000");
                g_assert_true(nil == u);
                g_assert_true(u.is_nil());
        } catch (...) {
                g_assert_not_reached();
        }
}

static void
test_uuid_random(void)
{
        auto str = uuid_string_random();
        g_assert_cmpint(str.size(), ==, 36);
        g_assert_true(uuid_string_is_valid(str));
}

static void
test_uuid_namespace(void)
{
        auto str = uuid_namespace_dns.str();
        g_assert_cmpstr(str.c_str(), ==, "6ba7b810-9dad-11d1-80b4-00c04fd430c8");

        str = uuid_namespace_url.str();
        g_assert_cmpstr(str.c_str(), ==, "6ba7b811-9dad-11d1-80b4-00c04fd430c8");

        str = uuid_namespace_oid.str();
        g_assert_cmpstr(str.c_str(), ==, "6ba7b812-9dad-11d1-80b4-00c04fd430c8");

        str = uuid_namespace_x500.str();
        g_assert_cmpstr(str.c_str(), ==, "6ba7b814-9dad-11d1-80b4-00c04fd430c8");
}

static void
test_uuid_generate_v3(void)
{
        auto u = uuid(uuid_v3, uuid_namespace_dns, "gnome.org");
        g_assert_true(u == gnome_uuid3);
}

static void
test_uuid_generate_v4(void)
{
        auto a = uuid(uuid_v4);
        auto b = uuid(uuid_v4);

        g_assert_false(a.is_nil());
        g_assert_false(b.is_nil());
        g_assert_false(a == b);
}

static void
test_uuid_generate_v5(void)
{
        auto u = uuid(uuid_v5, uuid_namespace_dns,"gnome.org");
        g_assert_true(u == gnome_uuid5);
}

static void
test_uuid_to_string(void)
{
        auto str = uuid_namespace_x500.str(uuid::format::SIMPLE);
        g_assert_cmpstr(str.c_str(), ==, "6ba7b814-9dad-11d1-80b4-00c04fd430c8");

        str = uuid_namespace_x500.str(uuid::format::BRACED);
        g_assert_cmpstr(str.c_str(), ==, "{6ba7b814-9dad-11d1-80b4-00c04fd430c8}");

        str = uuid_namespace_x500.str(uuid::format::URN);
        g_assert_cmpstr(str.c_str(), ==, "urn:uuid:6ba7b814-9dad-11d1-80b4-00c04fd430c8");

}

int
main(int argc,
     char** argv)
{
        g_test_init(&argc, &argv, nullptr);

        /* uuid Tests */
        g_test_add_func("/vte/uuid/equal", test_uuid_equal);
        g_test_add_func("/vte/uuid/string", test_uuid_string);
        g_test_add_func("/vte/uuid/random", test_uuid_random);
        g_test_add_func("/vte/uuid/namespace", test_uuid_namespace);
        g_test_add_func("/vte/uuid/generate_v3", test_uuid_generate_v3);
        g_test_add_func("/vte/uuid/generate_v4", test_uuid_generate_v4);
        g_test_add_func("/vte/uuid/generate_v5", test_uuid_generate_v5);
        g_test_add_func("/vte/uuid/to-string", test_uuid_to_string);

        return g_test_run();
}
