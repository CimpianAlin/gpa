/* gpakeyselector.c  -  The GNU Privacy Assistant
 *      Copyright (C) 2003 Miguel Coca.
 *
 * This file is part of GPA
 *
 * GPA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GPA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "gpa.h"
#include "gpakeyselector.h"

/* Callbacks */

void gpa_key_selector_start  (GpaContext *context, gpointer data);
void gpa_key_selector_next_key (GpaContext *context, GpgmeKey key,
				gpointer data);
void gpa_key_selector_done (GpaContext *context, GpgmeError err, 
			    gpointer data);

/* GObject */

static GObjectClass *parent_class = NULL;

static void
gpa_key_selector_finalize (GObject *object)
{  
  GpaKeySelector *sel = GPA_KEY_SELECTOR (object);

  gpa_context_destroy (sel->context);
  /* Dereference all keys in the list */
  g_list_foreach (sel->keys, (GFunc) gpgme_key_unref, NULL);
  g_list_free (sel->keys);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gpa_key_selector_class_init (GpaKeySelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gpa_key_selector_finalize;
}

typedef enum
{
  GPA_KEY_SELECTOR_COLUMN_KEYID,
  GPA_KEY_SELECTOR_COLUMN_USERID,
  GPA_KEY_SELECTOR_COLUMN_KEY,
  GPA_KEY_SELECTOR_N_COLUMNS
} GpaKeySelectorColumn;

static void
gpa_key_selector_init (GpaKeySelector *selector)
{
  GtkListStore *store;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkTreeSelection *selection = 
    gtk_tree_view_get_selection (GTK_TREE_VIEW (selector));

  selector->secret = FALSE;
  selector->keys = NULL;
  /* Init the model */
  store = gtk_list_store_new (GPA_KEY_SELECTOR_N_COLUMNS, G_TYPE_STRING,
			      G_TYPE_STRING, G_TYPE_POINTER);

  /* The view */
  gtk_tree_view_set_model (GTK_TREE_VIEW (selector), GTK_TREE_MODEL (store));
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (selector), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Key ID"), renderer,
						     "text",
						     GPA_KEY_SELECTOR_COLUMN_KEYID,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (selector), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("User Name"), renderer,
						     "text",
						     GPA_KEY_SELECTOR_COLUMN_USERID,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (selector), column);

  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

  /* The context used for listing keys */
  selector->context = gpa_context_new ();
  g_signal_connect (G_OBJECT (selector->context), "start",
		    G_CALLBACK (gpa_key_selector_start), selector);
  g_signal_connect (G_OBJECT (selector->context), "next_key",
		    G_CALLBACK (gpa_key_selector_next_key), selector);
  g_signal_connect (G_OBJECT (selector->context), "done",
		    G_CALLBACK (gpa_key_selector_done), selector);
}

GType
gpa_key_selector_get_type (void)
{
  static GType key_selector_type = 0;
  
  if (!key_selector_type)
    {
      static const GTypeInfo key_selector_info =
      {
        sizeof (GpaKeySelectorClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gpa_key_selector_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GpaKeySelector),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gpa_key_selector_init,
      };
      
      key_selector_type = g_type_register_static (GTK_TYPE_TREE_VIEW,
						  "GpaKeySelector",
						  &key_selector_info, 0);
    }
  
  return key_selector_type;
}

/* API */

GtkWidget *gpa_key_selector_new (gboolean secret)
{
  GtkWidget *sel = (GtkWidget*) g_object_new (GPA_KEY_SELECTOR_TYPE, NULL);

  GPA_KEY_SELECTOR (sel)->secret = secret;
  gpgme_op_keylist_start (GPA_KEY_SELECTOR(sel)->context->ctx,
			  NULL, secret);

  return sel;
}

/* Return a list of selected GpgmeKey's. The caller must free the list and
 * dereference the keys.
 */
GList *gpa_key_selector_get_selected_keys (GpaKeySelector * selector)
{
  GtkTreeSelection *selection = 
    gtk_tree_view_get_selection (GTK_TREE_VIEW (selector));
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));
  GList *list = gtk_tree_selection_get_selected_rows (selection, &model);
  GList *keys = NULL;
  GList *cur;

  for (cur = list; cur; cur = g_list_next (list))
    {
      GpgmeKey key;
      GtkTreeIter iter;
      GtkTreePath *path = cur->data;
      GValue value = {0,};

      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get_value (model, &iter, GPA_KEY_SELECTOR_COLUMN_KEY,
				&value);
      key = g_value_get_pointer (&value);
      g_value_unset(&value);
      
      gpgme_key_ref (key);
      keys = g_list_append (keys, key);
    }

  g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (list);
  
  return keys;
}

gboolean gpa_key_selector_has_selection (GpaKeySelector * selector)
{
  int selected =  gtk_tree_selection_count_selected_rows 
    (gtk_tree_view_get_selection (GTK_TREE_VIEW (selector)));
  return (selected > 0);
}

/* Internal */

void gpa_key_selector_start  (GpaContext *context, gpointer data)
{
  GpaKeySelector *selector = data;
  
  /* Disable the list while we are loading it */
  gtk_widget_set_sensitive (GTK_WIDGET (selector), FALSE);
}

void gpa_key_selector_next_key (GpaContext *context, GpgmeKey key,
				gpointer data)
{
  GpaKeySelector *selector = data;
  GtkListStore *store;
  GtkTreeIter iter;
  const gchar *keyid;
  gchar *userid;
  
  selector->keys = g_list_append (selector->keys, key);
  store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (selector)));  /* The Key ID */
  keyid = gpa_gpgme_key_get_short_keyid (key, 0);
  /* The user ID */
  userid = gpa_gpgme_key_get_userid (key, 0);
  /* Append it to the list */
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      GPA_KEY_SELECTOR_COLUMN_KEYID, keyid, 
		      GPA_KEY_SELECTOR_COLUMN_USERID, userid,
		      GPA_KEY_SELECTOR_COLUMN_KEY, key, -1);
  /* If this is a secret key selector, select the default key */
  if (selector->secret) {
    const gchar *key_fpr = gpgme_key_get_string_attr (key, GPGME_ATTR_FPR, NULL, 0);
    const gchar *default_key = gpa_options_get_default_key (gpa_options_get_instance());

    if (g_str_equal (key_fpr, default_key)) {
      gtk_tree_selection_select_iter 
	(gtk_tree_view_get_selection (GTK_TREE_VIEW (selector)),&iter);
    }
  }
  /* Clean up */
  g_free (userid);
}

void gpa_key_selector_done (GpaContext *context, GpgmeError err, 
			    gpointer data)
{
  GpaKeySelector *selector = data;
  GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model 
					(GTK_TREE_VIEW (selector)));

  if (err != GPGME_No_Error) {
    gpa_gpgme_warning (err);
  }
  /* Sort the column */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                        GPA_KEY_SELECTOR_COLUMN_USERID,
					GTK_SORT_ASCENDING);
  /* Enable the list again */
  gtk_widget_set_sensitive (GTK_WIDGET (selector), TRUE);
}

