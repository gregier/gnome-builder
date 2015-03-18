/* ide-buffer.h
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

#ifndef IDE_BUFFER_H
#define IDE_BUFFER_H

#include <gtksourceview/gtksourcebuffer.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUFFER             (ide_buffer_get_type ())
#define IDE_BUFFER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_BUFFER, IdeBuffer))
#define IDE_BUFFER_CONST(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_BUFFER, IdeBuffer const))
#define IDE_BUFFER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_BUFFER, IdeBufferClass))
#define IDE_IS_BUFFER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_BUFFER))
#define IDE_IS_BUFFER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_BUFFER))
#define IDE_BUFFER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_BUFFER, IdeBufferClass))

#define IDE_BUFFER_LINE_FLAGS_DIAGNOSTICS_MASK \
  ((IDE_BUFFER_LINE_FLAGS_ERROR | IDE_BUFFER_LINE_FLAGS_WARNING | IDE_BUFFER_LINE_FLAGS_NOTE))

typedef struct _IdeBufferClass IdeBufferClass;

typedef enum
{
  IDE_BUFFER_LINE_FLAGS_NONE     = 0,
  IDE_BUFFER_LINE_FLAGS_ADDED    = 1 << 0,
  IDE_BUFFER_LINE_FLAGS_CHANGED  = 1 << 1,
  IDE_BUFFER_LINE_FLAGS_ERROR    = 1 << 2,
  IDE_BUFFER_LINE_FLAGS_WARNING  = 1 << 3,
  IDE_BUFFER_LINE_FLAGS_NOTE     = 1 << 4,
} IdeBufferLineFlags;

struct _IdeBufferClass
{
  GtkSourceBufferClass parent_class;
};

struct _IdeBuffer
{
  GtkSourceBuffer parent_instance;
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeBuffer, g_object_unref)

GBytes             *ide_buffer_get_content               (IdeBuffer         *self);
IdeContext         *ide_buffer_get_context               (IdeBuffer         *self);
IdeDiagnostic      *ide_buffer_get_diagnostic_at_iter    (IdeBuffer         *self,
                                                          const GtkTextIter *iter);
IdeFile            *ide_buffer_get_file                  (IdeBuffer         *self);
IdeBufferLineFlags  ide_buffer_get_line_flags            (IdeBuffer         *buffer,
                                                          guint              line);
gboolean            ide_buffer_get_highlight_diagnostics (IdeBuffer         *self);
const gchar        *ide_buffer_get_style_scheme_name     (IdeBuffer         *self);
const gchar        *ide_buffer_get_title                 (IdeBuffer         *self);
GType               ide_buffer_get_type                  (void);
void                ide_buffer_set_file                  (IdeBuffer         *self,
                                                          IdeFile           *file);
void                ide_buffer_set_highlight_diagnostics (IdeBuffer         *self,
                                                          gboolean           highlight_diagnostics);
void                ide_buffer_set_style_scheme_name     (IdeBuffer         *self,
                                                          const gchar       *style_scheme_name);
void                ide_buffer_trim_trailing_whitespace  (IdeBuffer         *self);

G_END_DECLS

#endif /* IDE_BUFFER_H */
