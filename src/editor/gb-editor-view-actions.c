/* gb-editor-view-actions.c
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

#define G_LOG_DOMAIN "gb-editor-view"

#include <glib/gi18n.h>
#include <ide.h>
#include <string.h>

#include "gb-editor-frame-private.h"
#include "gb-editor-view-actions.h"
#include "gb-editor-view-private.h"
#include "gb-html-document.h"
#include "gb-view-grid.h"
#include "gb-widget.h"
#include "gb-workbench.h"

static void
gb_editor_view_actions_source_view_notify (IdeSourceView *source_view,
                                           GParamSpec    *pspec,
                                           GActionMap    *actions)
{
  g_autoptr(GVariant) param = NULL;
  GtkSourceView *gsv;
  GAction *action = NULL;

  g_assert (IDE_IS_SOURCE_VIEW (source_view));
  g_assert (pspec != NULL);
  g_assert (G_IS_ACTION_MAP (actions));

  gsv = GTK_SOURCE_VIEW (source_view);

  if (g_str_equal (pspec->name, "show-line-numbers"))
    {
      gboolean show_line_numbers;

      action = g_action_map_lookup_action (actions, "show-line-numbers");
      show_line_numbers = gtk_source_view_get_show_line_numbers (gsv);
      param = g_variant_new_boolean (show_line_numbers);
    }
  else if (g_str_equal (pspec->name, "show-right-margin"))
    {
      gboolean show_right_margin;

      action = g_action_map_lookup_action (actions, "show-right-margin");
      show_right_margin = gtk_source_view_get_show_right_margin (gsv);
      param = g_variant_new_boolean (show_right_margin);
    }
  else if (g_str_equal (pspec->name, "highlight-current-line"))
    {
      gboolean highlight_current_line;

      action = g_action_map_lookup_action (actions, "highlight-current-line");
      g_object_get (gsv, "highlight-current-line", &highlight_current_line, NULL);
      param = g_variant_new_boolean (highlight_current_line);
    }
  else if (g_str_equal (pspec->name, "auto-indent"))
    {
      gboolean auto_indent;

      action = g_action_map_lookup_action (actions, "auto-indent");
      g_object_get (source_view, "auto-indent", &auto_indent, NULL);
      param = g_variant_new_boolean (auto_indent);
    }
  else if (g_str_equal (pspec->name, "tab-width"))
    {
      guint tab_width;

      action = g_action_map_lookup_action (actions, "tab-width");
      g_object_get (source_view, "tab-width", &tab_width, NULL);
      param = g_variant_new_int32 (tab_width);
    }
  else if (g_str_equal (pspec->name, "insert-spaces-instead-of-tabs"))
    {
      gboolean use_spaces;

      action = g_action_map_lookup_action (actions, "use-spaces");
      g_object_get (source_view, "insert-spaces-instead-of-tabs", &use_spaces, NULL);
      param = g_variant_new_boolean (use_spaces);
    }
  else if (g_str_equal (pspec->name, "smart-backspace"))
    {
      gboolean smart_backspace;

      action = g_action_map_lookup_action (actions, "smart-backspace");
      g_object_get (source_view, "smart-backspace", &smart_backspace, NULL);
      param = g_variant_new_boolean (smart_backspace);
    }

  if (action && param)
    {
      g_simple_action_set_state (G_SIMPLE_ACTION (action), param);
      param = NULL;
    }
}

static void
gb_editor_view_actions_language (GSimpleAction *action,
                                 GVariant      *variant,
                                 gpointer       user_data)
{
  GbEditorView *self = user_data;
  GtkSourceLanguageManager *manager;
  GtkSourceLanguage *language;
  GtkSourceBuffer *buffer;
  const gchar *name;

  g_assert (GB_IS_EDITOR_VIEW (self));

  manager = gtk_source_language_manager_get_default ();
  name = g_variant_get_string (variant, NULL);
  buffer = GTK_SOURCE_BUFFER (self->document);

  if (name != NULL)
    {
      language = gtk_source_language_manager_get_language (manager, name);
      gtk_source_buffer_set_language (buffer, language);
      gb_editor_view_actions_update (self);
    }
}

#define STATE_HANDLER_BOOLEAN(name,propname)                       \
static void                                                        \
gb_editor_view_actions_##name (GSimpleAction *action,              \
                               GVariant      *variant,             \
                               gpointer       user_data)           \
{                                                                  \
  GbEditorView *self = user_data;                                  \
  gboolean val;                                                    \
                                                                   \
  g_assert (GB_IS_EDITOR_VIEW (self));                             \
                                                                   \
  val = g_variant_get_boolean (variant);                           \
  g_object_set (self->frame1->source_view, propname, val, NULL);   \
  if (self->frame2)                                                \
    g_object_set (self->frame2->source_view, propname, val, NULL); \
}

#define STATE_HANDLER_INT(name,propname)                           \
static void                                                        \
gb_editor_view_actions_##name (GSimpleAction *action,              \
                               GVariant      *variant,             \
                               gpointer       user_data)           \
{                                                                  \
  GbEditorView *self = user_data;                                  \
  gint val;                                                        \
                                                                   \
  g_assert (GB_IS_EDITOR_VIEW (self));                             \
                                                                   \
  val = g_variant_get_int32 (variant);                             \
  g_object_set (self->frame1->source_view, propname, val, NULL);   \
  if (self->frame2)                                                \
    g_object_set (self->frame2->source_view, propname, val, NULL); \
}

STATE_HANDLER_BOOLEAN (auto_indent, "auto-indent")
STATE_HANDLER_BOOLEAN (show_line_numbers, "show-line-numbers")
STATE_HANDLER_BOOLEAN (show_right_margin, "show-right-margin")
STATE_HANDLER_BOOLEAN (highlight_current_line, "highlight-current-line")
STATE_HANDLER_BOOLEAN (use_spaces, "insert-spaces-instead-of-tabs")
STATE_HANDLER_BOOLEAN (smart_backspace, "smart-backspace")
STATE_HANDLER_INT (tab_width, "tab-width")

static void
save_file_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(GbEditorView) self = user_data;
  GError *error = NULL;

  if (!ide_buffer_manager_save_file_finish (buffer_manager, result, &error))
    {
      /* info bar */
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  gb_widget_fade_hide (GTK_WIDGET (self->progress_bar));
}

