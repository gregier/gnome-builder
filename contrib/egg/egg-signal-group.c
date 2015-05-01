/* egg-signal-group.c
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

#define G_LOG_DOMAIN "egg-signal-group"

#include <glib/gi18n.h>

#include "egg-signal-group.h"

struct _EggSignalGroup
{
  GObject    parent_instance;

  GObject   *target;
  GPtrArray *handlers;
  GType      target_type;
  gint       block_count;
};

struct _EggSignalGroupClass
{
  GObjectClass parent_class;

  void (*bind)   (EggSignalGroup *self,
                  GObject        *target);
  void (*unbind) (EggSignalGroup *self,
                  GObject        *target);
};

typedef struct
{
  gulong       handler_id;
  GClosure    *closure;
  const gchar *detailed_signal;
  guint        connect_after : 1;
} SignalHandler;

G_DEFINE_TYPE (EggSignalGroup, egg_signal_group, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_TARGET,
  PROP_TARGET_TYPE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
egg_signal_group__target_weak_notify (gpointer  data,
                                      GObject  *where_object_was)
{
  EggSignalGroup *self = data;
  gsize i;

  g_assert (EGG_IS_SIGNAL_GROUP (self));
  g_assert (where_object_was != NULL);
  g_assert (self->target == where_object_was);

  for (i = 0; i < self->handlers->len; i++)
    {
      SignalHandler *handler;

      handler = g_ptr_array_index (self->handlers, i);
      handler->handler_id = 0;
    }

  g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_TARGET]);
}

static void
egg_signal_group_bind_handler (EggSignalGroup *self,
                               SignalHandler  *handler)
{
  gsize i;

  g_assert (self != NULL);
  g_assert (self->target != NULL);
  g_assert (handler != NULL);
  g_assert (handler->detailed_signal != NULL);
  g_assert (handler->closure != NULL);
  g_assert_cmpint (handler->handler_id, ==, 0);

  handler->handler_id =  g_signal_connect_closure (self->target,
                                                   handler->detailed_signal,
                                                   handler->closure,
                                                   handler->connect_after);

  for (i = 0; i < self->block_count; i++)
    g_signal_handler_block (self->target, handler->handler_id);
}

static void
egg_signal_group_bind (EggSignalGroup *self,
                       GObject        *target)
{
  g_assert (EGG_IS_SIGNAL_GROUP (self));
  g_assert (self->target == NULL);
  g_assert (!target || G_IS_OBJECT (target));

  if (target != NULL)
    {
      gsize i;

      self->target = target;
      g_object_weak_ref (self->target,
                         egg_signal_group__target_weak_notify,
                         self);

      for (i = 0; i < self->handlers->len; i++)
        {
          SignalHandler *handler;

          handler = g_ptr_array_index (self->handlers, i);
          egg_signal_group_bind_handler (self, handler);
        }
    }
}

static void
egg_signal_group_unbind (EggSignalGroup *self)
{
  g_return_if_fail (EGG_IS_SIGNAL_GROUP (self));

  if (self->target != NULL)
    {
      GObject *target;
      gsize i;

      target = self->target;
      self->target = NULL;

      g_object_weak_unref (target,
                           egg_signal_group__target_weak_notify,
                           self);

      for (i = 0; i < self->handlers->len; i++)
        {
          SignalHandler *handler;
          gulong handler_id;

          handler = g_ptr_array_index (self->handlers, i);

          g_assert (handler != NULL);
          g_assert (handler->detailed_signal != NULL);
          g_assert (handler->closure != NULL);
          g_assert_cmpint (handler->handler_id, !=, 0);

          handler_id = handler->handler_id;
          handler->handler_id = 0;

          g_signal_handler_disconnect (target, handler_id);
        }
    }
}

void
egg_signal_group_block (EggSignalGroup *self)
{
  g_return_if_fail (EGG_IS_SIGNAL_GROUP (self));

  self->block_count++;

  if (self->target != NULL)
    {
      gsize i;

      for (i = 0; i < self->handlers->len; i++)
        {
          SignalHandler *handler;

          handler = g_ptr_array_index (self->handlers, i);

          g_assert (handler != NULL);
          g_assert (handler->detailed_signal != NULL);
          g_assert (handler->closure != NULL);
          g_assert_cmpint (handler->handler_id, !=, 0);

          g_signal_handler_block (self->target, handler->handler_id);
        }
    }
}

void
egg_signal_group_unblock (EggSignalGroup *self)
{
  g_return_if_fail (EGG_IS_SIGNAL_GROUP (self));

  self->block_count--;

  if (self->target != NULL)
    {
      gsize i;

      for (i = 0; i < self->handlers->len; i++)
        {
          SignalHandler *handler;

          handler = g_ptr_array_index (self->handlers, i);

          g_assert (handler != NULL);
          g_assert (handler->detailed_signal != NULL);
          g_assert (handler->closure != NULL);
          g_assert_cmpint (handler->handler_id, !=, 0);

          g_signal_handler_unblock (self->target, handler->handler_id);
        }
    }
}

/**
 * egg_signal_group_get_target:
 *
 * Gets the target instance for the signal group.
 *
 * All signals that are registered will be connected
 * or disconnected when this property changes.
 *
 * Returns: (transfer none): The #EggSignalGroup:target property.
 */
