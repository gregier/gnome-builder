/* ide-unsaved-files.c
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

#define G_LOG_DOMAIN "ide-unsaved-files"

#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-global.h"
#include "ide-internal.h"
#include "ide-project.h"
#include "ide-unsaved-file.h"
#include "ide-unsaved-files.h"

typedef struct
{
  gint64           sequence;
  GFile           *file;
  GBytes          *content;
  gchar           *temp_path;
  gint             temp_fd;
  IdeUnsavedFiles *backptr;
} UnsavedFile;

typedef struct
{
  GPtrArray *unsaved_files;
  gint64     sequence;
} IdeUnsavedFilesPrivate;

typedef struct
{
  GPtrArray *unsaved_files;
  gchar     *drafts_directory;
} AsyncState;

G_DEFINE_TYPE_WITH_PRIVATE (IdeUnsavedFiles, ide_unsaved_files, IDE_TYPE_OBJECT)

gchar *
get_drafts_directory (IdeContext *context)
{
  IdeProject *project;
  const gchar *project_name;

  project = ide_context_get_project (context);
  project_name = ide_project_get_id (project);

  return g_build_filename (g_get_user_data_dir (),
                           ide_get_program_name (),
                           "drafts",
                           project_name,
                           NULL);
}

static void
async_state_free (gpointer data)
{
  AsyncState *state = data;

  if (state)
    {
      g_free (state->drafts_directory);
      g_ptr_array_unref (state->unsaved_files);
      g_slice_free (AsyncState, state);
    }
}

static void
unsaved_file_free (gpointer data)
{
  UnsavedFile *uf = data;

  if (uf)
    {
      g_clear_object (&uf->file);
      g_clear_pointer (&uf->content, g_bytes_unref);

      if (uf->temp_path != NULL)
        {
           g_unlink (uf->temp_path);
           g_clear_pointer (&uf->temp_path, g_free);
        }

      if (uf->temp_fd != -1)
        {
          g_close (uf->temp_fd, NULL);
          uf->temp_fd = -1;
        }

      g_slice_free (UnsavedFile, uf);
    }
}

static UnsavedFile *
unsaved_file_copy (const UnsavedFile *uf)
{
  UnsavedFile *copy;

  copy = g_slice_new0 (UnsavedFile);
  copy->file = g_object_ref (uf->file);
  copy->content = g_bytes_ref (uf->content);

  return copy;
}

static gboolean
unsaved_file_save (UnsavedFile  *uf,
                   const gchar  *path,
                   GError      **error)
{
  gboolean ret;

  g_assert (uf);
  g_assert (uf->content);
  g_assert (path);

  ret = g_file_set_contents (path,
                             g_bytes_get_data (uf->content, NULL),
                             g_bytes_get_size (uf->content),
                             error);
  return ret;
}

static gchar *
hash_uri (const gchar *uri)
{
  GChecksum *checksum;
  gchar *ret;

  checksum = g_checksum_new (G_CHECKSUM_SHA1);
  g_checksum_update (checksum, (guchar *)uri, strlen (uri));
  ret = g_strdup (g_checksum_get_string (checksum));
  g_checksum_free (checksum);

  return ret;
}

static void
ide_unsaved_files_save_worker (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
  GString *manifest;
  AsyncState *state = task_data;
  g_autofree gchar *manifest_path = NULL;
  GError *error = NULL;
  gsize i;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_UNSAVED_FILES (source_object));
  g_assert (state);

  /* ensure that the directory exists */
  if (g_mkdir_with_parents (state->drafts_directory, 0700) != 0)
    {
      error = g_error_new_literal (G_IO_ERROR,
                                   g_io_error_from_errno (errno),
                                   "Failed to create drafts directory");
      g_task_return_error (task, error);
      return;
    }

  manifest = g_string_new (NULL);
  manifest_path = g_build_filename (state->drafts_directory,
                                    "manifest",
                                    NULL);

  for (i = 0; i < state->unsaved_files->len; i++)
    {
      g_autofree gchar *path = NULL;
      g_autofree gchar *uri = NULL;
      g_autofree gchar *hash = NULL;
      UnsavedFile *uf;

      uf = g_ptr_array_index (state->unsaved_files, i);

      uri = g_file_get_uri (uf->file);

      g_string_append_printf (manifest, "%s\n", uri);

      hash = hash_uri (uri);
      path = g_build_filename (state->drafts_directory, hash, NULL);

      if (!unsaved_file_save (uf, path, &error))
        {
          g_task_return_error (task, error);
          goto cleanup;
        }
    }

  if (!g_file_set_contents (manifest_path,
                            manifest->str, manifest->len,
                            &error))
    {
      g_task_return_error (task, error);
      goto cleanup;
    }

  g_task_return_boolean (task, TRUE);

