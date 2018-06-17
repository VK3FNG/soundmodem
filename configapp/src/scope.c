/* 
 * Sooundmodem Scope Widget
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "scope.h"
#include "snm-compat-gtk2.h"
#include <gtk/gtkgc.h>
#include <gtk/gtkmain.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* --------------------------------------------------------------------- */

#define PRIO G_PRIORITY_LOW

static void scope_class_init(ScopeClass *klass);
static void scope_init(Scope *scope);
static void scope_finalize(GObject *object);
static gint scope_expose(GtkWidget *widget, GdkEventExpose *event);
static void scope_realize(GtkWidget *widget);
static void scope_unrealize(GtkWidget *widget);
static void scope_size_allocate(GtkWidget *widget, GtkAllocation *allocation);
static void scope_send_configure (Scope *scope);
static gint idle_callback(gpointer data);

static GtkWidgetClass *parent_class = NULL;
static ScopeClass *scope_class = NULL;


guint scope_get_type(void)
{
	static guint scope_type = 0;

	if (!scope_type)
	{
		static const GTypeInfo scope_info =
		{
			sizeof(ScopeClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc)scope_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof(Scope),
			0,		/* n_preallocs */
			(GInstanceInitFunc)scope_init,
		};
		scope_type = g_type_register_static 
		  (GTK_TYPE_WIDGET, "Scope", &scope_info, 0);
	}
	return scope_type;
}

static void scope_class_init(ScopeClass *klass)
{
	GObjectClass   *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass*)klass;
	widget_class = (GtkWidgetClass*)klass;

	parent_class = g_type_class_peek(GTK_TYPE_WIDGET);
	scope_class = klass;

	object_class->finalize = scope_finalize;
	widget_class->expose_event = scope_expose;
	widget_class->realize = scope_realize;
	widget_class->unrealize = scope_unrealize;
	widget_class->size_allocate = scope_size_allocate;
}

static void scope_init(Scope *scope)
{
	scope->idlefunc = 0;
	/* initialize the colors */
	scope->tracecol.red = 11796;
	scope->tracecol.green = 53740;
	scope->tracecol.blue = 4588;
	scope->gridcol.red = 52429;
	scope->gridcol.green = 52429;
	scope->gridcol.blue = 52429;
	scope->trace_gc = scope->grid_gc = NULL;
	scope->pixmap = NULL;
	/* initialize the data */
	memset(&scope->y, 0, sizeof(scope->y));
}

static void scope_realize(GtkWidget *widget)
{
	Scope *scope;
	GdkWindowAttr attributes;
	gint attributes_mask;
	GdkGCValues gc_values;
	GtkAllocation allocation;
	GdkWindow *window;
	GtkStyle *style;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(IS_SCOPE(widget));

	scope = SCOPE(widget);
	gtk_widget_set_realized(widget, TRUE);
	gtk_widget_get_allocation(widget, &allocation);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual(widget);
	attributes.colormap = gtk_widget_get_colormap(widget);
	attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	gtk_widget_set_has_window(widget, TRUE);
	window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributes_mask);
	gtk_widget_set_window(widget, window);
	gdk_window_set_user_data(window, scope);

	gtk_widget_style_attach(widget);
	style = gtk_widget_get_style(widget);
	gtk_style_set_background(style, window, GTK_STATE_NORMAL);

	/* gc's if necessary */
	if (!gdk_colormap_alloc_color(style->colormap, &scope->tracecol,
				      FALSE, TRUE))
		g_warning("unable to allocate color: ( %d %d %d )",
			  scope->tracecol.red, scope->tracecol.green, scope->tracecol.blue);
	gc_values.foreground = scope->tracecol;
	scope->trace_gc = gtk_gc_get(style->depth, 
				    style->colormap,
				    &gc_values, GDK_GC_FOREGROUND);
	if (!gdk_colormap_alloc_color(style->colormap, &scope->gridcol,
				      FALSE, TRUE))
		g_warning("unable to allocate color: ( %d %d %d )",
			  scope->gridcol.red, scope->gridcol.green, scope->gridcol.blue);
	gc_values.foreground = scope->gridcol;
	scope->grid_gc = gtk_gc_get(style->depth,
				   style->colormap,
				   &gc_values, GDK_GC_FOREGROUND);
	/* create backing store */
	scope->pixmap = gdk_pixmap_new(window, SCOPE_WIDTH, SCOPE_HEIGHT, -1);

	scope_send_configure(SCOPE(widget));
}

static void scope_unrealize(GtkWidget *widget)
{
	Scope *scope;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(IS_SCOPE(widget));

	scope = SCOPE(widget);
	if (scope->idlefunc)
		g_source_remove(scope->idlefunc);
	if (scope->trace_gc)
		gtk_gc_release(scope->trace_gc);
	if (scope->grid_gc)
		gtk_gc_release(scope->grid_gc);
	scope->trace_gc = scope->grid_gc = NULL;
	if (scope->pixmap)
			g_object_unref(scope->pixmap);
	scope->pixmap = NULL;
	if (GTK_WIDGET_CLASS(parent_class)->unrealize)
		(*GTK_WIDGET_CLASS(parent_class)->unrealize)(widget);
}

