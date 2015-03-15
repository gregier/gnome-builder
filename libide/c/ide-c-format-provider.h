/* ide-c-format-provider.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_C_FORMAT_PROVIDER_H
#define IDE_C_FORMAT_PROVIDER_H

#include <gtksourceview/gtksourcecompletionprovider.h>

G_BEGIN_DECLS

#define IDE_TYPE_C_FORMAT_PROVIDER (ide_c_format_provider_get_type())

G_DECLARE_FINAL_TYPE (IdeCFormatProvider, ide_c_format_provider, IDE, C_FORMAT_PROVIDER, GObject)

G_END_DECLS

#endif /* IDE_C_FORMAT_PROVIDER_H */
