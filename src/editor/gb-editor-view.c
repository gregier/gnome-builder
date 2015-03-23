/* gb-editor-view.c
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

#include "gb-editor-frame-private.h"
#include "gb-editor-view.h"
#include "gb-editor-view-actions.h"
#include "gb-editor-view-private.h"
#include "gb-widget.h"

G_DEFINE_TYPE (GbEditorView, gb_editor_view, GB_TYPE_VIEW)

enum {
  PROP_0,
  PROP_DOCUMENT,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static GbDocument *
gb_editor_view_get_document (GbView *view)
{
  GbEditorView *self = (GbEditorView *)view;

  g_assert (GB_IS_EDITOR_VIEW (self));

  return GB_DOCUMENT (self->document);
}

static GbEditorFrame *
gb_editor_view_get_last_focused (GbEditorView *self)
{
  /* TODO: track focus */
  return self->frame1;
}

static void
gb_editor_view_navigate_to (GbView            *view,
                            IdeSourceLocation *location)
{
  GbEditorView *self = (GbEditorView *)view;
  GbEditorFrame *frame;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  guint line;
  guint line_offset;

  IDE_ENTRY;

  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (location != NULL);

  frame = gb_editor_view_get_last_focused (self);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (frame->source_view));

  line = ide_source_location_get_line (location);
  line_offset = ide_source_location_get_line_offset (location);

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);
  for (; line_offset; line_offset--)
    if (gtk_text_iter_ends_line (&iter) || !gtk_text_iter_forward_char (&iter))
      break;

  gtk_text_buffer_select_range (buffer, &iter, &iter);
  ide_source_view_scroll_to_iter (frame->source_view, &iter, 0.0, TRUE, 1.0, 0.5, TRUE);

  IDE_EXIT;
}

static gboolean
language_to_string (GBinding     *binding,
                    const GValue *from_value,
                    GValue       *to_value,
                    gpointer      user_data)
{
  GtkSourceLanguage *language;

  language = g_value_get_object (from_value);
  if (language != NULL)
    g_value_set_string (to_value, gtk_source_language_get_name (language));
  return TRUE;
}

static gboolean
gb_editor_view_get_modified (GbView *view)
{
  GbEditorView *self = (GbEditorView *)view;

  g_assert (GB_IS_EDITOR_VIEW (self));

  return gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (self->document));
}

static void
gb_editor_view__buffer_modified_changed (GbEditorView  *self,
                                         GParamSpec    *pspec,
                                         GtkTextBuffer *buffer)
{
  g_assert (GB_IS_EDITOR_VIEW (self));

  g_object_notify (G_OBJECT (self), "modified");
}

static void
force_scroll_to_top (IdeSourceView *source_view)
{
  GtkAdjustment *vadj;
  GtkAdjustment *hadj;
  gdouble lower;

  /*
   * FIXME:
   *
   * See the comment in gb_editor_view__buffer_changed_on_volume()
   */

  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (source_view));
  hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (source_view));

  lower = gtk_adjustment_get_lower (vadj);
  gtk_adjustment_set_value (vadj, lower);

  lower = gtk_adjustment_get_lower (hadj);
  gtk_adjustment_set_value (hadj, lower);
}

static gboolean
no_really_scroll_to_the_top (gpointer data)
{
  g_autoptr(GbEditorView) self = data;

  force_scroll_to_top (self->frame1->source_view);
  if (self->frame2 != NULL)
    force_scroll_to_top (self->frame2->source_view);

  return G_SOURCE_REMOVE;
}

static void
gb_editor_view__buffer_changed_on_volume (GbEditorView *self,
                                          GParamSpec   *pspec,
                                          IdeBuffer    *buffer)
{
  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (ide_buffer_get_changed_on_volume (buffer))
    gtk_revealer_set_reveal_child (self->modified_revealer, TRUE);
  else if (gtk_revealer_get_reveal_child (self->modified_revealer))
    {
      GtkTextIter iter;

      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);
      gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);

      /*
       * FIXME:
       *
       * Without this delay, I see a condition with split view where the
       * non-focused split will just render blank. Well that isn't totally
       * correct, it renders empty gutters and proper line grid background. But
       * no textual content. And the adjustment is way out of sync. Even
       * changing the adjustment manually doesn't help. So whatever, I'll
       * insert a short delay and we'll pick up after the textview has
       * stablized.
       */
      g_timeout_add (10, no_really_scroll_to_the_top, g_object_ref (self));

      gtk_revealer_set_reveal_child (self->modified_revealer, FALSE);
    }
}

static void
gb_editor_view__buffer_notify_title (GbEditorView *self,
                                     GParamSpec   *pspec,
                                     IdeBuffer    *buffer)
{
  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (GB_IS_EDITOR_DOCUMENT (buffer));

  g_object_notify (G_OBJECT (self), "title");
}

