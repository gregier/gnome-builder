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
gb_initial_setup_dialog_show_more (GbInitialSetupDialog *self)
{
  g_assert (GB_INITIAL_SETUP_DIALOG (self));
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

  gb_initial_setup_dialog_set_page (self, GB_INITIAL_SETUP_PAGE_SELECT_PROJECT);

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