cleanup:
  g_string_free (manifest, TRUE);
}

static AsyncState *
async_state_new (IdeUnsavedFiles *files)
{
  IdeContext *context;
  AsyncState *state;

  g_assert (IDE_IS_UNSAVED_FILES (files));

  context = ide_object_get_context (IDE_OBJECT (files));

  state = g_slice_new (AsyncState);
  state->unsaved_files = g_ptr_array_new_with_free_func (unsaved_file_free);
  state->drafts_directory = get_drafts_directory (context);

  return state;
}

void
ide_unsaved_files_save_async (IdeUnsavedFiles     *files,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  IdeUnsavedFilesPrivate *priv;
  g_autoptr(GTask) task = NULL;
  AsyncState *state;
  gsize i;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (files));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  priv = ide_unsaved_files_get_instance_private (files);

  state = async_state_new (files);

  for (i = 0; i < priv->unsaved_files->len; i++)
    {
      UnsavedFile *uf;
      UnsavedFile *uf_copy;

      uf = g_ptr_array_index (priv->unsaved_files, i);
      uf_copy = unsaved_file_copy (uf);
      g_ptr_array_add (state->unsaved_files, uf_copy);
    }

  task = g_task_new (files, cancellable, callback, user_data);
  g_task_set_task_data (task, state, async_state_free);
  g_task_run_in_thread (task, ide_unsaved_files_save_worker);
}

gboolean
ide_unsaved_files_save_finish (IdeUnsavedFiles  *files,
                               GAsyncResult     *result,
                               GError          **error)
{
  g_return_val_if_fail (IDE_IS_UNSAVED_FILES (files), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_unsaved_files_restore_worker (GTask        *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
  AsyncState *state = task_data;
  g_autofree gchar *manifest_contents = NULL;
  g_autofree gchar *manifest_path = NULL;
  gchar **lines;
  GError *error = NULL;
  gsize len;
  gsize i;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_UNSAVED_FILES (source_object));
  g_assert (state);

  manifest_path = g_build_filename (state->drafts_directory,
                                    "manifest",
                                    NULL);

  g_debug ("Loading drafts manifest %s", manifest_path);

  if (!g_file_test (manifest_path, G_FILE_TEST_IS_REGULAR))
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  if (!g_file_get_contents (manifest_path, &manifest_contents, &len, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  lines = g_strsplit (manifest_contents, "\n", 0);

  for (i = 0; lines [i]; i++)
    {
      g_autoptr(GFile) file = NULL;
      gchar *contents = NULL;
      g_autofree gchar *hash = NULL;
      g_autofree gchar *path = NULL;
      UnsavedFile *unsaved;
      gsize data_len;

      if (!*lines [i])
        continue;

      file = g_file_new_for_uri (lines [i]);
      if (!file || !g_file_query_exists (file, NULL))
        continue;

      hash = hash_uri (lines [i]);
      path = g_build_filename (state->drafts_directory, hash, NULL);

      g_debug ("Loading draft for \"%s\" from \"%s\"", lines [i], path);

      if (!g_file_get_contents (path, &contents, &data_len, &error))
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
          continue;
        }

      unsaved = g_slice_new0 (UnsavedFile);
      unsaved->file = g_object_ref (file);
      unsaved->content = g_bytes_new_take (contents, data_len);

      g_ptr_array_add (state->unsaved_files, unsaved);
    }

  g_strfreev (lines);

  g_task_return_boolean (task, TRUE);
}

void
ide_unsaved_files_restore_async (IdeUnsavedFiles     *files,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  AsyncState *state;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (files));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  state = async_state_new (files);

  task = g_task_new (files, cancellable, callback, user_data);
  g_task_set_task_data (task, state, async_state_free);
  g_task_run_in_thread (task, ide_unsaved_files_restore_worker);
}

gboolean
ide_unsaved_files_restore_finish (IdeUnsavedFiles  *files,
                                  GAsyncResult     *result,
                                  GError          **error)
{
  AsyncState *state;
  gsize i;

  g_return_val_if_fail (IDE_IS_UNSAVED_FILES (files), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  state = g_task_get_task_data (G_TASK (result));

  for (i = 0; i < state->unsaved_files->len; i++)
    {
      UnsavedFile *uf;

      uf = g_ptr_array_index (state->unsaved_files, i);
      ide_unsaved_files_update (files, uf->file, uf->content);
    }

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_unsaved_files_move_to_front (IdeUnsavedFiles *self,
                                 guint            index)
{
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);
  UnsavedFile *new_front;
  UnsavedFile *old_front;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (self));

  new_front = g_ptr_array_index (priv->unsaved_files, index);
  old_front = g_ptr_array_index (priv->unsaved_files, 0);

  /*
   * TODO: We could shift all these items down, but it probably isnt' worth
   *       the effort. We will just move-to-front after a miss and ping
   *       pong the old item back to the front.
   */
  priv->unsaved_files->pdata[0] = new_front;
  priv->unsaved_files->pdata[index] = old_front;
}

static void
ide_unsaved_files_remove_draft (IdeUnsavedFiles *self,
                                GFile           *file)
{
  IdeContext *context;
  g_autofree gchar *drafts_directory = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *hash = NULL;
  g_autofree gchar *path = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_UNSAVED_FILES (self));
  g_assert (G_IS_FILE (file));

  context = ide_object_get_context (IDE_OBJECT (self));
  drafts_directory = get_drafts_directory (context);
  uri = g_file_get_uri (file);
  hash = hash_uri (uri);
  path = g_build_filename (drafts_directory, hash, NULL);

  g_debug ("Removing draft for \"%s\"", uri);

  g_unlink (path);

  IDE_EXIT;
}

