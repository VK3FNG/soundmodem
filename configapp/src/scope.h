/* 
 * Scope Widget
 * Copyright (C) 1999-2000 Thomas Sailer <sailer@ife.ee.ethz.ch>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __SCOPE_H__
#define __SCOPE_H__

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define SCOPE(obj)          G_TYPE_CHECK_INSTANCE_CAST(obj, scope_get_type(), Scope)
#define SCOPE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST(klass, scope_get_type(), ScopeClass)
#define IS_SCOPE(obj)       G_TYPE_CHECK_INSTANCE_TYPE(obj, scope_get_type())

typedef struct _Scope        Scope;
typedef struct _ScopeClass   ScopeClass;

#define SCOPE_NUMSAMPLES  512

#define SCOPE_WIDTH    (SCOPE_NUMSAMPLES)
#define SCOPE_HEIGHT   384

struct _Scope 
{
	GtkWidget widget;

	guint idlefunc;
	GdkGC *trace_gc;
	GdkGC *grid_gc;
        GdkGC *pointer_gc;
	GdkColor tracecol;
	GdkColor gridcol;

	GdkPixmap *pixmap;

        short y[SCOPE_WIDTH];
};

struct _ScopeClass
{
	GtkWidgetClass parent_class;
};


guint scope_get_type(void);
GtkWidget* scope_new(const char *name, void *dummy0, void *dummy1, unsigned int dummy2, unsigned int dummy3);
void scope_setdata(Scope *scope, short *samples);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __SCOPE_H__ */
