/* gpgmetools.c - The GNU Privacy Assistant
 *      Copyright (C) 2002, Miguel Coca.
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

/* A set of auxiliary functions for common tasks related to GPGME */

#include <errno.h>
#include "gpa.h"
#include "gtktools.h"
#include "gpgmetools.h"
#include "gpgmeparsers.h"

/* Report an unexpected error in GPGME and quit the application */
void _gpa_gpgme_error (GpgmeError err, const char *file, int line)
{
  gchar *message = g_strdup_printf (_("Fatal Error in GPGME library\n"
                                      "(invoked from file %s, line %i):\n\n"
                                      "\t%s\n\n"
                                      "The application will be terminated"),
                                    file, line, gpgme_strerror (err));
  gpa_window_error (message, NULL);
  g_free (message);
  exit (EXIT_FAILURE);
}


/* Write the contents of the GpgmeData object to the file. Receives a
 * filehandle instead of the filename, so that the caller can make sure the
 * file is accesible before putting anything into data.
 */
void dump_data_to_file (GpgmeData data, FILE *file)
{
  char buffer[128];
  int nread;
  GpgmeError err;

  err = gpgme_data_rewind (data);
  if (err != GPGME_No_Error)
    gpa_gpgme_error (err);
  while ( !(err = gpgme_data_read (data, buffer, sizeof(buffer), &nread)) ) 
    {
      fwrite ( buffer, nread, 1, file );
    }
  if (err != GPGME_EOF)
    gpa_gpgme_error (err);
  return;
}

/* Not really a gpgme function, but needed in most places dump_data_to_file
 * is used.
 * Opens a file for writing, asking the user to overwrite if it exists and
 * reporting any errors. Returns NULL on failure, but you can assume the user
 * has been informed of the error (or maybe he just didn't want to
 * overwrite!) */
FILE *gpa_fopen (const char *filename, GtkWidget *parent)
{
  FILE *target;
  
  /* If the file exists, ask before overwriting */
  if (g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      GtkWidget *msgbox = gtk_message_dialog_new 
	(GTK_WINDOW(parent), GTK_DIALOG_MODAL,
	 GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE, 
	 _("The file %s already exists.\n"
	   "Do you want to overwrite it?"), filename);
      gtk_dialog_add_buttons (GTK_DIALOG (msgbox),
			      GTK_STOCK_YES, GTK_RESPONSE_YES,
			      GTK_STOCK_NO, GTK_RESPONSE_NO, NULL);
      if (gtk_dialog_run (GTK_DIALOG (msgbox)) == GTK_RESPONSE_NO)
	{
	  gtk_widget_destroy (msgbox);
	  return NULL;
	}
      gtk_widget_destroy (msgbox);
    }
  target = fopen (filename, "w");
  if (!target)
    {
      gchar *message;
      message = g_strdup_printf ("%s: %s", filename, strerror(errno));
      gpa_window_error (message, parent);
      g_free (message);
      return NULL;
    }
  return target;
}

/* Do a gpgme_data_new_from_file and report any GPGME_File_Error to the user.
 */
GpgmeError gpa_gpgme_data_new_from_file (GpgmeData *data,
					 const char *filename,
					 GtkWidget *parent)
{
  GpgmeError err;
  err = gpgme_data_new_from_file (data, filename, 1);
  if (err == GPGME_File_Error)
    {
      gchar *message;
      message = g_strdup_printf ("%s: %s", filename, strerror(errno));
      gpa_window_error (message, NULL);
      g_free (message);
    }
  return err;
}

/* Read the contents of the clipboard into the GpgmeData object.
 */
void fill_data_from_clipboard (GpgmeData data, GtkClipboard *clipboard)
{
  GpgmeError err;
  gchar *text = gtk_clipboard_wait_for_text (clipboard);
  if (text)
    {
      err = gpgme_data_write (data, text, strlen (text));
      if (err != GPGME_No_Error)
        {
          gpa_gpgme_error (err);
        }
    }
  g_free (text);
}

/* Write the contents of the GpgmeData into the clipboard
 */
