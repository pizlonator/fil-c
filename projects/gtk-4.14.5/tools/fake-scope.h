#pragma once

#include <gtk.h>

G_DECLARE_FINAL_TYPE (FakeScope, fake_scope, FAKE, SCOPE, GtkBuilderCScope)

FakeScope * fake_scope_new           (void);
GPtrArray * fake_scope_get_types     (FakeScope *self);
GPtrArray * fake_scope_get_callbacks (FakeScope *self);
