/* 
 * Sooundmodem Spectrum Widget
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

#include "spectrum.h"
#include "snm-compat-gtk2.h"
#include <gtk/gtkgc.h>
#include <gtk/gtkmain.h>
#include <math.h>
#include <string.h>

#include <stdio.h>
#include <string.h>

/* --------------------------------------------------------------------- */

static inline float hamming(float x)
{
        return 0.54-0.46*cos(2*M_PI*x);
}

/* ---------------------------------------------------------------------- */

/*
 * This fft routine is from ~gabriel/src/filters/fft/fft.c;
 * I am unsure of the original source.  The file contains no
 * copyright notice or description.
 * The declaration is changed to the prototype form but the
 * function body is unchanged.  (J. T. Buck)
 */

#define SWAP(a, b) tempr=(a); (a)=(b); (b)=tempr

/*
 * Replace data by its discrete Fourier transform, if isign is
 * input as 1, or by its inverse discrete Fourier transform, if 
 * "isign" is input as -1.  "data'"is a complex array of length "nn",
 * input as a real array data[0..2*nn-1]. "nn" MUST be an integer
 * power of 2 (this is not checked for!?)
 */

static void fft_rif(float *data, int nn, int isign) 
{
        int n;
        int mmax;
        int m, j, istep, i;
        float wtemp, wr, wpr, wpi, wi, theta;
        float tempr, tempi;

        data--;
        n = nn << 1;
        j = 1;

        for (i = 1; i < n; i += 2) {
                if(j > i) {
                        SWAP(data[j], data[i]);
                        SWAP(data[j+1], data[i+1]);
                }
                m= n >> 1;
                while (m >= 2 && j >m) {
                        j -= m;
                        m >>= 1;
                }
                j += m;
        }
        mmax = 2;
        while (n > mmax) {
                istep = 2*mmax;
                theta = -6.28318530717959/(isign*mmax);
                wtemp = sin(0.5*theta);
                wpr = -2.0*wtemp*wtemp;
                wpi = sin(theta);
                wr = 1.0;
                wi = 0.0;
                for (m = 1; m < mmax; m += 2) {
                        for (i = m; i < n; i += istep) {
                                j = i + mmax;
                                tempr = wr*data[j] - wi*data[j+1];
                                tempi = wr*data[j+1] + wi*data[j];
                                data[j] = data[i] - tempr;
                                data[j+1] = data[i+1] - tempi;
                                data[i] += tempr;
                                data[i+1] += tempi;
                        }
                        wr = (wtemp=wr)*wpr - wi*wpi+wr;
                        wi = wi*wpr + wtemp*wpi + wi;
                }
                mmax = istep;
        }
}
        
#undef SWAP

/* --------------------------------------------------------------------- */

static inline float fsqr(float x) 
{ 
        return x*x;
}

/* --------------------------------------------------------------------- */

#define PRIO G_PRIORITY_LOW
#define DBSPAN 80

static void spectrum_class_init(SpectrumClass *klass);
static void spectrum_init(Spectrum *spec);
static void spectrum_finalize(GObject *object);
static gint spectrum_expose(GtkWidget *widget, GdkEventExpose *event);
static void spectrum_realize(GtkWidget *widget);
static void spectrum_unrealize(GtkWidget *widget);
static void spectrum_size_allocate(GtkWidget *widget, GtkAllocation *allocation);
static void spectrum_send_configure (Spectrum *spec);
static gint idle_callback(gpointer data);

static GtkWidgetClass *parent_class = NULL;
static SpectrumClass *spectrum_class = NULL;


guint spectrum_get_type(void)
{
	static guint spectrum_type = 0;

	if (!spectrum_type)
	{
		static const GTypeInfo spectrum_info =
		{
			sizeof(SpectrumClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc)spectrum_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof(Spectrum),
			0,		/* n_preallocs */
			(GInstanceInitFunc)spectrum_init,
		};
		spectrum_type = g_type_register_static 
		  (GTK_TYPE_WIDGET, "Spectrum", &spectrum_info, 0);
	}
	return spectrum_type;
}

static void spectrum_class_init(SpectrumClass *klass)
{
	GObjectClass   *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass*)klass;
	widget_class = (GtkWidgetClass*)klass;

	parent_class = g_type_class_peek(GTK_TYPE_WIDGET);
	spectrum_class = klass;

	object_class->finalize = spectrum_finalize;
	widget_class->expose_event = spectrum_expose;
	widget_class->realize = spectrum_realize;
	widget_class->unrealize = spectrum_unrealize;
	widget_class->size_allocate = spectrum_size_allocate;
}