void dump_data_to_clipboard (GpgmeData data, GtkClipboard *clipboard)
{
  char buffer[128];
  int nread;
  GpgmeError err;
  gchar *text = NULL;
  gint len = 0;

  err = gpgme_data_rewind (data);
  if (err != GPGME_No_Error)
    gpa_gpgme_error (err);
  while ( !(err = gpgme_data_read (data, buffer, sizeof(buffer), &nread)) ) 
    {
      text = g_realloc (text, len+nread);
      strncpy (text+len, buffer, nread);
      len += nread;
    }
  if (err != GPGME_EOF)
    gpa_gpgme_error (err);
  gtk_clipboard_set_text (clipboard, text, len);
  g_free (text);
  return;
}

/* Assemble the parameter string for gpgme_op_genkey for GnuPG. We don't need
 * worry about the user ID being UTF-8 as long as we are using GTK+2, because
 * all user input is UTF-8 in it.
 */
static gchar * build_genkey_parms (GPAKeyGenParameters *params)
{
  gchar *string;
  gchar *key_algo;
  gchar *subkeys, *email, *comment, *expire;

  /* Choose which keys and subkeys to generate */
  switch (params->algo)
    {
    case GPA_KEYGEN_ALGO_DSA_ELGAMAL:
      key_algo = "DSA";
      subkeys = g_strdup_printf ("SubKey-Type: ELG-E\n"
                                 "Subkey-Length: %i\n", params->keysize);
      break;
    case GPA_KEYGEN_ALGO_DSA:
      key_algo = "DSA";
      subkeys = "";
      break;
    case GPA_KEYGEN_ALGO_RSA:
      key_algo = "RSA";
      subkeys = "";
      break;
    default:
      /* Can't happen */
      return NULL;
    }
  /* Build the extra fields of the user ID if supplied by the user */
  if (params->email && params->email[0])
    {
      email = g_strdup_printf ("Name-Email: %s\n", params->email);
    }
  else
    {
      email = "";
    }
  if (params->comment && params->comment[0])
    {
      comment = g_strdup_printf ("Name-Comment: %s\n", params->comment);
    }
  else
    {
      comment = "";
    }
  /* Build the expiration date string if needed */
  if (params->expiryDate)
    {
      expire = g_strdup_printf ("Expire-Date: %i-%i-%i\n", 
                                g_date_get_year(params->expiryDate),
                                g_date_get_month(params->expiryDate),
                                g_date_get_day(params->expiryDate));
    }
  else if (params->interval)
    {
      expire = g_strdup_printf ("Expire-Date: %i%c\n", params->interval,
                                params->unit);
    }
  else
    expire = "";
  /* Assemble the final parameter string */
  string = g_strdup_printf ("<GnupgKeyParms format=\"internal\">\n"
                            "Key-Type: %s\n"
                            "Key-Length: %i\n"
                            "%s" /* Subkeys */
                            "Name-Real: %s\n"
                            "%s" /* Email Address */
                            "%s" /* Comment */
                            "%s" /* Expiration date */
                            "Passphrase: %s\n"
                            "</GnupgKeyParms>\n", key_algo, params->keysize,
                            subkeys, params->userID, email, comment, expire,
                            params->password);

  /* Free auxiliary strings if they are not empty */
  if (subkeys[0])
    {
      g_free (subkeys);
    }
  if (email[0])
    {
      g_free (email);
    }
  if (comment[0])
    {
      g_free (comment);
    }
  if (expire[0])
    {
      g_free (expire);
    }
  
  return string;
}

/* Generate a key with the given parameters. It prepares the parameters
 * required by Gpgme and returns whatever gpgme_op_genkey returns.
 */
GpgmeError gpa_generate_key (GPAKeyGenParameters *params)
{
  gchar *parm_string;
  GpgmeError err;

  parm_string = build_genkey_parms (params);
  err = gpgme_op_genkey (ctx, parm_string, NULL, NULL);
  g_free (parm_string);

  return err;
}