void
ide_unsaved_files_remove (IdeUnsavedFiles *self,
                          GFile           *file)
{
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);
  guint i;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (self));
  g_return_if_fail (G_IS_FILE (file));

  for (i = 0; i < priv->unsaved_files->len; i++)
    {
      UnsavedFile *unsaved;

      unsaved = g_ptr_array_index (priv->unsaved_files, i);

      if (g_file_equal (file, unsaved->file))
        {
          ide_unsaved_files_remove_draft (self, file);
          g_ptr_array_remove_index_fast (priv->unsaved_files, i);
          break;
        }
    }
}

static void
setup_tempfile (GFile  *file,
                gint   *temp_fd,
                gchar **temp_path)
{
  g_autofree gchar *name = NULL;
  const gchar *suffix;
  gchar *template;

  g_assert (G_IS_FILE (file));
  g_assert (temp_fd);
  g_assert (temp_path);

  *temp_fd = -1;
  *temp_path = NULL;

  name = g_file_get_basename (file);
  suffix = strrchr (name, '.') ?: "";
  template = g_strdup_printf ("builder_codeassistant_XXXXXX%s", suffix);
  *temp_fd = g_file_open_tmp (template, temp_path, NULL);
}

void
ide_unsaved_files_update (IdeUnsavedFiles *self,
                          GFile           *file,
                          GBytes          *content)
{
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);
  UnsavedFile *unsaved;
  guint i;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (self));
  g_return_if_fail (G_IS_FILE (file));

  priv->sequence++;

  if (!content)
    {
      ide_unsaved_files_remove (self, file);
      return;
    }

  for (i = 0; i < priv->unsaved_files->len; i++)
    {
      unsaved = g_ptr_array_index (priv->unsaved_files, i);

      if (g_file_equal (file, unsaved->file))
        {
          if (content != unsaved->content)
            {
              g_clear_pointer (&unsaved->content, g_bytes_unref);
              unsaved->content = g_bytes_ref (content);
              unsaved->sequence = priv->sequence;
            }

          /*
           * A file that get's updated is the most likely to get updated on
           * the next attempt. Therefore, we will simply move this entry to
           * the beginning of the array to increase it's chances of being the
           * first entry we check.
           */
          if (i != 0)
            ide_unsaved_files_move_to_front (self, i);

          return;
        }
    }

  unsaved = g_slice_new0 (UnsavedFile);
  unsaved->file = g_object_ref (file);
  unsaved->content = g_bytes_ref (content);
  unsaved->sequence = priv->sequence;
  setup_tempfile (file, &unsaved->temp_fd, &unsaved->temp_path);

  g_ptr_array_insert (priv->unsaved_files, 0, unsaved);
}

