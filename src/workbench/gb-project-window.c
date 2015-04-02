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
#include <ide.h>

#include "gb-editor-document.h"
#include "gb-project-window.h"
#include "gb-scrolled-window.h"
#include "gb-string.h"
#include "gb-widget.h"
#include "gb-workbench.h"

struct _GbProjectWindow
{
  GtkApplicationWindow parent_instance;

  GSettings       *settings;

  IdePatternSpec  *search_pattern;
  GList           *selected;

  GtkActionBar    *action_bar;
  GtkButton       *cancel_button;
  GtkButton       *delete_button;
  GtkHeaderBar    *header_bar;
  GtkListBox      *listbox;
  GtkButton       *new_button;
  GtkSearchBar    *search_bar;
  GtkToggleButton *search_button;
  GtkSearchEntry  *search_entry;
  GtkToggleButton *select_button;
};

G_DEFINE_TYPE (GbProjectWindow, gb_project_window, GTK_TYPE_APPLICATION_WINDOW)

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

static void
gb_project_window__check_toggled (GbProjectWindow *self,
                                  GtkCheckButton  *check_button)
{
  GtkWidget *row;

  g_assert (GB_IS_PROJECT_WINDOW (self));
  g_assert (GTK_IS_CHECK_BUTTON (check_button));

  for (row = GTK_WIDGET (check_button);
       (row != NULL) && !GTK_IS_LIST_BOX_ROW (row);
       row = gtk_widget_get_parent (row))
    {
      /* Do Nothing */
    }

  if (row == NULL)
    return;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_button)))
    self->selected = g_list_prepend (self->selected, row);
  else
    self->selected = g_list_remove (self->selected, row);

  gtk_widget_set_sensitive (GTK_WIDGET (self->delete_button), !!self->selected);
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

  /*
   * If we are in selection mode, just select the row instead.
   */
  if (gtk_toggle_button_get_active (self->select_button))
    {
      GtkToggleButton *check;

      check = g_object_get_data (G_OBJECT (row), "CHECK_BUTTON");

      if (check != NULL)
        {
          gboolean active;

          active = gtk_toggle_button_get_active (check);
          gtk_toggle_button_set_active (check, !active);
        }

      return;
    }

  project_info = g_object_get_data (G_OBJECT (row), "IDE_PROJECT_INFO");
  g_assert (!project_info || IDE_IS_PROJECT_INFO (project_info));

  if (project_info == NULL)
    {
      gtk_container_foreach (GTK_CONTAINER (listbox), (GtkCallback)gtk_widget_show, NULL);
      gtk_widget_hide (GTK_WIDGET (row));
      return;
    }

  directory = ide_project_info_get_directory (project_info);
  file = ide_project_info_get_file (project_info);

  if (file != NULL)
    directory = file;

  ide_context_new_async (directory,
                         NULL,
                         gb_project_window__context_new_cb,
                         g_object_ref (self));
}

static gboolean
is_recent_project (GbProjectWindow *self,
                   IdeProjectInfo  *info)
{
  gchar *uri;
  gboolean ret = FALSE;
  gchar **strv;
  GFile *file;
  gsize i;

  g_assert (GB_IS_PROJECT_WINDOW (self));
  g_assert (G_IS_SETTINGS (self->settings));
  g_assert (IDE_IS_PROJECT_INFO (info));

  file = ide_project_info_get_file (info);
  uri = g_file_get_uri (file);
  strv = g_settings_get_strv (self->settings, "project-history");

  for (i = 0; strv [i]; i++)
    {
      if (g_str_equal (strv [i], uri))
        {
          ret = TRUE;
          break;
        }
    }

  g_strfreev (strv);
  g_free (uri);

  return ret;
}

