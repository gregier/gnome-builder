/* ide-source-view-movement-helper.c
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

#define G_LOG_DOMAIN "ide-source-view"

#include "ide-debug.h"
#include "ide-enums.h"
#include "ide-source-iter.h"
#include "ide-source-view-movements.h"
#include "ide-vim-iter.h"

#define ANCHOR_BEGIN "SELECTION_ANCHOR_BEGIN"
#define ANCHOR_END   "SELECTION_ANCHOR_END"

#define TRACE_ITER(iter) \
  IDE_TRACE_MSG("%d:%d", gtk_text_iter_get_line(iter), \
                gtk_text_iter_get_line_offset(iter))

typedef struct
{
  IdeSourceView         *self;
  /* The target_offset contains the ideal character line_offset. This can
   * sometimes be further forward than designed when the line does not have
   * enough characters to get back to the original position. -1 indicates
   * no preference.
   */
  gint                  *target_offset;
  IdeSourceViewMovement  type;                        /* Type of movement */
  GtkTextIter            insert;                      /* Current insert cursor location */
  GtkTextIter            selection;                   /* Current selection cursor location */
  gint                   count;                       /* Repeat count for movement */
  gunichar               modifier;                    /* For forward/backward char search */
  guint                  extend_selection : 1;        /* If selection should be extended */
  guint                  exclusive : 1;               /* See ":help exclusive" in vim */
  guint                  ignore_select : 1;           /* Don't update selection after movement */
  guint                  ignore_target_offset : 1;    /* Don't propagate new line offset */
  guint                  ignore_scroll_to_insert : 1; /* Don't scroll to insert mark */
} Movement;

typedef struct
{
  gunichar jump_to;
  gunichar jump_from;
  guint    depth;
} MatchingBracketState;

static gboolean
is_single_line_selection (const GtkTextIter *begin,
                          const GtkTextIter *end)
{
  if (gtk_text_iter_compare (begin, end) < 0)
    return ((gtk_text_iter_get_line_offset (begin) == 0) &&
            (gtk_text_iter_get_line_offset (end) == 0) &&
            ((gtk_text_iter_get_line (begin) + 1) ==
             gtk_text_iter_get_line (end)));
  else
    return ((gtk_text_iter_get_line_offset (begin) == 0) &&
            (gtk_text_iter_get_line_offset (end) == 0) &&
            ((gtk_text_iter_get_line (end) + 1) ==
             gtk_text_iter_get_line (begin)));
}

static gboolean
is_single_char_selection (const GtkTextIter *begin,
                          const GtkTextIter *end)
{
  GtkTextIter tmp;

  g_assert (begin);
  g_assert (end);

  tmp = *begin;
  if (gtk_text_iter_forward_char (&tmp) && gtk_text_iter_equal (&tmp, end))
    return TRUE;

  tmp = *end;
  if (gtk_text_iter_forward_char (&tmp) && gtk_text_iter_equal (&tmp, begin))
    return TRUE;

  return FALSE;
}

static gboolean
text_iter_forward_to_nonspace_captive (GtkTextIter *iter)
{
  while (!gtk_text_iter_ends_line (iter) && g_unichar_isspace (gtk_text_iter_get_char (iter)))
    if (!gtk_text_iter_forward_char (iter))
      return FALSE;

  return !g_unichar_isspace (gtk_text_iter_get_char (iter));
}

static void
select_range (Movement    *mv,
              GtkTextIter *insert_iter,
              GtkTextIter *selection_iter)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextMark *selection;
  gint insert_off;
  gint selection_off;

  g_assert (insert_iter);
  g_assert (selection_iter);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));
  insert = gtk_text_buffer_get_insert (buffer);
  selection = gtk_text_buffer_get_selection_bound (buffer);

  mv->ignore_select = TRUE;

  /*
   * If the caller is requesting that we select a single character, we will
   * keep the iter before that character. This more closely matches the visual
   * mode in VIM.
   */
  insert_off = gtk_text_iter_get_offset (insert_iter);
  selection_off = gtk_text_iter_get_offset (selection_iter);
  if ((insert_off - selection_off) == 1)
    gtk_text_iter_order (insert_iter, selection_iter);

  gtk_text_buffer_move_mark (buffer, insert, insert_iter);
  gtk_text_buffer_move_mark (buffer, selection, selection_iter);
}

