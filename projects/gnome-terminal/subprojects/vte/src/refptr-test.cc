/*
 * Copyright © 2018 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include "refptr.hh"

/* Test object */

typedef struct _TestObject      TestObject;
typedef struct _TestObjectClass TestObjectClass;

struct _TestObject {
        GObject parent_instance;
};

struct _TestObjectClass{
        GObjectClass parent_class;
};

static GType test_object_get_type(void);

G_DEFINE_TYPE(TestObject, test_object, G_TYPE_OBJECT)

static void
test_object_init(TestObject* object)
{
}

static void
test_object_finalize(GObject *object)
{
        G_OBJECT_CLASS(test_object_parent_class)->finalize(object);
}

static void
test_object_class_init(TestObjectClass* klass)
{
        GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
        gobject_class->finalize = test_object_finalize;
}

static TestObject*
test_object_new(void)
{
        return reinterpret_cast<TestObject*>(g_object_new(test_object_get_type(),
                                                          nullptr));
}

/* Tests */

typedef union {
        TestObject* obj;
        void* ptr;
} obj_t;

static void
test_glib_refptr(void)
{
        obj_t obj1;
        obj1.obj = test_object_new();
        g_object_add_weak_pointer(G_OBJECT(obj1.obj), &obj1.ptr);
        auto ptr1 = vte::glib::RefPtr<TestObject>{obj1.obj};
        g_assert_true(ptr1.get() == obj1.obj);

        auto ptr2 = std::move(ptr1);
        g_assert_true(ptr1.get() == nullptr);
        g_assert_true(ptr2.get() == obj1.obj);

        obj_t obj2;
        obj2.obj = test_object_new();
        g_object_add_weak_pointer(G_OBJECT(obj2.obj), &obj2.ptr);
        g_assert_nonnull(obj2.ptr);
        ptr2.reset(obj2.obj);
        g_assert_null(obj1.ptr);
        g_assert_true(ptr2.get() == obj2.obj);
        g_assert_nonnull(obj2.ptr);

        ptr2 = nullptr;
        g_assert_null(obj2.ptr);

        obj_t obj3;
        obj3.obj = test_object_new();
        g_object_add_weak_pointer(G_OBJECT(obj3.obj), &obj3.ptr);
        g_assert_nonnull(obj3.ptr);
        auto ptr3 = vte::glib::RefPtr<TestObject>{obj3.obj};
        TestObject* obj4 = ptr3.release();
        g_assert_null(ptr3.get());
        g_assert_nonnull(obj4);
        g_object_unref(obj4);
        g_assert_null(obj3.ptr);

        obj_t obj5;
        obj5.obj = test_object_new();
        g_object_add_weak_pointer(G_OBJECT(obj5.obj), &obj5.ptr);
        g_assert_nonnull(obj5.ptr);
        vte::glib::RefPtr<TestObject> ptr5{obj5.obj};

        obj_t obj6;
        obj6.obj = test_object_new();
        g_object_add_weak_pointer(G_OBJECT(obj6.obj), &obj6.ptr);
        g_assert_nonnull(obj6.ptr);

        ptr5.reset(obj6.obj);
        g_assert_null(obj5.ptr);

        ptr5.reset();
        g_assert_null(obj6.ptr);
        g_assert_null(ptr5.get());
}

int
main(int argc,
     char* argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/glib/refptr", test_glib_refptr);

        return g_test_run();
}
