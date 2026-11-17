/*
 * Copyright © 2024 Christian Hergert
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef void (*VteSchedulerCallback) (GtkWidget *widget,
                                      gpointer   user_data);

gpointer _vte_scheduler_add_callback    (GtkWidget            *widget,
                                         VteSchedulerCallback  callback,
                                         gpointer              user_data);
void     _vte_scheduler_remove_callback (GtkWidget            *widget,
                                         gpointer              handler);

G_END_DECLS
