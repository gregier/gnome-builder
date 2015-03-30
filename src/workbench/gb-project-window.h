/* gb-project-window.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef GB_PROJECT_WINDOW_H
#define GB_PROJECT_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_PROJECT_WINDOW (gb_project_window_get_type())

G_DECLARE_FINAL_TYPE (GbProjectWindow, gb_project_window, GB, PROJECT_WINDOW, GtkApplicationWindow)

GbProjectWindow *gb_project_window_new (void);

G_END_DECLS

#endif /* GB_PROJECT_WINDOW_H */