static void scope_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	GtkAllocation alloc;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(IS_SCOPE(widget));
	g_return_if_fail(allocation != NULL);
	
	alloc = *allocation;
	alloc.width = SCOPE_WIDTH;
	alloc.height = SCOPE_HEIGHT;
	gtk_widget_set_allocation(widget, &alloc);

	if (gtk_widget_get_realized(widget)) {
		gdk_window_move_resize (gtk_widget_get_window(widget),
					allocation->x, allocation->y,
					allocation->width, allocation->height);
		scope_send_configure(SCOPE(widget));
	}
}

static void scope_send_configure(Scope *scope)
{
	GtkWidget *widget;
	GdkEventConfigure event;
	GtkAllocation allocation;

	widget = GTK_WIDGET(scope);
	gtk_widget_get_allocation(widget, &allocation);

	event.type = GDK_CONFIGURE;
	event.window = gtk_widget_get_window(widget);
	event.send_event = TRUE;
	event.x = allocation.x;
	event.y = allocation.y;
	event.width = allocation.width;
	event.height = allocation.height;
  
	gtk_widget_event(widget, (GdkEvent*)&event);
}


GtkWidget* scope_new(const char *name, void *dummy0, void *dummy1, unsigned int dummy2, unsigned int dummy3)
{
	Scope *scope;
	
	scope = g_object_new(scope_get_type(), NULL);
	memset(&scope->y, 0, sizeof(scope->y));
	return GTK_WIDGET(scope);
}

static void scope_finalize(GObject *object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_SCOPE(object));
	(*G_OBJECT_CLASS(parent_class)->finalize)(object);
}

static gint scope_expose(GtkWidget *widget, GdkEventExpose *event)
{
	Scope *scope;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(IS_SCOPE(widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	if (gtk_widget_is_drawable(widget)) {
		scope = SCOPE(widget);
		if (!scope->idlefunc)
			scope->idlefunc = g_idle_add_full(PRIO, idle_callback, scope, NULL);
	}
	return FALSE;
}

static void draw(Scope *scope)
{
	guint segcnt, i;
	GdkPoint pt[SCOPE_WIDTH+1];
	GdkSegment seg[100], *segp;
	GtkWidget *widget;
	GtkAllocation allocation;
	GtkStyle *style;

	widget = GTK_WIDGET(scope);
	g_return_if_fail(gtk_widget_is_drawable(widget));
	g_return_if_fail(scope->pixmap);
	gtk_widget_get_allocation(widget, &allocation);
	style = gtk_widget_get_style(widget);
	/* calculate grid segments */
	for (segp = seg, segcnt = i = 0; i < SCOPE_WIDTH; i += SCOPE_WIDTH/8) {
		segp->x1 = segp->x2 = i;
		segp->y1 = SCOPE_HEIGHT/2-5;
		segp->y2 = SCOPE_HEIGHT/2+5;
		segp++;
		segcnt++;
	}
        segp->y1 = segp->y2 = SCOPE_HEIGHT/2;
        segp->x1 = 0;
        segp->x2 = SCOPE_WIDTH-1;
        segp++;
        segcnt++;
	/* copy data points */
	for (i = 0; i < SCOPE_WIDTH; i++) {
		pt[i].x = i;
		pt[i].y = ((32767-(int)scope->y[i])*SCOPE_HEIGHT) >> 16;
	}
	/* clear window */
	gdk_draw_rectangle(scope->pixmap, style->base_gc[gtk_widget_get_state(widget)],
			   TRUE, 0, 0, 
			   allocation.width, 
			   allocation.height);
	/* draw grid */
	gdk_draw_segments(scope->pixmap, scope->grid_gc, seg, segcnt);
	/* draw trace */
	gdk_draw_lines(scope->pixmap, scope->trace_gc, pt, SCOPE_WIDTH);
	/* draw to screen */
	gdk_draw_drawable(gtk_widget_get_window(widget), style->base_gc[gtk_widget_get_state(widget)], scope->pixmap, 
			  0, 0, 0, 0, allocation.width, allocation.height);
}


static gint idle_callback(gpointer data)
{
	g_return_val_if_fail(data != NULL, FALSE);
	g_return_val_if_fail(IS_SCOPE(data), FALSE);
	SCOPE(data)->idlefunc = 0;
	if (!gtk_widget_is_drawable(GTK_WIDGET(data)))
		return FALSE;
	draw(SCOPE(data));
	return FALSE;  /* don't call this callback again */
}

void scope_setdata(Scope *scope, short *samples)
{
	g_return_if_fail(scope != NULL);
	g_return_if_fail(IS_SCOPE(scope));
        memcpy(scope->y, samples, sizeof(scope->y));
	if (gtk_widget_is_drawable(GTK_WIDGET(scope))) {
		if (!scope->idlefunc)
			scope->idlefunc = g_idle_add_full(PRIO, idle_callback, scope, NULL);
	}
}

#if 0
void scope_setmarker(Scope *scope, int pointer)
{
	g_return_if_fail(scope != NULL);
	g_return_if_fail(IS_SCOPE(scope));
	if (pointer >= 0 && pointer < SCOPE_WIDTH)
		scope->pointer = pointer;
	if (GTK_WIDGET_DRAWABLE(GTK_WIDGET(scope))) {
		if (!scope->idlefunc)
			scope->idlefunc = g_idle_add_full(PRIO, idle_callback, scope, NULL);
	}
}
#endif
