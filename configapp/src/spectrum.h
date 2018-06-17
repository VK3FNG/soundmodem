/* 
 * Spectrum Widget
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

#ifndef __SPECTRUM_H__
#define __SPECTRUM_H__

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define SPECTRUM(obj)          G_TYPE_CHECK_INSTANCE_CAST(obj, spectrum_get_type(), Spectrum)
#define SPECTRUM_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST(klass, spectrum_get_type(), SpectrumClass)
#define IS_SPECTRUM(obj)       G_TYPE_CHECK_INSTANCE_TYPE(obj, spectrum_get_type())

typedef struct _Spectrum        Spectrum;
typedef struct _SpectrumClass   SpectrumClass;

#define SPECTRUM_NUMSAMPLES  1024

#define SPECTRUM_WIDTH    ((SPECTRUM_NUMSAMPLES) >> 1)
#define SPECTRUM_HEIGHT   384

struct _Spectrum 
{
	GtkWidget widget;

	guint idlefunc;
	GdkGC *trace_gc;
	GdkGC *grid_gc;
	GdkGC *pointer_gc;
	GdkColor tracecol;
	GdkColor gridcol;
	GdkColor pointercol;

	GdkPixmap *pixmap;

	/* marker */
        unsigned int pointer;

	unsigned short y[SPECTRUM_WIDTH];
	float window[SPECTRUM_NUMSAMPLES];
};

struct _SpectrumClass
{
	GtkWidgetClass parent_class;
};


guint spectrum_get_type(void);
GtkWidget* spectrum_new(const char *name, void *dummy0, void *dummy1, unsigned int dummy2, unsigned int dummy3);
void spectrum_setdata(Spectrum *spec, short *samples);
void spectrum_setmarker(Spectrum *spec, int pointer);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __SPECTRUM_H__ */
