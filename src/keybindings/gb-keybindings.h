/* gb-keybindings.h
 *
 * Copyright (C) 2014-2015 Christian Hergert <christian@hergert.me>
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

#ifndef GB_KEYBINDINGS_H
#define GB_KEYBINDINGS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_KEYBINDINGS (gb_keybindings_get_type())

G_DECLARE_FINAL_TYPE (GbKeybindings, gb_keybindings, GB, KEYBINDINGS, GObject)

GbKeybindings  *gb_keybindings_new             (GtkApplication *application,
                                                const gchar    *mode);
GtkApplication *gb_keybindings_get_application (GbKeybindings *self);
const gchar    *gb_keybindings_get_mode        (GbKeybindings  *self);
void            gb_keybindings_set_mode        (GbKeybindings  *self,
                                                const gchar    *name);

G_END_DECLS

#endif /* GB_KEYBINDINGS_H */