static void
gb_editor_view_actions_save (GSimpleAction *action,
                             GVariant      *param,
                             gpointer       user_data)
{
  GbEditorView *self = user_data;
  IdeContext *context;
  IdeBufferManager *buffer_manager;
  IdeFile *file;
  IdeProgress *progress = NULL;

  g_assert (GB_IS_EDITOR_VIEW (self));

  file = ide_buffer_get_file (IDE_BUFFER (self->document));
  context = ide_buffer_get_context (IDE_BUFFER (self->document));
  buffer_manager = ide_context_get_buffer_manager (context);

#if 0
  if (!file || is_temporary (file))
    {
      /* todo: dialog */
    }
#endif

  ide_buffer_manager_save_file_async (buffer_manager,
                                      IDE_BUFFER (self->document),
                                      file,
                                      &progress,
                                      NULL,
                                      save_file_cb,
                                      g_object_ref (self));
  g_object_bind_property (progress, "fraction", self->progress_bar, "fraction",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_show (GTK_WIDGET (self->progress_bar));
  g_clear_object (&progress);
}

static void
gb_editor_view_actions__save_as_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  g_autoptr(GbEditorView) self = user_data;
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  GError *error = NULL;

  if (!ide_buffer_manager_save_file_finish (buffer_manager, result, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
}

static void
save_as_response (GtkWidget *widget,
                  gint       response,
                  gpointer   user_data)
{
  g_autoptr(GbEditorView) self = user_data;
  g_autoptr(GFile) target = NULL;
  g_autoptr(IdeProgress) progress = NULL;
  GtkFileChooser *chooser = (GtkFileChooser *)widget;

  g_assert (GTK_IS_FILE_CHOOSER (chooser));
  g_assert (GB_IS_EDITOR_VIEW (self));

  switch (response)
    {
    case GTK_RESPONSE_OK:
      target = gtk_file_chooser_get_file (chooser);
      break;

    case GTK_RESPONSE_CANCEL:
    default:
      break;
    }

  if (target != NULL)
    {
      IdeBufferManager *buffer_manager;
      IdeContext *context;
      IdeProject *project;
      IdeBuffer *buffer = IDE_BUFFER (self->document);
      g_autoptr(IdeFile) file = NULL;

      context = ide_buffer_get_context (buffer);
      project = ide_context_get_project (context);
      buffer_manager = ide_context_get_buffer_manager (context);
      file = ide_project_get_project_file (project, target);

      ide_buffer_manager_save_file_async (buffer_manager,
                                          buffer,
                                          file,
                                          &progress,
                                          NULL,
                                          gb_editor_view_actions__save_as_cb,
                                          g_object_ref (self));
    }

  gtk_widget_destroy (widget);
}

static void
gb_editor_view_actions_save_as (GSimpleAction *action,
                                GVariant      *param,
                                gpointer       user_data)
{
  GbEditorView *self = user_data;
  IdeBuffer *buffer;
  GtkWidget *suggested;
  GtkWidget *toplevel;
  GtkWidget *dialog;
  IdeFile *file;
  GFile *gfile;

  g_assert (GB_IS_EDITOR_VIEW (self));

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "action", GTK_FILE_CHOOSER_ACTION_SAVE,
                         "do-overwrite-confirmation", TRUE,
                         "local-only", FALSE,
                         "modal", TRUE,
                         "select-multiple", FALSE,
                         "show-hidden", FALSE,
                         "transient-for", toplevel,
                         "title", _("Save Document As"),
                         NULL);

  buffer = IDE_BUFFER (self->document);
  file = ide_buffer_get_file (buffer);
  gfile = ide_file_get_file (file);

  if (gfile != NULL)
    gtk_file_chooser_set_file (GTK_FILE_CHOOSER (dialog), gfile, NULL);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Save"), GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  suggested = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog),
                                                  GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (suggested),
                               GTK_STYLE_CLASS_SUGGESTED_ACTION);

  g_signal_connect (dialog, "response", G_CALLBACK (save_as_response), g_object_ref (self));

  gtk_window_present (GTK_WINDOW (dialog));
}

