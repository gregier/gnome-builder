/* gb-view-stack.c
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

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-document.h"
#include "gb-view.h"
#include "gb-view-grid.h"
#include "gb-view-stack.h"
#include "gb-view-stack-actions.h"
#include "gb-view-stack-private.h"
#include "gb-widget.h"
#include "gb-workbench.h"

G_DEFINE_TYPE (GbViewStack, gb_view_stack, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_ACTIVE_VIEW,
  LAST_PROP
};

enum {
  EMPTY,
  SPLIT,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

static void
gb_view_stack_add (GtkContainer *container,
                   GtkWidget    *child)
{
  GbViewStack *self = (GbViewStack *)container;

  g_assert (GB_IS_VIEW_STACK (self));

  if (GB_IS_VIEW (child))
    {
      GtkWidget *controls;

      gtk_widget_set_sensitive (GTK_WIDGET (self->close_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->document_button), TRUE);

      self->focus_history = g_list_prepend (self->focus_history, child);
      controls = gb_view_get_controls (GB_VIEW (child));
      if (controls)
        gtk_container_add (GTK_CONTAINER (self->controls_stack), controls);
      gtk_container_add (GTK_CONTAINER (self->stack), child);
      gb_view_set_back_forward_list (GB_VIEW (child), self->back_forward_list);
      gtk_stack_set_visible_child (self->stack, child);
    }
  else
    {
      GTK_CONTAINER_CLASS (gb_view_stack_parent_class)->add (container, child);
    }
}

void
gb_view_stack_remove (GbViewStack *self,
                      GbView      *view)
{
  GtkWidget *controls;

  g_assert (GB_IS_VIEW_STACK (self));
  g_assert (GB_IS_VIEW (view));

  self->focus_history = g_list_remove (self->focus_history, view);
  controls = gb_view_get_controls (view);
  if (controls)
    gtk_container_remove (GTK_CONTAINER (self->controls_stack), controls);
  gtk_container_remove (GTK_CONTAINER (self->stack), GTK_WIDGET (view));

  if (self->focus_history)
    {
      GtkWidget *child;

      child = self->focus_history->data;
      gtk_stack_set_visible_child (self->stack, child);
      gtk_widget_grab_focus (GTK_WIDGET (child));
    }
  else
    g_signal_emit (self, gSignals [EMPTY], 0);
}

static void
gb_view_stack_real_remove (GtkContainer *container,
                           GtkWidget    *child)
{
  GbViewStack *self = (GbViewStack *)container;

  g_assert (GB_IS_VIEW_STACK (self));

  if (GB_IS_VIEW (child))
    gb_view_stack_remove (self, GB_VIEW (child));
  else
    GTK_CONTAINER_CLASS (gb_view_stack_parent_class)->remove (container, child);
}

static void
gb_view_stack__notify_visible_child (GbViewStack *self,
                                     GParamSpec  *pspec,
                                     GtkStack    *stack)
{
  GtkWidget *visible_child;

  g_assert (GB_IS_VIEW_STACK (self));
  g_assert (GTK_IS_STACK (stack));

  visible_child = gtk_stack_get_visible_child (stack);

  gb_view_stack_set_active_view (self, visible_child);
}

static void
gb_view_stack_grab_focus (GtkWidget *widget)
{
  GbViewStack *self = (GbViewStack *)widget;
  GtkWidget *visible_child;

  g_assert (GB_IS_VIEW_STACK (self));

  visible_child = gtk_stack_get_visible_child (self->stack);
  if (visible_child)
    gtk_widget_grab_focus (visible_child);
}

static gboolean
gb_view_stack_is_empty (GbViewStack *self)
{
  g_return_val_if_fail (GB_IS_VIEW_STACK (self), FALSE);

  return (self->focus_history == NULL);
}

static void
gb_view_stack_real_empty (GbViewStack *self)
{
  g_assert (GB_IS_VIEW_STACK (self));

  /* its possible for a widget to be added during "empty" emission. */
  if (gb_view_stack_is_empty (self) && !self->destroyed)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->close_button), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->document_button), FALSE);
    }
}

