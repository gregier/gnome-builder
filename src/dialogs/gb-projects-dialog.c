/* gb-projects-dialog.c
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

#define G_LOG_DOMAIN "gb-projects-dialog"

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-application.h"
#include "gb-editor-document.h"
#include "gb-glib.h"
#include "gb-new-project-dialog.h"
#include "gb-projects-dialog.h"
#include "gb-recent-project-row.h"
#include "gb-scrolled-window.h"
#include "gb-string.h"
#include "gb-widget.h"
#include "gb-workbench.h"

struct _GbProjectsDialog
{
  GtkApplicationWindow parent_instance;

  IdeRecentProjects *recent_projects;
  IdePatternSpec    *search_pattern;

  GtkActionBar      *action_bar;
  GtkButton         *cancel_button;
  GtkButton         *delete_button;
  GtkHeaderBar      *header_bar;
  GtkListBox        *listbox;
  GtkButton         *new_button;
  GtkSearchBar      *search_bar;
  GtkToggleButton   *search_button;
  GtkSearchEntry    *search_entry;
  GtkToggleButton   *select_button;
};

G_DEFINE_TYPE (GbProjectsDialog, gb_projects_dialog, GTK_TYPE_APPLICATION_WINDOW)

static void
gb_projects_dialog__listbox_row_activated_cb (GbProjectsDialog *self,
                                              GtkListBoxRow    *row,
                                              GtkListBox       *listbox)
{
  IdeProjectInfo *project_info;
  GApplication *app;
  GFile *file;

  g_assert (GB_IS_PROJECTS_DIALOG (self));
  g_assert (GB_IS_RECENT_PROJECT_ROW (row));
  g_assert (GTK_IS_LIST_BOX (listbox));

  /*
   * If we are in selection mode, just select the row instead.
   */
  if (gtk_toggle_button_get_active (self->select_button))
    {
      gboolean selected;
      GList *children;
      GList *iter;

      selected = !gb_recent_project_row_get_selected (GB_RECENT_PROJECT_ROW (row));
      gb_recent_project_row_set_selected (GB_RECENT_PROJECT_ROW (row), selected);

      children = gtk_container_get_children (GTK_CONTAINER (listbox));
      for (iter = children; !selected && iter; iter = iter->next)
        {
          if (gb_recent_project_row_get_selected (iter->data))
            selected = TRUE;
        }
      g_list_free (children);

      gtk_widget_set_sensitive (GTK_WIDGET (self->delete_button), selected);

      return;
    }

  project_info = gb_recent_project_row_get_project_info (GB_RECENT_PROJECT_ROW (row));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  app = g_application_get_default ();
  file = ide_project_info_get_file (project_info);

  gb_application_open_project (GB_APPLICATION (app), file, NULL);

  gtk_widget_destroy (GTK_WIDGET (self));
}

