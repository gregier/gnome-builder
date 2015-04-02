/* gb-initial-setup-dialog.h
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

#ifndef GB_INITIAL_SETUP_DIALOG_H
#define GB_INITIAL_SETUP_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_INITIAL_SETUP_DIALOG (gb_initial_setup_dialog_get_type())
#define GB_TYPE_INITIAL_SETUP_PAGE   (gb_initial_setup_page_get_type())

typedef enum
{
  GB_INITIAL_SETUP_PAGE_SELECT_PROJECT = 1,
  GB_INITIAL_SETUP_PAGE_OPEN_PROJECT,
  GB_INITIAL_SETUP_PAGE_NEW_PROJECT,
} GbInitialSetupPage;

G_DECLARE_FINAL_TYPE (GbInitialSetupDialog, gb_initial_setup_dialog,
                      GB, INITIAL_SETUP_DIALOG,
                      GtkApplicationWindow)

GType              gb_initial_setup_page_get_type   (void);
GbInitialSetupPage gb_initial_setup_dialog_get_page (GbInitialSetupDialog *self);
void               gb_initial_setup_dialog_set_page (GbInitialSetupDialog *self,
                                                     GbInitialSetupPage    page);

G_END_DECLS

#endif /* GB_INITIAL_SETUP_DIALOG_H */