static GtkWidget *
create_row (GbProjectWindow *self,
            IdeProjectInfo  *project_info)
{
  g_autofree gchar *relative_path = NULL;
  const gchar *name;
  GtkListBoxRow *row;
  GtkBox *box;
  GtkBox *vbox;
  GtkImage *image;
  GtkArrow *arrow;
  GtkLabel *label;
  GtkLabel *label2;
  GtkCheckButton *check;
  GtkRevealer *revealer;
  GFile *directory;
  const gchar *icon_name = "folder";
  g_autoptr(GFile) home = NULL;

  g_assert (GB_IS_PROJECT_WINDOW (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  name = ide_project_info_get_name (project_info);
  directory = ide_project_info_get_directory (project_info);

  home = g_file_new_for_path (g_get_home_dir ());
  relative_path = g_file_get_relative_path (home, directory);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  g_object_set_data_full (G_OBJECT (row),
                          "IDE_PROJECT_INFO",
                          g_object_ref (project_info),
                          g_object_unref);

  box = g_object_new (GTK_TYPE_BOX,
                      "margin", 12,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "visible", TRUE,
                      NULL);

  check = g_object_new (GTK_TYPE_CHECK_BUTTON,
                        "vexpand", FALSE,
                        "valign", GTK_ALIGN_CENTER,
                        "visible", TRUE,
                        NULL);
  g_signal_connect_object (check,
                           "toggled",
                           G_CALLBACK (gb_project_window__check_toggled),
                           self,
                           G_CONNECT_SWAPPED);

  if (!g_file_is_native (directory))
    icon_name = "folder-remote";

  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", icon_name,
                        "pixel-size", 64,
                        "margin-end", 12,
                        "margin-start", 12,
                        "visible", TRUE,
                        NULL);

  vbox = g_object_new (GTK_TYPE_BOX,
                       "orientation", GTK_ORIENTATION_VERTICAL,
                       "valign", GTK_ALIGN_CENTER,
                       "vexpand", TRUE,
                       "visible", TRUE,
                       NULL);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", name,
                        "hexpand", TRUE,
                        "use-markup", TRUE,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (label)), "title");

  label2 = g_object_new (GTK_TYPE_LABEL,
                         "label", relative_path,
                         "hexpand", TRUE,
                         "use-markup", TRUE,
                         "visible", TRUE,
                         "xalign", 0.0f,
                         NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (label2)), "dim-label");

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
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (vbox));
  gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (label));
  gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (label2));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (arrow));
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (box));

  g_object_set_data (G_OBJECT (row), "CHECK_BUTTON", check);

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
#if 0
  if (!is_recent_project (self, project_info))
    gtk_widget_set_visible (row, FALSE);
#endif
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

  g_assert (!info1 || IDE_IS_PROJECT_INFO (info1));
  g_assert (!info2 || IDE_IS_PROJECT_INFO (info2));

  if (!info1)
    return 1;

  if (!info2)
    return -1;

  name1 = ide_project_info_get_name (info1);
  name2 = ide_project_info_get_name (info2);

  if (name1 == NULL)
    return 1;
  else if (name2 == NULL)
    return -1;
  else
    return strcasecmp (name1, name2);
}

static gboolean
gb_project_window__listbox_filter (GtkListBoxRow *row,
                                   gpointer       user_data)
{
  GbProjectWindow *self = user_data;
  IdeProjectInfo *info;
  const gchar *name;

  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GB_IS_PROJECT_WINDOW (self));

  info = g_object_get_data (G_OBJECT (row), "IDE_PROJECT_INFO");

  if (info == NULL || self->search_pattern == NULL)
    return TRUE;

  name = ide_project_info_get_name (info);

  return ide_pattern_spec_match (self->search_pattern, name);
}

static void
gb_project_window__select_button_notify_active (GbProjectWindow *self,
                                                GParamSpec      *pspec,
                                                GtkToggleButton *select_button)
{
  GtkStyleContext *style_context;
  gboolean active;

  g_assert (GB_IS_PROJECT_WINDOW (self));
  g_assert (GTK_IS_TOGGLE_BUTTON (select_button));

  active = gtk_toggle_button_get_active (select_button);
  style_context = gtk_widget_get_style_context (GTK_WIDGET (self->header_bar));

  if (active)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->action_bar), TRUE);
      gtk_widget_set_visible (GTK_WIDGET (self->new_button), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->select_button), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->cancel_button), TRUE);
      gtk_header_bar_set_show_close_button (self->header_bar, FALSE);
      gtk_header_bar_set_title (self->header_bar, _("(Click on items to select them)"));
      gtk_style_context_add_class (style_context, "selection-mode");
    }
  else
    {
      gtk_style_context_remove_class (style_context, "selection-mode");
      gtk_widget_set_visible (GTK_WIDGET (self->action_bar), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->new_button), TRUE);
      gtk_widget_set_visible (GTK_WIDGET (self->select_button), TRUE);
      gtk_widget_set_visible (GTK_WIDGET (self->cancel_button), FALSE);
      gtk_header_bar_set_show_close_button (self->header_bar, TRUE);
      gtk_header_bar_set_title (self->header_bar, _("Select Project"));
    }
}

