/* gpabackupop.h - The GpaBackupOperation object.
 *	Copyright (C) 2003, Miguel Coca.
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

#ifndef GPA_BACKUP_OP_H
#define GPA_BACKUP_OP_H

#include "gpa.h"
#include <glib.h>
#include <glib-object.h>
#include "gpaoperation.h"
#include "gpaprogressdlg.h"

/* GObject stuff */
#define GPA_BACKUP_OPERATION_TYPE	  (gpa_backup_operation_get_type ())
#define GPA_BACKUP_OPERATION(obj)	  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GPA_BACKUP_OPERATION_TYPE, GpaBackupOperation))
#define GPA_BACKUP_OPERATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass), GPA_BACKUP_OPERATION_TYPE, GpaBackupOperationClass))
#define GPA_IS_BACKUP_OPERATION(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GPA_BACKUP_OPERATION_TYPE))
#define GPA_IS_BACKUP_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GPA_BACKUP_OPERATION_TYPE))
#define GPA_BACKUP_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GPA_BACKUP_OPERATION_TYPE, GpaBackupOperationClass))

typedef struct _GpaBackupOperation GpaBackupOperation;
typedef struct _GpaBackupOperationClass GpaBackupOperationClass;

struct _GpaBackupOperation {
  GpaOperation parent;

  gpgme_key_t key;
  gchar *fpr, *key_id;
};

struct _GpaBackupOperationClass {
  GpaOperationClass parent_class;
};

GType gpa_backup_operation_get_type (void) G_GNUC_CONST;

/* API */					   
GpaBackupOperation* 
gpa_backup_operation_new (GtkWidget *window, gpgme_key_t key);

GpaBackupOperation* 
gpa_backup_operation_new_from_fpr (GtkWidget *window, const gchar *fpr);

#endif
