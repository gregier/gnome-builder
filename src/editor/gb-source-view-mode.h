/* gb-key-mode.h
 *
 * Copyright (C) 2015 Alexander Larsson <alexl@redhat.com>
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

#ifndef GB_SOURCE_VIEW_MODE_H
#define GB_SOURCE_VIEW_MODE_H

#include <gtk/gtk.h>
#include "gb-source-view.h"
#include "gb-types.h"

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_VIEW_MODE            (gb_source_view_mode_get_type())
#define GB_SOURCE_VIEW_MODE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_VIEW_MODE, GbSourceViewMode))
#define GB_SOURCE_VIEW_MODE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_VIEW_MODE, GbSourceViewMode const))
#define GB_SOURCE_VIEW_MODE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_VIEW_MODE, GbSourceViewModeClass))
#define GB_IS_SOURCE_VIEW_MODE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_VIEW_MODE))
#define GB_IS_SOURCE_VIEW_MODE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_VIEW_MODE))
#define GB_SOURCE_VIEW_MODE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_VIEW_MODE, GbSourceViewModeClass))

typedef struct _GbSourceViewMode        GbSourceViewMode;
typedef struct _GbSourceViewModeClass   GbSourceViewModeClass;
typedef struct _GbSourceViewModePrivate GbSourceViewModePrivate;

struct _GbSourceViewMode
{
  GtkWidget parent;

  /*< private >*/
  GbSourceViewModePrivate *priv;
};

struct _GbSourceViewModeClass
{
  GtkWidgetClass parent_class;

};


/**
 * GbSourceViewModeType:
 * @GB_SOURCE_VIEW_MODE_TRANSIENT: Transient
 * @GB_SOURCE_VIEW_MODE_PERMANENT: Permanent
 * @GB_SOURCE_VIEW_MODE_MODAL: Modal
 *
 * The type of keyboard mode.
 *
 */
typedef enum {
  GB_SOURCE_VIEW_MODE_TYPE_TRANSIENT,
  GB_SOURCE_VIEW_MODE_TYPE_PERMANENT,
  GB_SOURCE_VIEW_MODE_TYPE_MODAL
} GbSourceViewModeType;

GType    gb_source_view_mode_get_type (void);

GbSourceViewMode *gb_source_view_mode_new      (GtkWidget            *view,
                                                const char           *mode,
                                                GbSourceViewModeType  type);
gboolean          gb_source_view_mode_do_event (GbSourceViewMode     *mode,
                                                GdkEventKey          *event,
                                                gboolean             *remove);

G_END_DECLS

#endif /* GB_SOURCE_VIEW_MODE_H */
