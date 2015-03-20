/* gb-vim.c
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

#ifndef GB_VIM_H
#define GB_VIM_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GB_VIM_ERROR (gb_vim_error_quark())

typedef enum
{
  GB_VIM_ERROR_NOT_IMPLEMENTED,
  GB_VIM_ERROR_NOT_FOUND,
  GB_VIM_ERROR_NOT_NUMBER,
  GB_VIM_ERROR_NUMBER_OUT_OF_RANGE,
  GB_VIM_ERROR_CANNOT_FIND_COLORSCHEME,
  GB_VIM_ERROR_UNKNOWN_OPTION,
  GB_VIM_ERROR_NOT_SOURCE_VIEW,
} IdeVimError;

GQuark     gb_vim_error_quark (void);
gboolean   gb_vim_execute     (GtkSourceView  *source_view,
                               const gchar    *line,
                               GError        **error);
gchar    **gb_vim_complete    (const gchar    *line);

G_END_DECLS

#endif /* GB_VIM_H */
