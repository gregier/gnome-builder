/* ide-ref-ptr.h
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

#ifndef IDE_REF_PTR_H
#define IDE_REF_PTR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_REF_PTR (ide_ref_ptr_get_type())

typedef struct _IdeRefPtr IdeRefPtr;

GType      ide_ref_ptr_get_type (void);
IdeRefPtr *ide_ref_ptr_new      (gpointer        data,
                                 GDestroyNotify  free_func);
IdeRefPtr *ide_ref_ptr_ref      (IdeRefPtr      *self);
void       ide_ref_ptr_unref    (IdeRefPtr      *self);
gpointer   ide_ref_ptr_get      (IdeRefPtr      *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeRefPtr, ide_ref_ptr_unref)

G_END_DECLS

#endif /* IDE_REF_PTR_H */
