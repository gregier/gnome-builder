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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#define G_LOG_DOMAIN "gb-project-window"

#include <glib/gi18n.h>

#include "gb-editor-document.h"
#include "gb-project-window.h"
#include "gb-widget.h"
#include "gb-workbench.h"

struct _GbProjectWindow
{
  GtkApplicationWindow parent_instance;

  GtkListBox      *listbox;
  GtkSearchBar    *search_bar;
  GtkToggleButton *search_button;
  GtkToggleButton *select_button;
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
get_default_size (GtkRequisition *req)
{
  GdkScreen *screen;
  GdkRectangle rect;
  gint primary;

  screen = gdk_screen_get_default ();
  primary = gdk_screen_get_primary_monitor (screen);
 gdk_screen_get_monitor_geometry (screen, primary, &rect);

  req->width = rect.width * 0.75;
  req->height = rect.height * 0.75;
}

static IdeBuffer *
gb_project_window__buffer_manager_create_buffer_cb (IdeBufferManager *buffer_manager,
                                                    IdeFile          *file,
                                                    IdeContext       *context)
{
  return g_object_new (GB_TYPE_EDITOR_DOCUMENT,
                       "context", context,
                       "file", file,
                       "highlight-diagnostics", TRUE,
                       NULL);
}

static void
gb_project_window__context_new_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  g_autoptr(GbProjectWindow) self = user_data;
  g_autoptr(IdeContext) context = NULL;
  GbWorkbench *workbench;
  IdeBufferManager *bufmgr;
  GtkRequisition req;
  GError *error = NULL;

  g_assert (GB_IS_PROJECT_WINDOW (self));

  context = ide_context_new_finish (result, &error);

  if (context == NULL)
    {
      /* TODO: error dialog */
      g_warning ("%s", error->message);
      g_clear_error (&error);
      return;
    }

  bufmgr = ide_context_get_buffer_manager (context);
  g_signal_connect (bufmgr,
                    "create-buffer",
                    G_CALLBACK (gb_project_window__buffer_manager_create_buffer_cb),
                    context);

  get_default_size (&req);

  workbench = g_object_new (GB_TYPE_WORKBENCH,
                            "application", g_application_get_default (),
                            "context", context,
                            "default-width", req.width,
                            "default-height", req.height,
                            "title", _("Builder"),
                            NULL);
  gtk_window_maximize (GTK_WINDOW (workbench));
  gtk_window_present (GTK_WINDOW (workbench));

  gtk_window_close (GTK_WINDOW (self));
}

static void
gb_project_window__listbox_row_activated_cb (GbProjectWindow *self,
                                             GtkListBoxRow   *row,
                                             GtkListBox      *listbox)
{
  IdeProjectInfo *project_info;
  GFile *directory;
  GFile *file;

  g_assert (GB_IS_PROJECT_WINDOW (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (listbox));

  project_info = g_object_get_data (G_OBJECT (row), "IDE_PROJECT_INFO");
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  directory = ide_project_info_get_directory (project_info);
  file = ide_project_info_get_file (project_info);

  if (file != NULL)
    directory = file;

  ide_context_new_async (directory,
                         NULL,
                         gb_project_window__context_new_cb,
                         g_object_ref (self));
}

static GtkWidget *
create_row (GbProjectWindow *self,
            IdeProjectInfo  *project_info)
{
  g_autofree gchar *markup = NULL;
  const gchar *name;
  GtkListBoxRow *row;
  GtkBox *box;
  GtkImage *image;
  GtkArrow *arrow;
  GtkLabel *label;
  GtkCheckButton *check;
  GtkRevealer *revealer;

  g_assert (GB_IS_PROJECT_WINDOW (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  name = ide_project_info_get_name (project_info);
  markup = g_strdup_printf ("<b>%s</b>", name);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  g_object_set_data_full (G_OBJECT (row),
                          "IDE_PROJECT_INFO",
                          g_object_ref (project_info),
                          g_object_unref);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "visible", TRUE,
                      "margin", 12,
                      NULL);

  check = g_object_new (GTK_TYPE_CHECK_BUTTON,
                        "visible", TRUE,
                        NULL);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "folder",
                        "pixel-size", 64,
                        "margin-end", 12,
                        "margin-start", 12,
                        "visible", TRUE,
                        NULL);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", markup,
                        "hexpand", TRUE,
                        "use-markup", TRUE,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);

  revealer = g_object_new (GTK_TYPE_REVEALER,
                           "reveal-child", FALSE,
                           "transition-type", GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT,
                           "visible", TRUE,
                           NULL);
  g_object_bind_property (self->select_button, "active",
                          revealer, "reveal-child",
                          G_BINDING_SYNC_CREATE);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  arrow = g_object_new (GTK_TYPE_ARROW,
                        "arrow-type", GTK_ARROW_RIGHT,
                        "visible", TRUE,
                        NULL);
  G_GNUC_END_IGNORE_DEPRECATIONS

  gtk_container_add (GTK_CONTAINER (revealer), GTK_WIDGET (check));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (revealer));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (image));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (arrow));
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (box));

  return GTK_WIDGET (row);
}

