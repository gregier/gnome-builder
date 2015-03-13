/* ide-context.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-context"

#include <glib/gi18n.h>

#include "ide-async-helper.h"
#include "ide-back-forward-list.h"
#include "ide-buffer-manager.h"
#include "ide-build-system.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-device-manager.h"
#include "ide-global.h"
#include "ide-internal.h"
#include "ide-project.h"
#include "ide-script-manager.h"
#include "ide-search-engine.h"
#include "ide-search-provider.h"
#include "ide-service.h"
#include "ide-source-snippets-manager.h"
#include "ide-unsaved-files.h"
#include "ide-vcs.h"

struct _IdeContext
{
  GObject                   parent_instance;

  IdeBackForwardList       *back_forward_list;
  IdeBufferManager         *buffer_manager;
  IdeBuildSystem           *build_system;
  IdeDeviceManager         *device_manager;
  IdeScriptManager         *script_manager;
  IdeSearchEngine          *search_engine;
  IdeSourceSnippetsManager *snippets_manager;
  IdeProject               *project;
  GFile                    *project_file;
  gchar                    *root_build_dir;
  GHashTable               *services;
  IdeUnsavedFiles          *unsaved_files;
  IdeVcs                   *vcs;

  guint                     services_started : 1;
};

static void async_initable_init (GAsyncInitableIface *);

G_DEFINE_TYPE_EXTENDED (IdeContext, ide_context, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_init))

enum {
  PROP_0,
  PROP_BACK_FORWARD_LIST,
  PROP_BUFFER_MANAGER,
  PROP_BUILD_SYSTEM,
  PROP_DEVICE_MANAGER,
  PROP_PROJECT_FILE,
  PROP_ROOT_BUILD_DIR,
  PROP_SCRIPT_MANAGER,
  PROP_SEARCH_ENGINE,
  PROP_SNIPPETS_MANAGER,
  PROP_VCS,
  PROP_UNSAVED_FILES,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * ide_context_get_back_forward_list:
 *
 * Retrieves the global back forward list for the #IdeContext.
 *
 * Consumers of this should branch the #IdeBackForwardList and merge them
 * when there document stack is closed.
 *
 * See ide_back_forward_list_branch() and ide_back_forward_list_merge() for
 * more information.
 *
 * Returns: (transfer none): An #IdeBackForwardList.
 */
IdeBackForwardList *
ide_context_get_back_forward_list (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->back_forward_list;
}

/**
 * ide_context_get_buffer_manager:
 *
 * Gets the #IdeContext:buffer-manager property. The buffer manager is responsible for loading
 * and saving buffers (files) within the #IdeContext. It provides a convenient place for scripts
 * to hook into the load and save process.
 *
 * Returns: (transfer none): An #IdeBufferManager.
 */
IdeBufferManager *
ide_context_get_buffer_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->buffer_manager;
}

/**
 * ide_context_get_build_system:
 *
 * Fetches the "build-system" property of @context.
 *
 * Returns: (transfer none): An #IdeBuildSystem.
 */
IdeBuildSystem *
ide_context_get_build_system (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->build_system;
}

/**
 * ide_context_get_device_manager:
 *
 * Retrieves the "device-manager" property. The device manager is responsible
 * for connecting and disconnecting to physical or virtual devices within
 * LibIDE.
 *
 * Returns: (transfer none): An #IdeDeviceManager.
 */
IdeDeviceManager *
ide_context_get_device_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->device_manager;
}

/**
 * ide_context_get_root_build_dir:
 *
 * Retrieves the "root-build-dir" for the context. This is the root directory
 * that will contain builds made for various devices.
 *
 * Returns: A string containing the "root-build-dir" property.
 */
const gchar *
ide_context_get_root_build_dir (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->root_build_dir;
}

/**
 * ide_context_set_root_build_dir:
 * @root_build_dir: the path to the root build directory.
 *
 * Sets the "root-build-dir" property. This is the root directory that will
 * be used when building projects for projects that support building out of
 * tree.
 */
void
ide_context_set_root_build_dir (IdeContext  *self,
                                const gchar *root_build_dir)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (root_build_dir);

  if (self->root_build_dir != root_build_dir)
    {
      g_free (self->root_build_dir);
      self->root_build_dir = g_strdup (root_build_dir);
      g_object_notify_by_pspec (G_OBJECT (self),
                                gParamSpecs [PROP_ROOT_BUILD_DIR]);
    }
}