static void
gb_project_window__cancel_button_clicked (GbProjectWindow *self,
                                          GtkButton       *cancel_button)
{
  GList *rows;
  GList *iter;

  g_assert (GB_IS_PROJECT_WINDOW (self));
  g_assert (GTK_IS_BUTTON (cancel_button));

  /* reset back to normal mode */
  gtk_toggle_button_set_active (self->select_button, FALSE);

  /* uncheck rows */
  rows = gtk_container_get_children (GTK_CONTAINER (self->listbox));
  for (iter = rows; iter; iter = iter->next)
    {
      GtkToggleButton *check;

      check = g_object_get_data (iter->data, "CHECK_BUTTON");

      if (check != NULL)
        gtk_toggle_button_set_active (check, FALSE);
    }
  g_list_free (rows);

  /* clear selection list */
  g_clear_pointer (&self->selected, (GDestroyNotify)g_list_free);
}

static void
gb_project_window__search_entry_changed (GbProjectWindow *self,
                                         GtkEntry        *entry)
{
  const gchar *text;

  g_assert (GB_IS_PROJECT_WINDOW (self));
  g_assert (GTK_IS_ENTRY (entry));

  g_clear_pointer (&self->search_pattern, (GDestroyNotify)ide_pattern_spec_unref);

  text = gtk_entry_get_text (entry);

  if (!gb_str_empty0 (text))
    self->search_pattern = ide_pattern_spec_new (text);

  gtk_list_box_invalidate_filter (self->listbox);
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
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_signal_connect_object (self->search_entry,
                           "changed",
                           G_CALLBACK (gb_project_window__search_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->select_button,
                           "notify::active",
                           G_CALLBACK (gb_project_window__select_button_notify_active),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->cancel_button,
                           "clicked",
                           G_CALLBACK (gb_project_window__cancel_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_header_func (self->listbox,
                                gb_project_window__listbox_header_cb,
                                NULL, NULL);

  gtk_list_box_set_sort_func (self->listbox,
                              gb_project_window__listbox_sort,
                              NULL, NULL);

  gtk_list_box_set_filter_func (self->listbox,
                                gb_project_window__listbox_filter,
                                self, NULL);

  ide_project_miner_mine_async (miner,
                                NULL,
                                gb_project_window__miner_mine_cb,
                                g_object_ref (self));

  G_OBJECT_CLASS (gb_project_window_parent_class)->constructed (object);
}

static gboolean
gb_project_window_key_press_event (GtkWidget   *widget,
                                   GdkEventKey *event)
{
  GbProjectWindow *self = (GbProjectWindow *)widget;
  gboolean ret;

  ret = GTK_WIDGET_CLASS (gb_project_window_parent_class)->key_press_event (widget, event);

  if (!ret)
    {
      switch (event->keyval)
        {
        case GDK_KEY_Escape:
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->select_button)))
            {
              gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->select_button), FALSE);
              ret = TRUE;
            }
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->search_button)))
            {
              gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->search_button), FALSE);
              ret = TRUE;
            }
          break;

        case GDK_KEY_f:
          if ((event->state & GDK_CONTROL_MASK) != 0)
            {
              gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->search_button), TRUE);
              ret = TRUE;
            }
          break;

        default:
          break;
        }
    }


  return ret;
}

static void
gb_project_window_finalize (GObject *object)
{
  GbProjectWindow *self = (GbProjectWindow *)object;

  g_clear_object (&self->settings);
  g_clear_pointer (&self->selected, (GDestroyNotify)g_list_free);
  g_clear_pointer (&self->search_pattern, (GDestroyNotify)ide_pattern_spec_unref);

  G_OBJECT_CLASS (gb_project_window_parent_class)->finalize (object);
}

static void
gb_project_window_class_init (GbProjectWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_project_window_constructed;
  object_class->finalize = gb_project_window_finalize;

  widget_class->key_press_event = gb_project_window_key_press_event;

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-project-window.ui");

  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, action_bar);
  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, cancel_button);
  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, delete_button);
  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, header_bar);
  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, new_button);
  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, listbox);
  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, search_bar);
  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, search_button);
  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, search_entry);
  GB_WIDGET_CLASS_BIND (klass, GbProjectWindow, select_button);

  g_type_ensure (GB_TYPE_SCROLLED_WINDOW);
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

  self->settings = g_settings_new ("org.gnome.builder");
}