gpointer
egg_signal_group_get_target (EggSignalGroup *self)
{
  g_return_val_if_fail (EGG_IS_SIGNAL_GROUP (self), NULL);

  return (gpointer)self->target;
}

/**
 * egg_signal_group_set_target:
 * @self: An #EggSignalGroup.
 * @target: (nullable) (ctype GObject*): The instance for which to connect signals.
 *
 * Sets the target instance to connect signals to. Any signal that has been registered
 * with egg_signal_group_connect_object() or similar functions will be connected to this
 * object.
 *
 * If #EggSignalGroup:target was previously set, signals will be disconnected from that
 * object prior to connecting to this object.
 */
void
egg_signal_group_set_target (EggSignalGroup *self,
                             gpointer        target)
{
  g_return_if_fail (EGG_IS_SIGNAL_GROUP (self));
  g_return_if_fail (!target || G_IS_OBJECT (target));

  /*
   * We cannot give meaningful warnings if EggSignalGroup:target is set before
   * EggSignalGroup:target-type. We might be able to delay some of this until
   * constructed, but that is more effort than it is worth right now.
   */
  if ((target != NULL) &&
      (self->target_type != G_TYPE_NONE) &&
      !g_type_is_a (G_OBJECT_TYPE (target), self->target_type))
    {
      g_warning ("Attempt to set EggSignalGroup:target to something other than %s",
                 g_type_name (self->target_type));
      return;
    }

  if (target != self->target)
    {
      egg_signal_group_unbind (self);
      egg_signal_group_bind (self, target);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_TARGET]);
    }
}

static void
signal_handler_free (gpointer data)
{
  SignalHandler *handler = data;

  if (handler != NULL)
    {
      g_clear_pointer (&handler->closure, g_closure_unref);
      handler->handler_id = 0;
      handler->detailed_signal = NULL;
      handler->connect_after = FALSE;
      g_slice_free (SignalHandler, handler);
    }
}

static void
egg_signal_group_dispose (GObject *object)
{
  EggSignalGroup *self = (EggSignalGroup *)object;

  egg_signal_group_unbind (self);
  g_clear_pointer (&self->handlers, g_ptr_array_unref);

  G_OBJECT_CLASS (egg_signal_group_parent_class)->dispose (object);
}

static void
egg_signal_group_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EggSignalGroup *self = EGG_SIGNAL_GROUP (object);

  switch (prop_id)
    {
    case PROP_TARGET:
      g_value_set_object (value, egg_signal_group_get_target (self));
      break;

    case PROP_TARGET_TYPE:
      g_value_set_gtype (value, self->target_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_signal_group_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EggSignalGroup *self = EGG_SIGNAL_GROUP (object);

  switch (prop_id)
    {
    case PROP_TARGET:
      egg_signal_group_set_target (self, g_value_get_object (value));
      break;

    case PROP_TARGET_TYPE:
      self->target_type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_signal_group_class_init (EggSignalGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = egg_signal_group_dispose;
  object_class->get_property = egg_signal_group_get_property;
  object_class->set_property = egg_signal_group_set_property;

  gParamSpecs [PROP_TARGET] =
    g_param_spec_object ("target",
                         _("Target"),
                         _("The target instance for which to connect signals."),
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TARGET, gParamSpecs [PROP_TARGET]);

  gParamSpecs [PROP_TARGET_TYPE] =
    g_param_spec_gtype ("target-type",
                        _("Target Type"),
                        _("The GType of the target property."),
                        G_TYPE_OBJECT,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TARGET_TYPE, gParamSpecs [PROP_TARGET_TYPE]);
}

static void
egg_signal_group_init (EggSignalGroup *self)
{
  self->handlers = g_ptr_array_new_with_free_func (signal_handler_free);
  self->target_type = G_TYPE_NONE;
}

EggSignalGroup *
egg_signal_group_new (GType target_type)
{
  return g_object_new (EGG_TYPE_SIGNAL_GROUP,
                       "target-type", target_type,
                       NULL);
}

void
egg_signal_group_connect_object (EggSignalGroup *self,
                                 const gchar    *detailed_signal,
                                 GCallback       callback,
                                 gpointer        data,
                                 GConnectFlags   flags)
{
  SignalHandler *handler;
  GClosure *closure;

  g_return_if_fail (EGG_IS_SIGNAL_GROUP (self));
  g_return_if_fail (detailed_signal != NULL);
  g_return_if_fail (callback != NULL);

  if ((flags & G_CONNECT_SWAPPED) != 0)
    closure = g_cclosure_new_object_swap (callback, data);
  else
    closure = g_cclosure_new_object (callback, data);

  handler = g_slice_new0 (SignalHandler);
  handler->detailed_signal = g_intern_string (detailed_signal);
  handler->closure = g_closure_ref (closure);
  handler->connect_after = ((flags & G_CONNECT_AFTER) != 0);

  g_closure_sink (closure);

  g_ptr_array_add (self->handlers, handler);

  if (self->target != NULL)
    egg_signal_group_bind_handler (self, handler);
}