/**
 * ide_context_get_snippets_manager:
 *
 * Gets the #IdeContext:snippets-manager property.
 *
 * Returns: (transfer none): An #IdeSourceSnippetsManager.
 */
IdeSourceSnippetsManager *
ide_context_get_snippets_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->snippets_manager;
}

/**
 * ide_context_get_unsaved_files:
 *
 * Returns the unsaved files for the #IdeContext. These are the contents of
 * open buffers in the IDE.
 *
 * Returns: (transfer none): An #IdeUnsavedFiles.
 */
IdeUnsavedFiles *
ide_context_get_unsaved_files (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->unsaved_files;
}

/**
 * ide_context_get_vcs:
 *
 * Retrieves the #IdeVcs used to load the project. If no version control system
 * could be found, this will return an #IdeDirectoryVcs.
 *
 * Returns: (transfer none): An #IdeVcs.
 */
IdeVcs *
ide_context_get_vcs (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->vcs;
}

static void
ide_context_new_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GAsyncInitable *initable = (GAsyncInitable *)object;
  GError *error = NULL;
  GTask *task = user_data;

  g_return_if_fail (G_IS_ASYNC_INITABLE (initable));
  g_return_if_fail (G_IS_TASK (task));

  object = g_async_initable_new_finish (initable, result, &error);

  if (!object)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, object, g_object_unref);

  g_object_unref (task);
}

void
ide_context_new_async (GFile               *project_file,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (G_IS_FILE (project_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_async_initable_new_async (IDE_TYPE_CONTEXT,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              ide_context_new_cb,
                              g_object_ref (task),
                              "project-file", project_file,
                              NULL);
  g_object_unref (task);
}

/**
 * ide_context_new_finish:
 *
 * Returns: (transfer full): An #IdeContext or %NULL upon failure and
 *   @error is set.
 */
IdeContext *
ide_context_new_finish (GAsyncResult  *result,
                        GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

/**
 * ide_context_get_project:
 *
 * Retrieves the #IdeProject for the context.
 *
 * Returns: (transfer none): An #IdeContext.
 */
IdeProject *
ide_context_get_project (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->project;
}

/**
 * ide_context_get_project_file:
 *
 * Retrieves a #GFile containing the project file that was used to load
 * the context.
 *
 * Returns: (transfer none): A #GFile.
 */
GFile *
ide_context_get_project_file (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->project_file;
}

static void
ide_context_set_project_file (IdeContext *self,
                              GFile      *project_file)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));

  if (project_file != self->project_file)
    {
      if (g_set_object (&self->project_file, project_file))
        g_object_notify_by_pspec (G_OBJECT (self),
                                  gParamSpecs [PROP_PROJECT_FILE]);
    }
}

/**
 * ide_context_get_script_manager:
 *
 * Retrieves the script manager for the context.
 *
 * Returns: (transfer none): An #IdeScriptManager.
 */
IdeScriptManager *
ide_context_get_script_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->script_manager;
}

/**
 * ide_context_get_search_engine:
 *
 * Retrieves the search engine for the context.
 *
 * Returns: (transfer none): An #IdeSearchEngine.
 */
IdeSearchEngine *
ide_context_get_search_engine (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->search_engine;
}

static gpointer
ide_context_create_service (IdeContext *self,
                            GType       service_type)
{
  IdeService *service;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);
  g_return_val_if_fail (g_type_is_a (service_type, IDE_TYPE_SERVICE), NULL);

  service = g_object_new (service_type,
                          "context", self,
                          NULL);

  g_hash_table_insert (self->services,
                       GINT_TO_POINTER (service_type),
                       service);

  if (self->services_started)
    ide_service_start (service);

  return service;
}

/**
 * ide_context_get_service_typed:
 * @service_type: A #GType of the service desired.
 *
 * Retrieves a service matching @service_type. If no match was found, a type
 * implementing the requested service type will be returned. If no matching
 * service type could be found, then an instance of the service will be
 * created, started, and returned.
 *
 * Returns: (transfer none) (nullable): An #IdeService or %NULL.
 */
gpointer
ide_context_get_service_typed (IdeContext *self,
                               GType       service_type)
{
  IdeService *service;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);
  g_return_val_if_fail (g_type_is_a (service_type, IDE_TYPE_SERVICE), NULL);

  service = g_hash_table_lookup (self->services,
                                 GINT_TO_POINTER (service_type));

  if (service)
    return service;

  g_hash_table_iter_init (&iter, self->services);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      IdeService *item = value;

      if (g_type_is_a (G_TYPE_FROM_INSTANCE (item), service_type))
        return item;
    }

  return ide_context_create_service (self, service_type);
}