static void
ensure_anchor_selected (Movement *mv)
{
  GtkTextBuffer *buffer;
  GtkTextMark *selection_mark;
  GtkTextMark *insert_mark;
  GtkTextIter anchor_begin;
  GtkTextIter anchor_end;
  GtkTextIter insert_iter;
  GtkTextIter selection_iter;
  GtkTextMark *selection_anchor_begin;
  GtkTextMark *selection_anchor_end;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));

  selection_anchor_begin = gtk_text_buffer_get_mark (buffer, ANCHOR_BEGIN);
  selection_anchor_end = gtk_text_buffer_get_mark (buffer, ANCHOR_END);

  if (!selection_anchor_begin || !selection_anchor_end)
    return;

  gtk_text_buffer_get_iter_at_mark (buffer, &anchor_begin, selection_anchor_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &anchor_end, selection_anchor_end);

  insert_mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &insert_iter, insert_mark);

  selection_mark = gtk_text_buffer_get_selection_bound (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &selection_iter, selection_mark);

  if ((gtk_text_iter_compare (&selection_iter, &anchor_end) < 0) &&
      (gtk_text_iter_compare (&insert_iter, &anchor_end) < 0))
    {
      if (gtk_text_iter_compare (&insert_iter, &selection_iter) < 0)
        select_range (mv, &insert_iter, &anchor_end);
      else
        select_range (mv, &anchor_end, &selection_iter);
    }
  else if ((gtk_text_iter_compare (&selection_iter, &anchor_begin) > 0) &&
           (gtk_text_iter_compare (&insert_iter, &anchor_begin) > 0))
    {
      if (gtk_text_iter_compare (&insert_iter, &selection_iter) < 0)
        select_range (mv, &anchor_begin, &selection_iter);
      else
        select_range (mv, &insert_iter, &anchor_begin);
    }
}

static gboolean
text_iter_forward_to_empty_line (GtkTextIter *iter,
                                 GtkTextIter *bounds)
{
  if (!gtk_text_iter_forward_char (iter))
    return FALSE;

  while (gtk_text_iter_compare (iter, bounds) < 0)
    {
      if (gtk_text_iter_starts_line (iter) && gtk_text_iter_ends_line (iter))
        return TRUE;
      if (!gtk_text_iter_forward_char (iter))
        return FALSE;
    }

  return FALSE;
}

static void
ide_source_view_movements_get_selection (Movement *mv)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;

  g_assert (mv);
  g_assert (IDE_IS_SOURCE_VIEW (mv->self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));

  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &mv->insert, mark);

  mark = gtk_text_buffer_get_selection_bound (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &mv->selection, mark);
}

static void
ide_source_view_movements_select_range (Movement *mv)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;

  g_assert (mv);
  g_assert (IDE_IS_SOURCE_VIEW (mv->self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));

  if (mv->extend_selection)
    gtk_text_buffer_select_range (buffer, &mv->insert, &mv->selection);
  else
    gtk_text_buffer_select_range (buffer, &mv->insert, &mv->insert);

  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (mv->self), mark);
}

