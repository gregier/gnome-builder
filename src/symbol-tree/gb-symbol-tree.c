/* gb-symbol-tree.c
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

#include "gb-symbol-tree.h"
#include "gb-symbol-tree-builder.h"

struct _GbSymbolTree
{
  GbTree parent_instance;
};

G_DEFINE_TYPE (GbSymbolTree, gb_symbol_tree, GB_TYPE_TREE)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_symbol_tree_new (void)
{
  return g_object_new (GB_TYPE_SYMBOL_TREE, NULL);
}

static void
gb_symbol_tree_finalize (GObject *object)
{
  GbSymbolTree *self = (GbSymbolTree *)object;

  G_OBJECT_CLASS (gb_symbol_tree_parent_class)->finalize (object);
}

static void
gb_symbol_tree_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbSymbolTree *self = GB_SYMBOL_TREE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_symbol_tree_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbSymbolTree *self = GB_SYMBOL_TREE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_symbol_tree_class_init (GbSymbolTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_symbol_tree_finalize;
  object_class->get_property = gb_symbol_tree_get_property;
  object_class->set_property = gb_symbol_tree_set_property;
}

static void
gb_symbol_tree_init (GbSymbolTree *self)
{
  GbTreeBuilder *builder;

  builder = gb_symbol_tree_builder_new ();
  gb_tree_add_builder (GB_TREE (self), builder);
}