static void
gb_editor_view_set_document (GbEditorView     *self,
                             GbEditorDocument *document)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (self));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  if (g_set_object (&self->document, document))
    {
      if (self->frame1)
        gb_editor_frame_set_document (self->frame1, document);

      if (self->frame2)
        gb_editor_frame_set_document (self->frame2, document);

      g_settings_bind (self->settings, "style-scheme-name",
                       document, "style-scheme-name",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (self->settings, "highlight-matching-brackets",
                       document, "highlight-matching-brackets",
                       G_SETTINGS_BIND_GET);

      g_object_bind_property_full (document, "language", self->tweak_button,
                                   "label", G_BINDING_SYNC_CREATE,
                                   language_to_string, NULL, NULL, NULL);

      g_signal_connect_object (document,
                               "modified-changed",
                               G_CALLBACK (gb_editor_view__buffer_modified_changed),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (document,
                               "notify::title",
                               G_CALLBACK (gb_editor_view__buffer_notify_title),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (document,
                               "notify::changed-on-volume",
                               G_CALLBACK (gb_editor_view__buffer_changed_on_volume),
                               self,
                               G_CONNECT_SWAPPED);

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_DOCUMENT]);

      gb_editor_view_actions_update (self);
    }
}

static GbView *
gb_editor_view_create_split (GbView *view)
{
  GbEditorView *self = (GbEditorView *)view;
  GbView *ret;

  g_assert (GB_IS_EDITOR_VIEW (self));

  ret = g_object_new (GB_TYPE_EDITOR_VIEW,
                      "document", self->document,
                      "visible", TRUE,
                      NULL);

  return ret;
}

static void
gb_editor_view_grab_focus (GtkWidget *widget)
{
  GbEditorView *self = (GbEditorView *)widget;

  g_assert (GB_IS_EDITOR_VIEW (self));

  /* todo: track last focus frame */

  gtk_widget_grab_focus (GTK_WIDGET (self->frame1->source_view));
}

static void
gb_editor_view_set_split_view (GbView   *view,
                               gboolean  split_view)
{
  GbEditorView *self = (GbEditorView *)view;

  g_assert (GB_IS_EDITOR_VIEW (self));

  if (split_view && (self->frame2 != NULL))
    return;

  if (!split_view && (self->frame2 == NULL))
    return;

  if (split_view)
    {
      self->frame2 = g_object_new (GB_TYPE_EDITOR_FRAME,
                                   "document", self->document,
                                   "visible", TRUE,
                                   NULL);
      gtk_container_add_with_properties (GTK_CONTAINER (self->paned), GTK_WIDGET (self->frame2),
                                         "shrink", FALSE,
                                         "resize", TRUE,
                                         NULL);
      gtk_widget_grab_focus (GTK_WIDGET (self->frame2));
    }
  else
    {
      GtkWidget *copy = GTK_WIDGET (self->frame2);

      self->frame2 = NULL;
      gtk_container_remove (GTK_CONTAINER (self->paned), copy);
      gtk_widget_grab_focus (GTK_WIDGET (self->frame1));
    }
}

static void
gb_editor_view_set_back_forward_list (GbView             *view,
                                      IdeBackForwardList *back_forward_list)
{
  GbEditorView *self = (GbEditorView *)view;

  g_assert (GB_IS_VIEW (view));
  g_assert (IDE_IS_BACK_FORWARD_LIST (back_forward_list));

  g_object_set (self->frame1, "back-forward-list", back_forward_list, NULL);
  if (self->frame2)
    g_object_set (self->frame2, "back-forward-list", back_forward_list, NULL);
}

static void
gb_editor_view_hide_reload_bar (GbEditorView *self,
                                GtkWidget    *button)
{
  g_assert (GB_IS_EDITOR_VIEW (self));

  gtk_revealer_set_reveal_child (self->modified_revealer, FALSE);
}

static void
gb_editor_view_finalize (GObject *object)
{
  GbEditorView *self = (GbEditorView *)object;

  g_clear_object (&self->document);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gb_editor_view_parent_class)->finalize (object);
}

static void
gb_editor_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbEditorView *self = GB_EDITOR_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_set_object (value, gb_editor_view_get_document (GB_VIEW (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbEditorView *self = GB_EDITOR_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      gb_editor_view_set_document (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_view_class_init (GbEditorViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GbViewClass *view_class = GB_VIEW_CLASS (klass);

  object_class->finalize = gb_editor_view_finalize;
  object_class->get_property = gb_editor_view_get_property;
  object_class->set_property = gb_editor_view_set_property;

  widget_class->grab_focus = gb_editor_view_grab_focus;

  view_class->create_split = gb_editor_view_create_split;
  view_class->get_document = gb_editor_view_get_document;
  view_class->get_modified = gb_editor_view_get_modified;
  view_class->set_split_view = gb_editor_view_set_split_view;
  view_class->set_back_forward_list = gb_editor_view_set_back_forward_list;
  view_class->navigate_to = gb_editor_view_navigate_to;

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _("Document"),
                         _("The editor document."),
                         GB_TYPE_EDITOR_DOCUMENT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT, gParamSpecs [PROP_DOCUMENT]);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-view.ui");

  GB_WIDGET_CLASS_BIND (klass, GbEditorView, frame1);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, modified_cancel_button);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, modified_revealer);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, paned);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, progress_bar);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, tweak_button);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, tweak_widget);

  g_type_ensure (GB_TYPE_EDITOR_FRAME);
  g_type_ensure (GB_TYPE_EDITOR_TWEAK_WIDGET);
}

static void
gb_editor_view_init (GbEditorView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->settings = g_settings_new ("org.gnome.builder.editor");

  gb_editor_view_actions_init (self);

  g_signal_connect_object (self->modified_cancel_button,
                           "clicked",
                           G_CALLBACK (gb_editor_view_hide_reload_bar),
                           self,
                           G_CONNECT_SWAPPED);
}