static void
ide_source_view_movements_nth_char (Movement *mv)
{
  gtk_text_iter_set_line_offset (&mv->insert, 0);

  for (; mv->count > 0; mv->count--)
    {
      if (gtk_text_iter_ends_line (&mv->insert))
        break;
      gtk_text_iter_forward_char (&mv->insert);
    }

  if (!mv->exclusive)
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_previous_char (Movement *mv)
{
  mv->count = MAX (1, mv->count);

  for (; mv->count; mv->count--)
    {
      if (gtk_text_iter_starts_line (&mv->insert))
        break;
      gtk_text_iter_backward_char (&mv->insert);
    }

  if (!mv->exclusive)
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_next_char (Movement *mv)
{
  mv->count = MAX (1, mv->count);

  for (; mv->count; mv->count--)
    {
      if (gtk_text_iter_ends_line (&mv->insert))
        break;
      gtk_text_iter_forward_char (&mv->insert);
    }

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_first_char (Movement *mv)
{
  gtk_text_iter_set_line_offset (&mv->insert, 0);
}

static void
ide_source_view_movements_first_nonspace_char (Movement *mv)
{
  gunichar ch;

  if (gtk_text_iter_get_line_offset (&mv->insert) != 0)
    gtk_text_iter_set_line_offset (&mv->insert, 0);

  while (!gtk_text_iter_ends_line (&mv->insert) &&
         (ch = gtk_text_iter_get_char (&mv->insert)) &&
         g_unichar_isspace (ch))
    gtk_text_iter_forward_char (&mv->insert);

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_line_chars (Movement *mv)
{
  GtkTextIter orig = mv->insert;

  /*
   * Selects the current position up to the first nonspace character.
   * If the cursor is at the line start, we will select the newline.
   * If only whitespace exists, we will select line offset of 0.
   */

  if (gtk_text_iter_starts_line (&mv->insert))
    {
      gtk_text_iter_backward_char (&mv->insert);
    }
  else
    {
      gunichar ch;

      gtk_text_iter_set_line_offset (&mv->insert, 0);

      while (!gtk_text_iter_ends_line (&mv->insert) &&
             (ch = gtk_text_iter_get_char (&mv->insert)) &&
             g_unichar_isspace (ch))
        gtk_text_iter_forward_char (&mv->insert);

      if (gtk_text_iter_ends_line (&mv->insert) ||
          (gtk_text_iter_compare (&orig, &mv->insert) <= 0))
        gtk_text_iter_set_line_offset (&mv->insert, 0);
    }

  if (!mv->exclusive)
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_line_end (Movement *mv)
{
  if (!gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_to_line_end (&mv->insert);

  if (!mv->exclusive)
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_middle_char (Movement *mv)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (mv->self);
  GdkWindow *window;
  GdkRectangle rect;
  guint line_offset;
  int width;
  int chars_in_line;

  gtk_text_view_get_iter_location (text_view, &mv->insert, &rect);
  window = gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT);

  width = gdk_window_get_width (window);
  if (rect.width <= 0)
    return;

  chars_in_line = width / rect.width;
  if (chars_in_line == 0)
    return;

  gtk_text_iter_set_line_offset (&mv->insert, 0);

  for (line_offset = chars_in_line / 2; line_offset; line_offset--)
    if (!gtk_text_iter_forward_char (&mv->insert))
      break;

  if (!mv->exclusive)
    if (!gtk_text_iter_ends_line (&mv->insert))
      gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_last_char (Movement *mv)
{
  if (!gtk_text_iter_ends_line (&mv->insert))
    {
      gtk_text_iter_forward_to_line_end (&mv->insert);
      if (mv->exclusive && !gtk_text_iter_starts_line (&mv->insert))
        gtk_text_iter_backward_char (&mv->insert);
    }
}

static void
ide_source_view_movements_first_line (Movement *mv)
{
  gtk_text_iter_set_line (&mv->insert, mv->count);
  gtk_text_iter_set_line_offset (&mv->insert, 0);
}

static void
ide_source_view_movements_nth_line (Movement *mv)
{
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));

  if (mv->count < 1)
    gtk_text_buffer_get_end_iter (buffer, &mv->insert);
  else
    gtk_text_iter_set_line (&mv->insert, mv->count - 1);

  gtk_text_iter_set_line_offset (&mv->insert, 0);

  while (!gtk_text_iter_ends_line (&mv->insert) &&
         g_unichar_isspace (gtk_text_iter_get_char (&mv->insert)))
    if (!gtk_text_iter_forward_char (&mv->insert))
      break;
}

static void
ide_source_view_movements_last_line (Movement *mv)
{
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));
  gtk_text_buffer_get_end_iter (buffer, &mv->insert);
  gtk_text_iter_set_line_offset (&mv->insert, 0);

  if (mv->count)
    {
      gint line;

      line = gtk_text_iter_get_line (&mv->insert) - mv->count;
      gtk_text_iter_set_line (&mv->insert, MAX (0, line));
    }
}

static void
ide_source_view_movements_next_line (Movement *mv)
{
  GtkTextBuffer *buffer;
  gboolean has_selection;
  guint line;
  guint offset = 0;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));

  /* check for linewise */
  has_selection = !gtk_text_iter_equal (&mv->insert, &mv->selection) || !mv->exclusive;

  line = gtk_text_iter_get_line (&mv->insert);

  if ((*mv->target_offset) > 0)
    offset = *mv->target_offset;

  /*
   * If we have a whole line selected (from say `V`), then we need to swap
   * the cursor and selection. This feels to me like a slight bit of a hack.
   * There may be cause to actually have a selection mode and know the type
   * of selection (line vs individual characters).
   */
  if (is_single_line_selection (&mv->insert, &mv->selection))
    {
      guint target_line;

      if (gtk_text_iter_compare (&mv->insert, &mv->selection) < 0)
        gtk_text_iter_order (&mv->selection, &mv->insert);

      target_line = gtk_text_iter_get_line (&mv->insert) + 1;
      gtk_text_iter_set_line (&mv->insert, target_line);

      if (target_line != gtk_text_iter_get_line (&mv->insert))
        goto select_to_end;

      select_range (mv, &mv->insert, &mv->selection);
      ensure_anchor_selected (mv);
      return;
    }

  if (is_single_char_selection (&mv->insert, &mv->selection))
    {
      if (gtk_text_iter_compare (&mv->insert, &mv->selection) < 0)
        *mv->target_offset = ++offset;
    }

  gtk_text_buffer_get_iter_at_line (buffer, &mv->insert, line + 1);
  if ((line + 1) == gtk_text_iter_get_line (&mv->insert))
    {
      for (; offset; offset--)
        if (!gtk_text_iter_ends_line (&mv->insert))
          if (!gtk_text_iter_forward_char (&mv->insert))
            break;
      if (has_selection)
        {
          select_range (mv, &mv->insert, &mv->selection);
          ensure_anchor_selected (mv);
        }
      else
        gtk_text_buffer_select_range (buffer, &mv->insert, &mv->insert);
    }
  else
    {
select_to_end:
      gtk_text_buffer_get_end_iter (buffer, &mv->insert);
      if (has_selection)
        {
          select_range (mv, &mv->insert, &mv->selection);
          ensure_anchor_selected (mv);
        }
      else
        gtk_text_buffer_select_range (buffer, &mv->insert, &mv->insert);
    }

  /* make sure selection/insert are up to date */
  if (!gtk_text_buffer_get_has_selection (buffer))
    mv->selection = mv->insert;
}

