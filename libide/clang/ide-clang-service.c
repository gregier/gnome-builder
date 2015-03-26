/* ide-clang-service.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "gb-clang-service"

#include <clang-c/Index.h>
#include <glib/gi18n.h>

#include "ide-build-system.h"
#include "ide-clang-private.h"
#include "ide-clang-service.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-file.h"
#include "ide-highlight-index.h"
#include "ide-unsaved-file.h"
#include "ide-unsaved-files.h"

struct _IdeClangService
{
  IdeService    parent_instance;

  GHashTable   *cached_units;
  GRWLock       cached_rwlock;
  CXIndex       index;
  GCancellable *cancellable;
};

typedef struct
{
  IdeFile    *file;
  CXIndex     index;
  gchar      *source_filename;
  gchar     **command_line_args;
  GPtrArray  *unsaved_files;
  gint64      sequence;
  guint       options;
} ParseRequest;

typedef struct
{
  IdeHighlightIndex *index;
  CXFile             file;
  const gchar       *filename;
} IndexRequest;

G_DEFINE_TYPE (IdeClangService, ide_clang_service, IDE_TYPE_SERVICE)

static void
parse_request_free (gpointer data)
{
  ParseRequest *request = data;

  g_free (request->source_filename);
  g_strfreev (request->command_line_args);
  g_ptr_array_unref (request->unsaved_files);
  g_clear_object (&request->file);
  g_slice_free (ParseRequest, request);
}

static enum CXChildVisitResult
ide_clang_service_build_index_visitor (CXCursor     cursor,
                                       CXCursor     parent,
                                       CXClientData user_data)
{
  IndexRequest *request = user_data;
  enum CXCursorKind kind;
  const gchar *word;
  CXString cxstr;

  g_assert (request != NULL);

  kind = clang_getCursorKind (cursor);

  switch (kind)
    {
    case CXCursor_TypedefDecl:
    case CXCursor_TypeAliasDecl:
      cxstr = clang_getCursorSpelling (cursor);
      word = clang_getCString (cxstr);
      ide_highlight_index_insert (request->index, word, IDE_HIGHLIGHT_KIND_TYPE_NAME);
      break;

    case CXCursor_FunctionDecl:
      cxstr = clang_getCursorSpelling (cursor);
      word = clang_getCString (cxstr);
      ide_highlight_index_insert (request->index, word, IDE_HIGHLIGHT_KIND_FUNCTION_NAME);
      break;

    case CXCursor_MacroDefinition:
    case CXCursor_MacroExpansion:
      cxstr = clang_getCursorSpelling (cursor);
      word = clang_getCString (cxstr);
      ide_highlight_index_insert (request->index, word, IDE_HIGHLIGHT_KIND_MACRO_NAME);
      break;

    default:
      break;
    }

  return CXChildVisit_Continue;
}

static IdeHighlightIndex *
ide_clang_service_build_index (IdeClangService   *self,
                               CXTranslationUnit  tu,
                               ParseRequest      *request)
{
  IdeHighlightIndex *index;
  IndexRequest client_data;
  CXCursor cursor;
  CXFile file;

  g_assert (IDE_IS_CLANG_SERVICE (self));
  g_assert (tu != NULL);
  g_assert (request != NULL);

  file = clang_getFile (tu, request->source_filename);
  if (file == NULL)
    return NULL;

  index = ide_highlight_index_new ();

  client_data.index = index;
  client_data.file = file;
  client_data.filename = request->source_filename;

  cursor = clang_getTranslationUnitCursor (tu);
  clang_visitChildren (cursor, ide_clang_service_build_index_visitor, &client_data);

  return index;
}

static void
ide_clang_service_parse_worker (GTask        *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  g_autoptr(IdeClangTranslationUnit) ret = NULL;
  g_autoptr(IdeHighlightIndex) index = NULL;
  IdeClangService *self = source_object;
  CXTranslationUnit tu = NULL;
  ParseRequest *request = task_data;
  IdeContext *context;
  const gchar * const *argv;
  GFile *gfile;
  gsize argc = 0;
  const gchar *detail_error = NULL;
  enum CXErrorCode code;
  GArray *ar;
  gsize i;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_CLANG_SERVICE (source_object));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ar = g_array_new (FALSE, FALSE, sizeof (struct CXUnsavedFile));

  for (i = 0; i < request->unsaved_files->len; i++)
    {
      IdeUnsavedFile *iuf = g_ptr_array_index (request->unsaved_files, i);
      struct CXUnsavedFile uf;
      GBytes *content;
      GFile *file;

      file = ide_unsaved_file_get_file (iuf);
      content = ide_unsaved_file_get_content (iuf);

      uf.Filename = g_file_get_path (file);
      uf.Contents = g_bytes_get_data (content, NULL);
      uf.Length = g_bytes_get_size (content);

      g_array_append_val (ar, uf);
    }

  argv = (const gchar * const *)request->command_line_args;
  argc = argv ? g_strv_length (request->command_line_args) : 0;

  code = clang_parseTranslationUnit2 (request->index,
                                      request->source_filename,
                                      argv, argc,
                                      (struct CXUnsavedFile *)ar->data,
                                      ar->len,
                                      request->options,
                                      &tu);

  switch (code)
    {
    case CXError_Success:
      index = ide_clang_service_build_index (self, tu, request);
#ifndef IDE_DISABLE_TRACE
      ide_highlight_index_dump (index);
#endif
      break;

    case CXError_Failure:
      detail_error = _("Unknown failure");
      break;

    case CXError_Crashed:
      detail_error = _("Clang crashed");
      break;

    case CXError_InvalidArguments:
      detail_error = _("Invalid arguments");
      break;

    case CXError_ASTReadError:
      detail_error = _("AST read error");
      break;

    default:
      break;
    }

  if (!tu)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Failed to create translation unit: %s"),
                               detail_error ? detail_error : "");
      goto cleanup;
    }

  context = ide_object_get_context (source_object);
  gfile = ide_file_get_file (request->file);
  ret = _ide_clang_translation_unit_new (context, tu, gfile, index, request->sequence);

  g_rw_lock_writer_lock (&self->cached_rwlock);
  g_hash_table_replace (self->cached_units,
                        g_object_ref (request->file),
                        g_object_ref (ret));
  g_rw_lock_writer_unlock (&self->cached_rwlock);

  g_task_return_pointer (task, g_object_ref (ret), g_object_unref);

cleanup:
  g_array_unref (ar);
}

static void
ide_clang_service__get_build_flags_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(GTask) task = user_data;
  ParseRequest *request;
  gchar **argv;
  GError *error = NULL;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_TASK (task));

  request = g_task_get_task_data (task);

  argv = ide_build_system_get_build_flags_finish (build_system, result, &error);

  if (!argv)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        g_message ("%s", error->message);
      g_clear_error (&error);
      argv = g_new0 (gchar*, 1);
    }

  request->command_line_args = argv;

#ifndef IDE_DISABLE_TRACE
  {
    gchar *cflags;

    cflags = g_strjoinv (" ", argv);
    IDE_TRACE_MSG ("CFLAGS = %s", cflags);
    g_free (cflags);
  }
#endif

  g_task_run_in_thread (task, ide_clang_service_parse_worker);
}

/**
 * ide_clang_service_get_translation_unit_async:
 * @min_sequence: The minimum change sequence number to reuse a cached unit.
 *
 * This function is used to asynchronously retrieve the translation unit for
 * a particular file.
 *
 * If the translation unit is up to date, then no parsing will occur and the
 * existing translation unit will be used.
 *
 * If the translation unit is out of date, then the source file(s) will be
 * parsed via clang_parseTranslationUnit() asynchronously.
 */