/**
 * ide_unsaved_files_to_array:
 *
 * This retrieves all of the unsaved file buffers known to the context.
 * These are handy if you need to pass modified state to parsers such as
 * clang.
 *
 * Call g_ptr_array_unref() on the resulting #GPtrArray when no longer in use.
 *
 * If you would like to hold onto an unsaved file instance, call
 * ide_unsaved_file_ref() to increment it's reference count.
 *
 * Returns: (transfer container) (element-type IdeUnsavedFile*): A #GPtrArray
 *   containing #IdeUnsavedFile elements.
 */
GPtrArray *
ide_unsaved_files_to_array (IdeUnsavedFiles *self)
{
  IdeUnsavedFilesPrivate *priv;
  GPtrArray *ar;
  gsize i;

  g_return_val_if_fail (IDE_IS_UNSAVED_FILES (self), NULL);

  priv = ide_unsaved_files_get_instance_private (self);

  ar = g_ptr_array_new ();
  g_ptr_array_set_free_func (ar, (GDestroyNotify)ide_unsaved_file_unref);

  for (i = 0; i < priv->unsaved_files->len; i++)
    {
      IdeUnsavedFile *item;
      UnsavedFile *uf;

      uf = g_ptr_array_index (priv->unsaved_files, i);
      item = _ide_unsaved_file_new (uf->file, uf->content, uf->temp_path, uf->sequence);

      g_ptr_array_add (ar, item);
    }

  return ar;
}

gboolean
ide_unsaved_files_contains (IdeUnsavedFiles *self,
                            GFile           *file)
{
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);
  guint i;

  g_return_val_if_fail (IDE_IS_UNSAVED_FILES (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  for (i = 0; i < priv->unsaved_files->len; i++)
    {
      UnsavedFile *uf;

      uf = g_ptr_array_index (priv->unsaved_files, i);

      if (g_file_equal (uf->file, file))
        return TRUE;
    }

  return FALSE;
}

/**
 * ide_unsaved_files_get_unsaved_file:
 *
 * Retrieves the unsaved file content for a particular file. If no unsaved
 * file content is registered, %NULL is returned.
 *
 * Returns: (nullable) (transfer full): An #IdeUnsavedFile or %NULL.
 */
IdeUnsavedFile *
ide_unsaved_files_get_unsaved_file (IdeUnsavedFiles *self,
                                    GFile           *file)
{
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);
  IdeUnsavedFile *ret = NULL;
  gsize i;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_UNSAVED_FILES (self), NULL);

#ifdef IDE_ENABLE_TRACE
  {
    gchar *path;

    path = g_file_get_path (file);
    IDE_TRACE_MSG ("%s", path);
    g_free (path);
  }
#endif

  for (i = 0; i < priv->unsaved_files->len; i++)
    {
      UnsavedFile *uf;

      uf = g_ptr_array_index (priv->unsaved_files, i);

      if (g_file_equal (uf->file, file))
        {
          IDE_TRACE_MSG ("Hit");
          ret = _ide_unsaved_file_new (uf->file, uf->content, uf->temp_path, uf->sequence);
          goto complete;
        }
    }

  IDE_TRACE_MSG ("Miss");

complete:
  IDE_RETURN (ret);
}

gint64
ide_unsaved_files_get_sequence (IdeUnsavedFiles *self)
{
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_UNSAVED_FILES (self), -1);

  return priv->sequence;
}

static void
ide_unsaved_files_finalize (GObject *object)
{
  IdeUnsavedFiles *self = (IdeUnsavedFiles *)object;
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);

  g_clear_pointer (&priv->unsaved_files, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_unsaved_files_parent_class)->finalize (object);
}

static void
ide_unsaved_files_class_init (IdeUnsavedFilesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_unsaved_files_finalize;
}

static void
ide_unsaved_files_init (IdeUnsavedFiles *self)
{
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);

  priv->unsaved_files = g_ptr_array_new_with_free_func (unsaved_file_free);
}

void
ide_unsaved_files_clear (IdeUnsavedFiles *self)
{
  g_autoptr(GPtrArray) ar = NULL;
  gsize i;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (self));

  ar = ide_unsaved_files_to_array (self);

  for (i = 0; i < ar->len; i++)
    {
      IdeUnsavedFile *uf;
      GFile *file;

      uf = g_ptr_array_index (ar, i);
      file = ide_unsaved_file_get_file (uf);
      ide_unsaved_files_remove (self, file);
    }
}
