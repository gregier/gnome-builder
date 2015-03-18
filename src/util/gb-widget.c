/* gb-widget.c
 *
 * Copyright (C) 2014 Christian Hergert <christian.hergert@mongodb.com>
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

#include <ide.h>
#include <math.h>

#include "gb-cairo.h"
#include "gb-rgba.h"
#include "gb-widget.h"
#include "gb-workbench.h"

/**
 * gb_widget_get_workbench:
 *
 * Returns: (transfer none) (type GbWorkbench*): A #GbWorkbench or %NULL.
 */
GbWorkbench *
gb_widget_get_workbench (GtkWidget *widget)
{
  GtkWidget *toplevel;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  toplevel = gtk_widget_get_toplevel (widget);

  if (GB_IS_WORKBENCH (toplevel))
    return GB_WORKBENCH (toplevel);

  return NULL;
}

void
gb_widget_add_style_class (gpointer     widget,
                           const gchar *class_name)
{
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (class_name);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_add_class (context, class_name);
}

cairo_surface_t *
gb_widget_snapshot (GtkWidget *widget,
                    gint       width,
                    gint       height,
                    gdouble    alpha,
                    gboolean   draw_border)
{
  cairo_surface_t *surface;
  GtkAllocation alloc;
  gdouble x_ratio = 1.0;
  gdouble y_ratio = 1.0;
  cairo_t *cr;

  /*
   * XXX: This function conflates the drawing of borders and snapshoting.
   *      Totally not ideal, but we can clean that up later.
   */

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);

  gtk_widget_get_allocation (widget, &alloc);

  if ((width != alloc.width) || (height != alloc.height))
    {
      if (alloc.width > alloc.height)
        {
          x_ratio = (gdouble) width / (gdouble) alloc.width;
          y_ratio = (gdouble) width / (gdouble) alloc.width;
        }
      else
        {
          x_ratio = (gdouble) height / (gdouble) alloc.height;
          y_ratio = (gdouble) height / (gdouble) alloc.height;
        }
      cairo_scale (cr, x_ratio, y_ratio);
    }

  gtk_widget_draw (widget, cr);

  cairo_destroy (cr);

  {
    cairo_surface_t *other;
    GdkRectangle rect = {
      3,
      3,
      ceil (alloc.width * x_ratio) - 6,
      ceil (alloc.height * y_ratio) - 6
    };

    other = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create (other);

    cairo_save (cr);

    if (draw_border)
      {
        gdk_cairo_rectangle (cr, &rect);
        cairo_clip (cr);
      }

    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_paint_with_alpha (cr, alpha);

    cairo_restore (cr);

    if (draw_border)
      {
        GdkRGBA rgba;

        gb_cairo_rounded_rectangle (cr, &rect, 3, 3);

        gdk_rgba_parse (&rgba, "#729fcf");
        gb_rgba_shade (&rgba, &rgba, 0.8);
        gdk_cairo_set_source_rgba (cr, &rgba);
        cairo_set_line_width (cr, 3.0);

        cairo_stroke (cr);

        gb_cairo_rounded_rectangle (cr, &rect, 1, 1);

        gdk_rgba_parse (&rgba, "#729fcf");
        gb_rgba_shade (&rgba, &rgba, 1.2);
        gdk_cairo_set_source_rgba (cr, &rgba);

        cairo_set_line_width (cr, 1.0);
        cairo_stroke (cr);
      }

    cairo_surface_destroy (surface);
    surface = other;
  }

  return surface;
}

static void
hide_callback (gpointer data)
{
  GtkWidget *widget = data;

  gtk_widget_hide (widget);
  gtk_widget_set_opacity (widget, 1.0);
  g_object_unref (widget);
}

void
gb_widget_fade_hide (GtkWidget *widget)
{
  GdkFrameClock *frame_clock;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_widget_get_visible (widget))
    {
      frame_clock = gtk_widget_get_frame_clock (widget);
      ide_object_animate_full (widget,
                               IDE_ANIMATION_LINEAR,
                               1000,
                               frame_clock,
                               hide_callback,
                               g_object_ref (widget),
                               "opacity", 0.0,
                               NULL);
    }
}