static void
ide_source_view_movements_previous_line (Movement *mv)
{
  GtkTextBuffer *buffer;
  gboolean has_selection;
  guint line;
  guint offset = 0;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));

  /* check for linewise */
  has_selection = !gtk_text_iter_equal (&mv->insert, &mv->selection) || !mv->exclusive;

  line = gtk_text_iter_get_line (&mv->insert);

  if ((*mv->target_offset) > 0)
    offset = *mv->target_offset;

  if (line == 0)
    return;

  /*
   * If we have a whole line selected (from say `V`), then we need to swap the cursor and
   * selection. This feels to me like a slight bit of a hack.  There may be cause to actually have
   * a selection mode and know the type of selection (line vs individual characters).
   */
  if (is_single_line_selection (&mv->insert, &mv->selection))
    {
      if (gtk_text_iter_compare (&mv->insert, &mv->selection) > 0)
        gtk_text_iter_order (&mv->insert, &mv->selection);
      gtk_text_iter_set_line (&mv->insert, gtk_text_iter_get_line (&mv->insert) - 1);
      select_range (mv, &mv->insert, &mv->selection);
      ensure_anchor_selected (mv);
      return;
    }

  if (is_single_char_selection (&mv->insert, &mv->selection))
    {
      if (gtk_text_iter_compare (&mv->insert, &mv->selection) > 0)
        {
          if (offset)
            --offset;
          *mv->target_offset = offset;
        }
    }

  gtk_text_buffer_get_iter_at_line (buffer, &mv->insert, line - 1);
  if ((line - 1) == gtk_text_iter_get_line (&mv->insert))
    {
      for (; offset; offset--)
        if (!gtk_text_iter_ends_line (&mv->insert))
          if (!gtk_text_iter_forward_char (&mv->insert))
            break;

      if (has_selection)
        {
          if (gtk_text_iter_equal (&mv->insert, &mv->selection))
            gtk_text_iter_backward_char (&mv->insert);
          select_range (mv, &mv->insert, &mv->selection);
          ensure_anchor_selected (mv);
        }
      else
        gtk_text_buffer_select_range (buffer, &mv->insert, &mv->insert);
    }

  /* make sure selection/insert are up to date */
  if (!gtk_text_buffer_get_has_selection (buffer))
    mv->selection = mv->insert;
}

static void
ide_source_view_movements_screen_top (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GdkRectangle rect;

  ide_source_view_get_visible_rect (mv->self, &rect);
  gtk_text_view_get_iter_at_location (text_view, &mv->insert, rect.x, rect.y);
  gtk_text_iter_set_line_offset (&mv->insert, 0);
}

static void
ide_source_view_movements_screen_middle (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GdkRectangle rect;

  ide_source_view_get_visible_rect (mv->self, &rect);
  gtk_text_view_get_iter_at_location (text_view, &mv->insert, rect.x, rect.y + (rect.height / 2));
  gtk_text_iter_set_line_offset (&mv->insert, 0);
}

static void
ide_source_view_movements_screen_bottom (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GdkRectangle rect;

  ide_source_view_get_visible_rect (mv->self, &rect);
  gtk_text_view_get_iter_at_location (text_view, &mv->insert, rect.x, rect.y + rect.height - 1);
  gtk_text_iter_set_line_offset (&mv->insert, 0);
}

