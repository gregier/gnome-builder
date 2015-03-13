/* ide-internal.h
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

#ifndef IDE_PRIVATE_H
#define IDE_PRIVATE_H

#include <clang-c/Index.h>
#include <gtksourceview/gtksource.h>

#include "ide-back-forward-item.h"
#include "ide-back-forward-list.h"
#include "ide-clang-translation-unit.h"
#include "ide-diagnostic.h"
#include "ide-types.h"
#include "ide-source-view.h"
#include "ide-source-view-mode.h"

G_BEGIN_DECLS

void                _ide_back_forward_list_load_async  (IdeBackForwardList    *self,
                                                        GFile                 *file,
                                                        GCancellable          *cancellable,
                                                        GAsyncReadyCallback    callback,
                                                        gpointer               user_data);
gboolean            _ide_back_forward_list_load_finish (IdeBackForwardList    *self,
                                                        GAsyncResult          *result,
                                                        GError               **error);
void                _ide_back_forward_list_save_async  (IdeBackForwardList    *self,
                                                        GFile                 *file,
                                                        GCancellable          *cancellable,
                                                        GAsyncReadyCallback    callback,
                                                        gpointer               user_data);
gboolean            _ide_back_forward_list_save_finish (IdeBackForwardList    *self,
                                                        GAsyncResult          *result,
                                                        GError               **error);
IdeBackForwardItem *_ide_back_forward_list_find        (IdeBackForwardList    *self,
                                                        IdeFile               *file);
void                _ide_diagnostic_add_range          (IdeDiagnostic         *self,
                                                        IdeSourceRange        *range);
IdeDiagnostic      *_ide_diagnostic_new                (IdeDiagnosticSeverity  severity,
                                                        const gchar           *text,
                                                        IdeSourceLocation     *location);
void                _ide_diagnostic_take_range         (IdeDiagnostic         *self,
                                                        IdeSourceRange        *range);
void                _ide_diagnostician_add_provider    (IdeDiagnostician      *self,
                                                        IdeDiagnosticProvider *provider);
void                _ide_diagnostician_remove_provider (IdeDiagnostician      *self,
                                                        IdeDiagnosticProvider *provider);
IdeDiagnostics     *_ide_diagnostics_new               (GPtrArray             *ar);
GtkSourceFile      *_ide_file_get_source_file          (IdeFile               *file);
void                _ide_project_set_name              (IdeProject            *project,
                                                        const gchar           *name);
void                _ide_search_context_add_provider   (IdeSearchContext      *context,
                                                        IdeSearchProvider     *provider,
                                                        gsize                  max_results);
IdeSourceRange     *_ide_source_range_new              (IdeSourceLocation     *begin,
                                                        IdeSourceLocation     *end);
gboolean            _ide_source_view_mode_do_event     (IdeSourceViewMode     *mode,
                                                        GdkEventKey           *event,
                                                        gboolean              *remove);
IdeSourceViewMode  *_ide_source_view_mode_new          (GtkWidget             *view,
                                                        const char            *mode,
                                                        IdeSourceViewModeType  type);
void                _ide_source_view_set_count         (IdeSourceView         *self,
                                                        guint                  count);
void                _ide_source_view_set_modifier      (IdeSourceView         *self,
                                                        gunichar               modifier);
IdeUnsavedFile     *_ide_unsaved_file_new              (GFile                 *file,
                                                        GBytes                *content,
                                                        const gchar           *temp_path,
                                                        gint64                 sequence);

G_END_DECLS

#endif /* IDE_PRIVATE_H */