static void
gb_projects_dialog__recent_projects_added (GbProjectsDialog  *self,
                                           IdeProjectInfo    *project_info,
                                           IdeRecentProjects *recent_projects)
{
  GtkWidget *row;

  g_assert (GB_IS_PROJECTS_DIALOG (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (IDE_IS_RECENT_PROJECTS (recent_projects));

  row = g_object_new (GB_TYPE_RECENT_PROJECT_ROW,
                      "project-info", project_info,
                      "visible", TRUE,
                      NULL);
  g_object_bind_property (self->select_button, "active", row, "selection-mode",
                          G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (self->listbox), row);
}

static void
gb_projects_dialog__recent_projects_discover_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  IdeRecentProjects *recent_projects = (IdeRecentProjects *)object;
  g_autoptr(GbProjectsDialog) self = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_RECENT_PROJECTS (recent_projects));
  g_assert (GB_IS_PROJECTS_DIALOG (self));

  if (!ide_recent_projects_discover_finish (recent_projects, result, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
}

static void
gb_projects_dialog__listbox_header_cb (GtkListBoxRow *row,
                                       GtkListBoxRow *before,
                                       gpointer       user_data)
{
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (!before || GTK_IS_LIST_BOX_ROW (before));

  if (before != NULL)
    {
      GtkWidget *header;

      header =  g_object_new (GTK_TYPE_SEPARATOR,
                              "orientation", GTK_ORIENTATION_HORIZONTAL,
                              "visible", TRUE,
                              NULL);
      gtk_list_box_row_set_header (row, header);
    }
}

static gint
gb_projects_dialog__listbox_sort_cb (GtkListBoxRow *row1,
                                     GtkListBoxRow *row2,
                                     gpointer       user_data)
{
  IdeProjectInfo *info1;
  IdeProjectInfo *info2;
  const gchar *name1;
  const gchar *name2;
  GDateTime *dt1;
  GDateTime *dt2;
  gint ret;

  g_assert (GB_IS_RECENT_PROJECT_ROW (row1));
  g_assert (GB_IS_RECENT_PROJECT_ROW (row2));

  info1 = gb_recent_project_row_get_project_info (GB_RECENT_PROJECT_ROW (row1));
  info2 = gb_recent_project_row_get_project_info (GB_RECENT_PROJECT_ROW (row2));

  g_assert (IDE_IS_PROJECT_INFO (info1));
  g_assert (IDE_IS_PROJECT_INFO (info2));

  dt1 = ide_project_info_get_last_modified_at (info1);
  dt2 = ide_project_info_get_last_modified_at (info2);

  ret = g_date_time_compare (dt2, dt1);

  if (ret != 0)
    return ret;

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
gb_projects_dialog__listbox_filter_cb (GtkListBoxRow *row,
                                       gpointer       user_data)
{
  GbProjectsDialog *self = user_data;
  IdeProjectInfo *info;
  const gchar *name;

  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GB_IS_PROJECTS_DIALOG (self));

  info = gb_recent_project_row_get_project_info (GB_RECENT_PROJECT_ROW (row));
  g_assert (IDE_IS_PROJECT_INFO (info));

  if (self->search_pattern == NULL)
    return TRUE;

  name = ide_project_info_get_name (info);

  return ide_pattern_spec_match (self->search_pattern, name);
}

static void
gb_projects_dialog__select_button_notify_active (GbProjectsDialog *self,
                                                 GParamSpec       *pspec,
                                                 GtkToggleButton  *select_button)
{
  GtkStyleContext *style_context;
  gboolean active;

  g_assert (GB_IS_PROJECTS_DIALOG (self));
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
gb_projects_dialog__cancel_button_clicked (GbProjectsDialog *self,
                                           GtkButton        *cancel_button)
{
  GList *rows;
  GList *iter;

  g_assert (GB_IS_PROJECTS_DIALOG (self));
  g_assert (GTK_IS_BUTTON (cancel_button));

  /* reset back to normal mode */
  gtk_toggle_button_set_active (self->select_button, FALSE);

  /* uncheck rows */
  rows = gtk_container_get_children (GTK_CONTAINER (self->listbox));
  for (iter = rows; iter; iter = iter->next)
    gb_recent_project_row_set_selected (iter->data, FALSE);
  g_list_free (rows);
}

static void
gb_projects_dialog__search_entry_activate (GbProjectsDialog *self,
                                           GtkEntry         *entry)
{
  GtkListBoxRow *row;

  g_assert (GB_IS_PROJECTS_DIALOG (self));
  g_assert (GTK_IS_ENTRY (entry));

  /* FIXME: We use 1 because 0 doesn't work and we have no API to get
   *        the first row taking the sort/filter into account.
   */
  row = gtk_list_box_get_row_at_y (self->listbox, 1);
  if (row != NULL)
    g_signal_emit_by_name (row, "activate");
}

static void
gb_projects_dialog__search_entry_changed (GbProjectsDialog *self,
                                          GtkEntry         *entry)
{
  const gchar *text;

  g_assert (GB_IS_PROJECTS_DIALOG (self));
  g_assert (GTK_IS_ENTRY (entry));

  g_clear_pointer (&self->search_pattern, (GDestroyNotify)ide_pattern_spec_unref);

  text = gtk_entry_get_text (entry);
  if (!gb_str_empty0 (text))
    self->search_pattern = ide_pattern_spec_new (text);

  gtk_list_box_invalidate_filter (self->listbox);
}

static void
gb_projects_dialog__window_open_project (GbProjectsDialog   *self,
                                         GFile              *project_file,
                                         GbNewProjectDialog *dialog)
{
  GApplication *app = g_application_get_default ();

  g_assert (GB_IS_PROJECTS_DIALOG (self));
  g_assert (G_IS_FILE (project_file));
  g_assert (GB_IS_NEW_PROJECT_DIALOG (dialog));
  g_assert (GB_IS_APPLICATION (app));

  gb_application_open_project (GB_APPLICATION (app), project_file, NULL);
  gtk_widget_destroy (GTK_WIDGET (dialog));
  gtk_widget_destroy (GTK_WIDGET (self));
}

static void
gb_projects_dialog__new_button_clicked (GbProjectsDialog *self,
                                       GtkButton       *new_button)
{
  GtkWindow *window;

  g_assert (GB_IS_PROJECTS_DIALOG (self));
  g_assert (GTK_IS_BUTTON (new_button));

  window = g_object_new (GB_TYPE_NEW_PROJECT_DIALOG,
                         "destroy-with-parent", TRUE,
                         "transient-for", self,
                         "type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
                         "visible", TRUE,
                         NULL);

  g_signal_connect_object (window,
                           "open-project",
                           G_CALLBACK (gb_projects_dialog__window_open_project),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_window_present (window);
}

static void
gb_projects_dialog_constructed (GObject *object)
{
  GbProjectsDialog *self = (GbProjectsDialog *)object;

  G_OBJECT_CLASS (gb_projects_dialog_parent_class)->constructed (object);

  g_signal_connect_object (self->recent_projects,
                           "added",
                           G_CALLBACK (gb_projects_dialog__recent_projects_added),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_bind_property (self->search_button, "active",
                          self->search_bar, "search-mode-enabled",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_signal_connect_object (self->search_entry,
                           "activate",
                           G_CALLBACK (gb_projects_dialog__search_entry_activate),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->search_entry,
                           "changed",
                           G_CALLBACK (gb_projects_dialog__search_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->select_button,
                           "notify::active",
                           G_CALLBACK (gb_projects_dialog__select_button_notify_active),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->cancel_button,
                           "clicked",
                           G_CALLBACK (gb_projects_dialog__cancel_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->new_button,
                           "clicked",
                           G_CALLBACK (gb_projects_dialog__new_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_header_func (self->listbox,
                                gb_projects_dialog__listbox_header_cb,
                                NULL, NULL);

  gtk_list_box_set_sort_func (self->listbox,
                              gb_projects_dialog__listbox_sort_cb,
                              NULL, NULL);

  gtk_list_box_set_filter_func (self->listbox,
                                gb_projects_dialog__listbox_filter_cb,
                                self, NULL);

  ide_recent_projects_discover_async (self->recent_projects,
                                      NULL, /* TODO: cancellable */
                                      gb_projects_dialog__recent_projects_discover_cb,
                                      g_object_ref (self));
}

static gboolean
gb_projects_dialog_key_press_event (GtkWidget   *widget,
                                   GdkEventKey *event)
{
  GbProjectsDialog *self = (GbProjectsDialog *)widget;
  gboolean ret;

  ret = GTK_WIDGET_CLASS (gb_projects_dialog_parent_class)->key_press_event (widget, event);

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
gb_projects_dialog_finalize (GObject *object)
{
  GbProjectsDialog *self = (GbProjectsDialog *)object;

  g_clear_object (&self->recent_projects);
  g_clear_pointer (&self->search_pattern, (GDestroyNotify)ide_pattern_spec_unref);

  G_OBJECT_CLASS (gb_projects_dialog_parent_class)->finalize (object);
}

static void
gb_projects_dialog_class_init (GbProjectsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_projects_dialog_constructed;
  object_class->finalize = gb_projects_dialog_finalize;

  widget_class->key_press_event = gb_projects_dialog_key_press_event;

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-projects-dialog.ui");

  GB_WIDGET_CLASS_BIND (klass, GbProjectsDialog, action_bar);
  GB_WIDGET_CLASS_BIND (klass, GbProjectsDialog, cancel_button);
  GB_WIDGET_CLASS_BIND (klass, GbProjectsDialog, delete_button);
  GB_WIDGET_CLASS_BIND (klass, GbProjectsDialog, header_bar);
  GB_WIDGET_CLASS_BIND (klass, GbProjectsDialog, new_button);
  GB_WIDGET_CLASS_BIND (klass, GbProjectsDialog, listbox);
  GB_WIDGET_CLASS_BIND (klass, GbProjectsDialog, search_bar);
  GB_WIDGET_CLASS_BIND (klass, GbProjectsDialog, search_button);
  GB_WIDGET_CLASS_BIND (klass, GbProjectsDialog, search_entry);
  GB_WIDGET_CLASS_BIND (klass, GbProjectsDialog, select_button);

  g_type_ensure (GB_TYPE_SCROLLED_WINDOW);
}

static void
gb_projects_dialog_init (GbProjectsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->listbox,
                           "row-activated",
                           G_CALLBACK (gb_projects_dialog__listbox_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->recent_projects = ide_recent_projects_new ();
}