/* Backup a key. It exports both the public and secret keys to a file.
 * Returns TRUE on success and FALSE on error. It displays errors to the
 * user.
 */
gboolean gpa_backup_key (const gchar *fpr, const char *filename)
{
  gchar *header, *pub_key, *sec_key;
  gchar *err;
  FILE *file;
  gint ret_code;
  GpaEngineInfo info;
  gchar *header_argv[] = 
    {
      NULL, "--batch", "--no-tty", "--fingerprint", fpr, NULL
    };
  gchar *pub_argv[] = 
    {
      NULL, "--batch", "--no-tty", "--armor", "--export", fpr, NULL
    };
  gchar *sec_argv[] = 
    {
      NULL, "--batch", "--no-tty", "--armor", "--export-secret-key", fpr, NULL
    };

  gpa_parse_engine_info (&info);
  header_argv[0] = info.path;
  pub_argv[0] = info.path;
  sec_argv[0] = info.path;
  /* Open the file */
  file = fopen (filename, "w");
  if (!file)
    {
      gchar message[256];
      g_snprintf (message, sizeof(message), "%s: %s",
		  filename, strerror(errno));
      gpa_window_error (message, NULL);
      return FALSE;
    }
  /* Get the keys and write them into the file */
  fputs (_("************************************************************************\n"
	   "* WARNING: This file is a backup of your secret key. Please keep it in *\n"
	   "* a safe place.                                                        *\n"
	   "************************************************************************\n\n"),
	 file);
  fputs (_("The key backed up in this file is:\n\n"), file);
  if( !g_spawn_sync (NULL, header_argv, NULL, 0, NULL, NULL, &header,
		     &err, &ret_code, NULL))
    {
      return FALSE;
    }
  fputs (header, file);
  g_free (err);
  g_free (header);
  fputs ("\n", file);
  if( !g_spawn_sync (NULL, pub_argv, NULL, 0, NULL, NULL, &pub_key,
		     &err, &ret_code, NULL))
    {
      return FALSE;
    }
  fputs (pub_key, file);
  g_free (err);
  g_free (pub_key);
  fputs ("\n", file);
  if( !g_spawn_sync (NULL, sec_argv, NULL, 0, NULL, NULL, &sec_key,
		     &err, &ret_code, NULL))
    {
      return FALSE;
    }
  fputs (sec_key, file);
  g_free (err);
  g_free (sec_key);
  g_free (info.path);
  g_free (info.version);

  fclose (file);
  return TRUE;
}

void
gpa_key_gen_free_parameters(GPAKeyGenParameters * params)
{
  free (params->userID);
  free (params->email);
  free (params->comment);
  if (params->expiryDate)
    g_date_free (params->expiryDate);
  free (params);
}

GPAKeyGenParameters *
key_gen_params_new(void)
{
  GPAKeyGenParameters * params = xmalloc (sizeof (*params));
  params->userID = NULL;
  params->email = NULL;
  params->comment = NULL;
  params->expiryDate = NULL;
  params->interval = 0;
  params->generate_revocation = FALSE;
  params->send_to_server = FALSE;
  return params;
}

static gchar *algorithm_strings[] = {
  N_("DSA and ElGamal (default)"),
  N_("DSA (sign only)"),
  N_("RSA (sign only)"),
};

const gchar *
gpa_algorithm_string (GPAKeyGenAlgo algo)
{
  return _(algorithm_strings[algo]);
}

GPAKeyGenAlgo
gpa_algorithm_from_string (const gchar * string)
{
  GPAKeyGenAlgo result;

  result = GPA_KEYGEN_ALGO_FIRST;
  while (result <= GPA_KEYGEN_ALGO_LAST &&
	 strcmp (string, _(algorithm_strings[result])) != 0)
    result++;
  return result;
}

