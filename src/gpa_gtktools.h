/* gpa_gtktools.h  -  The GNU Privacy Assistant
 *      Copyright (C) 2000 Free Software Foundation, Inc.
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

#include <gtk/gtk.h>

extern GtkWidget *gpa_space_new ( void );
extern GtkWidget *gpa_widget_hjustified_new (
  GtkWidget *widget, GtkJustification jtype
);
extern GtkWidget *gpa_button_new (
  GtkAccelGroup *accelGroup, gchar *labelText
);
extern GtkWidget *gpa_buttonCancel_new (
  GtkWidget *window, GtkAccelGroup *accelGroup, gchar *labelText
);
extern GtkWidget *gpa_check_button_new (
  GtkAccelGroup *accelGroup, gchar *labelText
);
extern GtkWidget *gpa_radio_button_new (
  GtkAccelGroup *accelGroup, gchar *labelText
);
extern GtkWidget *gpa_radio_button_new_from_widget (
  GtkRadioButton *widget, GtkAccelGroup *accelGroup, gchar *labelText
);
extern GtkWidget *gpa_toggle_button_new (
  GtkAccelGroup *accelGroup, gchar *labelText
);
extern void gpa_connect_by_accelerator (
  GtkLabel *label, GtkWidget *widget,
  GtkAccelGroup *accelGroup, gchar *labelText
);
extern void gpa_widget_set_centered ( GtkWidget *widget, GtkWidget *parent );
