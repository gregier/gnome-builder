/* gb-initial-setup-dialog.c
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

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-initial-setup-dialog.h"
#include "gb-scrolled-window.h"
#include "gb-widget.h"

struct _GbInitialSetupDialog
{
  GtkApplicationWindow  parent_instance;

  GbInitialSetupPage    page;

  GtkButton            *back_button;
  GtkHeaderBar         *header_bar;
  GtkWidget            *page_new_project;
  GtkWidget            *page_open_project;
  GtkWidget            *page_select_project;
  GtkListBoxRow        *row_open_project;
  GtkListBoxRow        *row_view_more;
  GtkListBoxRow        *row_recent_projects;
  GtkListBoxRow        *row_new_project;
  GtkListBox           *select_list_box;
  GtkStack             *stack;
};

G_DEFINE_TYPE (GbInitialSetupDialog, gb_initial_setup_dialog, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_PAGE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static gchar *
_g_date_time_format_for_display (GDateTime *datetime)
{
  GDateTime *now;
  GTimeSpan diff;
  gint years;

  g_return_val_if_fail (datetime != NULL, NULL);

  now = g_date_time_new_now_utc ();
  diff = g_date_time_difference (now, datetime) / G_USEC_PER_SEC;

  if (diff < 0)
    return g_strdup ("");
  else if (diff < (60 * 45))
    return g_strdup (_("Just now"));
  else if (diff < (60 * 90))
    return g_strdup (_("An hour ago"));
  else if (diff < (60 * 60 * 24 * 2))
    return g_strdup (_("Yesterday"));
  else if (diff < (60 * 60 * 24 * 7))
    return g_date_time_format (datetime, "%A");
  else if (diff < (60 * 60 * 24 * 365))
    return g_date_time_format (datetime, "%B");
  else if (diff < (60 * 60 * 24 * 365 * 1.5))
    return g_strdup (_("About a year ago"));

  years = MAX (2, diff / (60 * 60 * 24 * 365));

  return g_strdup_printf (_("About %u years ago"), years);
}

GbInitialSetupPage
gb_initial_setup_dialog_get_page (GbInitialSetupDialog *self)
{
  g_return_val_if_fail (GB_IS_INITIAL_SETUP_DIALOG (self), 0);

  return self->page;
}

void
gb_initial_setup_dialog_set_page (GbInitialSetupDialog *self,
                                  GbInitialSetupPage    page)
{
  g_return_if_fail (GB_IS_INITIAL_SETUP_DIALOG (self));
  g_return_if_fail (page >= GB_INITIAL_SETUP_PAGE_SELECT_PROJECT);
  g_return_if_fail (page <= GB_INITIAL_SETUP_PAGE_NEW_PROJECT);

  if (page != self->page)
    {
      GtkWidget *child = NULL;
      gboolean back_button_visible = FALSE;
      const gchar *label;

      self->page = page;

      switch (page)
        {
        case GB_INITIAL_SETUP_PAGE_SELECT_PROJECT:
          child = self->page_select_project;
          label = _("Select Project");
          break;

        case GB_INITIAL_SETUP_PAGE_NEW_PROJECT:
          child = self->page_new_project;
          label = _("New Project");
          back_button_visible = TRUE;
          break;

        case GB_INITIAL_SETUP_PAGE_OPEN_PROJECT:
          child = self->page_open_project;
          label = _("Open Project");
          back_button_visible = TRUE;
          break;

        default:
          g_assert_not_reached ();
          break;
        }

      g_print ("Changing page to %s\n", label);

      gtk_stack_set_visible_child (self->stack, child);
      gtk_header_bar_set_title (self->header_bar, label);
      gtk_widget_set_visible (GTK_WIDGET (self->back_button), back_button_visible);

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_PAGE]);
    }
}

static void
gb_initial_setup_dialog__miner_discovered_cb (GbInitialSetupDialog *self,
                                              IdeProjectInfo       *project_info,
                                              IdeProjectMiner      *miner)
{
  g_autofree gchar *display_date = NULL;
  const gchar *project_name;
  GDateTime *last_modified_at;
  GtkWidget *box;
  GtkWidget *row;
  GtkWidget *label;
  GtkWidget *date_label;

  g_assert (GB_IS_INITIAL_SETUP_DIALOG (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (IDE_IS_PROJECT_MINER (miner));

  project_name = ide_project_info_get_name (project_info);

  last_modified_at = ide_project_info_get_last_modified_at (project_info);
  if (last_modified_at)
    display_date = _g_date_time_format_for_display (last_modified_at);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  g_object_set_data_full (G_OBJECT (row), "IDE_PROJECT_INFO",
                          g_object_ref (project_info), g_object_unref);
  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "visible", TRUE,
                      NULL);
  label = g_object_new (GTK_TYPE_LABEL,
                        "hexpand", TRUE,
                        "label", project_name,
                        "margin", 12,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);
  date_label = g_object_new (GTK_TYPE_LABEL,
                             "label", display_date,
                             "margin", 12,
                             "visible", TRUE,
                             "xalign", 1.0f,
                             NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (date_label), "dim-label");
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_container_add (GTK_CONTAINER (box), date_label);
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_container_add (GTK_CONTAINER (self->select_list_box), row);
}

static void
gb_initial_setup_dialog__miner_mine_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  g_autoptr(GbInitialSetupDialog) self = user_data;
  IdeProjectMiner *miner = (IdeProjectMiner *)object;
  g_autoptr(GError) error = NULL;

  g_assert (GB_IS_INITIAL_SETUP_DIALOG (self));
  g_assert (IDE_IS_PROJECT_MINER (miner));

  if (!ide_project_miner_mine_finish (miner, result, &error))
    g_warning ("%s", error->message);
}

static void
gb_initial_setup_dialog_show_more (GbInitialSetupDialog *self)
{
  g_autoptr(IdeProjectMiner) miner = NULL;

  g_assert (GB_INITIAL_SETUP_DIALOG (self));

  gtk_widget_set_visible (GTK_WIDGET (self->row_view_more), FALSE);

  miner = g_object_new (IDE_TYPE_AUTOTOOLS_PROJECT_MINER, NULL);
  g_signal_connect_object (miner,
                           "discovered",
                           G_CALLBACK (gb_initial_setup_dialog__miner_discovered_cb),
                           self,
                           G_CONNECT_SWAPPED);
  ide_project_miner_mine_async (miner,
                                NULL,
                                gb_initial_setup_dialog__miner_mine_cb,
                                g_object_ref (self));
}

static gint
gb_initial_setup_dialog__select_list_box_sort_func (GtkListBoxRow *row1,
                                                    GtkListBoxRow *row2,
                                                    gpointer       user_data)
{
  IdeProjectInfo *info1;
  IdeProjectInfo *info2;
  GDateTime *dt1;
  GDateTime *dt2;
  GTimeSpan diff;

  g_assert (GTK_IS_LIST_BOX_ROW (row1));
  g_assert (GTK_IS_LIST_BOX_ROW (row2));

  info1 = g_object_get_data (G_OBJECT (row1), "IDE_PROJECT_INFO");
  info2 = g_object_get_data (G_OBJECT (row2), "IDE_PROJECT_INFO");

  if (info1 == NULL)
    return -1;
  else if (info2 == NULL)
    return 1;

  dt1 = ide_project_info_get_last_modified_at (info1);
  dt2 = ide_project_info_get_last_modified_at (info2);

  if (dt1 == NULL)
    return 1;
  else if (dt2 == NULL)
    return -1;

  diff = g_date_time_difference (dt1, dt2);

  if (diff < 0)
    return 1;
  else if (diff > 0)
    return -1;

  return 0;
}

static void
gb_initial_setup_dialog__select_list_box_header_func (GtkListBoxRow *row,
                                                      GtkListBoxRow *before,
                                                      gpointer       user_data)
{
  GbInitialSetupDialog *self = user_data;

  g_assert (GB_IS_INITIAL_SETUP_DIALOG (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (!before || GTK_IS_LIST_BOX_ROW (before));

  if (before != NULL)
    {
      GtkWidget *header;

      header = g_object_new (GTK_TYPE_SEPARATOR,
                             "orientation", GTK_ORIENTATION_HORIZONTAL,
                             "visible", TRUE,
                             NULL);
      gtk_list_box_row_set_header (row, header);
    }
}

static void
gb_initial_setup_dialog__select_list_box_row_activated (GbInitialSetupDialog *self,
                                                        GtkListBoxRow        *row,
                                                        GtkListBox           *list_box)
{
  g_assert (GB_IS_INITIAL_SETUP_DIALOG (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if (row == self->row_new_project)
    gb_initial_setup_dialog_set_page (self, GB_INITIAL_SETUP_PAGE_NEW_PROJECT);
  else if (row == self->row_open_project)
    gb_initial_setup_dialog_set_page (self, GB_INITIAL_SETUP_PAGE_OPEN_PROJECT);
  else if (row == self->row_view_more)
    gb_initial_setup_dialog_show_more (self);
}

static void
gb_initial_setup_dialog__back_button_clicked (GbInitialSetupDialog *self,
                                              GtkButton            *back_button)
{
  g_assert (GB_IS_INITIAL_SETUP_DIALOG (self));
  g_assert (GTK_IS_BUTTON (back_button));

  gb_initial_setup_dialog_set_page (self, GB_INITIAL_SETUP_PAGE_SELECT_PROJECT);
}

static void
gb_initial_setup_dialog_finalize (GObject *object)
{
  //GbInitialSetupDialog *self = (GbInitialSetupDialog *)object;

  G_OBJECT_CLASS (gb_initial_setup_dialog_parent_class)->finalize (object);
}

static void
gb_initial_setup_dialog_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GbInitialSetupDialog *self = GB_INITIAL_SETUP_DIALOG (object);

  switch (prop_id)
    {
    case PROP_PAGE:
      g_value_set_enum (value, gb_initial_setup_dialog_get_page (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_initial_setup_dialog_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GbInitialSetupDialog *self = GB_INITIAL_SETUP_DIALOG (object);

  switch (prop_id)
    {
    case PROP_PAGE:
      gb_initial_setup_dialog_set_page (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_initial_setup_dialog_class_init (GbInitialSetupDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_initial_setup_dialog_finalize;
  object_class->get_property = gb_initial_setup_dialog_get_property;
  object_class->set_property = gb_initial_setup_dialog_set_property;

  gParamSpecs [PROP_PAGE] =
    g_param_spec_enum ("page",
                       _("Page"),
                       _("The current setup page."),
                       GB_TYPE_INITIAL_SETUP_PAGE,
                       GB_INITIAL_SETUP_PAGE_SELECT_PROJECT,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PAGE, gParamSpecs [PROP_PAGE]);

  GB_WIDGET_CLASS_TEMPLATE (widget_class, "gb-initial-setup-dialog.ui");

  GB_WIDGET_CLASS_BIND (widget_class, GbInitialSetupDialog, back_button);
  GB_WIDGET_CLASS_BIND (widget_class, GbInitialSetupDialog, header_bar);
  GB_WIDGET_CLASS_BIND (widget_class, GbInitialSetupDialog, page_new_project);
  GB_WIDGET_CLASS_BIND (widget_class, GbInitialSetupDialog, page_open_project);
  GB_WIDGET_CLASS_BIND (widget_class, GbInitialSetupDialog, page_select_project);
  GB_WIDGET_CLASS_BIND (widget_class, GbInitialSetupDialog, row_open_project);
  GB_WIDGET_CLASS_BIND (widget_class, GbInitialSetupDialog, row_view_more);
  GB_WIDGET_CLASS_BIND (widget_class, GbInitialSetupDialog, row_recent_projects);
  GB_WIDGET_CLASS_BIND (widget_class, GbInitialSetupDialog, row_new_project);
  GB_WIDGET_CLASS_BIND (widget_class, GbInitialSetupDialog, select_list_box);
  GB_WIDGET_CLASS_BIND (widget_class, GbInitialSetupDialog, stack);

  g_type_ensure (GB_TYPE_SCROLLED_WINDOW);
}

static void
gb_initial_setup_dialog_init (GbInitialSetupDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->select_list_box,
                                gb_initial_setup_dialog__select_list_box_header_func,
                                self,
                                NULL);

  gtk_list_box_set_sort_func (self->select_list_box,
                              gb_initial_setup_dialog__select_list_box_sort_func,
                              self,
                              NULL);

  g_signal_connect_object (self->back_button,
                           "clicked",
                           G_CALLBACK (gb_initial_setup_dialog__back_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->select_list_box,
                           "row-activated",
                           G_CALLBACK (gb_initial_setup_dialog__select_list_box_row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  gb_initial_setup_dialog_set_page (self, GB_INITIAL_SETUP_PAGE_SELECT_PROJECT);
}

GType
gb_initial_setup_page_get_type (void)
{
  static gsize type_id;

  if (g_once_init_enter (&type_id))
    {
      static const GEnumValue values[] = {
        { GB_INITIAL_SETUP_PAGE_SELECT_PROJECT,
          "GB_INITIAL_SETUP_PAGE_SELECT_PROJECT",
          "select-project" },
        { GB_INITIAL_SETUP_PAGE_OPEN_PROJECT,
          "GB_INITIAL_SETUP_PAGE_OPEN_PROJECT",
          "open-project" },
        { GB_INITIAL_SETUP_PAGE_NEW_PROJECT,
          "GB_INITIAL_SETUP_PAGE_NEW_PROJECT",
          "new-project" },
        { 0 }
      };
      gsize _type_id;

      _type_id = g_enum_register_static ("GbInitialSetupPage", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
