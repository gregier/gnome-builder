/* ide-project.c
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

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-file.h"
#include "ide-project.h"
#include "ide-project-files.h"
#include "ide-project-item.h"
#include "ide-vcs.h"

struct _IdeProject
{
  IdeObject       parent_instance;

  GRWLock         rw_lock;
  IdeProjectItem *root;
  gchar          *name;
};

typedef struct
{
  GFile *orig_file;
  GFile *new_file;
} RenameFile;

G_DEFINE_TYPE (IdeProject, ide_project, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NAME,
  PROP_ROOT,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

void
ide_project_reader_lock (IdeProject *self)
{
  g_return_if_fail (IDE_IS_PROJECT (self));

  g_rw_lock_reader_lock (&self->rw_lock);
}

void
ide_project_reader_unlock (IdeProject *self)
{
  g_return_if_fail (IDE_IS_PROJECT (self));

  g_rw_lock_reader_unlock (&self->rw_lock);
}

void
ide_project_writer_lock (IdeProject *self)
{
  g_return_if_fail (IDE_IS_PROJECT (self));

  g_rw_lock_writer_lock (&self->rw_lock);
}

void
ide_project_writer_unlock (IdeProject *self)
{
  g_return_if_fail (IDE_IS_PROJECT (self));

  g_rw_lock_writer_unlock (&self->rw_lock);
}

const gchar *
ide_project_get_name (IdeProject *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT (self), NULL);

  return self->name;
}

void
_ide_project_set_name (IdeProject  *self,
                       const gchar *name)
{
  g_return_if_fail (IDE_IS_PROJECT (self));

  if (self->name != name)
    {
      g_free (self->name);
      self->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_NAME]);
    }
}

/**
 * ide_project_get_root:
 *
 * Retrieves the root item of the project tree.
 *
 * You must be holding the reader lock while calling and using the result of
 * this function. Other thread may be accessing or modifying the tree without
 * your knowledge. See ide_project_reader_lock() and ide_project_reader_unlock()
 * for more information.
 *
 * If you need to modify the tree, you must hold a writer lock that has been
 * acquired with ide_project_writer_lock() and released with
 * ide_project_writer_unlock() when you are no longer modifiying the tree.
 *
 * Returns: (transfer none): An #IdeProjectItem.
 */
IdeProjectItem *
ide_project_get_root (IdeProject *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT (self),  NULL);

  return self->root;
}

static void
ide_project_set_root (IdeProject     *self,
                      IdeProjectItem *root)
{
  g_autoptr(IdeProjectItem) allocated = NULL;
  IdeContext *context;

  g_return_if_fail (IDE_IS_PROJECT (self));
  g_return_if_fail (!root || IDE_IS_PROJECT_ITEM (root));

  context = ide_object_get_context (IDE_OBJECT (self));

  if (!root)
    {
      allocated = g_object_new (IDE_TYPE_PROJECT_ITEM,
                                "context", context,
                                NULL);
      root = allocated;
    }

  if (g_set_object (&self->root, root))
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_ROOT]);
}

/**
 * ide_project_get_files:
 * @self: A #IdeProject.
 *
 * Gets the #IdeProjectFiles instance within the project tree.
 * If it has not yet been loaded, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): An #IdeProjectFiles or %NULL.
 */