static void
ide_source_view_movements_scroll_by_lines (Movement *mv,
                                           gint      lines)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GtkAdjustment *vadj;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GdkRectangle rect;
  gdouble amount;
  gdouble value;
  gdouble upper;

  if (lines == 0)
    return;

  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (mv->self));
  buffer = gtk_text_view_get_buffer (text_view);

  gtk_text_buffer_get_bounds (buffer, &begin, &end);

  if (lines > 0)
    {
      if (gtk_text_iter_get_line (&end) == gtk_text_iter_get_line (&mv->insert))
        return;
    }
  else if (lines < 0)
    {
      if (gtk_text_iter_get_line (&begin) == gtk_text_iter_get_line (&mv->insert))
        return;
    }
  else
    g_assert_not_reached ();

  gtk_text_view_get_iter_location (text_view, &mv->insert, &rect);

  amount = lines * rect.height;

  value = gtk_adjustment_get_value (vadj);
  upper = gtk_adjustment_get_upper (vadj);
  gtk_adjustment_set_value (vadj, CLAMP (value + amount, 0, upper));

  mv->ignore_scroll_to_insert = TRUE;
  ide_source_view_place_cursor_onscreen (mv->self);
}

static void
ide_source_view_movements_scroll (Movement *mv)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  gint count = MAX (1, mv->count);

  if (mv->type == IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN)
    count = -count;

  ide_source_view_movements_scroll_by_lines (mv, count);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &mv->insert, mark);
  ide_source_view_move_mark_onscreen (mv->self, mark);
}

static void
ide_source_view_movements_move_page (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GdkRectangle rect;
  GtkTextIter iter_top;
  GtkTextIter iter_bottom;
  GtkTextIter scroll_iter;
  gint scrolloff;
  gint half_page;
  gint line_top;
  gint line_bottom;

  gtk_text_view_get_visible_rect (text_view, &rect);
  gtk_text_view_get_iter_at_location (text_view, &iter_top, rect.x, rect.y);
  gtk_text_view_get_iter_at_location (text_view, &iter_bottom,
                                      rect.x + rect.width,
                                      rect.y + rect.height);

  buffer = gtk_text_view_get_buffer (text_view);

  scrolloff = ide_source_view_get_scroll_offset (mv->self);
  line_top = gtk_text_iter_get_line (&iter_top);
  line_bottom = gtk_text_iter_get_line (&iter_bottom);

  half_page = MAX (1, (line_bottom - line_top) / 2);

  switch ((int)mv->type)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_UP:
      ide_source_view_movements_scroll_by_lines (mv, -half_page);
      gtk_text_iter_backward_lines (&mv->insert, half_page);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_DOWN:
      ide_source_view_movements_scroll_by_lines (mv, half_page);
      gtk_text_iter_forward_lines (&mv->insert, half_page);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP:
      gtk_text_buffer_get_iter_at_line (buffer, &mv->insert, MAX (0, line_top - scrolloff));
      text_iter_forward_to_nonspace_captive (&mv->insert);
      ide_source_view_movements_select_range (mv);

      mark = gtk_text_buffer_get_mark (buffer, "scroll-mark");
      gtk_text_buffer_get_iter_at_line (buffer, &scroll_iter, line_top);
      gtk_text_buffer_move_mark (buffer, mark, &scroll_iter);
      gtk_text_view_scroll_to_mark (text_view, mark, 0.0, TRUE, 1.0, 1.0);

      mv->ignore_select = TRUE;
      mv->ignore_scroll_to_insert = TRUE;
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN:
      gtk_text_buffer_get_iter_at_line (buffer, &mv->insert, line_bottom + scrolloff);
      text_iter_forward_to_nonspace_captive (&mv->insert);
      ide_source_view_movements_select_range (mv);

      mark = gtk_text_buffer_get_mark (buffer, "scroll-mark");
      gtk_text_buffer_get_iter_at_line (buffer, &scroll_iter, line_bottom);
      gtk_text_buffer_move_mark (buffer, mark, &scroll_iter);
      gtk_text_view_scroll_to_mark (text_view, mark, 0.0, TRUE, 1.0, 0.0);

      mv->ignore_select = TRUE;
      mv->ignore_scroll_to_insert = TRUE;
      break;

    default:
      g_assert_not_reached();
    }
}

static gboolean
bracket_predicate (gunichar ch,
                   gpointer user_data)
{
  MatchingBracketState *state = user_data;

  if (ch == state->jump_from)
    state->depth++;
  else if (ch == state->jump_to)
    state->depth--;

  return (state->depth == 0);
}

