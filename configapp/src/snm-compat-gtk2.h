#ifndef SNM_COMPAT_GTK2_H
#define SNM_COMPAT_GTK2_H

/* To be included only from C files, not headers.  */


#ifndef HAVE_GTK_DIALOG_GET_ACTION_AREA
#define gtk_dialog_get_action_area(_d_) (_d_)->action_area
#endif

#ifndef HAVE_GTK_DIALOG_GET_CONTENT_AREA
#define gtk_dialog_get_content_area(_d_) (_d_)->vbox
#endif

#ifndef HAVE_GTK_WIDGET_GET_ALLOCATION
#define gtk_widget_get_allocation(_w_,_a_) (*(_a_) = (_w_)->allocation)
#endif

#ifndef HAVE_GTK_WIDGET_SET_ALLOCATION
#define gtk_widget_set_allocation(_w_,_a_) ((_w_)->allocation = *(_a_))
#endif

#ifndef HAVE_GTK_WIDGET_GET_REALIZED
#define gtk_widget_get_realized(_w_) GTK_WIDGET_REALIZED(_w_)
#endif

#ifndef HAVE_GTK_WIDGET_SET_REALIZED
#define gtk_widget_set_realized(_w_,_r_) do { if (_r_) GTK_WIDGET_SET_FLAGS(_w_, GTK_REALIZED); else GTK_WIDGET_UNSET_FLAGS(_w_, GTK_REALIZED); } while (0)
#endif

#ifndef HAVE_GTK_WIDGET_GET_STATE
#define gtk_widget_get_state(_w_) (_w_)->state
#endif

#ifndef HAVE_GTK_WIDGET_GET_WINDOW
#define gtk_widget_get_window(_w_) (_w_)->window
#endif

#ifndef HAVE_GTK_WIDGET_SET_WINDOW
#define gtk_widget_set_window(_w_,_o_) ((_w_)->window = (_o_))
#endif

#ifndef HAVE_GTK_WIDGET_SET_HAS_WINDOW
#define gtk_widget_set_has_window(_w_, _h_) {}
#endif

#ifndef HAVE_GTK_WIDGET_IS_DRAWABLE
#define gtk_widget_is_drawable(_w_) GTK_WIDGET_DRAWABLE(_w_)
#endif

#ifndef HAVE_GTK_WIDGET_SET_CAN_DEFAULT
#define gtk_widget_set_can_default(_w_,_d_) do { if (_d_) GTK_WIDGET_SET_FLAGS(_w_, GTK_CAN_DEFAULT); else GTK_WIDGET_UNSET_FLAGS(_w_, GTK_CAN_DEFAULT); } while (0)
#endif

#ifndef HAVE_GTK_WIDGET_SET_CAN_FOCUS
#define gtk_widget_set_can_focus(_w_,_f_) do { if (_f_) GTK_WIDGET_SET_FLAGS(_w_, GTK_CAN_FOCUS); else GTK_WIDGET_UNSET_FLAGS(_w_, GTK_CAN_FOCUS); } while (0)
#endif

#ifndef HAVE_GTK_WIDGET_STYLE_ATTACH
#define gtk_widget_style_attach(_w_) (_w_->style = gtk_style_attach (_w_->style, _w_->window))
#endif

#endif
