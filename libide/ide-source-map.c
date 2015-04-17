/* ide-source-map.c
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
#include <string.h>

#include "ide-source-map.h"
#include "ide-source-view.h"

struct _IdeSourceMap
{
  GtkBin         parent_instance;

  GtkWidget     *view;
  GtkTextBuffer *buffer;

  gdouble        ratio;
};

G_DEFINE_TYPE (IdeSourceMap, ide_source_map, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_VIEW,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
ide_source_map_new (GtkWidget *view)
{
  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (view), NULL);

  return g_object_new (IDE_TYPE_SOURCE_MAP,
                       "view", view,
                       NULL);
}

/**
 * ide_source_map_get_view:
 *
 * Returns: (transfer none): A #GtkWidget.
 */
GtkWidget *
ide_source_map_get_view (IdeSourceMap *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_MAP (self), NULL);

  return self->view;
}

static void
ide_source_map__buffer_changed (IdeSourceMap  *self,
                                GtkTextBuffer *buffer)
{
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
ide_source_map__view_notify_buffer (IdeSourceMap  *self,
                                    GParamSpec    *pspec,
                                    IdeSourceView *view)
{
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  if (self->buffer != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->buffer,
                                            G_CALLBACK (ide_source_map__buffer_changed),
                                            self);
      ide_clear_weak_pointer (&self->buffer);
    }

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

  if (buffer != NULL)
    {
      ide_set_weak_pointer (&self->buffer, buffer);
      g_signal_connect_object (buffer,
                               "changed",
                               G_CALLBACK (ide_source_map__buffer_changed),
                               self,
                               G_CONNECT_SWAPPED);
      ide_source_map__buffer_changed (self, buffer);
    }
}

static void
ide_source_map__view_size_allocate (IdeSourceMap  *self,
                                    GtkAllocation *alloc,
                                    IdeSourceView *view)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (alloc != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (view));

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
ide_source_map_connect (IdeSourceMap *self,
                        GtkWidget    *view)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  g_signal_connect_object (view,
                           "notify::buffer",
                           G_CALLBACK (ide_source_map__view_notify_buffer),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (view,
                           "size-allocate",
                           G_CALLBACK (ide_source_map__view_size_allocate),
                           self,
                           G_CONNECT_SWAPPED);

  ide_source_map__view_notify_buffer (self, NULL, IDE_SOURCE_VIEW (view));
}

static void
ide_source_map_disconnect (IdeSourceMap *self,
                           GtkWidget    *view)
{
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (ide_source_map__buffer_changed),
                                        self);

  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (ide_source_map__view_notify_buffer),
                                        self);

  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (ide_source_map__view_size_allocate),
                                        self);
}

void
ide_source_map_set_view (IdeSourceMap *self,
                         GtkWidget    *view)
{
  g_return_if_fail (IDE_IS_SOURCE_MAP (self));
  g_return_if_fail (!view || GTK_IS_WIDGET (view));

  if (view != self->view)
    {
      if (self->view != NULL)
        {
          ide_source_map_disconnect (self, self->view);
          ide_clear_weak_pointer (&self->view);
        }

      if (view != NULL)
        {
          ide_set_weak_pointer (&self->view, view);
          ide_source_map_connect (self, self->view);
        }

      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_VIEW]);
    }
}