static void
ide_source_view_movements_match_special (Movement *mv)
{
  MatchingBracketState state;
  GtkTextIter copy;
  gboolean is_forward = FALSE;
  gboolean ret;

  copy = mv->insert;

  state.depth = 1;
  state.jump_from = gtk_text_iter_get_char (&mv->insert);

  switch (state.jump_from)
    {
    case '{':
      state.jump_to = '}';
      is_forward = TRUE;
      break;

    case '[':
      state.jump_to = ']';
      is_forward = TRUE;
      break;

    case '(':
      state.jump_to = ')';
      is_forward = TRUE;
      break;

    case '}':
      state.jump_to = '{';
      is_forward = FALSE;
      break;

    case ']':
      state.jump_to = '[';
      is_forward = FALSE;
      break;

    case ')':
      state.jump_to = '(';
      is_forward = FALSE;
      break;

    default:
      return;
    }

  if (is_forward)
    ret = gtk_text_iter_forward_find_char (&mv->insert, bracket_predicate, &state, NULL);
  else
    ret = gtk_text_iter_backward_find_char (&mv->insert, bracket_predicate, &state, NULL);

  if (!ret)
    mv->insert = copy;
  else if (!mv->exclusive)
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_scroll_center (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GtkTextMark *insert;
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (text_view);
  insert = gtk_text_buffer_get_insert (buffer);

  switch ((int)mv->type)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_BOTTOM:
      ide_source_view_scroll_to_mark (mv->self, insert, 0.0, TRUE, 1.0, 1.0, TRUE);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_TOP:
      ide_source_view_scroll_to_mark (mv->self, insert, 0.0, TRUE, 1.0, 0.0, TRUE);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_CENTER:
      ide_source_view_scroll_to_mark (mv->self, insert, 0.0, TRUE, 1.0, 0.5, TRUE);
      break;

    default:
      break;
    }
}