static void
gb_project_window__miner_discovered_cb (GbProjectWindow *self,
                                        IdeProjectInfo  *project_info,
                                        IdeProjectMiner *miner)
{
  GtkWidget *row;

  g_assert (GB_IS_PROJECT_WINDOW (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (IDE_IS_PROJECT_MINER (miner));

  row = create_row (self, project_info);

  gtk_container_add (GTK_CONTAINER (self->listbox), row);
}

static void
gb_project_window__miner_mine_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr(GbProjectWindow) self = user_data;
  IdeProjectMiner *miner = (IdeProjectMiner *)object;
  GError *error = NULL;

  g_assert (GB_IS_PROJECT_WINDOW (self));

  if (!ide_project_miner_mine_finish (miner, result, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
}

static void
gb_project_window__listbox_header_cb (GtkListBoxRow *row,
                                      GtkListBoxRow *before,
                                      gpointer       user_data)
{
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (!before || GTK_IS_LIST_BOX_ROW (before));

  if (before != NULL)
    gtk_list_box_row_set_header (row,
                                 g_object_new (GTK_TYPE_SEPARATOR,
                                               "orientation", GTK_ORIENTATION_HORIZONTAL,
                                               "visible", TRUE,
                                               NULL));
}

static gint
gb_project_window__listbox_sort (GtkListBoxRow *row1,
                                 GtkListBoxRow *row2,
                                 gpointer       user_data)
{
  IdeProjectInfo *info1;
  IdeProjectInfo *info2;
  const gchar *name1;
  const gchar *name2;

  g_assert (GTK_IS_LIST_BOX_ROW (row1));
  g_assert (GTK_IS_LIST_BOX_ROW (row2));

  info1 = g_object_get_data (G_OBJECT (row1), "IDE_PROJECT_INFO");
  info2 = g_object_get_data (G_OBJECT (row2), "IDE_PROJECT_INFO");

  g_assert (IDE_IS_PROJECT_INFO (info1));
  g_assert (IDE_IS_PROJECT_INFO (info2));

  name1 = ide_project_info_get_name (info1);
  name2 = ide_project_info_get_name (info2);

  if (name1 == NULL)
    return 1;
  else if (name2 == NULL)
    return -1;
  else
    return strcasecmp (name1, name2);
}

static void
gb_project_window_constructed (GObject *object)
{
  GbProjectWindow *self = (GbProjectWindow *)object;
  g_autoptr(IdeProjectMiner) miner = NULL;

  miner = g_object_new (IDE_TYPE_AUTOTOOLS_PROJECT_MINER,
                        "root-directory", NULL,
                        NULL);

  g_signal_connect_object (miner,
                           "discovered",
                           G_CALLBACK (gb_project_window__miner_discovered_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_bind_property (self->search_button, "active",
                          self->search_bar, "search-mode-enabled",
                          G_BINDING_SYNC_CREATE);

  ide_project_miner_mine_async (miner,
                                NULL,
                                gb_project_window__miner_mine_cb,
                                g_object_ref (self));

  gtk_list_box_set_header_func (self->listbox,
                                gb_project_window__listbox_header_cb,
                                NULL, NULL);
  gtk_list_box_set_sort_func (self->listbox,
                              gb_project_window__listbox_sort,
                              NULL, NULL);

  G_OBJECT_CLASS (gb_project_window_parent_class)->constructed (object);
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

  object_class->constructed = gb_project_window_constructed;
  object_class->finalize = gb_project_window_finalize;
  object_class->get_property = gb_project_window_get_property;
  object_class->set_property = gb_project_window_set_property;

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-project-window.ui");

  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, listbox);
  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, search_bar);
  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, search_button);
  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, select_button);
}

static void
gb_project_window_init (GbProjectWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->listbox,
                           "row-activated",
                           G_CALLBACK (gb_project_window__listbox_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
}