static GtkSizeRequestMode
ide_source_map_real_get_request_mode (GtkWidget *widget)
{
  g_assert (IDE_IS_SOURCE_MAP (widget));

  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
ide_source_map_real_get_preferred_width (GtkWidget *widget,
                                         gint      *preferred_width,
                                         gint      *natural_width)
{
  g_assert (IDE_IS_SOURCE_MAP (widget));
  g_assert (preferred_width != NULL);
  g_assert (natural_width != NULL);

  /* TODO: Is there a better option here? */

  *preferred_width = 100;
  *natural_width = 100;
}

static void
ide_source_map_real_get_preferred_height_for_width (GtkWidget *widget,
                                                    gint       width,
                                                    gint      *minimum_height,
                                                    gint      *natural_height)
{
  IdeSourceMap *self = (IdeSourceMap *)widget;
  PangoLayout *layout;
  GtkRequisition req;
  GtkAllocation alloc;
  gdouble ratio;
  guint margin;
  gint px_width;
  gint px_height;

  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (!self->view || IDE_IS_SOURCE_VIEW (self->view));
  g_assert (minimum_height != NULL);
  g_assert (natural_height != NULL);

  *minimum_height = 0;
  *natural_height = 0;

  if (self->view == NULL)
    return;

  gtk_widget_get_allocation (self->view, &alloc);
  gtk_widget_get_preferred_size (self->view, NULL, &req);

  layout = gtk_widget_create_pango_layout (self->view, "X");
  pango_layout_get_pixel_size (layout, &px_width, &px_height);
  g_object_unref (layout);

  if (px_width == 0 || px_height == 0)
    return;

  margin = gtk_source_view_get_right_margin_position (GTK_SOURCE_VIEW (self->view));
  ratio = (gdouble)width / ((gdouble)px_width * (gdouble)margin);

  *natural_height = req.height * ratio;
  *minimum_height = alloc.height * ratio;

  if (*natural_height > alloc.height)
    *natural_height = alloc.height;

  if (*natural_height < *minimum_height)
    *natural_height = *minimum_height;

  self->ratio = ratio;
}

static gboolean
ide_source_map_real_draw (GtkWidget *widget,
                          cairo_t   *cr)
{
  IdeSourceMap *self = (IdeSourceMap *)widget;
  GdkRectangle area;
  GtkAllocation alloc;

  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (cr != NULL);

  if (self->view == NULL)
    return FALSE;

  gtk_widget_get_allocation (widget, &alloc);

  /* todo: handle files that are too big with aspect ratio */
  /* todo: only render changed area */
  /* todo: render from offscreen pixmap */
  area.x = 0;
  area.width = 100 * self->ratio; /* todo: cache margin width */
  area.y = 0;
  area.height = alloc.height * self->ratio;

  cairo_save (cr);
  cairo_scale (cr, self->ratio, self->ratio);
  gtk_text_view_draw_text_with_area (GTK_TEXT_VIEW (self->view), cr, &area);
  gtk_widget_draw (self->view, cr);
  cairo_restore (cr);

  return FALSE;
}

static void
ide_source_map_dispose (GObject *object)
{
  IdeSourceMap *self = (IdeSourceMap *)object;

  ide_source_map_set_view (self, NULL);

  G_OBJECT_CLASS (ide_source_map_parent_class)->dispose (object);
}

static void
ide_source_map_finalize (GObject *object)
{
  IdeSourceMap *self = (IdeSourceMap *)object;

  ide_clear_weak_pointer (&self->view);

  G_OBJECT_CLASS (ide_source_map_parent_class)->finalize (object);
}

static void
ide_source_map_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeSourceMap *self = IDE_SOURCE_MAP (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      g_value_set_object (value, ide_source_map_get_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_map_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeSourceMap *self = IDE_SOURCE_MAP (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      ide_source_map_set_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_map_class_init (IdeSourceMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_source_map_dispose;
  object_class->finalize = ide_source_map_finalize;
  object_class->get_property = ide_source_map_get_property;
  object_class->set_property = ide_source_map_set_property;

  widget_class->draw = ide_source_map_real_draw;
  widget_class->get_preferred_height_for_width = ide_source_map_real_get_preferred_height_for_width;
  widget_class->get_preferred_width = ide_source_map_real_get_preferred_width;
  widget_class->get_request_mode = ide_source_map_real_get_request_mode;

  gParamSpecs [PROP_VIEW] =
    g_param_spec_object ("view",
                         _("View"),
                         _("The view to be mapped."),
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_VIEW, gParamSpecs [PROP_VIEW]);
}

static void
ide_source_map_init (IdeSourceMap *self)
{
  gtk_widget_add_events (GTK_WIDGET (self),
                         (GDK_ENTER_NOTIFY_MASK |
                          GDK_LEAVE_NOTIFY_MASK |
                          GDK_POINTER_MOTION_MASK |
                          GDK_BUTTON_PRESS_MASK));
}
