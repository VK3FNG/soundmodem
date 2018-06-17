/*****************************************************************************/

/*
 *      configapp.h  --  Configuration Application Header File.
 *
 *      Copyright (C) 2000
 *        Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*****************************************************************************/

#ifndef _CONFIGAPP_H
#define _CONFIGAPP_H

/* ---------------------------------------------------------------------- */

#define _GNU_SOURCE
#define _REENTRANT

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#include "modem.h"
#include "soundio.h"

/* ---------------------------------------------------------------------- */

extern struct modulator modchain_x;
extern struct demodulator demodchain_x;
extern struct modemparams chaccparams_x[];
extern struct modemparams pktkissparams_x[];
extern struct modemparams pktmkissparams_x[];
extern struct modemparams ioparam_type[];

extern GtkWidget *mainwindow, *specwindow, *scopewindow, *receivewindow, *p3dwindow;
extern GtkTreeModel *configmodel;
extern void new_configuration(const gchar *name);
extern void new_channel(const gchar *cfgname, const gchar *name);
extern void renumber_channels(void);
extern void error_dialog(const gchar *text);
extern int xml_newconfig(const char *newname);
extern const char *xml_newchannel(const char *cfgname);
extern int xml_deleteconfig(const char *newname);
extern int xml_deletechannel(const char *cfgname, const char *chname);
extern int xml_setprop(const char *cfgname, const char *chname, const char *typname, const char *propname, const char *data);
extern int xml_getprop(const char *cfgname, const char *chname, const char *typname, const char *propname, char *buf, unsigned int bufsz);
extern GtkTreeModel *create_configmodel(void);

/* diagnostics stuff */
extern void diag_stop(void);


/* ---------------------------------------------------------------------- */
#endif /* _CONFIGAPP_H */
