/* ide-language-defaults.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_LANGUAGE_DEFAULTS_H
#define IDE_LANGUAGE_DEFAULTS_H

#include <gio/gio.h>

G_BEGIN_DECLS

void     ide_language_defaults_init_async  (GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
gboolean ide_language_defaults_init_finish (GAsyncResult         *result,
                                            GError              **error);

G_END_DECLS

#endif /* IDE_LANGUAGE_DEFAULTS_H */