static void
navigate_to_cb (GbViewStack        *self,
                IdeBackForwardItem *item,
                IdeBackForwardList *back_forward_list)
{
  IdeSourceLocation *srcloc;

  g_assert (GB_IS_VIEW_STACK (self));
  g_assert (IDE_IS_BACK_FORWARD_ITEM (item));
  g_assert (IDE_IS_BACK_FORWARD_LIST (back_forward_list));

  srcloc = ide_back_forward_item_get_location (item);
  gb_view_stack_focus_location (self, srcloc);
}

static void
gb_view_stack_context_handler (GtkWidget  *widget,
                               IdeContext *context)
{
  IdeBackForwardList *back_forward;
  GbViewStack *self = (GbViewStack *)widget;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context)
    {
      GList *children;
      GList *iter;

      ide_set_weak_pointer (&self->context, context);

      back_forward = ide_context_get_back_forward_list (context);

      g_clear_object (&self->back_forward_list);
      self->back_forward_list = ide_back_forward_list_branch (back_forward);

      g_signal_connect_object (self->back_forward_list,
                               "navigate-to",
                               G_CALLBACK (navigate_to_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_object_bind_property (self->back_forward_list, "can-go-backward",
                              self->go_backward, "sensitive",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property (self->back_forward_list, "can-go-forward",
                              self->go_forward, "sensitive",
                              G_BINDING_SYNC_CREATE);

      children = gtk_container_get_children (GTK_CONTAINER (self->stack));
      for (iter = children; iter; iter = iter->next)
        gb_view_set_back_forward_list (iter->data, self->back_forward_list);
      g_list_free (children);
    }
}

static void
gb_view_stack_on_workbench_unload (GbWorkbench *workbench,
                                   IdeContext  *context,
                                   GbViewStack *self)
{
  IdeBackForwardList *back_forward_list;

  g_assert (GB_IS_WORKBENCH (workbench));
  g_assert (IDE_IS_CONTEXT (context));
  g_assert (GB_IS_VIEW_STACK (self));

  if (self->back_forward_list)
    {
      back_forward_list = ide_context_get_back_forward_list (context);
      ide_back_forward_list_merge (back_forward_list, self->back_forward_list);
    }
}

static void
gb_view_stack_hierarchy_changed (GtkWidget *widget,
                                 GtkWidget *old_toplevel)
{
  GbViewStack *self = (GbViewStack *)widget;
  GtkWidget *toplevel;

  g_assert (GB_IS_VIEW_STACK (self));

  if (GB_IS_WORKBENCH (old_toplevel))
    {
      g_signal_handlers_disconnect_by_func (old_toplevel,
                                            G_CALLBACK (gb_view_stack_on_workbench_unload),
                                            self);
    }

  toplevel = gtk_widget_get_toplevel (widget);

  if (GB_IS_WORKBENCH (toplevel))
    {
      g_signal_connect (toplevel,
                        "unload",
                        G_CALLBACK (gb_view_stack_on_workbench_unload),
                        self);
    }
}

static void
gb_view_stack_destroy (GtkWidget *widget)
{
  GbViewStack *self = (GbViewStack *)widget;

  self->destroyed = TRUE;

  GTK_WIDGET_CLASS (gb_view_stack_parent_class)->destroy (widget);
}

static void
gb_view_stack_constructed (GObject *object)
{
  GbViewStack *self = (GbViewStack *)object;

  G_OBJECT_CLASS (gb_view_stack_parent_class)->constructed (object);

  gb_view_stack_actions_init (self);
}

static void
gb_view_stack_finalize (GObject *object)
{
  GbViewStack *self = (GbViewStack *)object;

  g_clear_pointer (&self->focus_history, g_list_free);
  ide_clear_weak_pointer (&self->context);
  ide_clear_weak_pointer (&self->title_binding);
  ide_clear_weak_pointer (&self->active_view);
  g_clear_object (&self->back_forward_list);

  G_OBJECT_CLASS (gb_view_stack_parent_class)->finalize (object);
}

static void
gb_view_stack_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GbViewStack *self = GB_VIEW_STACK (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_VIEW:
      g_value_set_object (value, gb_view_stack_get_active_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_view_stack_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GbViewStack *self = GB_VIEW_STACK (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_VIEW:
      gb_view_stack_set_active_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_view_stack_class_init (GbViewStackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->constructed = gb_view_stack_constructed;
  object_class->finalize = gb_view_stack_finalize;
  object_class->get_property = gb_view_stack_get_property;
  object_class->set_property = gb_view_stack_set_property;

  widget_class->destroy = gb_view_stack_destroy;
  widget_class->grab_focus = gb_view_stack_grab_focus;
  widget_class->hierarchy_changed = gb_view_stack_hierarchy_changed;

  container_class->add = gb_view_stack_add;
  container_class->remove = gb_view_stack_real_remove;

  gParamSpecs [PROP_ACTIVE_VIEW] =
    g_param_spec_object ("active-view",
                         _("Active View"),
                         _("The active view."),
                         GB_TYPE_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ACTIVE_VIEW, gParamSpecs [PROP_ACTIVE_VIEW]);

  gSignals [EMPTY] =
    g_signal_new_class_handler ("empty",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (gb_view_stack_real_empty),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE,
                                0);

  gSignals [SPLIT] = g_signal_new ("split",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, NULL,
                                   g_cclosure_marshal_generic,
                                   G_TYPE_NONE,
                                   2,
                                   GB_TYPE_VIEW,
                                   GB_TYPE_VIEW_GRID_SPLIT);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-view-stack.ui");
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, close_button);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, controls_stack);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, document_button);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, go_backward);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, go_forward);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, modified_label);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, popover);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, stack);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, title_label);
}

