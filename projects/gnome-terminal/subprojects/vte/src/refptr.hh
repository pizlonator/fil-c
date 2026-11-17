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

#pragma once

#include <memory>
#include <glib-object.h>

#include "cxx-utils.hh"

namespace vte {

namespace glib {

template<typename T>
using RefPtr = vte::FreeablePtr<T, decltype(&g_object_unref), &g_object_unref>;

template<typename T>
RefPtr<T>
make_ref(T* obj)
{
        if (obj)
                g_object_ref(obj);
        return RefPtr<T>{obj};
}

template<typename T>
RefPtr<T>
make_ref_sink(T* obj)
{
        if (obj)
                g_object_ref_sink(obj);
        return RefPtr<T>{obj};
}

template<typename T>
RefPtr<T>
take_ref(T* obj)
{
        return RefPtr<T>{obj};
}

template<typename T>
RefPtr<T>
acquire_ref(GWeakRef* wr)
{
        return take_ref(reinterpret_cast<T*>(g_weak_ref_get(wr)));
}

template<typename T>
RefPtr<T>
ref(RefPtr<T> const& obj)
{
        return make_ref(obj.get());
}

} // namespace glib

namespace base {

template<class T>
class Unreffer {
public:
        void operator()(T* obj) const
        {
                if (obj)
                        obj->unref();
        }
};

template<class T>
using RefPtr = std::unique_ptr<T, Unreffer<T>>;

template<class T>
RefPtr<T>
make_ref(T* obj)
{
        if (obj)
                obj->ref();
        return RefPtr<T>{obj};
}

template<class T>
RefPtr<T>
take_ref(T* obj)
{
        return RefPtr<T>{obj};
}

} // namespace base

} // namespace vte
