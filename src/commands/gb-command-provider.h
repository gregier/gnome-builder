/* gb-command-provider.h
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

#ifndef GB_COMMAND_PROVIDER_H
#define GB_COMMAND_PROVIDER_H

#include <gio/gio.h>

#include "gb-command.h"
#include "gb-view.h"
#include "gb-workbench-types.h"

G_BEGIN_DECLS

#define GB_TYPE_COMMAND_PROVIDER            (gb_command_provider_get_type())
#define GB_COMMAND_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_COMMAND_PROVIDER, GbCommandProvider))
#define GB_COMMAND_PROVIDER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_COMMAND_PROVIDER, GbCommandProvider const))
#define GB_COMMAND_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_COMMAND_PROVIDER, GbCommandProviderClass))
#define GB_IS_COMMAND_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_COMMAND_PROVIDER))
#define GB_IS_COMMAND_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_COMMAND_PROVIDER))
#define GB_COMMAND_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_COMMAND_PROVIDER, GbCommandProviderClass))

typedef struct _GbCommandProvider        GbCommandProvider;
typedef struct _GbCommandProviderClass   GbCommandProviderClass;
typedef struct _GbCommandProviderPrivate GbCommandProviderPrivate;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GbCommandProvider, g_object_unref)

struct _GbCommandProvider
{
  GObject parent;

  /*< private >*/
  GbCommandProviderPrivate *priv;
};

struct _GbCommandProviderClass
{
  GObjectClass parent;

  GbCommand *(*lookup) (GbCommandProvider *provider,
                        const gchar       *command_text);
  void       (*complete) (GbCommandProvider *provider,
                          GPtrArray         *completions,
                          const gchar       *command_text);
};

GType              gb_command_provider_get_type        (void);
GbCommandProvider *gb_command_provider_new             (GbWorkbench       *workbench);
GbWorkbench       *gb_command_provider_get_workbench   (GbCommandProvider *provider);
GbView            *gb_command_provider_get_active_view (GbCommandProvider *provider);
gint               gb_command_provider_get_priority    (GbCommandProvider *provider);
void               gb_command_provider_set_priority    (GbCommandProvider *provider,
                                                        gint               priority);
GbCommand         *gb_command_provider_lookup          (GbCommandProvider *provider,
                                                        const gchar       *command_text);
void               gb_command_provider_complete        (GbCommandProvider *provider,
                                                        GPtrArray         *completions,
                                                        const gchar       *initial_command_text);

G_END_DECLS

#endif /* GB_COMMAND_PROVIDER_H */