static void
gb_view_stack_init (GbViewStack *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->stack,
                           "notify::visible-child",
                           G_CALLBACK (gb_view_stack__notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);

  gb_widget_set_context_handler (self, gb_view_stack_context_handler);
}

GtkWidget *
gb_view_stack_new (void)
{
  return g_object_new (GB_TYPE_VIEW_STACK, NULL);
}

GtkWidget *
gb_view_stack_get_active_view (GbViewStack *self)
{
  g_return_val_if_fail (GB_IS_VIEW_STACK (self), NULL);

  return self->active_view;
}

void
gb_view_stack_set_active_view (GbViewStack *self,
                               GtkWidget   *active_view)
{
  g_return_if_fail (GB_IS_VIEW_STACK (self));
  g_return_if_fail (!active_view || GB_IS_VIEW (active_view));

  if (self->active_view != active_view)
    {
      if (self->active_view)
        {
          if (self->title_binding)
            g_binding_unbind (self->title_binding);
          ide_clear_weak_pointer (&self->title_binding);
          if (self->modified_binding)
            g_binding_unbind (self->modified_binding);
          ide_clear_weak_pointer (&self->modified_binding);
          gtk_label_set_label (self->title_label, NULL);
          ide_clear_weak_pointer (&self->active_view);
          gtk_widget_hide (GTK_WIDGET (self->controls_stack));
        }

      if (active_view)
        {
          GtkWidget *controls;
          GBinding *binding;
          GActionGroup *group;

          self->focus_history = g_list_remove (self->focus_history, active_view);
          self->focus_history = g_list_prepend (self->focus_history, active_view);

          if (active_view != gtk_stack_get_visible_child (self->stack))
            gtk_stack_set_visible_child (self->stack, active_view);
          binding = g_object_bind_property (active_view, "title",
                                            self->title_label, "label",
                                            G_BINDING_SYNC_CREATE);
          ide_set_weak_pointer (&self->title_binding, binding);
          binding = g_object_bind_property (active_view, "modified",
                                            self->modified_label, "visible",
                                            G_BINDING_SYNC_CREATE);
          ide_set_weak_pointer (&self->modified_binding, binding);
          ide_set_weak_pointer (&self->active_view, active_view);
          controls = gb_view_get_controls (GB_VIEW (active_view));
          if (controls)
            {
              gtk_stack_set_visible_child (self->controls_stack, controls);
              gtk_widget_show (GTK_WIDGET (self->controls_stack));
            }
          group = gtk_widget_get_action_group (active_view, "view");
          if (group)
            gtk_widget_insert_action_group (GTK_WIDGET (self), "view", group);
        }

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_ACTIVE_VIEW]);
    }
}

GtkWidget *
gb_view_stack_find_with_document (GbViewStack *self,
                                  GbDocument  *document)
{
  GtkWidget *ret = NULL;
  GList *iter;
  GList *children;

  g_return_val_if_fail (GB_IS_VIEW_STACK (self), NULL);
  g_return_val_if_fail (GB_IS_DOCUMENT (document), NULL);

  children = gtk_container_get_children (GTK_CONTAINER (self->stack));

  for (iter = children; iter; iter = iter->next)
    {
      GbView *view = iter->data;
      GbDocument *item;

      g_assert (GB_IS_VIEW (view));

      item = gb_view_get_document (view);

      if (item == document)
        {
          ret = GTK_WIDGET (view);
          break;
        }
    }

  g_list_free (children);

  return ret;
}