static void
ide_context_dispose (GObject *object)
{
  IdeContext *self = (IdeContext *)object;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  g_hash_table_iter_init (&iter, self->services);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      IdeService *service = value;

      g_assert (IDE_IS_SERVICE (service));

      if (ide_service_get_running (service))
        ide_service_stop (service);
    }

  G_OBJECT_CLASS (ide_context_parent_class)->dispose (object);

  IDE_EXIT;
}

static void
ide_context_finalize (GObject *object)
{
  IdeContext *self = (IdeContext *)object;

  IDE_ENTRY;

  g_clear_pointer (&self->services, g_hash_table_unref);
  g_clear_pointer (&self->root_build_dir, g_free);

  g_clear_object (&self->build_system);
  g_clear_object (&self->device_manager);
  g_clear_object (&self->project);
  g_clear_object (&self->project_file);
  g_clear_object (&self->unsaved_files);
  g_clear_object (&self->vcs);

  G_OBJECT_CLASS (ide_context_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_context_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeContext *self = IDE_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_BACK_FORWARD_LIST:
      g_value_set_object (value, ide_context_get_back_forward_list (self));
      break;

    case PROP_BUFFER_MANAGER:
      g_value_set_object (value, ide_context_get_buffer_manager (self));
      break;

    case PROP_BUILD_SYSTEM:
      g_value_set_object (value, ide_context_get_build_system (self));
      break;

    case PROP_DEVICE_MANAGER:
      g_value_set_object (value, ide_context_get_device_manager (self));
      break;

    case PROP_PROJECT_FILE:
      g_value_set_object (value, ide_context_get_project_file (self));
      break;

    case PROP_ROOT_BUILD_DIR:
      g_value_set_string (value, ide_context_get_root_build_dir (self));
      break;

    case PROP_SCRIPT_MANAGER:
      g_value_set_object (value, ide_context_get_script_manager (self));
      break;

    case PROP_SEARCH_ENGINE:
      g_value_set_object (value, ide_context_get_search_engine (self));
      break;

    case PROP_SNIPPETS_MANAGER:
      g_value_set_object (value, ide_context_get_snippets_manager (self));
      break;

    case PROP_UNSAVED_FILES:
      g_value_set_object (value, ide_context_get_unsaved_files (self));
      break;

    case PROP_VCS:
      g_value_set_object (value, ide_context_get_vcs (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_context_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  IdeContext *self = IDE_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      ide_context_set_project_file (self, g_value_get_object (value));
      break;

    case PROP_ROOT_BUILD_DIR:
      ide_context_set_root_build_dir (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_context_class_init (IdeContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_context_dispose;
  object_class->finalize = ide_context_finalize;
  object_class->get_property = ide_context_get_property;
  object_class->set_property = ide_context_set_property;

  gParamSpecs [PROP_BACK_FORWARD_LIST] =
    g_param_spec_object ("back-forward-list",
                         _("Back Forward List"),
                         _("Back/forward navigation history for the context."),
                         IDE_TYPE_BACK_FORWARD_LIST,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BACK_FORWARD_LIST,
                                   gParamSpecs [PROP_BACK_FORWARD_LIST]);

  gParamSpecs [PROP_BUFFER_MANAGER] =
    g_param_spec_object ("buffer-manager",
                         _("Buffer Manager"),
                         _("The buffer manager for the context."),
                         IDE_TYPE_BUFFER_MANAGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BUFFER_MANAGER,
                                   gParamSpecs [PROP_BUFFER_MANAGER]);

  gParamSpecs [PROP_BUILD_SYSTEM] =
    g_param_spec_object ("build-system",
                         _("Build System"),
                         _("The build system used by the context."),
                         IDE_TYPE_BUILD_SYSTEM,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BUILD_SYSTEM,
                                   gParamSpecs [PROP_BUILD_SYSTEM]);

  gParamSpecs [PROP_DEVICE_MANAGER] =
    g_param_spec_object ("device-manager",
                         _("Device Manager"),
                         _("The device manager for the context."),
                         IDE_TYPE_DEVICE_MANAGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DEVICE_MANAGER,
                                   gParamSpecs [PROP_DEVICE_MANAGER]);

  gParamSpecs [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         _("Project File"),
                         _("The project file for the context."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PROJECT_FILE,
                                   gParamSpecs [PROP_PROJECT_FILE]);

  gParamSpecs [PROP_ROOT_BUILD_DIR] =
    g_param_spec_string ("root-build-dir",
                         _("Root Build Dir"),
                         _("The root directory to perform builds within."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ROOT_BUILD_DIR,
                                   gParamSpecs [PROP_ROOT_BUILD_DIR]);

  gParamSpecs [PROP_SCRIPT_MANAGER] =
    g_param_spec_object ("script-manager",
                         _("Script Manager"),
                         _("The script manager for the context."),
                         IDE_TYPE_SCRIPT_MANAGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SCRIPT_MANAGER,
                                   gParamSpecs [PROP_SCRIPT_MANAGER]);

  gParamSpecs [PROP_SEARCH_ENGINE] =
    g_param_spec_object ("search-engine",
                         _("Search Engine"),
                         _("The search engine for the context."),
                         IDE_TYPE_SEARCH_ENGINE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SEARCH_ENGINE,
                                   gParamSpecs [PROP_SEARCH_ENGINE]);

  gParamSpecs [PROP_SNIPPETS_MANAGER] =
    g_param_spec_object ("snippets-manager",
                         _("Snippets Manager"),
                         _("The snippets manager for the context."),
                         IDE_TYPE_SOURCE_SNIPPETS_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SNIPPETS_MANAGER,
                                   gParamSpecs [PROP_SNIPPETS_MANAGER]);

  gParamSpecs [PROP_UNSAVED_FILES] =
    g_param_spec_object ("unsaved-files",
                         _("Unsaved Files"),
                         _("The unsaved files in the context."),
                         IDE_TYPE_UNSAVED_FILES,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_UNSAVED_FILES,
                                   gParamSpecs [PROP_UNSAVED_FILES]);

  gParamSpecs [PROP_VCS] =
    g_param_spec_object ("vcs",
                         _("Vcs"),
                         _("The vcs for the context."),
                         IDE_TYPE_VCS,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_VCS,
                                   gParamSpecs [PROP_VCS]);
}

static void
ide_context_init (IdeContext *self)
{
  IDE_ENTRY;

  g_autofree gchar *scriptsdir = NULL;

  self->root_build_dir = g_build_filename (g_get_user_cache_dir (),
                                           ide_get_program_name (),
                                           "builds",
                                           NULL);

  self->back_forward_list = g_object_new (IDE_TYPE_BACK_FORWARD_LIST,
                                          "context", self,
                                          NULL);

  self->buffer_manager = g_object_new (IDE_TYPE_BUFFER_MANAGER,
                                       "context", self,
                                       NULL);

  self->device_manager = g_object_new (IDE_TYPE_DEVICE_MANAGER,
                                       "context", self,
                                       NULL);

  self->project = g_object_new (IDE_TYPE_PROJECT,
                                "context", self,
                                NULL);

  self->services = g_hash_table_new_full (g_direct_hash,
                                          g_direct_equal,
                                          NULL,
                                          g_object_unref);

  self->unsaved_files = g_object_new (IDE_TYPE_UNSAVED_FILES,
                                      "context", self,
                                      NULL);

  self->search_engine = g_object_new (IDE_TYPE_SEARCH_ENGINE,
                                      "context", self,
                                      NULL);

  self->snippets_manager = g_object_new (IDE_TYPE_SOURCE_SNIPPETS_MANAGER,
                                         "context", self,
                                         NULL);

  scriptsdir = g_build_filename (g_get_user_config_dir (),
                                 ide_get_program_name (),
                                 "scripts",
                                 NULL);
  self->script_manager = g_object_new (IDE_TYPE_SCRIPT_MANAGER,
                                       "context", self,
                                       "scripts-directory", scriptsdir,
                                       NULL);

  IDE_EXIT;
}

static void
ide_context_init_project_name_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeContext *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GFileInfo) file_info = NULL;
  GFile *file = (GFile *)object;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (G_IS_TASK (task));

  self = g_task_get_source_object (task);

  file_info = g_file_query_info_finish (file, result, NULL);

  if (file_info &&
      (G_FILE_TYPE_DIRECTORY == g_file_info_get_file_type (file_info)))
    {
      g_autofree gchar *name = NULL;

      name = g_file_get_basename (file);
      _ide_project_set_name (self->project, name);
    }
  else
    {
      g_autoptr(GFile) parent = NULL;
      g_autofree gchar *name = NULL;

      parent = g_file_get_parent (file);
      name = g_file_get_basename (parent);

      _ide_project_set_name (self->project, name);
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_project_name (gpointer             source_object,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (source_object, cancellable, callback, user_data);

  if (!ide_project_get_name (self->project))
    g_file_query_info_async (self->project_file,
                             G_FILE_ATTRIBUTE_STANDARD_TYPE,
                             G_FILE_QUERY_INFO_NONE,
                             G_PRIORITY_DEFAULT,
                             g_task_get_cancellable (task),
                             ide_context_init_project_name_cb,
                             g_object_ref (task));
  else
    g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_vcs_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeContext *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeVcs) vcs = NULL;
  GError *error = NULL;

  g_return_if_fail (G_IS_ASYNC_RESULT (result));
  g_return_if_fail (G_IS_TASK (task));

  self = g_task_get_source_object (task);

  if (!(vcs = ide_vcs_new_finish (result, &error)))
    {
      g_task_return_error (task, error);
      return;
    }

  self->vcs = g_object_ref (vcs);

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_vcs (gpointer             source_object,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  IdeContext *context = source_object;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (context));

  task = g_task_new (source_object, cancellable, callback, user_data);

  ide_vcs_new_async (context,
                     G_PRIORITY_DEFAULT,
                     cancellable,
                     ide_context_init_vcs_cb,
                     g_object_ref (task));
}

static void
ide_context_init_build_system_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr(IdeBuildSystem) build_system = NULL;
  g_autoptr(GTask) task = user_data;
  IdeContext *self;
  GError *error = NULL;

  self = g_task_get_source_object (task);

  if (!(build_system = ide_build_system_new_finish (result, &error)))
    {
      g_task_return_error (task, error);
      return;
    }

  self->build_system = g_object_ref (build_system);

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_build_system (gpointer             source_object,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (self, cancellable, callback, user_data);
  ide_build_system_new_async (self,
                              self->project_file,
                              cancellable,
                              ide_context_init_build_system_cb,
                              g_object_ref (task));
}

static void
ide_context_init_unsaved_files_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeUnsavedFiles *unsaved_files = (IdeUnsavedFiles *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (unsaved_files));

  if (!ide_unsaved_files_restore_finish (unsaved_files, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_unsaved_files (gpointer             source_object,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (self, cancellable, callback, user_data);
  ide_unsaved_files_restore_async (self->unsaved_files,
                                   cancellable,
                                   ide_context_init_unsaved_files_cb,
                                   g_object_ref (task));
}

static void
ide_context_init_scripts_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeScriptManager *manager = (IdeScriptManager *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_SCRIPT_MANAGER (manager));
  g_assert (G_IS_TASK (task));

  if (!ide_script_manager_load_finish (manager, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_scripts (gpointer             source_object,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  ide_script_manager_load_async (self->script_manager,
                                 cancellable,
                                 ide_context_init_scripts_cb,
                                 g_object_ref (task));
}

static void
ide_context_init_snippets_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeSourceSnippetsManager *manager = (IdeSourceSnippetsManager *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPETS_MANAGER (manager));

  if (!ide_source_snippets_manager_load_finish (manager, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_snippets (gpointer             source_object,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (self, cancellable, callback, user_data);

  ide_source_snippets_manager_load_async (self->snippets_manager,
                                          cancellable,
                                          ide_context_init_snippets_cb,
                                          g_object_ref (task));
}

static void
ide_context__back_forward_list_load_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeBackForwardList *back_forward_list = (IdeBackForwardList *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_BACK_FORWARD_LIST (back_forward_list));
  g_assert (G_IS_TASK (task));

  if (!_ide_back_forward_list_load_finish (back_forward_list, result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          g_clear_error (&error);
        }
      else
        {
          g_task_return_error (task, error);
          return;
        }
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_back_forward_list (gpointer             source_object,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) file = NULL;
  const gchar *project_name;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (self, cancellable, callback, user_data);

  project_name = ide_project_get_name (self->project);
  name = g_strdup_printf ("%s.back-forward-list", project_name);
  path = g_build_filename (g_get_user_data_dir (),
                           "gnome-builder",
                           g_strdelimit (name, " \t\n", '_'),
                           NULL);
  file = g_file_new_for_path (path);

  IDE_TRACE_MSG ("Loading %s", path);

  _ide_back_forward_list_load_async (self->back_forward_list,
                                     file,
                                     cancellable,
                                     ide_context__back_forward_list_load_cb,
                                     g_object_ref (task));

  IDE_EXIT;
}

static void
ide_context_init_services (gpointer             source_object,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GIOExtensionPoint *point;
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;
  const GList *extensions;
  const GList *iter;
  GHashTableIter hiter;
  gpointer v;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (self, cancellable, callback, user_data);
  point = g_io_extension_point_lookup (IDE_SERVICE_EXTENSION_POINT);
  extensions = g_io_extension_point_get_extensions (point);

  for (iter = extensions; iter; iter = iter->next)
    {
      GIOExtension *extension = iter->data;
      IdeService *service;
      GType type_id;

      type_id = g_io_extension_get_type (extension);

      if (!g_type_is_a (type_id, IDE_TYPE_SERVICE))
        {
          g_warning (_("\"%s\" is not a service, ignoring extension point."),
                     g_type_name (type_id));
          continue;
        }

      service = ide_context_get_service_typed (self, type_id);

      if (!service)
        {
          g_warning (_("Failed to create service of type \"%s\"."),
                     g_type_name (type_id));
          continue;
        }

      g_debug (_("Service of type \"%s\" registered."), g_type_name (type_id));
    }

  self->services_started = TRUE;

  g_hash_table_iter_init (&hiter, self->services);

  while (g_hash_table_iter_next (&hiter, NULL, &v))
    {
      IdeService *service = v;

      if (!ide_service_get_running (service))
        ide_service_start (service);
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_search_engine (gpointer             source_object,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeContext *self = source_object;
  GIOExtensionPoint *point;
  const GList *iter;
  const GList *list;

  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  point = g_io_extension_point_lookup (IDE_SEARCH_PROVIDER_EXTENSION_POINT);

  if (!point)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("Missing extension point for %s"),
                               IDE_SEARCH_PROVIDER_EXTENSION_POINT);
      return;
    }

  list = g_io_extension_point_get_extensions (point);

  for (iter = list; iter; iter = iter->next)
    {
      GIOExtension *extension = iter->data;
      IdeSearchProvider *provider;
      GType gtype;

      gtype = g_io_extension_get_type (extension);

      if (!g_type_is_a (gtype, IDE_TYPE_SEARCH_PROVIDER))
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_DATA,
                                   _("%s is not an IdeSearchProvider."),
                                   g_type_name (gtype));
          return;
        }

      provider = g_object_new (gtype, "context", self, NULL);
      ide_search_engine_add_provider (self->search_engine, provider);
      g_object_unref (provider);
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_async (GAsyncInitable      *initable,
                        int                  io_priority,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  IdeContext *context = (IdeContext *)initable;

  g_return_if_fail (G_IS_ASYNC_INITABLE (context));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_async_helper_run (context,
                        cancellable,
                        callback,
                        user_data,
                        ide_context_init_services,
                        ide_context_init_build_system,
                        ide_context_init_vcs,
                        ide_context_init_project_name,
                        ide_context_init_back_forward_list,
                        ide_context_init_unsaved_files,
                        ide_context_init_search_engine,
                        ide_context_init_snippets,
                        ide_context_init_scripts,
                        NULL);

  /* TODO: Restore buffer state? */
}

static gboolean
ide_context_init_finish (GAsyncInitable  *initable,
                         GAsyncResult    *result,
                         GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_CONTEXT (initable), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
async_initable_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_context_init_async;
  iface->init_finish = ide_context_init_finish;
}

static void
ide_context_unload__unsaved_files_save_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeUnsavedFiles *unsaved_files = (IdeUnsavedFiles *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  if (!ide_unsaved_files_save_finish (unsaved_files, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

/**
 * ide_context_unload_async:
 *
 * This function attempts to unload various components in the #IdeContext. This should be called
 * before you dispose the context. Unsaved buffers will be persisted to the drafts directory.
 * More operations may be added in the future.
 */
void
ide_context_unload_async (IdeContext          *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  IdeUnsavedFiles *unsaved_files;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  unsaved_files = ide_context_get_unsaved_files (self);

  ide_unsaved_files_save_async (unsaved_files,
                                cancellable,
                                ide_context_unload__unsaved_files_save_cb,
                                g_object_ref (task));

  /*
   * TODO:
   *
   * Detach devices?
   * Anything else?
   */

  IDE_EXIT;
}

gboolean
ide_context_unload_finish (IdeContext    *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  GTask *task = (GTask *)result;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), FALSE);

  ret = g_task_propagate_boolean (task, error);

  IDE_RETURN (ret);
}
