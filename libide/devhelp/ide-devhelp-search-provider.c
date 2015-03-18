/* ide-devhelp-search-provider.c
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erick.red@gmail.com>
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

#define G_LOG_DOMAIN "devhelp-search"

#include "ide-devhelp-search-provider.h"

#include <ctype.h>
#include <glib/gi18n.h>
#include <devhelp/devhelp.h>

#include "ide-search-reducer.h"
#include "ide-search-result.h"
#include "ide-search-context.h"

struct _IdeDevhelpSearchProvider
{
  IdeSearchProvider  parent;

  DhBookManager     *book_manager;
  DhKeywordModel    *keywords_model;
};

G_DEFINE_TYPE (IdeDevhelpSearchProvider, ide_devhelp_search_provider, IDE_TYPE_SEARCH_PROVIDER)

static GQuark gQuarkLink;

static void
ide_devhelp_search_provider_populate (IdeSearchProvider *provider,
                                      IdeSearchContext  *context,
                                      const gchar       *search_terms,
                                      gsize              max_results,
                                      GCancellable      *cancellable)
{
  IdeDevhelpSearchProvider *self = (IdeDevhelpSearchProvider *)provider;
  g_auto(IdeSearchReducer) reducer = { 0 };
  IdeContext *idecontext;
  GtkTreeIter iter;
  gboolean valid;
  gint count = 0;;
  gint total;

  g_assert (IDE_IS_DEVHELP_SEARCH_PROVIDER (self));
  g_assert (IDE_IS_SEARCH_CONTEXT (context));
  g_assert (search_terms);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (search_terms [0] == '\0')
    {
      ide_search_context_provider_completed (context, provider);
      return;
    }

  idecontext = ide_object_get_context (IDE_OBJECT (provider));

  dh_keyword_model_filter (self->keywords_model, search_terms, NULL, NULL);

  ide_search_reducer_init (&reducer, context, provider, max_results);

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->keywords_model), &iter);
  total = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (self->keywords_model), NULL);

  while (valid)
    {
      g_autoptr(IdeSearchResult) result = NULL;
      g_autofree gchar *name = NULL;
      DhLink *link = NULL;
      gfloat score = (total - count) / (gfloat)total;

      gtk_tree_model_get (GTK_TREE_MODEL (self->keywords_model), &iter,
                          DH_KEYWORD_MODEL_COL_NAME, &name,
                          DH_KEYWORD_MODEL_COL_LINK, &link,
                          -1);

      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->keywords_model), &iter);

      /* we traverse from best to worst, so just break */
      if (!ide_search_reducer_accepts (&reducer, score))
        break;

      count++;

      if ((dh_link_get_flags (link) & DH_LINK_FLAGS_DEPRECATED) != 0)
        {
          gchar *italic_name = g_strdup_printf ("<i>%s</i>", name);
          g_free (name);
          name = italic_name;
        }

      result = ide_search_result_new (idecontext, name, dh_link_get_book_name (link), score);
      g_object_set_qdata_full (G_OBJECT (result), gQuarkLink, dh_link_get_uri (link), g_free);

      /* push the result through the search reducer */
      ide_search_reducer_push (&reducer, result);
    }

  ide_search_context_provider_completed (context, provider);
}

static const gchar *
ide_devhelp_search_provider_get_verb (IdeSearchProvider *provider)
{
  return _("Documentation");
}

static void
ide_devhelp_search_provider_constructed (GObject *object)
{
  IdeDevhelpSearchProvider *self = IDE_DEVHELP_SEARCH_PROVIDER (object);

  dh_book_manager_populate (self->book_manager);
  dh_keyword_model_set_words (self->keywords_model, self->book_manager);
}

static void
ide_devhelp_search_provider_finalize (GObject *object)
{
  IdeDevhelpSearchProvider *self = IDE_DEVHELP_SEARCH_PROVIDER (object);

  g_clear_object (&self->book_manager);

  G_OBJECT_CLASS (ide_devhelp_search_provider_parent_class)->finalize (object);
}

static void
ide_devhelp_search_provider_class_init (IdeDevhelpSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchProviderClass *provider_class = IDE_SEARCH_PROVIDER_CLASS (klass);

  object_class->constructed = ide_devhelp_search_provider_constructed;
  object_class->finalize = ide_devhelp_search_provider_finalize;

  provider_class->get_verb = ide_devhelp_search_provider_get_verb;
  provider_class->populate = ide_devhelp_search_provider_populate;

  gQuarkLink = g_quark_from_static_string ("LINK");
}

static void
ide_devhelp_search_provider_init (IdeDevhelpSearchProvider *self)
{
  self->book_manager = dh_book_manager_new ();
  self->keywords_model = dh_keyword_model_new ();
}