static gboolean
set_split_view (gpointer data)
{
  g_autoptr(GbEditorView) self = data;

  g_assert (GB_IS_EDITOR_VIEW (self));

  gb_view_set_split_view (GB_VIEW (self), (self->frame2 == NULL));

  return G_SOURCE_REMOVE;
}

static void
gb_editor_view_actions_toggle_split (GSimpleAction *action,
                                     GVariant      *param,
                                     gpointer       user_data)
{
  GbEditorView *self = user_data;

  g_assert (GB_IS_EDITOR_VIEW (self));

  g_timeout_add (0, set_split_view, g_object_ref (self));
}

static void
gb_editor_view_actions_close (GSimpleAction *action,
                              GVariant      *param,
                              gpointer       user_data)
{
  GbEditorView *self = user_data;

  g_assert (GB_IS_EDITOR_VIEW (self));

  /* just close our current frame if we have split view */
  if (self->frame2 != NULL)
    {
      /* todo: swap frame1/frame2 if frame2 was last focused. */
      g_timeout_add (0, set_split_view, g_object_ref (self));
    }
  else
    {
      gb_widget_activate_action (GTK_WIDGET (self), "view-stack", "close", NULL);
    }
}

static gboolean
has_suffix (const gchar          *path,
            const gchar * const *allowed_suffixes)
{
  const gchar *dot;
  gsize i;

  dot = strrchr (path, '.');
  if (!dot)
    return FALSE;

  dot++;

  for (i = 0; allowed_suffixes [i]; i++)
    {
      if (g_str_equal (dot, allowed_suffixes [i]))
        return TRUE;
    }

  return FALSE;
}

static void
gb_editor_view_actions_find_other_file_worker (GTask        *task,
                                               gpointer      source_object,
                                               gpointer      task_data,
                                               GCancellable *cancellable)
{
  GbEditorView *self = source_object;
  const gchar *src_suffixes[] = { "c", "cc", "cpp", "cxx", NULL };
  const gchar *hdr_suffixes[] = { "h", "hh", "hpp", "hxx", NULL };
  const gchar **target = NULL;
  IdeFile *file;
  g_autofree gchar *prefix = NULL;
  const gchar *path;
  gsize i;

  g_assert (GB_IS_EDITOR_VIEW (self));

  file = ide_buffer_get_file (IDE_BUFFER (self->document));
  path = ide_file_get_path (file);

  if (has_suffix (path, src_suffixes))
    {
      target = hdr_suffixes;
    }
  else if (has_suffix (path, hdr_suffixes))
    {
      target = src_suffixes;
    }
  else
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               "File is missing a suffix.");
      return;
    }

  prefix = g_strndup (path, strrchr (path, '.') - path);

  for (i = 0; target [i]; i++)
    {
      g_autofree gchar *new_path = NULL;

      new_path = g_strdup_printf ("%s.%s", prefix, target [i]);

      if (g_file_test (new_path, G_FILE_TEST_IS_REGULAR))
        {
          g_autoptr(GFile) gfile = NULL;

          gfile = g_file_new_for_path (new_path);
          g_task_return_pointer (task, g_object_ref (gfile), g_object_unref);
          return;
        }
    }

  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_FOUND,
                           "Failed to locate other file.");
}

static void
find_other_file_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GbEditorView *self = (GbEditorView *)object;
  GTask *task = (GTask *)result;
  g_autoptr(GFile) ret = NULL;

  ret = g_task_propagate_pointer (task, NULL);

  if (ret)
    {
      GbWorkbench *workbench;

      workbench = gb_widget_get_workbench (GTK_WIDGET (self));
      gb_workbench_open (workbench, ret);
    }
}

static void
gb_editor_view_actions_find_other_file (GSimpleAction *action,
                                        GVariant      *param,
                                        gpointer       user_data)
{
  GbEditorView *self = user_data;
  g_autoptr(GTask) task = NULL;

  g_assert (GB_IS_EDITOR_VIEW (self));

  task = g_task_new (self, NULL, find_other_file_cb, NULL);
  g_task_run_in_thread (task, gb_editor_view_actions_find_other_file_worker);
}