/* Ownertrust strings */
const gchar *gpa_key_ownertrust_string (GpgmeKey key)
{
  GpgmeValidity trust;
  trust = gpgme_key_get_ulong_attr (key, GPGME_ATTR_OTRUST, NULL, 0);
  switch (trust) 
    {
    case GPGME_VALIDITY_UNKNOWN:
    case GPGME_VALIDITY_UNDEFINED:
    default:
      return _("Unknown");
      break;
    case GPGME_VALIDITY_NEVER:
      return _("Never");
      break;
    case GPGME_VALIDITY_MARGINAL:
      return _("Marginal");
      break;
    case GPGME_VALIDITY_FULL:
      return _("Full");
      break;
    case GPGME_VALIDITY_ULTIMATE:
      return _("Ultimate");
      break;
    }
}

/* Key validity strings */
const gchar *gpa_key_validity_string (GpgmeKey key)
{
  GpgmeValidity valid;
  valid = gpgme_key_get_ulong_attr (key, GPGME_ATTR_VALIDITY, NULL, 0);
  switch (valid) 
    {
    case GPGME_VALIDITY_UNKNOWN:
    case GPGME_VALIDITY_UNDEFINED:
    case GPGME_VALIDITY_NEVER:
    case GPGME_VALIDITY_MARGINAL:
    default:
      if (gpgme_key_get_ulong_attr (key, GPGME_ATTR_KEY_REVOKED, NULL, 0))
        {
          return _("Revoked");
        }
      else if (gpgme_key_get_ulong_attr (key, GPGME_ATTR_KEY_EXPIRED, NULL, 0))
        {
          return _("Expired");
        }
      else if (gpgme_key_get_ulong_attr (key, GPGME_ATTR_KEY_DISABLED, NULL, 
                                         0))
        {
          return _("Disabled");
        }
      else if (gpgme_key_get_ulong_attr (key, GPGME_ATTR_UID_INVALID, NULL, 0))
        {
          return _("Incomplete");
        }
      else
        {
          return _("Unknown");
        }
      break;
    case GPGME_VALIDITY_FULL:
    case GPGME_VALIDITY_ULTIMATE:
      return _("Fully Valid");
    }
}

static GtkWidget *passphrase_question_label (const gchar *desc)
{
  GtkWidget *label;
  gchar *input;
  gchar *text;
  gchar *keyid, *userid, *action;
  gint i;
  input = g_strdup (desc);
  /* The first line tells us whether this is the first time we ask for a
   * passphrase or not. */
  action = input;
  for (i = 0; input[i] != '\n'; i++)
    {
    }
  input[i++] = '\0';
  /* The first word in the second line is the key ID */
  keyid = input+i;
  for (i = 0; input[i] != ' '; i++)
    {
    }
  input[i++] = '\0';
  /* The rest of the line is the user ID */
  userid = input+i;
  for (i = 0; input[i] != '\n'; i++)
    {
    }
  input[i++] = '\0';
  /* Build the label widget */
  if (g_str_equal (action, "ENTER"))
    {
      text = g_strdup_printf ("%s\n\n%s %s\n%s %s",
                              _("Please enter the passphrase for"
                                " the following key:"),
                              _("User Name:"), userid,
                              _("Key ID:"), keyid);
    }
  else
    {
      text = g_strdup_printf ("%s\n\n%s %s\n%s %s",
                              _("Wrong passphrase, please try again:"),
                              _("User Name:"), userid,
                              _("Key ID:"), keyid);
    }
  label = gtk_label_new (text);
  g_free (input);
  g_free (text);
  return label;
}

