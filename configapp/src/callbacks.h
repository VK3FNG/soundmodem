#include <gtk/gtk.h>


gboolean
on_mainwindow_destroy_event            (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_mainwindow_delete_event             (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_new_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_newconfiguration_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_newchannel_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_deleteconfiguration_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_deletechannel_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_quit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_diagscope_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_diagspectrum_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_diagtransmit_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_diagreceive_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_about_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_configtree_cursor_changed           (GtkTreeView     *tree_view,
                                        gpointer         user_data);

void
on_new_configuration_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_comboentry2_changed                 (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_wspec_delete_event                  (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_spec_button_press_event             (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_spec_motion_event                   (GtkWidget       *widget,
                                        GdkEventMotion  *event,
                                        gpointer         user_data);

void
on_ptt_toggled                         (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_clearbutton_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_specwindow_delete_event             (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_scopewindow_delete_event            (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_dcdfreeze_toggled                   (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

gboolean
on_receivewindow_delete_event          (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_dcd_toggled                         (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_diagmodem_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

GtkWidget*
create_led_pixmap (gchar *widget_name, gchar *string1, gchar *string2,
                gint int1, gint int2);

GtkWidget*
create_led_pixmap (gchar *widget_name, gchar *string1, gchar *string2,
                gint int1, gint int2);

GtkWidget*
create_led_pixmap (gchar *widget_name, gchar *string1, gchar *string2,
                gint int1, gint int2);

GtkWidget*
create_led_pixmap (gchar *widget_name, gchar *string1, gchar *string2,
                gint int1, gint int2);

GtkWidget*
create_led_pixmap (gchar *widget_name, gchar *string1, gchar *string2,
                gint int1, gint int2);

GtkWidget*
create_led_pixmap (gchar *widget_name, gchar *string1, gchar *string2,
                gint int1, gint int2);

gboolean
on_p3dwindow_delete_event              (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_diagp3dmodem_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_diagpassall_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