static void
ide_source_view_movements_next_word_end (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_vim_iter_forward_word_end (&mv->insert);

  /* prefer an empty line before word */
  text_iter_forward_to_empty_line (&copy, &mv->insert);
  if (gtk_text_iter_compare (&copy, &mv->insert) < 0)
    mv->insert = copy;

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_next_full_word_end (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_vim_iter_forward_WORD_end (&mv->insert);

  /* prefer an empty line before word */
  text_iter_forward_to_empty_line (&copy, &mv->insert);
  if (gtk_text_iter_compare (&copy, &mv->insert) < 0)
    mv->insert = copy;

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_next_word_start (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_vim_iter_forward_word_start (&mv->insert);

  /* prefer an empty line before word */
  text_iter_forward_to_empty_line (&copy, &mv->insert);
  if (gtk_text_iter_compare (&copy, &mv->insert) < 0)
    mv->insert = copy;

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_next_full_word_start (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_vim_iter_forward_WORD_start (&mv->insert);

  /* prefer an empty line before word */
  text_iter_forward_to_empty_line (&copy, &mv->insert);
  if (gtk_text_iter_compare (&copy, &mv->insert) < 0)
    mv->insert = copy;

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_previous_word_start (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_source_iter_backward_visible_word_start (&mv->insert);

  /*
   * Vim treats an empty line as a word.
   */
  if (gtk_text_iter_backward_char (&copy))
    if (gtk_text_iter_get_char (&copy) == '\n')
      mv->insert = copy;

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_previous_full_word_start (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_source_iter_backward_full_word_start (&mv->insert);

  /*
   * Vim treats an empty line as a word.
   */
  if (gtk_text_iter_backward_char (&copy))
    if (gtk_text_iter_get_char (&copy) == '\n')
      mv->insert = copy;

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_previous_word_end (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_vim_iter_backward_word_end (&mv->insert);

  /*
   * Vim treats an empty line as a word.
   */
  while ((gtk_text_iter_compare (&copy, &mv->insert) > 0) &&
         gtk_text_iter_backward_char (&copy))
    {
      if (gtk_text_iter_starts_line (&copy) &&
          gtk_text_iter_ends_line (&copy))
        mv->insert = copy;
    }

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_previous_full_word_end (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_vim_iter_backward_WORD_end (&mv->insert);

  /*
   * Vim treats an empty line as a word.
   */
  while ((gtk_text_iter_compare (&copy, &mv->insert) > 0) &&
         gtk_text_iter_backward_char (&copy))
    {
      if (gtk_text_iter_starts_line (&copy) &&
          gtk_text_iter_ends_line (&copy))
        mv->insert = copy;
    }

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_paragraph_start (Movement *mv)
{
  _ide_vim_iter_backward_paragraph_start (&mv->insert);

  if (mv->exclusive)
    {
      while (g_unichar_isspace (gtk_text_iter_get_char (&mv->insert)))
        {
          if (!gtk_text_iter_forward_char (&mv->insert))
            break;
        }
    }
}

static void
ide_source_view_movements_paragraph_end (Movement *mv)
{
  _ide_vim_iter_forward_paragraph_end (&mv->insert);

  if (mv->exclusive)
    {
      gboolean adjust = FALSE;

      while (g_unichar_isspace (gtk_text_iter_get_char (&mv->insert)))
        {
          adjust = TRUE;
          if (!gtk_text_iter_backward_char (&mv->insert))
            break;
        }

      if (adjust)
        gtk_text_iter_forward_char (&mv->insert);
    }
}

static void
ide_source_view_movements_sentence_start (Movement *mv)
{
  _ide_vim_iter_backward_sentence_start (&mv->insert);
}

static void
ide_source_view_movements_sentence_end (Movement *mv)
{
  _ide_vim_iter_forward_sentence_end (&mv->insert);
}

static void
ide_source_view_movements_line_percentage (Movement *mv)
{
  GtkTextBuffer *buffer;
  GtkTextIter end;
  guint end_line;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));
  gtk_text_buffer_get_end_iter (buffer, &end);
  end_line = gtk_text_iter_get_line (&end);

  if (!mv->count)
    {
      gtk_text_iter_set_line (&mv->insert, 0);
    }
  else
    {
      guint line;

      mv->count = MAX (1, mv->count);
      line = (float)end_line * (mv->count / 100.0);
      gtk_text_iter_set_line (&mv->insert, line);
    }

  mv->count = 0;

  ide_source_view_movements_first_nonspace_char (mv);
}

static void
ide_source_view_movements_previous_unmatched (Movement *mv,
                                              gunichar  target,
                                              gunichar  opposite)
{
  GtkTextIter copy;
  guint count = 1;

  g_assert (mv);
  g_assert (target);
  g_assert (opposite);

  copy = mv->insert;

  do
    {
      gunichar ch;

      if (!gtk_text_iter_backward_char (&mv->insert))
        {
          mv->insert = copy;
          return;
        }

      ch = gtk_text_iter_get_char (&mv->insert);

      if (ch == target)
        count--;
      else if (ch == opposite)
        count++;

      if (!count)
        {
          if (!mv->exclusive)
            gtk_text_iter_forward_char (&mv->insert);
          return;
        }
    }
  while (TRUE);

  g_assert_not_reached ();
}

static void
ide_source_view_movements_next_unmatched (Movement *mv,
                                          gunichar  target,
                                          gunichar  opposite)
{
  GtkTextIter copy;
  guint count = 1;

  g_assert (mv);
  g_assert (target);
  g_assert (opposite);

  copy = mv->insert;

  do
    {
      gunichar ch;

      if (!gtk_text_iter_forward_char (&mv->insert))
        {
          mv->insert = copy;
          return;
        }

      ch = gtk_text_iter_get_char (&mv->insert);

      if (ch == target)
        count--;
      else if (ch == opposite)
        count++;

      if (!count)
        {
          if (!mv->exclusive)
            gtk_text_iter_forward_char (&mv->insert);
          return;
        }
    }
  while (TRUE);

  g_assert_not_reached ();
}

static gboolean
find_match (gunichar ch,
            gpointer data)
{
  Movement *mv = data;

  return (mv->modifier == ch);
}

static void
ide_source_view_movements_next_match_modifier (Movement *mv)
{
  GtkTextIter insert;
  GtkTextIter bounds;

  bounds = insert = mv->insert;
  gtk_text_iter_forward_to_line_end (&bounds);

  if (gtk_text_iter_forward_find_char (&insert, find_match, mv, &bounds))
    {
      if (!mv->exclusive)
        gtk_text_iter_forward_char (&insert);
      mv->insert = insert;
    }
}

static void
ide_source_view_movements_previous_match_modifier (Movement *mv)
{
  GtkTextIter insert;
  GtkTextIter bounds;

  bounds = insert = mv->insert;
  gtk_text_iter_set_line_offset (&bounds, 0);

  if (gtk_text_iter_backward_find_char (&insert, find_match, mv, &bounds))
    {
      if (!mv->exclusive)
        gtk_text_iter_forward_char (&insert);
      mv->insert = insert;
    }
}

void
_ide_source_view_apply_movement (IdeSourceView         *self,
                                 IdeSourceViewMovement  movement,
                                 gboolean               extend_selection,
                                 gboolean               exclusive,
                                 guint                  count,
                                 gunichar               modifier,
                                 gint                  *target_offset)
{
  Movement mv = { 0 };
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  gsize i;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

#ifndef IDE_DISABLE_TRACE
  {
    GEnumValue *enum_value;
    GEnumClass *enum_class;

    enum_class = g_type_class_ref (IDE_TYPE_SOURCE_VIEW_MOVEMENT);
    enum_value = g_enum_get_value (enum_class, movement);
    IDE_TRACE_MSG ("movement(%s, extend_selection=%s, exclusive=%s, count=%u)",
                   enum_value->value_nick,
                   extend_selection ? "YES" : "NO",
                   exclusive ? "YES" : "NO",
                   count);
    g_type_class_unref (enum_class);
  }
#endif

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  insert = gtk_text_buffer_get_insert (buffer);

  mv.self = self;
  mv.target_offset = target_offset;
  mv.type = movement;
  mv.extend_selection = extend_selection;
  mv.exclusive = exclusive;
  mv.count = count;
  mv.ignore_select = FALSE;
  mv.ignore_target_offset = FALSE;
  mv.modifier = modifier;

  ide_source_view_movements_get_selection (&mv);

  switch (movement)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_OFFSET:
      gtk_text_iter_backward_chars (&mv.insert, MAX (1, mv.count));
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_OFFSET:
      gtk_text_iter_forward_chars (&mv.insert, MAX (1, mv.count));
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NTH_CHAR:
      ide_source_view_movements_nth_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_CHAR:
      ide_source_view_movements_previous_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_CHAR:
      ide_source_view_movements_next_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_CHAR:
      ide_source_view_movements_first_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_NONSPACE_CHAR:
      ide_source_view_movements_first_nonspace_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_MIDDLE_CHAR:
      ide_source_view_movements_middle_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LAST_CHAR:
      ide_source_view_movements_last_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_START:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_full_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_START:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_full_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_END:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_full_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_END:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_full_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_START:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_START:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_END:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_END:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SENTENCE_START:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_sentence_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SENTENCE_END:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_sentence_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PARAGRAPH_START:
      for (i = MAX (1, mv.count); i > 0; i--)
        {
          mv.exclusive = exclusive && i == 1;
          ide_source_view_movements_paragraph_start (&mv);
        }
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PARAGRAPH_END:
      for (i = MAX (1, mv.count); i > 0; i--)
        {
          mv.exclusive = exclusive && i == 1;
          ide_source_view_movements_paragraph_end (&mv);
        }
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_LINE:
      mv.ignore_target_offset = TRUE;
      mv.ignore_select = TRUE;
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_LINE:
      mv.ignore_target_offset = TRUE;
      mv.ignore_select = TRUE;
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_LINE:
      ide_source_view_movements_first_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NTH_LINE:
      ide_source_view_movements_nth_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LAST_LINE:
      ide_source_view_movements_last_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LINE_PERCENTAGE:
      ide_source_view_movements_line_percentage (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LINE_CHARS:
      ide_source_view_movements_line_chars (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LINE_END:
      ide_source_view_movements_line_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_UP:
    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_DOWN:
    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP:
    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_move_page (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN:
    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_UP:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_scroll (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_TOP:
      ide_source_view_movements_screen_top (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_MIDDLE:
      ide_source_view_movements_screen_middle (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_BOTTOM:
      ide_source_view_movements_screen_bottom (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_MATCH_SPECIAL:
      ide_source_view_movements_match_special (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_TOP:
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_CENTER:
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_BOTTOM:
      ide_source_view_movements_scroll_center (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_UNMATCHED_BRACE:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_unmatched (&mv, '{', '}');
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_UNMATCHED_BRACE:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_unmatched (&mv, '}', '{');
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_UNMATCHED_PAREN:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_unmatched (&mv, '(', ')');
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_UNMATCHED_PAREN:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_unmatched (&mv, ')', '(');
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_MATCH_MODIFIER:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_match_modifier (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_MATCH_MODIFIER:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_match_modifier (&mv);
      break;

    default:
      g_return_if_reached ();
    }

  if (!mv.ignore_select)
    ide_source_view_movements_select_range (&mv);

  if (!mv.ignore_target_offset)
    *target_offset = gtk_text_iter_get_line_offset (&mv.insert);

  if (!mv.ignore_scroll_to_insert)
    ide_source_view_scroll_mark_onscreen (self, insert);
}
