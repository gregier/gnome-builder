/* ide-project-miner.h
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

#ifndef IDE_PROJECT_MINER_H
#define IDE_PROJECT_MINER_H

#include <gio/gio.h>

#include "ide-project-info.h"

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT_MINER            (ide_project_miner_get_type())
#define IDE_PROJECT_MINER_EXTENSION_POINT "org.gnome.builder.extensions.project-miner"

G_DECLARE_DERIVABLE_TYPE (IdeProjectMiner, ide_project_miner, IDE, PROJECT_MINER, GObject)

struct _IdeProjectMinerClass
{
  GObjectClass parent;

  void     (*discovered)  (IdeProjectMiner      *self,
                           IdeProjectInfo       *project_info);
  void     (*mine_async)  (IdeProjectMiner      *self,
                           GCancellable         *cancellable,
                           GAsyncReadyCallback   callback,
                           gpointer              user_data);
  gboolean (*mine_finish) (IdeProjectMiner      *self,
                           GAsyncResult         *result,
                           GError              **error);
};

void     ide_project_miner_emit_discovered (IdeProjectMiner      *self,
                                            IdeProjectInfo       *project_info);
void     ide_project_miner_mine_async      (IdeProjectMiner      *self,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
gboolean ide_project_miner_mine_finish     (IdeProjectMiner      *self,
                                            GAsyncResult         *result,
                                            GError              **error);

G_END_DECLS

#endif /* IDE_PROJECT_MINER_H */
