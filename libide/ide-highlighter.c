/* ide-highlighter.c
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

#include "ide-highlighter.h"

G_DEFINE_ABSTRACT_TYPE (IdeHighlighter, ide_highlighter, IDE_TYPE_OBJECT)

static void
ide_highlighter_class_init (IdeHighlighterClass *klass)
{
}

static void
ide_highlighter_init (IdeHighlighter *self)
{
}

IdeHighlightKind
ide_highlighter_next (IdeHighlighter    *self,
                      const GtkTextIter *range_begin,
                      const GtkTextIter *range_end,
                      GtkTextIter       *match_begin,
                      GtkTextIter       *match_end)
{
  g_return_val_if_fail (IDE_IS_HIGHLIGHTER (self), 0);
  g_return_val_if_fail (range_begin, 0);
  g_return_val_if_fail (range_end, 0);
  g_return_val_if_fail (match_begin, 0);
  g_return_val_if_fail (match_end, 0);

  if (IDE_HIGHLIGHTER_GET_CLASS (self)->next)
    return IDE_HIGHLIGHTER_GET_CLASS (self)->next (self,
                                                   range_begin, range_end,
                                                   match_begin, match_end);

  return IDE_HIGHLIGHT_KIND_NONE;
}
