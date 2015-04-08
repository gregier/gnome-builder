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
#include <libgit2-glib/ggit.h>

#include "gb-new-project-dialog.h"
#include "gb-string.h"
#include "gb-widget.h"

#define ANIMATION_DURATION_MSEC 250

struct _GbNewProjectDialog
{
  GtkDialog             parent_instance;

  gdouble               progress_fraction;

  GtkButton            *back_button;
  GtkButton            *cancel_button;
  GtkFileChooserWidget *clone_location_button;
  GtkProgressBar       *clone_progress;
  GtkEntry             *clone_uri_entry;
  GtkButton            *create_button;
  GtkFileChooserWidget *file_chooser;
  GtkHeaderBar         *header_bar;
  GtkListBox           *open_list_box;
  GtkListBoxRow        *row_clone_remote;
  GtkListBoxRow        *row_open_local;
  GtkBox               *page_clone_remote;
  GtkBox               *page_open_project;
  GtkStack             *stack;
};

typedef struct
{
  gchar *uri;
  GFile *location;
} CloneRequest;

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
clone_request_free (gpointer data)
{
  CloneRequest *req = data;

  if (req)
    {
      g_free (req->uri);
      g_clear_object (&req->location);
      g_free (req);
    }
}

static CloneRequest *
clone_request_new (const gchar *uri,
                   GFile       *location)
{
  CloneRequest *req;

  g_assert (uri);
  g_assert (location);

  req = g_new0 (CloneRequest, 1);
  req->uri = g_strdup (uri);
  req->location = g_object_ref (location);

  return req;
}

static void
gb_new_project_dialog_back (GbNewProjectDialog *self)
{
  GtkWidget *child;

  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));

  child = gtk_stack_get_visible_child (self->stack);

  if (child == GTK_WIDGET (self->page_open_project))
    g_signal_emit_by_name (self, "close");

  if (gtk_widget_get_sensitive (GTK_WIDGET (self->back_button)))
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->page_open_project));
}

static gboolean
open_after_timeout (gpointer user_data)
{
  GbNewProjectDialog *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);
  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));

  gtk_widget_set_sensitive (GTK_WIDGET (self->back_button), TRUE);

  file = g_task_propagate_pointer (task, &error);

  if (file == NULL)
    g_warning ("%s", error->message);
  else
    g_signal_emit (self, gSignals [OPEN_PROJECT], 0, file);

  return G_SOURCE_REMOVE;
}

static void
gb_new_project_dialog__clone_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GbNewProjectDialog *self = (GbNewProjectDialog *)object;
  GTask *task = (GTask *)result;

  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));
  g_assert (G_IS_TASK (task));

  ide_object_animate_full (self->clone_progress,
                           IDE_ANIMATION_EASE_IN_OUT_QUAD,
                           ANIMATION_DURATION_MSEC,
                           NULL,
                           (GDestroyNotify)gb_widget_fade_hide,
                           self->clone_progress,
                           "fraction", 1.0,
                           NULL);

  /*
   * Wait for a second so animations can complete before opening
   * the project. Otherwise, it's pretty jarring to the user.
   */
  g_timeout_add_seconds (1, open_after_timeout, g_object_ref (task));
}

static gboolean
update_progress_cb (gpointer data)
{
  g_autoptr(GbNewProjectDialog) self = data;

  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));

  ide_object_animate (self->clone_progress,
                      IDE_ANIMATION_EASE_IN_OUT_QUAD,
                      ANIMATION_DURATION_MSEC,
                      NULL,
                      "fraction", self->progress_fraction,
                      NULL);

  return G_SOURCE_REMOVE;
}

static void
transfer_progress_cb (GgitRemoteCallbacks  *callbacks,
                      GgitTransferProgress *stats,
                      gpointer              user_data)
{
  GbNewProjectDialog *self = user_data;
  guint total;
  guint received;

  g_assert (GGIT_IS_REMOTE_CALLBACKS (callbacks));
  g_assert (stats != NULL);

  total = ggit_transfer_progress_get_total_objects (stats);
  received = ggit_transfer_progress_get_received_objects (stats);
  if (total == 0)
    return;

  self->progress_fraction = (gdouble)received / (gdouble)total;

  g_timeout_add (0, update_progress_cb, g_object_ref (self));
}