void
ide_clang_service_get_translation_unit_async (IdeClangService     *self,
                                              IdeFile             *file,
                                              gint64               min_sequence,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  g_autoptr(IdeClangTranslationUnit) cached = NULL;
  IdeUnsavedFiles *unsaved_files;
  IdeBuildSystem *build_system;
  IdeContext *context;
  g_autoptr(GTask) task = NULL;
  ParseRequest *request;
  const gchar *path;
  GFile *gfile;

  g_return_if_fail (IDE_IS_CLANG_SERVICE (self));

  task = g_task_new (self, cancellable, callback, user_data);
  context = ide_object_get_context (IDE_OBJECT (self));
  unsaved_files = ide_context_get_unsaved_files (context);
  build_system = ide_context_get_build_system (context);

  g_rw_lock_reader_lock (&self->cached_rwlock);
  cached = g_hash_table_lookup (self->cached_units, file);
  if (cached)
    g_object_ref (cached);
  g_rw_lock_reader_unlock (&self->cached_rwlock);

  if (min_sequence <= 0)
    min_sequence = ide_unsaved_files_get_sequence (unsaved_files);

  if (cached)
    {
      if (ide_clang_translation_unit_get_sequence (cached) >= min_sequence)
        {
          g_task_return_pointer (task, g_object_ref (cached), g_object_unref);
          return;
        }
    }

  gfile = ide_file_get_file (file);

  if (!gfile || !(path = g_file_get_path (gfile)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("File must be saved locally to parse."));
      return;
    }

  request = g_slice_new0 (ParseRequest);
  request->file = g_object_ref (file);
  request->index = self->index;
  request->source_filename = g_strdup (path);
  request->command_line_args = NULL;
  request->unsaved_files = ide_unsaved_files_get_unsaved_files (unsaved_files);
  request->sequence = ide_unsaved_files_get_sequence (unsaved_files);
  request->options = clang_defaultEditingTranslationUnitOptions ();

  g_task_set_task_data (task, request, parse_request_free);

  /*
   * Request the build flags necessary to build this module from the build system.
   */

  ide_build_system_get_build_flags_async (build_system,
                                          file,
                                          cancellable,
                                          ide_clang_service__get_build_flags_cb,
                                          g_object_ref (task));
}