void
gb_view_stack_focus_document (GbViewStack *self,
                              GbDocument  *document)
{
  GtkWidget *view;

  g_return_if_fail (GB_IS_VIEW_STACK (self));
  g_return_if_fail (GB_IS_DOCUMENT (document));

  view = gb_view_stack_find_with_document (self, document);

  if (view != NULL && GB_IS_VIEW (view))
    {
      gb_view_stack_set_active_view (self, view);
      gtk_widget_grab_focus (view);
      return;
    }

  view = gb_document_create_view (document);

  if (view == NULL)
    {
      g_warning ("Document %s failed to create a view",
                 gb_document_get_title (document));
      return;
    }

  if (!GB_IS_VIEW (view))
    {
      g_warning ("Document %s did not create a GbView instance.",
                 gb_document_get_title (document));
      return;
    }

  gb_view_stack_add (GTK_CONTAINER (self), view);
  gb_view_stack_set_active_view (self, view);
  gtk_widget_grab_focus (view);
}

static void
gb_view_stack__navigate_to_load_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  GbViewStack *self;
  g_autoptr(GTask) task = user_data;
  IdeSourceLocation *location;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(GError) error = NULL;
  GtkWidget *active_view;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  self = g_task_get_source_object (task);
  location = g_task_get_task_data (task);

  buffer = ide_buffer_manager_load_file_finish (buffer_manager, result, &error);

  if (buffer == NULL)
    {
      /* todo: message dialog? */
      g_warning ("%s", error->message);
      return;
    }

  g_assert (GB_IS_DOCUMENT (buffer));
  g_assert (location != NULL);

  gb_view_stack_focus_document (self, GB_DOCUMENT (buffer));
  active_view = gb_view_stack_get_active_view (self);
  g_assert (GB_DOCUMENT (buffer) == gb_view_get_document (GB_VIEW (active_view)));
  gb_view_navigate_to (GB_VIEW (active_view), location);

  g_task_return_boolean (task, TRUE);
}

void
gb_view_stack_focus_location (GbViewStack       *self,
                              IdeSourceLocation *location)
{
  IdeBufferManager *buffer_manager;
  IdeBuffer *buffer;
  IdeFile *file;

  g_return_if_fail (GB_IS_VIEW_STACK (self));
  g_return_if_fail (location != NULL);

  if (self->context == NULL)
    return;

  file = ide_source_location_get_file (location);

  g_assert (file != NULL);
  g_assert (IDE_IS_FILE (file));

  buffer_manager = ide_context_get_buffer_manager (self->context);
  buffer = ide_buffer_manager_find_buffer (buffer_manager, file);

  if (buffer != NULL && GB_IS_DOCUMENT (buffer))
    {
      GtkWidget *active_view;

      gb_view_stack_focus_document (self, GB_DOCUMENT (buffer));
      active_view = gb_view_stack_get_active_view (self);
      g_assert (GB_DOCUMENT (buffer) == gb_view_get_document (GB_VIEW (active_view)));
      gb_view_navigate_to (GB_VIEW (active_view), location);
    }
  else
    {
      g_autoptr(GTask) task = NULL;

      task = g_task_new (self, NULL, NULL, NULL);
      g_task_set_task_data (task, ide_source_location_ref (location),
                            (GDestroyNotify)ide_source_location_unref);
      ide_buffer_manager_load_file_async (buffer_manager, file, FALSE, NULL, NULL,
                                          gb_view_stack__navigate_to_load_cb,
                                          g_object_ref (task));
    }
}

GbDocument *
gb_view_stack_find_document_typed (GbViewStack *self,
                                   GType        document_type)
{
  GList *iter;

  g_return_val_if_fail (GB_IS_VIEW_STACK (self), NULL);
  g_return_val_if_fail (g_type_is_a (document_type, GB_TYPE_DOCUMENT), NULL);

  for (iter = self->focus_history; iter; iter = iter->next)
    {
      GbDocument *document;

      document = gb_view_get_document (iter->data);
      if (g_type_is_a (G_TYPE_FROM_INSTANCE (document), document_type))
        return document;
    }

  return NULL;
}