static void
gb_new_project_dialog__clone_worker (GTask        *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  GbNewProjectDialog *self = source_object;
  GgitRepository *repository;
  g_autoptr(GFile) workdir = NULL;
  CloneRequest *req = task_data;
  GgitCloneOptions *clone_options;
  GgitRemoteCallbacks *callbacks;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (GB_IS_NEW_PROJECT_DIALOG (source_object));
  g_assert (req != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  clone_options = ggit_clone_options_new ();

  ggit_clone_options_set_is_bare (clone_options, FALSE);
  ggit_clone_options_set_checkout_branch (clone_options, "master");

  callbacks = g_object_new (GGIT_TYPE_REMOTE_CALLBACKS, NULL);
  g_signal_connect (callbacks, "transfer-progress", G_CALLBACK (transfer_progress_cb), self);
  ggit_clone_options_set_remote_callbacks (clone_options, callbacks);

  repository = ggit_repository_clone (req->uri, req->location, clone_options, &error);

  g_object_unref (callbacks);

  if (repository == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  workdir = ggit_repository_get_workdir (repository);
  g_task_return_pointer (task, g_object_ref (workdir), g_object_unref);

  g_object_unref (repository);
}

static void
gb_new_project_dialog_begin_clone (GbNewProjectDialog *self)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) location = NULL;
  CloneRequest *req;
  const gchar *uri;

  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));

  gtk_widget_set_sensitive (GTK_WIDGET (self->back_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->create_button), FALSE);

  uri = gtk_entry_get_text (self->clone_uri_entry);
  location = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self->clone_location_button));
  req = clone_request_new (uri, location);
  task = g_task_new (self, NULL, gb_new_project_dialog__clone_cb, self);
  g_task_set_task_data (task, req, clone_request_free);
  g_task_run_in_thread (task, gb_new_project_dialog__clone_worker);
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
  else if (visible_child == GTK_WIDGET (self->page_clone_remote))
    {
      gb_new_project_dialog_begin_clone (self);
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
  else if (row == self->row_clone_remote)
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->page_clone_remote));
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
      gtk_header_bar_set_title (self->header_bar, _("Select Project File"));
    }
  else if (visible_child == GTK_WIDGET (self->page_open_project))
    {
      gtk_widget_hide (GTK_WIDGET (self->back_button));
      gtk_widget_show (GTK_WIDGET (self->cancel_button));
      gtk_widget_set_sensitive (GTK_WIDGET (self->create_button), FALSE);
      gtk_header_bar_set_title (self->header_bar, _("New Project"));
    }
  else if (visible_child == GTK_WIDGET (self->page_clone_remote))
    {
      gtk_widget_hide (GTK_WIDGET (self->cancel_button));
      gtk_widget_show (GTK_WIDGET (self->back_button));
      gtk_widget_set_sensitive (GTK_WIDGET (self->create_button), FALSE);
      gtk_header_bar_set_title (self->header_bar, _("Clone Repository"));
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
gb_new_project_dialog__open_list_box_header_func (GtkListBoxRow *row,
                                                  GtkListBoxRow *before,
                                                  gpointer       user_data)
{
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
gb_new_project_dialog__clone_uri_entry_changed (GbNewProjectDialog *self,
                                                GtkEntry           *entry)
{
  const gchar *text;
  gboolean is_valid;

  g_assert (GB_IS_NEW_PROJECT_DIALOG (self));
  g_assert (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (entry);
  is_valid = ide_vcs_uri_is_valid (text);

  gtk_widget_set_sensitive (GTK_WIDGET (self->create_button), is_valid);

  if (is_valid)
    {
      g_object_set (self->clone_uri_entry,
                    "secondary-icon-name", NULL,
                    NULL);
    }
  else
    {
      g_object_set (self->clone_uri_entry,
                    "secondary-icon-name", "dialog-warning-symbolic",
                    NULL);
    }
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
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, clone_location_button);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, clone_progress);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, clone_uri_entry);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, create_button);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, file_chooser);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, header_bar);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, open_list_box);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, page_clone_remote);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, page_open_project);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, row_open_local);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, row_clone_remote);
  GB_WIDGET_CLASS_BIND (klass, GbNewProjectDialog, stack);
}

static void
gb_new_project_dialog_init (GbNewProjectDialog *self)
{
  g_autofree gchar *path = NULL;
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

  g_signal_connect_object (self->clone_uri_entry,
                           "changed",
                           G_CALLBACK (gb_new_project_dialog__clone_uri_entry_changed),
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

  gtk_list_box_set_header_func (self->open_list_box,
                                gb_new_project_dialog__open_list_box_header_func,
                                NULL, NULL);

  path = g_build_filename (g_get_home_dir (), Q_("Directory|Projects"), NULL);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (self->file_chooser), path);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (self->clone_location_button), path);

  g_object_notify (G_OBJECT (self->stack), "visible-child");
}