IdeProjectFiles *
ide_project_get_files (IdeProject *self)
{
  GSequenceIter *iter;
  GSequence *children;
  IdeProjectItem *root;

  g_return_val_if_fail (IDE_IS_PROJECT (self), NULL);

  root = ide_project_get_root (self);
  children = ide_project_item_get_children (root);

  if (children == NULL)
    return NULL;

  for (iter = g_sequence_get_begin_iter (children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      IdeProjectItem *item = g_sequence_get (iter);

      if (IDE_IS_PROJECT_FILES (item))
        return IDE_PROJECT_FILES (item);
    }

  return NULL;
}

/**
 * ide_project_get_file_for_path:
 * @path: A relative path from the project root.
 *
 * Retrieves an #IdeFile for the path specified. #IdeFile provides access to
 * language specific features via ide_file_get_language().
 *
 * You must hold the reader lock while calling this function. See
 * ide_project_reader_lock() and ide_project_reader_unlock() for more
 * information.
 *
 * Returns: (transfer full) (nullable): An #IdeFile or %NULL if no matching
 *   file could be found.
 */
IdeFile *
ide_project_get_file_for_path (IdeProject  *self,
                               const gchar *path)
{
  IdeProjectFiles *files;
  IdeFile *file = NULL;

  g_return_val_if_fail (IDE_IS_PROJECT (self), NULL);
  g_return_val_if_fail (path, NULL);

  ide_project_reader_lock (self);

  files = ide_project_get_files (self);
  if (files != NULL)
    file = ide_project_files_get_file_for_path (files, path);

  ide_project_reader_unlock (self);

  if (!file)
    {
      IdeContext *context;
      IdeVcs *vcs;
      GFile *workdir;
      g_autoptr(GFile) gfile = NULL;

      /*
       * Okay, we couldn't find an existing item that matches this path, so let's
       * synthesize one (but not add it to the tree). This could be hit in common
       * cases like new files that are not yet added to the project.
       */
      context = ide_object_get_context (IDE_OBJECT (self));
      g_assert (IDE_IS_CONTEXT (context));
      vcs = ide_context_get_vcs (context);
      workdir = ide_vcs_get_working_directory (vcs);
      gfile = g_file_get_child (workdir, path);
      file = g_object_new (IDE_TYPE_FILE,
                           "context", context,
                           "path", path,
                           "file", gfile,
                           NULL);
    }

  return file;
}

/**
 * ide_project_get_project_file:
 * @self: A #IdeProject.
 * @gfile: A #GFile.
 *
 * Gets a new #IdeFile representing @gfile.
 *
 * Returns: (transfer full): An #IdeFile.
 */
IdeFile *
ide_project_get_project_file (IdeProject *self,
                              GFile      *gfile)
{
  g_autofree gchar *relpath = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  g_return_val_if_fail (IDE_IS_PROJECT (self), NULL);
  g_return_val_if_fail (G_IS_FILE (gfile), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  relpath = g_file_get_relative_path (workdir, gfile);
  if (relpath)
    return ide_project_get_file_for_path (self, relpath);

  return g_object_new (IDE_TYPE_FILE,
                       "context", context,
                       "path", g_file_get_path (gfile),
                       "file", gfile,
                       NULL);
}

void
ide_project_add_file (IdeProject     *self,
                      IdeProjectFile *file)
{
  IdeProjectFiles *files;

  g_return_if_fail (IDE_IS_PROJECT (self));
  g_return_if_fail (IDE_IS_PROJECT_FILE (file));


  ide_project_writer_lock (self);
  files = ide_project_get_files (self);
  ide_project_files_add_file (files, file);
  ide_project_writer_unlock (self);
}

static void
ide_project_finalize (GObject *object)
{
  IdeProject *self = (IdeProject *)object;

  g_clear_object (&self->root);
  g_clear_pointer (&self->name, g_free);
  g_rw_lock_clear (&self->rw_lock);

  G_OBJECT_CLASS (ide_project_parent_class)->finalize (object);
}

static void
ide_project_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeProject *self = IDE_PROJECT (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, ide_project_get_name (self));
      break;

    case PROP_ROOT:
      g_value_set_object (value, ide_project_get_root (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  IdeProject *self = IDE_PROJECT (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      ide_project_set_root (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_class_init (IdeProjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_project_finalize;
  object_class->get_property = ide_project_get_property;
  object_class->set_property = ide_project_set_property;

  gParamSpecs [PROP_NAME] =
    g_param_spec_string ("name",
                         _("Name"),
                         _("The name of the project."),
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_NAME,
                                   gParamSpecs [PROP_NAME]);

  gParamSpecs [PROP_ROOT] =
    g_param_spec_object ("root",
                         _("Root"),
                         _("The root object for the project."),
                         IDE_TYPE_PROJECT_ITEM,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ROOT,
                                   gParamSpecs [PROP_ROOT]);
}

static void
ide_project_init (IdeProject *self)
{
  g_rw_lock_init (&self->rw_lock);
}

static void
ide_project_rename_file_worker (GTask        *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  IdeProject *self = source_object;
  IdeProjectFiles *files;
  IdeProjectItem *item;
  IdeContext *context;
  IdeVcs *vcs;
  RenameFile *op = task_data;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autofree gchar *path = NULL;
  GError *error = NULL;
  GFile *workdir;

  g_assert (IDE_IS_PROJECT (self));
  g_assert (op != NULL);
  g_assert (G_IS_FILE (op->orig_file));
  g_assert (G_IS_FILE (op->new_file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_project_writer_lock (self);

  files = ide_project_get_files (self);
  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  path = g_file_get_relative_path (workdir, op->new_file);

#ifdef IDE_ENABLE_TRACE
  {
    gchar *old_path = g_file_get_uri (op->orig_file);
    gchar *new_path = g_file_get_uri (op->new_file);
    IDE_TRACE_MSG ("Renaming %s to %s", old_path, new_path);
    g_free (old_path);
    g_free (new_path);
  }
#endif

  if (path == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               _("Destination file must be within the project tree."));
      goto cleanup;
    }

  item = ide_project_files_find_file (files, op->orig_file);

  if (item == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               _("Source file must be within the project tree."));
      goto cleanup;
    }

  if (!g_file_move (op->orig_file, op->new_file, G_FILE_COPY_NONE, cancellable, NULL, NULL, &error))
    {
      g_task_return_error (task, error);
      goto cleanup;
    }

  file_info = g_file_query_info (op->new_file,
                                 G_FILE_ATTRIBUTE_STANDARD_NAME","
                                 G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
                                 G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                 G_FILE_QUERY_INFO_NONE,
                                 cancellable,
                                 &error);

  if (file_info == NULL)
    {
      g_task_return_error (task, error);
      goto cleanup;
    }

  g_object_ref (item);
  ide_project_item_remove (ide_project_item_get_parent (item), item);
  g_object_set (item,
                "file", op->new_file,
                "path", path,
                "file-info", file_info,
                NULL);
  ide_project_files_add_file (files, IDE_PROJECT_FILE (item));
  g_object_unref (item);

  g_task_return_boolean (task, TRUE);

cleanup:
  ide_project_writer_unlock (self);
}

static void
rename_file_free (gpointer data)
{
  RenameFile *op = data;

  if (op != NULL)
    {
      g_object_unref (op->new_file);
      g_object_unref (op->orig_file);
      g_slice_free (RenameFile, op);
    }
}

void
ide_project_rename_file_async (IdeProject          *self,
                               GFile               *orig_file,
                               GFile               *new_file,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  RenameFile *op;

  g_return_if_fail (IDE_IS_PROJECT (self));
  g_return_if_fail (G_IS_FILE (orig_file));
  g_return_if_fail (G_IS_FILE (new_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  op = g_slice_new0 (RenameFile);
  op->orig_file = g_object_ref (orig_file);
  op->new_file = g_object_ref (new_file);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, op, rename_file_free);
  g_task_run_in_thread (task, ide_project_rename_file_worker);
}

gboolean
ide_project_rename_file_finish (IdeProject    *self,
                                GAsyncResult  *result,
                                GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_PROJECT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
ide_project_trash_file__file_trash_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GTask) task = user_data;
  IdeProject *self;
  IdeProjectFiles *files;
  IdeProjectItem *item;
  GError *error = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_PROJECT (self));

  if (!g_file_trash_finish (file, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  ide_project_writer_lock (self);
  files = ide_project_get_files (self);
  item = ide_project_files_find_file (files, file);
  if (item != NULL)
    ide_project_item_remove (ide_project_item_get_parent (item), item);
  ide_project_writer_unlock (self);

  g_task_return_boolean (task, TRUE);
}

static gboolean
file_is_ancestor (GFile *file,
                  GFile *maybe_child)
{
  gchar *path;
  gboolean ret;

  path = g_file_get_relative_path (file, maybe_child);
  ret = (path != NULL);
  g_free (path);

  return ret;
}

void
ide_project_trash_file_async (IdeProject          *self,
                              GFile               *file,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_PROJECT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  task = g_task_new (self, cancellable, callback, user_data);

  if (!file_is_ancestor (workdir, file))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               _("File must be within the project tree."));
      IDE_EXIT;
    }

  g_file_trash_async (file,
                      G_PRIORITY_DEFAULT,
                      cancellable,
                      ide_project_trash_file__file_trash_cb,
                      g_object_ref (task));

  IDE_EXIT;
}

gboolean
ide_project_trash_file_finish (IdeProject    *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  GTask *task = (GTask *)result;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_PROJECT (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  ret = g_task_propagate_boolean (task, error);

  IDE_RETURN (ret);
}