/**
 * ide_clang_service_get_translation_unit_finish:
 *
 * Completes an asychronous request to get a translation unit for a given file.
 * See ide_clang_service_get_translation_unit_async() for more information.
 *
 * Returns: (transfer full): An #IdeClangTranslationUnit or %NULL up on failure.
 */
IdeClangTranslationUnit *
ide_clang_service_get_translation_unit_finish (IdeClangService  *self,
                                               GAsyncResult     *result,
                                               GError          **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_CLANG_SERVICE (self), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_clang_service_start (IdeService *service)
{
  IdeClangService *self = (IdeClangService *)service;

  g_return_if_fail (IDE_IS_CLANG_SERVICE (self));
  g_return_if_fail (!self->index);

  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  self->index = clang_createIndex (0, 0);
  clang_CXIndex_setGlobalOptions (self->index,
                                  CXGlobalOpt_ThreadBackgroundPriorityForAll);

  IDE_SERVICE_CLASS (ide_clang_service_parent_class)->start (service);
}

static void
ide_clang_service_stop (IdeService *service)
{
  IdeClangService *self = (IdeClangService *)service;

  g_return_if_fail (IDE_IS_CLANG_SERVICE (self));
  g_return_if_fail (!self->index);

  g_cancellable_cancel (self->cancellable);

  IDE_SERVICE_CLASS (ide_clang_service_parent_class)->start (service);
}

static void
ide_clang_service_dispose (GObject *object)
{
  IdeClangService *self = (IdeClangService *)object;

  g_clear_pointer (&self->index, clang_disposeIndex);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (ide_clang_service_parent_class)->dispose (object);
}

static void
ide_clang_service_class_init (IdeClangServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeServiceClass *service_class = IDE_SERVICE_CLASS (klass);

  object_class->dispose = ide_clang_service_dispose;

  service_class->start = ide_clang_service_start;
  service_class->stop = ide_clang_service_stop;
}

static void
ide_clang_service_init (IdeClangService *self)
{
  g_rw_lock_init (&self->cached_rwlock);

  self->cached_units = g_hash_table_new_full ((GHashFunc)ide_file_hash,
                                              (GEqualFunc)ide_file_equal,
                                              g_object_unref,
                                              g_object_unref);
}

/**
 * ide_clang_service_get_cached_translation_unit:
 * @self: A #IdeClangService.
 *
 * Gets a cached translation unit if one exists for the file.
 *
 * Returns: (transfer full) (nullable): An #IdeClangTranslationUnit or %NULL.
 */
IdeClangTranslationUnit *
ide_clang_service_get_cached_translation_unit (IdeClangService *self,
                                               IdeFile         *file)
{
  IdeClangTranslationUnit *cached;

  g_return_val_if_fail (IDE_IS_CLANG_SERVICE (self), NULL);
  g_return_val_if_fail (IDE_IS_FILE (file), NULL);

  g_rw_lock_reader_lock (&self->cached_rwlock);
  cached = g_hash_table_lookup (self->cached_units, file);
  if (cached)
    g_object_ref (cached);
  g_rw_lock_reader_unlock (&self->cached_rwlock);

  return cached;
}