void
gb_widget_fade_show (GtkWidget *widget)
{
  GdkFrameClock *frame_clock;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!gtk_widget_get_visible (widget))
    {
      frame_clock = gtk_widget_get_frame_clock (widget);
      gtk_widget_set_opacity (widget, 0.0);
      gtk_widget_show (widget);
      ide_object_animate_full (widget,
                               IDE_ANIMATION_LINEAR,
                               500,
                               frame_clock,
                               NULL,
                               NULL,
                               "opacity", 1.0,
                               NULL);
    }
}

void
gb_widget_activate_action (GtkWidget   *widget,
                           const gchar *prefix,
                           const gchar *action_name,
                           GVariant    *parameter)
{
  GApplication *app;
  GtkWidget *toplevel;
  GActionGroup *group = NULL;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (prefix);
  g_return_if_fail (action_name);

  app = g_application_get_default ();
  toplevel = gtk_widget_get_toplevel (widget);

  while ((group == NULL) && (widget != NULL))
    {
      group = gtk_widget_get_action_group (widget, prefix);
      widget = gtk_widget_get_parent (widget);
    }

  if (!group && g_str_equal (prefix, "win") && G_IS_ACTION_GROUP (toplevel))
    group = G_ACTION_GROUP (toplevel);

  if (!group && g_str_equal (prefix, "app") && G_IS_ACTION_GROUP (app))
    group = G_ACTION_GROUP (app);

  if (group)
    {
      if (g_action_group_has_action (group, action_name))
        {
          g_action_group_activate_action (group, action_name, parameter);
          return;
        }
    }

  if (parameter && g_variant_is_floating (parameter))
    {
      parameter = g_variant_ref_sink (parameter);
      g_variant_unref (parameter);
    }

  g_warning ("Failed to resolve action %s.%s", prefix, action_name);
}

IdeContext *
gb_widget_get_context (GtkWidget *widget)
{
  GbWorkbench *workbench;
  IdeContext *context = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if ((workbench = gb_widget_get_workbench (widget)))
    context = gb_workbench_get_context (workbench);

  g_return_val_if_fail (!context || IDE_IS_CONTEXT (context), NULL);

  return context;
}

static void
gb_widget_notify_context (GtkWidget  *toplevel,
                          GParamSpec *pspec,
                          GtkWidget  *widget)
{
  GbWidgetContextHandler handler;
  IdeContext *context = NULL;

  handler = g_object_get_data (G_OBJECT (widget), "GB_CONTEXT_HANDLER");
  if (!handler)
    return;

  g_object_get (toplevel, "context", &context, NULL);
  handler (widget, context);
  g_clear_object (&context);
}

static void
gb_widget_hierarchy_changed (GtkWidget *widget,
                             GtkWidget *previous_toplevel,
                             gpointer   user_data)
{
  GtkWidget *toplevel;

  g_assert (GTK_IS_WIDGET (widget));

  if (GTK_IS_WINDOW (previous_toplevel))
    g_signal_handlers_disconnect_by_func (previous_toplevel,
                                          G_CALLBACK (gb_widget_notify_context),
                                          widget);

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    {
      g_signal_connect_object (toplevel,
                               "notify::context",
                               G_CALLBACK (gb_widget_notify_context),
                               widget,
                               0);
      gb_widget_notify_context (toplevel, NULL, widget);
    }
}

void
gb_widget_set_context_handler (gpointer               widget,
                               GbWidgetContextHandler handler)
{
  GtkWidget *toplevel;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_set_data (G_OBJECT (widget), "GB_CONTEXT_HANDLER", handler);

  g_signal_connect (widget,
                    "hierarchy-changed",
                    G_CALLBACK (gb_widget_hierarchy_changed),
                    NULL);

  if ((toplevel = gtk_widget_get_toplevel (widget)))
    gb_widget_hierarchy_changed (widget, NULL, NULL);
}
