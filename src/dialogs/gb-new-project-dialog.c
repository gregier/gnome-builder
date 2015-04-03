/* gb-new-project-dialog.c
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

#include "gb-new-project-dialog.h"
#include "gb-widget.h"

struct _GbNewProjectDialog
{
  GtkDialog             parent_instance;

  GtkButton            *back_button;
  GtkButton            *cancel_button;
  GtkButton            *create_button;
  GtkFileChooserWidget *file_chooser;
  GtkListBox           *open_list_box;
  GtkListBoxRow        *row_open_local;
  GtkBox               *page_open_project;
  GtkStack             *stack;
};

G_DEFINE_TYPE (GbNewProjectDialog, gb_new_project_dialog, GTK_TYPE_WINDOW)

enum {
  PROP_0,
  LAST_PROP
};

enum {
  BACK,
  CLOSE,
  OPEN_PROJECT,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint gSignals [LAST_SIGNAL];

static void
gb_new_project_dialog_back (GbNewProjectDialog *self)
{
  GtkWidget *child;

  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));

  child = gtk_stack_get_visible_child (self->stack);

  if (child == GTK_WIDGET (self->page_open_project))
    g_signal_emit_by_name (self, "close");
  else
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->page_open_project));
}

static void
gb_new_project_dialog__back_button_clicked (GbNewProjectDialog *self,
                                            GtkButton          *back_button)
{
  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));
  g_assert (GTK_IS_BUTTON (back_button));

  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->page_open_project));
}

static void
gb_new_project_dialog__cancel_button_clicked (GbNewProjectDialog *self,
                                              GtkButton          *cancel_button)
{
  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));
  g_assert (GTK_IS_BUTTON (cancel_button));

  gtk_window_close (GTK_WINDOW (self));
}

static void
gb_new_project_dialog__create_button_clicked (GbNewProjectDialog *self,
                                              GtkButton          *cancel_button)
{
  GtkWidget *visible_child;

  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));
  g_assert (GTK_IS_BUTTON (cancel_button));

  visible_child = gtk_stack_get_visible_child (self->stack);

  if (visible_child == GTK_WIDGET (self->file_chooser))
    {
      g_autoptr(GFile) file = NULL;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self->file_chooser));
      if (file != NULL)
        g_signal_emit (self, gSignals [OPEN_PROJECT], 0, file);
    }
}

static void
gb_new_project_dialog__open_list_box_row_activated (GbNewProjectDialog *self,
                                                    GtkListBoxRow      *row,
                                                    GtkListBox         *list_box)
{
  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if (row == self->row_open_local)
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->file_chooser));
}

static void
gb_new_project_dialog__stack_notify_visible_child (GbNewProjectDialog *self,
                                                   GParamSpec         *pspec,
                                                   GtkStack           *stack)
{
  GtkWidget *visible_child;

  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));
  g_assert (GTK_IS_STACK (stack));

  visible_child = gtk_stack_get_visible_child (stack);

  if (visible_child == GTK_WIDGET (self->file_chooser))
    {
      gtk_widget_hide (GTK_WIDGET (self->cancel_button));
      gtk_widget_show (GTK_WIDGET (self->back_button));
      gtk_widget_set_sensitive (GTK_WIDGET (self->create_button), FALSE);
    }
  else if (visible_child == GTK_WIDGET (self->page_open_project))
  {
      gtk_widget_hide (GTK_WIDGET (self->back_button));
      gtk_widget_show (GTK_WIDGET (self->cancel_button));
      gtk_widget_set_sensitive (GTK_WIDGET (self->create_button), FALSE);
  }
}

static GList *
gb_new_project_dialog_create_filters (GbNewProjectDialog *self)
{
  GtkFileFilter *filter;
  GList *list = NULL;

  /*
   * TODO: These should come from extension points in libide.
   */

  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));

  /* autotools filter (IdeAutotoolsBuildSystem) */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Autotools Project (configure.ac)"));
  gtk_file_filter_add_pattern (filter, "configure.ac");
  list = g_list_append (list, filter);

  /* any directory filter (IdeDirectoryBuildSystem) */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Any Directory"));
  gtk_file_filter_add_pattern (filter, "*");
  list = g_list_append (list, filter);

  return list;
}

