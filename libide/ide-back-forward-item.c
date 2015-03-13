/* ide-back-forward-item.c
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

#include "ide-back-forward-item.h"
#include "ide-file.h"
#include "ide-source-location.h"

#define NUM_LINES_CHAIN_MAX 5

struct _IdeBackForwardItem
{
  IdeObject          parent_instance;
  IdeSourceLocation *location;
};

G_DEFINE_TYPE (IdeBackForwardItem, ide_back_forward_item, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_LOCATION,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

IdeBackForwardItem *
ide_back_forward_item_new (IdeContext        *context,
                           IdeSourceLocation *location)
{
  return g_object_new (IDE_TYPE_BACK_FORWARD_ITEM,
                       "context", context,
                       "location", location,
                       NULL);
}

gboolean
ide_back_forward_item_chain (IdeBackForwardItem *self,
                             IdeBackForwardItem *other)
{
  IdeSourceLocation *loc1;
  IdeSourceLocation *loc2;
  IdeFile *file1;
  IdeFile *file2;
  gint line1;
  gint line2;

  g_return_val_if_fail (IDE_IS_BACK_FORWARD_ITEM (self), FALSE);
  g_return_val_if_fail (IDE_IS_BACK_FORWARD_ITEM (other), FALSE);

  loc1 = ide_back_forward_item_get_location (self);
  loc2 = ide_back_forward_item_get_location (other);

  file1 = ide_source_location_get_file (loc1);
  file2 = ide_source_location_get_file (loc2);

  if (!ide_file_equal (file1, file2))
    return FALSE;

  line1 = ide_source_location_get_line (loc1);
  line2 = ide_source_location_get_line (loc2);

  if (ABS (line1 - line2) <= NUM_LINES_CHAIN_MAX)
    {
      self->location = ide_source_location_ref (loc2);
      ide_source_location_unref (loc1);
      return TRUE;
    }

  return FALSE;
}

IdeSourceLocation *
ide_back_forward_item_get_location (IdeBackForwardItem *self)
{
  g_return_val_if_fail (IDE_IS_BACK_FORWARD_ITEM (self), NULL);

  return self->location;
}

static void
ide_back_forward_item_set_location (IdeBackForwardItem *self,
                                    IdeSourceLocation  *location)
{
  g_return_if_fail (IDE_IS_BACK_FORWARD_ITEM (self));
  g_return_if_fail (location);

  if (location != self->location)
    {
      g_clear_pointer (&self->location, ide_source_location_unref);
      self->location = ide_source_location_ref (location);
    }
}

static void
ide_back_forward_item_finalize (GObject *object)
{
  IdeBackForwardItem *self = (IdeBackForwardItem *)object;

  g_clear_pointer (&self->location, ide_source_location_unref);

  G_OBJECT_CLASS (ide_back_forward_item_parent_class)->finalize (object);
}

static void
ide_back_forward_item_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeBackForwardItem *self = IDE_BACK_FORWARD_ITEM (object);

  switch (prop_id)
    {
    case PROP_LOCATION:
      g_value_set_boxed (value, ide_back_forward_item_get_location (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_back_forward_item_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeBackForwardItem *self = IDE_BACK_FORWARD_ITEM (object);

  switch (prop_id)
    {
    case PROP_LOCATION:
      ide_back_forward_item_set_location (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_back_forward_item_class_init (IdeBackForwardItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_back_forward_item_finalize;
  object_class->get_property = ide_back_forward_item_get_property;
  object_class->set_property = ide_back_forward_item_set_property;

  /**
   * IdeBackForwardItem:location:
   *
   * The #IdeBackForwardItem:location property contains the location within
   * a source file to navigate to.
   */
  gParamSpecs [PROP_LOCATION] =
    g_param_spec_boxed ("location",
                        _("Location"),
                        _("The location of the navigation item."),
                        IDE_TYPE_SOURCE_LOCATION,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LOCATION, gParamSpecs [PROP_LOCATION]);
}

static void
ide_back_forward_item_init (IdeBackForwardItem *self)
{
}
