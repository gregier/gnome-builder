/* gb-project-window.c
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

#include "gb-project-window.h"
#include "gb-widget.h"

struct _GbProjectWindow
{
  GtkApplicationWindow parent_instance;

  GtkListBox *listbox;
};

G_DEFINE_TYPE (GbProjectWindow, gb_project_window, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbProjectWindow *
gb_project_window_new (void)
{
  return g_object_new (GB_TYPE_PROJECT_WINDOW, NULL);
}

static void
gb_project_window_finalize (GObject *object)
{
  GbProjectWindow *self = (GbProjectWindow *)object;

  G_OBJECT_CLASS (gb_project_window_parent_class)->finalize (object);
}

static void
gb_project_window_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbProjectWindow *self = GB_PROJECT_WINDOW (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_project_window_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbProjectWindow *self = GB_PROJECT_WINDOW (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_project_window_class_init (GbProjectWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_project_window_finalize;
  object_class->get_property = gb_project_window_get_property;
  object_class->set_property = gb_project_window_set_property;

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-project-window.ui");

  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, listbox);
}

static void
gb_project_window_init (GbProjectWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