static void spectrum_init(Spectrum *spec)
{
	spec->idlefunc = 0;
	/* initialize the colors */
	spec->tracecol.red = 11796;
	spec->tracecol.green = 53740;
	spec->tracecol.blue = 4588;
	spec->gridcol.red = 52429;
	spec->gridcol.green = 52429;
	spec->gridcol.blue = 52429;
	spec->pointercol.red = 65535;
	spec->pointercol.green = 0;
	spec->pointercol.blue = 65535;
	spec->trace_gc = spec->grid_gc = spec->pointer_gc = NULL;
	spec->pixmap = NULL;
	/* initialize the data */
	memset(&spec->y, 0, sizeof(spec->y));
	spec->pointer = 0;
}

static void spectrum_realize(GtkWidget *widget)
{
	Spectrum *spec;
	GdkWindowAttr attributes;
	gint attributes_mask;
	GdkGCValues gc_values;
	GtkAllocation allocation;
	GdkWindow *window;
	GtkStyle *style;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(IS_SPECTRUM(widget));

	spec = SPECTRUM(widget);
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
	gdk_window_set_user_data(window, spec);

	gtk_widget_style_attach(widget);
	style = gtk_widget_get_style(widget);
	gtk_style_set_background(style, window, GTK_STATE_NORMAL);

	/* gc's if necessary */
	if (!gdk_colormap_alloc_color(style->colormap, &spec->tracecol,
				      FALSE, TRUE))
		g_warning("unable to allocate color: ( %d %d %d )",
			  spec->tracecol.red, spec->tracecol.green, spec->tracecol.blue);
	gc_values.foreground = spec->tracecol;
	spec->trace_gc = gtk_gc_get(style->depth, 
				    style->colormap,
				    &gc_values, GDK_GC_FOREGROUND);
	if (!gdk_colormap_alloc_color(style->colormap, &spec->gridcol,
				      FALSE, TRUE))
		g_warning("unable to allocate color: ( %d %d %d )",
			  spec->gridcol.red, spec->gridcol.green, spec->gridcol.blue);
	gc_values.foreground = spec->gridcol;
	spec->grid_gc = gtk_gc_get(style->depth,
				   style->colormap,
				   &gc_values, GDK_GC_FOREGROUND);
	if (!gdk_colormap_alloc_color(style->colormap, 
				      &spec->pointercol, FALSE, TRUE))
		g_warning("unable to allocate color: ( %d %d %d )",
			  spec->pointercol.red, spec->pointercol.green, spec->pointercol.blue);
	gc_values.foreground = spec->pointercol;
	spec->pointer_gc = gtk_gc_get(style->depth,
				      style->colormap,
				      &gc_values, GDK_GC_FOREGROUND);
	/* create backing store */
	spec->pixmap = gdk_pixmap_new(window, SPECTRUM_WIDTH, SPECTRUM_HEIGHT, -1);

	spectrum_send_configure(SPECTRUM(widget));
}

static void spectrum_unrealize(GtkWidget *widget)
{
	Spectrum *spec;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(IS_SPECTRUM(widget));

	spec = SPECTRUM(widget);
	if (spec->idlefunc)
		g_source_remove(spec->idlefunc);
	if (spec->trace_gc)
		gtk_gc_release(spec->trace_gc);
	if (spec->grid_gc)
		gtk_gc_release(spec->grid_gc);
	if (spec->pointer_gc)
		gtk_gc_release(spec->pointer_gc);
	spec->trace_gc = spec->grid_gc = spec->pointer_gc = NULL;
	if (spec->pixmap)
			g_object_unref(spec->pixmap);
	spec->pixmap = NULL;
	if (GTK_WIDGET_CLASS(parent_class)->unrealize)
		(*GTK_WIDGET_CLASS(parent_class)->unrealize)(widget);
}

static void spectrum_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	GtkAllocation alloc;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(IS_SPECTRUM(widget));
	g_return_if_fail(allocation != NULL);

	alloc = *allocation;
	alloc.width = SPECTRUM_WIDTH;
	alloc.height = SPECTRUM_HEIGHT;
	gtk_widget_set_allocation(widget, &alloc);

	if (gtk_widget_get_realized(widget)) {
		gdk_window_move_resize (gtk_widget_get_window(widget),
					allocation->x, allocation->y,
					allocation->width, allocation->height);
		spectrum_send_configure(SPECTRUM(widget));
	}
}

static void spectrum_send_configure(Spectrum *spec)
{
	GtkWidget *widget;
	GdkEventConfigure event;
	GtkAllocation allocation;

	widget = GTK_WIDGET(spec);
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


GtkWidget* spectrum_new(const char *name, void *dummy0, void *dummy1, unsigned int dummy2, unsigned int dummy3)
{
	Spectrum *spec;
	unsigned int i;
	
	spec = g_object_new(spectrum_get_type(), NULL);
	memset(&spec->y, 0, sizeof(spec->y));
	for (i = 0; i < SPECTRUM_NUMSAMPLES; i++)
		spec->window[i] = (1.0 / 32767.0 / SPECTRUM_NUMSAMPLES) * hamming(i * (1.0 / (SPECTRUM_NUMSAMPLES-1)));
	return GTK_WIDGET(spec);
}

static void spectrum_finalize(GObject *object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_SPECTRUM(object));
	(*G_OBJECT_CLASS(parent_class)->finalize)(object);
}

