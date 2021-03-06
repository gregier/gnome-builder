/* ide-workbench-private.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_WORKBENCH_PRIVATE_H
#define IDE_WORKBENCH_PRIVATE_H

#include <libpeas/peas.h>

#include "ide-perspective.h"
#include "ide-workbench.h"

G_BEGIN_DECLS

struct _IdeWorkbench
{
  GtkApplicationWindow       parent;

  guint                      unloading : 1;

  IdeContext                *context;
  GCancellable              *cancellable;
  PeasExtensionSet          *addins;

  IdePerspective            *perspective;

  GtkStack                  *top_stack;
  GtkStack                  *titlebar_stack;
  GtkStack                  *perspectives_stack;
  GtkStackSwitcher          *perspectives_stack_switcher;
  GtkPopover                *perspectives_popover;

  GtkSizeGroup              *header_size_group;

  GObject                   *selection_owner;
};

void     ide_workbench_set_context         (IdeWorkbench *workbench,
                                            IdeContext   *context);
void     ide_workbench_actions_init        (IdeWorkbench *self);
void     ide_workbench_set_selection_owner (IdeWorkbench *self,
                                            GObject      *object);
GObject *ide_workbench_get_selection_owner (IdeWorkbench *self);

G_END_DECLS

#endif /* IDE_WORKBENCH_PRIVATE_H */