/* This is the function called by GPGME when it wants a passphrase */
const char * gpa_passphrase_cb (void *opaque, const char *desc, void **r_hd)
{
  GtkWidget * dialog;
  GtkWidget * hbox;
  GtkWidget * vbox;
  GtkWidget * entry;
  GtkWidget * pixmap;
  GtkResponseType response;

  if (desc)
    {
      dialog = gtk_dialog_new_with_buttons (_("Enter Passphrase"),
                                            NULL, GTK_DIALOG_MODAL,
                                            GTK_STOCK_OK,
                                            GTK_RESPONSE_OK,
                                            GTK_STOCK_CANCEL,
                                            GTK_RESPONSE_CANCEL,
                                            NULL);
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, 
                          TRUE, FALSE, 10);
      pixmap = gtk_image_new_from_stock (GTK_STOCK_DIALOG_QUESTION,
                                         GTK_ICON_SIZE_DIALOG);
      gtk_box_pack_start (GTK_BOX (hbox), pixmap, TRUE, FALSE, 10);
      vbox = gtk_vbox_new (TRUE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, FALSE, 10);
      /* The default button is OK */
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
      /* Set the contents of the dialog */
      gtk_box_pack_start_defaults (GTK_BOX (vbox),
                                   passphrase_question_label (desc));
      entry = gtk_entry_new ();
      gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
      gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox), entry, TRUE, FALSE, 10);
      gtk_widget_grab_focus (entry);
      /* Run the dialog */
      gtk_widget_show_all (dialog);
      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_hide (dialog);
      if (response == GTK_RESPONSE_OK)
        {
          return gtk_entry_get_text (GTK_ENTRY (entry));
        }
      else
        {
          gpgme_cancel (ctx);
          return "";
        }
    }
  else
    {
      /* Free the dialog */
      dialog = *r_hd;
      gtk_widget_destroy (dialog);
      return NULL;
    }
}

/* Convenience functions to access key attributes, which need to be filtered
 * before being displayed to the user. */

/* Return the user ID, making sure it is properly UTF-8 encoded.
 * Allocates a new string, which must be freed with g_free().
 */
gchar *gpa_gpgme_key_get_userid (GpgmeKey key, int idx)
{
  const char * uid;
  const gchar *s;

  uid = gpgme_key_get_string_attr (key, GPGME_ATTR_USERID, NULL, idx);
  if (!uid)
    {
      return NULL;
    }
  /* Make sure the encoding is UTF-8.
   * Test structure suggested by Werner Koch */
  for (s = uid; *s && !(*s & 0x80); s++)
    ;
  if (*s && !strchr (uid, 0xc3))
    {
      /* The string is Latin-1 */
      return  g_convert (uid, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
    }
  else
    {
      /* The string is already in UTF-8 */
      return g_strdup (uid);
    }
}

/* Return the key fingerprint, properly formatted according to the algorithm.
 * Allocates a new string, which must be freed with g_free().
 * This is based on code from GPAPA's extract_fingerprint.
 */
gchar *gpa_gpgme_key_get_fingerprint (GpgmeKey key, int idx)
{
  unsigned long algorithm = gpgme_key_get_ulong_attr (key, GPGME_ATTR_ALGO,
						      NULL, idx);
  const char *fpraw = gpgme_key_get_string_attr (key, GPGME_ATTR_FPR,
						 NULL, idx);

  if (algorithm == 1 || algorithm == 2 ||algorithm == 3 )  /* RSA */
    {
      char *fp = g_malloc (strlen (fpraw) + 16 + 1);
      const char *r = fpraw;
      char *q = fp;
      gint c = 0;
      while (*r)
	{
	  *q++ = *r++;
	  c++;
	  if (c < 32)
	    {
	      if (c % 2 == 0)
		*q++ = ' ';
	      if (c % 16 == 0)
		*q++ = ' ';
	    }
	}
      *q = 0;
      return fp;
    }
  else
    {
      char *fp = g_malloc (strlen (fpraw) + 10 + 1);
      const char *r = fpraw;
      char *q = fp;
      gint c = 0;
      while (*r)
	{
	  *q++ = *r++;
	  c++;
	  if (c < 40)
	    {
	      if (c % 4 == 0)
		*q++ = ' ';
	      if (c % 20 == 0)
		*q++ = ' ';
	    }
	}
      *q = 0;
      return fp;
    }
}

/* Return the short key ID of the indicated key. The returned string is valid
 * as long as the key is valid.
 */
const gchar *gpa_gpgme_key_get_short_keyid (GpgmeKey key, int idx)
{
  const char *keyid;
  keyid = gpgme_key_get_string_attr (key, GPGME_ATTR_KEYID, NULL, idx);
  if (!keyid)
    {
      return NULL;
    }
  else
    {
      return keyid+8;
    }
}