static gint spectrum_expose(GtkWidget *widget, GdkEventExpose *event)
{
	Spectrum *spec;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(IS_SPECTRUM(widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	if (gtk_widget_is_drawable(widget)) {
		spec = SPECTRUM(widget);
		if (!spec->idlefunc)
			spec->idlefunc = g_idle_add_full(PRIO, idle_callback, spec, NULL);
	}
	return FALSE;
}

static void draw(Spectrum *spec)
{
	guint segcnt, i;
	GdkPoint pt[SPECTRUM_WIDTH+1];
	GdkSegment seg[100], *segp;
	GtkWidget *widget;
	GtkAllocation allocation;
	GtkStyle *style;

	widget = GTK_WIDGET(spec);
	g_return_if_fail(gtk_widget_is_drawable(widget));
	g_return_if_fail(spec->pixmap);
	gtk_widget_get_allocation(widget, &allocation);
	style = gtk_widget_get_style(widget);
	/* calculate grid segments */
	for (segp = seg, segcnt = i = 0; i < SPECTRUM_WIDTH; i += SPECTRUM_WIDTH/8) {
		segp->x1 = segp->x2 = i;
		segp->y1 = 0;
		segp->y2 = SPECTRUM_HEIGHT-1;
		segp++;
		segcnt++;
	}
	for (i = 0; i < SPECTRUM_HEIGHT; i += (SPECTRUM_HEIGHT * 10 + DBSPAN/2) / DBSPAN) {
		segp->y1 = segp->y2 = i;
		segp->x1 = 0;
		segp->x2 = SPECTRUM_WIDTH-1;
		segp++;
		segcnt++;
	}
	/* copy data points */
	for (i = 0; i < SPECTRUM_WIDTH; i++) {
		pt[i].x = i;
		pt[i].y = spec->y[i];
	}
	/* clear window */
	gdk_draw_rectangle(spec->pixmap, style->base_gc[gtk_widget_get_state(widget)],
			   TRUE, 0, 0, 
			   allocation.width, 
			   allocation.height);
	/* draw grid */
	gdk_draw_segments(spec->pixmap, spec->grid_gc, seg, segcnt);
	/* draw markers */
	gdk_draw_line(spec->pixmap, spec->pointer_gc, 
		      spec->pointer,
		      0, 
		      spec->pointer, 
		      allocation.height-1);
	/* draw trace */
	gdk_draw_lines(spec->pixmap, spec->trace_gc, pt, SPECTRUM_WIDTH);
	/* draw to screen */
	gdk_draw_drawable(gtk_widget_get_window(widget), style->base_gc[gtk_widget_get_state(widget)], spec->pixmap, 
			  0, 0, 0, 0, allocation.width, allocation.height);
}


static gint idle_callback(gpointer data)
{
	g_return_val_if_fail(data != NULL, FALSE);
	g_return_val_if_fail(IS_SPECTRUM(data), FALSE);
	SPECTRUM(data)->idlefunc = 0;
	if (!gtk_widget_is_drawable(GTK_WIDGET(data)))
		return FALSE;
	draw(SPECTRUM(data));
	return FALSE;  /* don't call this callback again */
}

void spectrum_setdata(Spectrum *spec, short *samples)
{
	float fdata[2*SPECTRUM_NUMSAMPLES];
	float f;
	unsigned int i;

	g_return_if_fail(spec != NULL);
	g_return_if_fail(IS_SPECTRUM(spec));
	for (i = 0; i < SPECTRUM_NUMSAMPLES; i++) {
		fdata[2*i] = samples[i] * spec->window[i];
		fdata[2*i+1] = 0;
	}
	fft_rif(fdata, SPECTRUM_NUMSAMPLES, 1);
	for (i = 0; i < SPECTRUM_NUMSAMPLES; i++) {
		f = log10(fsqr(fdata[2*i]) + fsqr(fdata[2*i+1]));
		f *= -(10.0 * SPECTRUM_HEIGHT / DBSPAN);
		if (f < 0)
			f = 0;
		if (f > SPECTRUM_HEIGHT-1)
			f = SPECTRUM_HEIGHT-1;
		spec->y[i] = f;
	}
	if (gtk_widget_is_drawable(GTK_WIDGET(spec))) {
		if (!spec->idlefunc)
			spec->idlefunc = g_idle_add_full(PRIO, idle_callback, spec, NULL);
	}
}

void spectrum_setmarker(Spectrum *spec, int pointer)
{
	g_return_if_fail(spec != NULL);
	g_return_if_fail(IS_SPECTRUM(spec));
	if (pointer >= 0 && pointer < SPECTRUM_WIDTH)
		spec->pointer = pointer;
	if (gtk_widget_is_drawable(GTK_WIDGET(spec))) {
		if (!spec->idlefunc)
			spec->idlefunc = g_idle_add_full(PRIO, idle_callback, spec, NULL);
	}
}
