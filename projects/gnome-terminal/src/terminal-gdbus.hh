/*
 *  Copyright © 2011 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TERMINAL_RECEIVER_IMPL_H
#define TERMINAL_RECEIVER_IMPL_H

#include <glib-object.h>

#include "terminal-gdbus-generated.h"
#include "terminal-screen.hh"

G_BEGIN_DECLS

#define TERMINAL_TYPE_RECEIVER_IMPL         (terminal_receiver_impl_get_type ())
#define TERMINAL_RECEIVER_IMPL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_RECEIVER_IMPL, TerminalReceiverImpl))
#define TERMINAL_RECEIVER_IMPL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_RECEIVER_IMPL, TerminalReceiverImplClass))
#define TERMINAL_IS_RECEIVER_IMPL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_RECEIVER_IMPL))
#define TERMINAL_IS_RECEIVER_IMPL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_RECEIVER_IMPL))
#define TERMINAL_RECEIVER_IMPL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_RECEIVER_IMPL, TerminalReceiverImplClass))

typedef struct _TerminalReceiverImpl        TerminalReceiverImpl;
typedef struct _TerminalReceiverImplClass   TerminalReceiverImplClass;
typedef struct _TerminalReceiverImplPrivate TerminalReceiverImplPrivate;

struct _TerminalReceiverImpl
{
  TerminalReceiverSkeleton parent_instance;

  /*< private >*/
  TerminalReceiverImplPrivate *priv;
};

struct _TerminalReceiverImplClass
{
  TerminalReceiverSkeletonClass parent_class;
};

GType terminal_receiver_impl_get_type (void);

TerminalReceiverImpl *terminal_receiver_impl_new (TerminalScreen *screen);

TerminalScreen *terminal_receiver_impl_get_screen (TerminalReceiverImpl *impl);

void terminal_receiver_impl_unset_screen (TerminalReceiverImpl *impl);

/* ------------------------------------------------------------------------- */

#define TERMINAL_TYPE_FACTORY_IMPL              (terminal_factory_impl_get_type ())
#define TERMINAL_FACTORY_IMPL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), TERMINAL_TYPE_FACTORY_IMPL, TerminalFactoryImpl))
#define TERMINAL_FACTORY_IMPL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_FACTORY_IMPL, TerminalFactoryImplClass))
#define TERMINAL_IS_FACTORY_IMPL(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), TERMINAL_TYPE_FACTORY_IMPL))
#define TERMINAL_IS_FACTORY_IMPL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_FACTORY_IMPL))
#define TERMINAL_FACTORY_IMPL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_FACTORY_IMPL, TerminalFactoryImplClass))

typedef struct _TerminalFactoryImpl        TerminalFactoryImpl;
typedef struct _TerminalFactoryImplPrivate TerminalFactoryImplPrivate;
typedef struct _TerminalFactoryImplClass   TerminalFactoryImplClass;

struct _TerminalFactoryImplClass {
  TerminalFactorySkeletonClass parent_class;
};

struct _TerminalFactoryImpl
{
  TerminalFactorySkeleton parent_instance;

  TerminalFactoryImplPrivate *priv;
};

GType terminal_factory_impl_get_type (void);

TerminalFactory *terminal_factory_impl_new (void);

G_END_DECLS

#endif /* !TERMINAL_RECEIVER_IMPL_H */