static void
gb_new_project_dialog__file_chooser_selection_changed (GbNewProjectDialog *self,
                                                       GtkFileChooser     *file_chooser)
{
  g_autoptr(GFile) file = NULL;

  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));
  g_assert (GTK_IS_FILE_CHOOSER (file_chooser));

  file = gtk_file_chooser_get_file (file_chooser);

  gtk_widget_set_sensitive (GTK_WIDGET (self->create_button), !!file);
}

static void
gb_new_project_dialog__file_chooser_file_activated (GbNewProjectDialog *self,
                                                    GtkFileChooser     *file_chooser)
{
  g_autoptr(GFile) file = NULL;

  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));
  g_assert (GTK_IS_FILE_CHOOSER (file_chooser));

  file = gtk_file_chooser_get_file (file_chooser);
  if (file != NULL)
    g_signal_emit (self, gSignals [OPEN_PROJECT], 0, file);
}

static void
gb_new_project_dialog_close (GbNewProjectDialog *self)
{
  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));

  gtk_window_close (GTK_WINDOW (self));
}

static void
gb_new_project_dialog_finalize (GObject *object)
{
  GbNewProjectDialog *self = (GbNewProjectDialog *)object;

  G_OBJECT_CLASS (gb_new_project_dialog_parent_class)->finalize (object);
}

static void
gb_new_project_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GbNewProjectDialog *self = GB_NEW_PROJECT_DIALOG (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_new_project_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbNewProjectDialog *self = GB_NEW_PROJECT_DIALOG (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_new_project_dialog_class_init (GbNewProjectDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;

  object_class->finalize = gb_new_project_dialog_finalize;
  object_class->get_property = gb_new_project_dialog_get_property;
  object_class->set_property = gb_new_project_dialog_set_property;

  gSignals [BACK] =
    g_signal_new_class_handler ("back",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gb_new_project_dialog_back),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE,
                                0);

  gSignals [CLOSE] =
    g_signal_new_class_handler ("close",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gb_new_project_dialog_close),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE,
                                0);

  gSignals [OPEN_PROJECT] =
    g_signal_new ("open-project",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_FILE);

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Escape,
                                0,
                                "back",
                                0);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-new-project-dialog.ui");

  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, back_button);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, cancel_button);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, create_button);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, file_chooser);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, open_list_box);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, page_open_project);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, row_open_local);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, stack);
}

static void
gb_new_project_dialog_init (GbNewProjectDialog *self)
{
  GList *iter;
  GList *filters;

  gtk_widget_init_template (GTK_WIDGET (self));

  filters = gb_new_project_dialog_create_filters (self);
  for (iter = filters; iter; iter = iter->next)
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (self->file_chooser), iter->data);
  g_list_free (filters);

  g_signal_connect_object (self->stack,
                           "notify::visible-child",
                           G_CALLBACK (gb_new_project_dialog__stack_notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->back_button,
                           "clicked",
                           G_CALLBACK (gb_new_project_dialog__back_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->cancel_button,
                           "clicked",
                           G_CALLBACK (gb_new_project_dialog__cancel_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->create_button,
                           "clicked",
                           G_CALLBACK (gb_new_project_dialog__create_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->open_list_box,
                           "row-activated",
                           G_CALLBACK (gb_new_project_dialog__open_list_box_row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->file_chooser,
                           "selection-changed",
                           G_CALLBACK (gb_new_project_dialog__file_chooser_selection_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->file_chooser,
                           "file-activated",
                           G_CALLBACK (gb_new_project_dialog__file_chooser_file_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_notify (G_OBJECT (self->stack), "visible-child");
}
