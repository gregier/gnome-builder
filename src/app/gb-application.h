/* gb-application.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_APPLICATION_H
#define GB_APPLICATION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_APPLICATION (gb_application_get_type())

G_DECLARE_FINAL_TYPE (GbApplication, gb_application, GB, APPLICATION, GtkApplication)

void gb_application_show_projects_window (GbApplication *self);

G_END_DECLS

#endif /* GB_APPLICATION_H */
