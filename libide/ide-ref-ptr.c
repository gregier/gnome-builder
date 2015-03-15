/* ide-ref-ptr.c
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

#include "ide-ref-ptr.h"

G_DEFINE_BOXED_TYPE (IdeRefPtr, ide_ref_ptr, ide_ref_ptr_ref, ide_ref_ptr_unref)

struct _IdeRefPtr
{
  volatile gint  ref_count;
  gpointer       data;
  GDestroyNotify free_func;
};

IdeRefPtr *
ide_ref_ptr_new (gpointer       data,
                 GDestroyNotify free_func)
{
  IdeRefPtr *self;

  self = g_new0 (IdeRefPtr, 1);
  self->ref_count = 1;
  self->data = data;
  self->free_func = free_func;

  return self;
}

IdeRefPtr *
ide_ref_ptr_ref (IdeRefPtr *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_ref_ptr_unref (IdeRefPtr *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      if (self->free_func)
        g_clear_pointer (&self->data, self->free_func);
    }
}

gpointer
ide_ref_ptr_get (IdeRefPtr *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  return self->data;
}
