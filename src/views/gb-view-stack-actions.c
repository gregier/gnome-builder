/* gb-view-stack-actions.c
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

#include "gb-view-stack.h"
#include "gb-view-stack-actions.h"
#include "gb-view-stack-private.h"

static void
gb_view_stack_actions_close (GSimpleAction *action,
                             GVariant      *param,
                             gpointer       user_data)
{
  GbViewStack *self = user_data;

  g_assert (GB_IS_VIEW_STACK (self));
}

static void
gb_view_stack_actions_move_left (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  GbViewStack *self = user_data;

  g_assert (GB_IS_VIEW_STACK (self));
}

static void
gb_view_stack_actions_move_right (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  GbViewStack *self = user_data;

  g_assert (GB_IS_VIEW_STACK (self));
}

static void
gb_view_stack_actions_save (GSimpleAction *action,
                            GVariant      *param,
                            gpointer       user_data)
{
  GbViewStack *self = user_data;

  g_assert (GB_IS_VIEW_STACK (self));
}

static void
gb_view_stack_actions_save_as (GSimpleAction *action,
                               GVariant      *param,
                               gpointer       user_data)
{
  GbViewStack *self = user_data;

  g_assert (GB_IS_VIEW_STACK (self));
}

static void
gb_view_stack_actions_split_down (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  GbViewStack *self = user_data;

  g_assert (GB_IS_VIEW_STACK (self));
}

static void
gb_view_stack_actions_split_left (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  GbViewStack *self = user_data;

  g_assert (GB_IS_VIEW_STACK (self));
}

static void
gb_view_stack_actions_split_right (GSimpleAction *action,
                                   GVariant      *param,
                                   gpointer       user_data)
{
  GbViewStack *self = user_data;

  g_assert (GB_IS_VIEW_STACK (self));
}

static const GActionEntry gGbViewStackActions[] = {
  { "close",       gb_view_stack_actions_close },
  { "move-left",   gb_view_stack_actions_move_left },
  { "move-right",  gb_view_stack_actions_move_right },
  { "save",        gb_view_stack_actions_save },
  { "save-as",     gb_view_stack_actions_save_as },
  { "split-down",  gb_view_stack_actions_split_down },
  { "split-left",  gb_view_stack_actions_split_left },
  { "split-right", gb_view_stack_actions_split_right },
};

void
gb_view_stack_actions_init (GbViewStack *self)
{
  GSimpleActionGroup *actions;

  g_assert (GB_IS_VIEW_STACK (self));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), gGbViewStackActions,
                                   G_N_ELEMENTS (gGbViewStackActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "view", G_ACTION_GROUP (actions));
}
