/* ide-highlight-index.h
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

#ifndef IDE_HIGHLIGHT_INDEX_H
#define IDE_HIGHLIGHT_INDEX_H

#include "ide-highlighter.h"

G_BEGIN_DECLS

#define IDE_TYPE_HIGHLIGHT_INDEX (ide_highlight_index_get_type())

typedef struct _IdeHighlightIndex IdeHighlightIndex;

GType              ide_highlight_index_get_type (void);
IdeHighlightIndex *ide_highlight_index_new      (void);
IdeHighlightIndex *ide_highlight_index_ref      (IdeHighlightIndex *self);
void               ide_highlight_index_unref    (IdeHighlightIndex *self);
void               ide_highlight_index_insert   (IdeHighlightIndex *self,
                                                 const gchar       *word,
                                                 IdeHighlightKind   kind);
IdeHighlightKind   ide_highlight_index_lookup   (IdeHighlightIndex *self,
                                                 const gchar       *word);
void               ide_highlight_index_dump     (IdeHighlightIndex *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeHighlightIndex, ide_highlight_index_unref)

G_END_DECLS

#endif /* IDE_HIGHLIGHT_INDEX_H */