static void
gb_editor_view_actions_preview (GSimpleAction *action,
                                GVariant      *param,
                                gpointer       user_data)
{
  GbEditorView *self = user_data;
  GtkSourceLanguage *language;
  const gchar *lang_id = NULL;
  g_autoptr(GbDocument) document = NULL;

  g_assert (GB_IS_EDITOR_VIEW (self));

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self->document));
  if (!language)
    return;

  lang_id = gtk_source_language_get_id (language);
  if (!lang_id)
    return;

  if (g_str_equal (lang_id, "html"))
    {
      document = g_object_new (GB_TYPE_HTML_DOCUMENT,
                               "buffer", self->document,
                               NULL);
    }
  else if (g_str_equal (lang_id, "markdown"))
    {
      document = g_object_new (GB_TYPE_HTML_DOCUMENT,
                               "buffer", self->document,
                               NULL);
      gb_html_document_set_transform_func (GB_HTML_DOCUMENT (document),
                                           gb_html_markdown_transform);
    }

  if (document)
    {
      GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (self));

      while (parent && !GB_IS_VIEW_GRID (parent))
        parent = gtk_widget_get_parent (parent);

      if (parent == NULL)
        {
          while (parent && !GB_IS_VIEW_STACK (parent))
            parent = gtk_widget_get_parent (parent);
          g_assert (GB_IS_VIEW_STACK (parent));
          gb_view_stack_focus_document (GB_VIEW_STACK (parent), document);
          return;
        }

      g_assert (GB_IS_VIEW_GRID (parent));
      gb_view_grid_focus_document (GB_VIEW_GRID (parent), document);
    }
}

static GActionEntry GbEditorViewActions[] = {
  { "auto-indent", NULL, NULL, "false", gb_editor_view_actions_auto_indent },
  { "close", gb_editor_view_actions_close },
  { "find-other-file", gb_editor_view_actions_find_other_file },
  { "highlight-current-line", NULL, NULL, "false", gb_editor_view_actions_highlight_current_line },
  { "language", NULL, "s", "''", gb_editor_view_actions_language },
  { "preview", gb_editor_view_actions_preview },
  { "save", gb_editor_view_actions_save },
  { "save-as", gb_editor_view_actions_save_as },
  { "show-line-numbers", NULL, NULL, "false", gb_editor_view_actions_show_line_numbers },
  { "show-right-margin", NULL, NULL, "false", gb_editor_view_actions_show_right_margin },
  { "smart-backspace", NULL, NULL, "false", gb_editor_view_actions_smart_backspace },
  { "tab-width", NULL, "i", "8", gb_editor_view_actions_tab_width },
  { "toggle-split", gb_editor_view_actions_toggle_split },
  { "use-spaces", NULL, "b", "false", gb_editor_view_actions_use_spaces },
};

void
gb_editor_view_actions_init (GbEditorView *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), GbEditorViewActions,
                                   G_N_ELEMENTS (GbEditorViewActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "view", G_ACTION_GROUP (group));
  gtk_widget_insert_action_group (GTK_WIDGET (self->tweak_widget), "view", G_ACTION_GROUP (group));

#define WATCH_PROPERTY(name) \
  G_STMT_START { \
    g_signal_connect (self->frame1->source_view, \
                      "notify::"name, \
                      G_CALLBACK (gb_editor_view_actions_source_view_notify), \
                      group); \
    g_object_notify (G_OBJECT (self->frame1->source_view), name); \
  } G_STMT_END

  WATCH_PROPERTY ("auto-indent");
  WATCH_PROPERTY ("highlight-current-line");
  WATCH_PROPERTY ("insert-spaces-instead-of-tabs");
  WATCH_PROPERTY ("show-line-numbers");
  WATCH_PROPERTY ("show-right-margin");
  WATCH_PROPERTY ("smart-backspace");
  WATCH_PROPERTY ("tab-width");

#undef WATCH_PROPERTY
}

void
gb_editor_view_actions_update (GbEditorView *self)
{
  GtkSourceLanguage *language;
  const gchar *lang_id = NULL;
  GActionGroup *group;
  GAction *action;
  gboolean enabled;

  g_assert (GB_IS_EDITOR_VIEW (self));

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "view");
  if (!G_IS_SIMPLE_ACTION_GROUP (group))
    return;

  /* update preview sensitivity */
  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self->document));
  if (language)
    lang_id = gtk_source_language_get_id (language);
  enabled = ((g_strcmp0 (lang_id, "html") == 0) ||
             (g_strcmp0 (lang_id, "markdown") == 0));
  action = g_action_map_lookup_action (G_ACTION_MAP (group), "preview");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}
