/*****************************************************************************/

/*
 *      app.c  --  Configuration Application.
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

#define _GNU_SOURCE
#define _REENTRANT

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* AIX requires this to be the first thing in the file.  */
#ifndef __GNUC__
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
#pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif
#  endif
# endif
#endif

#include "configapp.h"

#include <gtk/gtk.h>

#include "interface.h"
#include "support.h"
#include "callbacks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  LABEL_COL,
  CFGNAME_COL,
  CHNAME_COL,
  NUM_COLUMNS
};

typedef struct _CallbackData CallbackData;
struct _CallbackData
{
  GtkTreeModel *model;
  GtkTreePath *path;
};

/* ---------------------------------------------------------------------- */

#ifdef WIN32

/* free result with g_free */

static gchar *strtogtk(const char *in)
{
        WCHAR *wch, *wp;
        GdkWChar *gch, *gp;
        unsigned int i, len;
        
        if (!(len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, in, -1, NULL, 0)))
                return NULL;
        wch = wp = alloca(sizeof(wch[0])*len);
        gch = gp = alloca(sizeof(gch[0])*(len+1));
        if (!MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, in, -1, wch, len))
                return NULL;
        for (i = 0, wp = wch, gp = gch; i < len && *wp; i++, wp++, gp++)
                *gp = *wp;
        *gp = 0;
        return gdk_wcstombs(gch);
}

static char *gtktostr(const gchar *in)
{
        union {
                GdkWChar g[4096];
                WCHAR w[4096];
        } u;
        GdkWChar *gp;
        WCHAR *wp;
        gint len;
        unsigned int i;
        int len2;
        char *ret;

        if ((len = gdk_mbstowcs(u.g, in, sizeof(u.g)/sizeof(u.g[0]))) == -1)
                return NULL;
        for (wp = u.w, gp = u.g, i = 0; i < len; i++, gp++, wp++)
                *wp = *gp;
        if (!(len2 = WideCharToMultiByte(CP_ACP, 0, u.w, len, NULL, 0, NULL, NULL)))
                return NULL;
        ret = g_malloc(len2+1);
        if (!ret)
                return NULL;
        WideCharToMultiByte(CP_ACP, 0, u.w, len, ret, len2, NULL, NULL);
        ret[len2] = 0;
        return ret;
}

#endif /* WIN32 */

/* ---------------------------------------------------------------------- */

static GtkWidget *create_notebookhead(GList *combo_items)
{
	GtkWidget *vbox, *hbox, *label, *combo, *hsep;
	GList *l;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	label = gtk_label_new(_("Mode"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 5);
	gtk_misc_set_padding(GTK_MISC(label), 7, 7);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	combo = gtk_combo_box_new_text();
	gtk_widget_show(combo);
	for (l = combo_items; l; l = l->next) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
					  (const gchar *)l->data);
	}
	gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, TRUE, 0);
	hsep = gtk_hseparator_new();
	gtk_widget_show(hsep);
	gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, TRUE, 3);
	g_object_set_data(G_OBJECT(vbox), "combo", combo);
	return vbox;
}

static GtkWidget *create_paramwidget(const struct modemparams *par, const char *cfgname, const char *chname, const char *typname)
{
	const struct modemparams *par2 = par;
	unsigned int parcnt = 0, i, j;
	GtkWidget *table, *w1;
	GtkObject *o1;
#ifndef HAVE_GTK_WIDGET_SET_TOOLTIP_TEXT
	GtkTooltips *tooltips;
#endif
        char buf[128];
	const char *value, * const *cp;
	double nval;
	int active;
#ifdef WIN32
        gchar *valueg;
        gchar *combov;
#endif
        
	if (!par)
		return gtk_vbox_new(FALSE, 0);
	while (par2->name) {
		par2++;
		parcnt++;
	}
	table = gtk_table_new(parcnt, 2, FALSE);
	gtk_widget_show(table);
#ifndef HAVE_GTK_WIDGET_SET_TOOLTIP_TEXT
	tooltips = gtk_tooltips_new();
#endif
	for (par2 = par, i = 0; i < parcnt; i++, par2++) {
		w1 = gtk_label_new(par2->label);
		gtk_widget_show(w1);
		gtk_table_attach(GTK_TABLE(table), w1, 0, 1, i, i+1, 
				 (GtkAttachOptions)(GTK_FILL), (GtkAttachOptions) 0, 5, 5);
		gtk_misc_set_alignment(GTK_MISC(w1), 0, 0.5);
		gtk_label_set_justify(GTK_LABEL(w1), GTK_JUSTIFY_LEFT);
		if (xml_getprop(cfgname, chname, typname, par2->name, buf, sizeof(buf)) > 0)
                        value = buf;
                else
			value = par2->dflt;
		switch (par2->type) {
		case MODEMPAR_STRING:
			w1 = gtk_entry_new();
#ifdef WIN32
                        valueg = strtogtk(value);
			gtk_entry_set_text(GTK_ENTRY(w1), valueg ?: "(null)");
                        g_free(valueg);
#else
			gtk_entry_set_text(GTK_ENTRY(w1), value);
#endif
			break;

		case MODEMPAR_COMBO:
			w1 = gtk_combo_box_entry_new_text();
			active = -1;
#ifdef WIN32
			valueg = strtogtk(value);
			for (cp = par2->u.c.combostr, j = 0; *cp && j < 8; j++, cp++) {
				combov = strtogtk(*cp);
				gtk_combo_box_append_text(GTK_COMBO_BOX(w1), combov);
				if (strcmp(combov, valueg) == 0)
					active = j;
				g_free(combov);
			}
			if (active == -1) {
				gtk_combo_box_append_text(GTK_COMBO_BOX(w1), valueg);
				active = j;
			}
			g_free (valueg);
#else
			for (cp = par2->u.c.combostr, j = 0; *cp && j < 8; j++, cp++) {
				gtk_combo_box_append_text(GTK_COMBO_BOX(w1), *cp);
				if (strcmp(*cp, value) == 0)
					active = j;
			}
			if (active == -1) {
				gtk_combo_box_append_text(GTK_COMBO_BOX(w1), value);
				active = j;
			}
#endif
			gtk_combo_box_set_active (GTK_COMBO_BOX(w1), active);
			break;

		case MODEMPAR_NUMERIC:
			nval = strtod(value, NULL);
			if (nval < par2->u.n.min)
				nval = par2->u.n.min;
			if (nval > par2->u.n.max)
				nval = par2->u.n.max;
			o1 = gtk_adjustment_new(nval, par2->u.n.min, par2->u.n.max, par2->u.n.step, 
						par2->u.n.pagestep, 0);
			w1 = gtk_spin_button_new(GTK_ADJUSTMENT(o1), par2->u.n.step, 0);
			break;

		case MODEMPAR_CHECKBUTTON:
			w1 = gtk_check_button_new();
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w1), (*value == '0') ? FALSE : TRUE);
			break;

		default:
			continue;
		}
		gtk_widget_show(w1);
		g_object_set_data(G_OBJECT(table), par2->name, w1);
		gtk_table_attach(GTK_TABLE(table), w1, 1, 2, i, i+1, 
				 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 5, 5);
		if (par2->tooltip)
#ifdef HAVE_GTK_WIDGET_SET_TOOLTIP_TEXT
			gtk_widget_set_tooltip_text (w1, par2->tooltip);
#else
			gtk_tooltips_set_tip(tooltips, w1, par2->tooltip, NULL);
#endif
	}
	return table;
}

static void update_paramwidget(GtkWidget *table, const struct modemparams *par, const char *cfgname, const char *chname, const char *typname)
{
	const struct modemparams *par2 = par;
	GtkWidget *w1;
	char buf[256], *txt2;
#ifdef WIN32
        char *txt;
#endif
        
	if (!par)
		return;
	for (par2 = par; par2->name; par2++) {
		w1 = g_object_get_data(G_OBJECT(table), par2->name);
		if (!w1)
			continue;
		switch (par2->type) {
		case MODEMPAR_STRING:
#ifdef WIN32
                        txt = gtktostr(gtk_entry_get_text(GTK_ENTRY(w1)));
			xml_setprop(cfgname, chname, typname, par2->name, txt ?: "(null)");
                        g_free(txt);
#else
			xml_setprop(cfgname, chname, typname, par2->name, gtk_entry_get_text(GTK_ENTRY(w1)));
#endif
			break;

		case MODEMPAR_COMBO:
			txt2 = gtk_combo_box_get_active_text (GTK_COMBO_BOX(w1));
#ifdef WIN32
                        txt = gtktostr(txt2);
			xml_setprop(cfgname, chname, typname, par2->name, txt ?: "(null)");
                        g_free(txt);
#else
			xml_setprop(cfgname, chname, typname, par2->name, txt2);
#endif
			g_free(txt2);
			break;

		case MODEMPAR_NUMERIC:
			sprintf(buf, "%g", gtk_spin_button_get_value(GTK_SPIN_BUTTON(w1)));
			xml_setprop(cfgname, chname, typname, par2->name, buf);
			break;

		case MODEMPAR_CHECKBUTTON:
			xml_setprop(cfgname, chname, typname, par2->name, 
				    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w1)) ? "1" : "0");
			break;

		default:
			continue;
		}
	}
}

static void on_iotypecombochg_changed(GtkEditable *editable, gpointer user_data);

static void cfg_select(const char *cfgname, const char *chname)
{
	GtkWidget *notebook, *w1, *w2, *combo;
	struct modemparams *ioparams = ioparams_soundcard;
	GList *ilist;
	char buf[128];
	unsigned int i;
	int active;

	g_print("config_select: cfg: %s  chan: %s\n", cfgname ?: "-", chname ?: "-");

	notebook = GTK_WIDGET(g_object_get_data(G_OBJECT(mainwindow), "confignotebook"));
	/* compute audio IO types */
	ilist = NULL;
	if (xml_getprop(cfgname, NULL, "audio", ioparam_type[0].name, buf, sizeof(buf)) <= 0) {
                buf[0] = 0;
		active = -1;
	}
	if (!strcmp(buf, ioparam_type[0].u.c.combostr[1])) {
		ioparams = ioparams_filein;
		active = 1;
	} else if (!strcmp(buf, ioparam_type[0].u.c.combostr[2])) {
		ioparams = ioparams_sim;
		active = 2;
#ifdef HAVE_ALSA
	} else if (!strcmp(buf, ioparam_type[0].u.c.combostr[3])) {
		ioparams = ioparams_alsasoundcard;
		active = 3;
#endif /* HAVE_ALSA */
        } else {
		ioparams = ioparams_soundcard;
		strncpy(buf, ioparam_type[0].u.c.combostr[0], sizeof(buf));
		active = 0;
	}
	for (i = 0; i < 8; i++)
		if (ioparam_type[0].u.c.combostr[i])
			ilist = g_list_append(ilist, (void *)ioparam_type[0].u.c.combostr[i]);
	w1 = create_notebookhead(ilist);
	g_object_set_data(G_OBJECT(w1), "cfgname", (void *)cfgname);
	g_object_set_data(G_OBJECT(w1), "chname", (void *)chname);
	g_list_free(ilist);
	combo = GTK_WIDGET(g_object_get_data(G_OBJECT(w1), "combo"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
	/* create rest of notebook */
	w2 = create_paramwidget(ioparams, cfgname, NULL, "audio");
	gtk_box_pack_start(GTK_BOX(w1), w2, TRUE, TRUE, 10);
	g_object_set_data(G_OBJECT(w1), "audio", w2);
	g_object_set_data(G_OBJECT(w1), "audioparams", ioparams);
	w2 = gtk_hseparator_new();
	gtk_widget_show(w2);
	gtk_box_pack_start(GTK_BOX(w1), w2, FALSE, TRUE, 0);
	w2 = create_paramwidget(pttparams, cfgname, NULL, "ptt");
	gtk_box_pack_start(GTK_BOX(w1), w2, TRUE, TRUE, 10);
	g_object_set_data(G_OBJECT(w1), "ptt", w2);
	w2 = gtk_hseparator_new();
	gtk_widget_show(w2);
	gtk_box_pack_start(GTK_BOX(w1), w2, FALSE, TRUE, 0);

	w2 = gtk_label_new(_("IO"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), w1, w2);
	/* connect change signal */
	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(on_iotypecombochg_changed), NULL);

	/* second page contains channel access parameters */
	w1 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(w1);
	g_object_set_data(G_OBJECT(w1), "cfgname", (void *)cfgname);
	g_object_set_data(G_OBJECT(w1), "chname", (void *)chname);
	w2 = create_paramwidget(chaccparams_x, cfgname, NULL, "chaccess");
	gtk_box_pack_start(GTK_BOX(w1), w2, TRUE, TRUE, 10);
	g_object_set_data(G_OBJECT(w1), "chacc", w2);

	w2 = gtk_label_new(_("Channel Access"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), w1, w2);
}

static void cfg_deselect(const char *cfgname, const char *chname)
{
	GtkWidget *notebook, *w1, *combo;
	struct modemparams *ioparams;
	gchar *text;

	g_print("config_deselect: cfg: %s  chan: %s\n", cfgname ?: "-", chname ?: "-");

	notebook = GTK_WIDGET(g_object_get_data(G_OBJECT(mainwindow), "confignotebook"));
	w1 = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 1);
	if (w1) {
		update_paramwidget(GTK_WIDGET(g_object_get_data(G_OBJECT(w1), "chacc")), chaccparams_x, cfgname, NULL, "chaccess");
		gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), 1);
	}
	w1 = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0);
	if (!w1)
		return;
	/* update type */
	combo = GTK_WIDGET(g_object_get_data(G_OBJECT(w1), "combo"));
	text = gtk_combo_box_get_active_text (GTK_COMBO_BOX(combo));
	xml_setprop(cfgname, NULL, "audio", ioparam_type[0].name, text);
	g_free(text);
	ioparams = (struct modemparams *)g_object_get_data(G_OBJECT(w1), "audioparams");
	update_paramwidget(GTK_WIDGET(g_object_get_data(G_OBJECT(w1), "audio")), ioparams, cfgname, NULL, "audio");
	update_paramwidget(GTK_WIDGET(g_object_get_data(G_OBJECT(w1), "ptt")), pttparams, cfgname, NULL, "ptt");
	gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), 0);
}

static guint iotypecombochg = 0;

static gint do_iotypecombochg_change(gpointer user_data)
{
	GtkWidget *notebook, *w;
	const char *cfgname, *chname;
	gint nbcurpage;

	iotypecombochg = 0;
	/* recreate notebook widgets */
	notebook = GTK_WIDGET(g_object_get_data(G_OBJECT(mainwindow), "confignotebook"));
	/* find config strings */
	w = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0);
	cfgname = g_object_get_data(G_OBJECT(w), "cfgname");
	chname = g_object_get_data(G_OBJECT(w), "chname");

	g_print("on_notebookcombo_changed: cfg: %s  chan: %s\n", cfgname ?: "-", chname ?: "-");

	nbcurpage = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
	cfg_deselect(cfgname, chname);
	g_print("Recreating menus\n");
	cfg_select(cfgname, chname);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), nbcurpage);
	g_print("Returning\n");
	return FALSE;
}

static void on_iotypecombochg_changed(GtkEditable *editable, gpointer user_data)
{
	if (!iotypecombochg)
		iotypecombochg = g_idle_add_full(G_PRIORITY_HIGH, do_iotypecombochg_change, 
						 NULL, NULL);
}


struct packetio {
	struct packetio *next;
	const char *name;
	const struct modemparams *params;
};

#ifndef WIN32
#ifdef HAVE_MKISS

static struct packetio pktkiss = { NULL, "KISS", pktkissparams_x };
static struct packetio packetchain = { &pktkiss, "MKISS", pktmkissparams_x };

#else /* HAVE_MKISS */

static struct packetio packetchain = { NULL, "KISS", pktkissparams_x };

#endif /* HAVE_MKISS */
#endif /* WIN32 */


static void on_notebookcombo_changed(GtkEditable *editable, gpointer user_data);

static void make_notebook_menus(const char *cfgname, const char *chname)
{
	GtkWidget *notebook, *w1, *w2, *combo;
	GList *ilist;
	struct modulator *modch = &modchain_x, *modch1 = &modchain_x;
	struct demodulator *demodch = &demodchain_x, *demodch1 = &demodchain_x;
#ifndef WIN32
	struct packetio *pktch = &packetchain, *pktch1 = &packetchain;
#endif /* WIN32 */
	char mode[128];
	int i, active;

	notebook = GTK_WIDGET(g_object_get_data(G_OBJECT(mainwindow), "confignotebook"));
	/* Modulator Tab */
	ilist = NULL;
	if (xml_getprop(cfgname, chname, "mod", "mode", mode, sizeof(mode)) <= 0)
                mode[0] = 0;
	for (i = 0, active = 0; modch; i++, modch = modch->next) {
		ilist = g_list_append(ilist, (void *)modch->name);
		if (!strcmp(mode, modch->name)) {
			modch1 = modch;
			active = i;
		}
	}
	w1 = create_notebookhead(ilist);
	g_list_free(ilist);
	combo = GTK_WIDGET(g_object_get_data(G_OBJECT(w1), "combo"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
	w2 = create_paramwidget(modch1->params, cfgname, chname, "mod");
	g_object_set_data(G_OBJECT(w1), "cfgname", (void *)cfgname);
	g_object_set_data(G_OBJECT(w1), "chname", (void *)chname);
	g_object_set_data(G_OBJECT(w1), "par", (void *)modch1->params);
	g_object_set_data(G_OBJECT(w1), "table", w2);
	gtk_box_pack_start(GTK_BOX(w1), w2, TRUE, TRUE, 1);
	w2 = gtk_label_new(_("Modulator"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), w1, w2);
	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(on_notebookcombo_changed), NULL);
	/* Demodulator Tab */
	ilist = NULL;
	if (xml_getprop(cfgname, chname, "demod", "mode", mode, sizeof(mode)) <= 0)
                mode[0] = 0;
	for (i = 0, active = 0; demodch; i++, demodch = demodch->next) {
		ilist = g_list_append(ilist, (void *)demodch->name);
		if (!strcmp(mode, demodch->name)) {
			demodch1 = demodch;
			active = i;
		}
	}
	w1 = create_notebookhead(ilist);
	g_list_free(ilist);
	combo = GTK_WIDGET(g_object_get_data(G_OBJECT(w1), "combo"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
	w2 = create_paramwidget(demodch1->params, cfgname, chname, "demod");
	g_object_set_data(G_OBJECT(w1), "cfgname", (void *)cfgname);
	g_object_set_data(G_OBJECT(w1), "chname", (void *)chname);
	g_object_set_data(G_OBJECT(w1), "par", (void *)demodch1->params);
	g_object_set_data(G_OBJECT(w1), "table", w2);
	gtk_box_pack_start(GTK_BOX(w1), w2, TRUE, TRUE, 1);
	w2 = gtk_label_new(_("Demodulator"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), w1, w2);
	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(on_notebookcombo_changed), NULL);
#ifndef WIN32
	/* Packet IO Tab */
	ilist = NULL;
	if (xml_getprop(cfgname, chname, "pkt", "mode", mode, sizeof(mode)) <= 0)
                mode[0] = 0;
	for (i = 0, active = 0; pktch; i++, pktch = pktch->next) {
		ilist = g_list_append(ilist, (void *)pktch->name);
		if (!strcmp(mode, pktch->name)) {
			pktch1 = pktch;
			active = i;
		}
	}
	w1 = create_notebookhead(ilist);
	g_list_free(ilist);
	combo = GTK_WIDGET(g_object_get_data(G_OBJECT(w1), "combo"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
	w2 = create_paramwidget(pktch1->params, cfgname, chname, "pkt");
	g_object_set_data(G_OBJECT(w1), "cfgname", (void *)cfgname);
	g_object_set_data(G_OBJECT(w1), "chname", (void *)chname);
	g_object_set_data(G_OBJECT(w1), "par", (void *)pktch1->params);
	g_object_set_data(G_OBJECT(w1), "table", w2);
	gtk_box_pack_start(GTK_BOX(w1), w2, TRUE, TRUE, 1);
	w2 = gtk_label_new(_("Packet IO"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), w1, w2);
	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(on_notebookcombo_changed), NULL);
#endif /* WIN32 */
}

static void destroy_notebook_menus(void)
{
	GtkWidget *w, *notebook, *combo;
	const char *cfgname, *chname;
	struct modemparams *par;
	gchar *text;
	
	notebook = GTK_WIDGET(g_object_get_data(G_OBJECT(mainwindow), "confignotebook"));
	/* find config strings */
	w = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0);
	cfgname = g_object_get_data(G_OBJECT(w), "cfgname");
	chname = g_object_get_data(G_OBJECT(w), "chname");
	if (!cfgname || !chname) {
		g_printerr("destroy_notebook_menus: cfgname or chname NULL!\n");
		return;
	}
	/* update modulator */
	combo = GTK_WIDGET(g_object_get_data(G_OBJECT(w), "combo"));
        text = gtk_combo_box_get_active_text (GTK_COMBO_BOX(combo));
	xml_setprop(cfgname, chname, "mod", "mode", text);
g_print("Modulator mode: %s\n", text);
        g_free(text);
	par = g_object_get_data(G_OBJECT(w), "par");
	update_paramwidget(GTK_WIDGET(g_object_get_data(G_OBJECT(w), "table")), par, cfgname, chname, "mod");
	/* update demodulator */
	w = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 1);
	combo = GTK_WIDGET(g_object_get_data(G_OBJECT(w), "combo"));
        text = gtk_combo_box_get_active_text (GTK_COMBO_BOX(combo));
	xml_setprop(cfgname, chname, "demod", "mode", text);
g_print("Demodulator mode: %s\n", text);
	g_free(text);
	par = g_object_get_data(G_OBJECT(w), "par");
	update_paramwidget(GTK_WIDGET(g_object_get_data(G_OBJECT(w), "table")), par, cfgname, chname, "demod");
	/* update KISS stuff */
#ifndef WIN32
	w = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 2);
	combo = GTK_WIDGET(g_object_get_data(G_OBJECT(w), "combo"));
	text = gtk_combo_box_get_active_text (GTK_COMBO_BOX(combo));
	xml_setprop(cfgname, chname, "pkt", "mode", text);
g_print("Packet IO mode: %s\n", text);
        g_free(text);
	par = g_object_get_data(G_OBJECT(w), "par");
	update_paramwidget(GTK_WIDGET(g_object_get_data(G_OBJECT(w), "table")), par, cfgname, chname, "pkt");
#endif /* WIN32 */
	/* delete pages */
#ifndef WIN32
	gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), 2);
#endif /* WIN32 */
	gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), 1);
	gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), 0);
}

static guint notebookcombochg = 0;

static gint do_notebookcombo_change(gpointer user_data)
{
	GtkWidget *notebook, *w;
	const char *cfgname, *chname;
	gint nbcurpage;

	notebookcombochg = 0;
	/* recreate notebook widgets */
	notebook = GTK_WIDGET(g_object_get_data(G_OBJECT(mainwindow), "confignotebook"));
	/* find config strings */
	w = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0);
	cfgname = g_object_get_data(G_OBJECT(w), "cfgname");
	chname = g_object_get_data(G_OBJECT(w), "chname");

	g_print("on_notebookcombo_changed: cfg: %s  chan: %s\n", cfgname ?: "-", chname ?: "-");
	nbcurpage = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
	destroy_notebook_menus();
	g_print("Recreating menus\n");
	make_notebook_menus(cfgname, chname);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), nbcurpage);
	g_print("Returning\n");
	return FALSE;
}

static void on_notebookcombo_changed(GtkEditable *editable, gpointer user_data)
{
	/*GtkWidget *vbox = GTK_WIDGET(user_data);*/

	if (!notebookcombochg)
		notebookcombochg = g_idle_add_full(G_PRIORITY_HIGH, do_notebookcombo_change, 
						   NULL, NULL);
}

static int
compare_sels (char* config_a, char *channel_a, char *config_b, char *channel_b)
{
	
	if (config_a && !config_b)
		return 1;
	if (config_b && !config_a)
		return -1;
	if (config_a) {
		int res = strcmp(config_a, config_b);
		if (res != 0)
			return res;
	}
	/* Same config */
	if (channel_a && !channel_b)
		return 1;
	if (channel_b && !channel_a)
		return -1;
	if (channel_a)
		return strcmp(channel_a, channel_b);
	/* If we get here, all inputs were NULL */
	return 0;
}

void on_configtree_selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeIter       iter;
	char *cfgname = NULL, *chname = NULL;
	char *old_cfgname = NULL, *old_chname = NULL;

	printf("on_configtree_selection_changed\n");
	old_cfgname = g_object_get_data(G_OBJECT(configmodel), "cfgname");
	old_chname = g_object_get_data(G_OBJECT(configmodel), "chname");
	if (gtk_tree_selection_get_selected(selection, &configmodel, &iter))
		gtk_tree_model_get(configmodel, &iter, CFGNAME_COL, &cfgname, CHNAME_COL, &chname, -1);
	if (old_cfgname) {
		if (compare_sels(old_cfgname, old_chname, cfgname, chname) == 0) {
			printf("no change\n");
			return;
		} else {
			if (old_chname) {
				if (notebookcombochg)
					g_source_remove(notebookcombochg);
				notebookcombochg = 0;
				destroy_notebook_menus();
				gtk_widget_hide(GTK_WIDGET(g_object_get_data(G_OBJECT(mainwindow), "diagnostics")));
				diag_stop();

			} else {
				cfg_deselect(old_cfgname, old_chname);
			}
		}
	}
	if (chname) {
		make_notebook_menus(cfgname, chname);
		gtk_widget_show(GTK_WIDGET(g_object_get_data(G_OBJECT(mainwindow), "diagnostics")));
	} else if (cfgname) {
		cfg_select(cfgname, chname);
	}
	g_object_set_data(G_OBJECT(configmodel), "cfgname", g_strdup(cfgname));
	g_object_set_data(G_OBJECT(configmodel), "chname", g_strdup(chname));
	g_free(old_cfgname);
	g_free(old_chname);

	if (cfgname && chname) {
		gtk_widget_show(g_object_get_data(G_OBJECT(mainwindow), "newchannel"));
		gtk_widget_show(g_object_get_data(G_OBJECT(mainwindow), "deleteconfiguration"));
		gtk_widget_show(g_object_get_data(G_OBJECT(mainwindow), "deletechannel"));
	} else if (cfgname) {
		gtk_widget_show(g_object_get_data(G_OBJECT(mainwindow), "newchannel"));
		gtk_widget_show(g_object_get_data(G_OBJECT(mainwindow), "deleteconfiguration"));
		gtk_widget_hide(g_object_get_data(G_OBJECT(mainwindow), "deletechannel"));
	} else {
		gtk_widget_hide(g_object_get_data(G_OBJECT(mainwindow), "newchannel"));
		gtk_widget_hide(g_object_get_data(G_OBJECT(mainwindow), "deleteconfiguration"));
		gtk_widget_hide(g_object_get_data(G_OBJECT(mainwindow), "deletechannel"));
	}
	g_print("selection: cfg: %s  chan: %s\n", cfgname ?: "-", chname ?: "-");
}


GtkTreeModel *
create_configmodel(void)
{
	GtkTreeStore *model;
	GtkWidget *view;
	GtkTreeSelection *selection;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	
	model = gtk_tree_store_new(NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	view = g_object_get_data(G_OBJECT(mainwindow), "configtree");
	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(model));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	g_signal_connect_after((gpointer) selection, "changed",
			       G_CALLBACK(on_configtree_selection_changed),
			       NULL);

	gtk_tree_selection_set_mode(GTK_TREE_SELECTION(selection),
				    GTK_SELECTION_BROWSE);
	cell = gtk_cell_renderer_text_new();

	column = gtk_tree_view_column_new_with_attributes("Configurations",
							  cell,
							  "text", LABEL_COL,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view),
				    GTK_TREE_VIEW_COLUMN(column));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);

	return GTK_TREE_MODEL(model);
}


/* ---------------------------------------------------------------------- */

static void dounselect(void)
{
	GtkWidget *view;
	GtkTreeSelection *selection;

	view = g_object_get_data(G_OBJECT(mainwindow), "configtree");
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_unselect_all(selection);
}

void on_quit_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	dounselect();
        gtk_main_quit();
}

gboolean on_mainwindow_destroy_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	g_print("destroy event\n");
	dounselect();
        gtk_main_quit();
	return TRUE;
}

gboolean on_mainwindow_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	g_print("delete event\n");
	dounselect();
        gtk_main_quit();
	return FALSE;
}

static void on_aboutok_clicked(GtkButton *button, gpointer user_data)
{
	gtk_widget_hide(GTK_WIDGET(user_data));
	gtk_widget_destroy(GTK_WIDGET(user_data));
}

void on_about_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *dlg = create_aboutwindow();

	g_signal_connect(G_OBJECT(g_object_get_data(G_OBJECT(dlg), "aboutok")), 
			 "clicked", G_CALLBACK(on_aboutok_clicked), dlg);
	gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(mainwindow));
	gtk_widget_show(dlg);
}


/* ---------------------------------------------------------------------- */

static gboolean findconfigitem(const gchar *name, GtkTreeIter *iter)
{
	char *str;

	if (!gtk_tree_model_get_iter_first(configmodel, iter))
		return FALSE;
	do {
		gtk_tree_model_get(configmodel, iter, 0, &str, -1);
		if (strcmp(name, str) == 0) {
			g_free(str);
			return TRUE;
		}
		g_free(str);
	} while (gtk_tree_model_iter_next(configmodel, iter));
	return FALSE;
}

static gboolean findchannelitem(const gchar *name, GtkTreeIter *itercfg, GtkTreeIter *iter)
{
	char *str;

	if (!gtk_tree_model_iter_children(configmodel, iter, itercfg))
		return FALSE;
	do {
		gtk_tree_model_get(configmodel, iter, 0, &str, -1);
		if (strcmp(name, str) == 0) {
			g_free(str);
			return TRUE;
		}
		g_free(str);
	} while (gtk_tree_model_iter_next(configmodel, iter));
	return FALSE;
}

void new_configuration(const gchar *name)
{
	GtkTreeIter iter;

	if (findconfigitem(name, &iter))
		return;
	gtk_tree_store_append(GTK_TREE_STORE(configmodel), &iter, NULL);
	gtk_tree_store_set(GTK_TREE_STORE(configmodel), &iter, 
			    LABEL_COL, name, 
			    CFGNAME_COL, name, -1);
}

void new_channel(const gchar *cfgname, const gchar *name)
{
	GtkTreeIter iter, child_iter;
	GtkTreeView *view;
	GtkTreePath *path;

	if (!findconfigitem(cfgname, &iter)) {
		g_printerr("Could not find configuration \"%s\"\n", cfgname);
		return;
	}
	gtk_tree_store_append(GTK_TREE_STORE(configmodel), &child_iter, &iter);
	gtk_tree_store_set(GTK_TREE_STORE(configmodel), &child_iter, 
			   LABEL_COL, name, 
			   CFGNAME_COL, cfgname, 
			   CHNAME_COL, name, -1);
	path = gtk_tree_model_get_path(configmodel, &iter);
	view = g_object_get_data(G_OBJECT(mainwindow), "configtree");
	gtk_tree_view_expand_row(view, path, FALSE);
	gtk_tree_path_free(path);
}

static void renumber_onecfg(GtkTreeIter *cfgiter)
{
	GtkTreeIter iter;
	unsigned int cnt = 0;
	gchar buf[64];

	if (!gtk_tree_model_iter_children(configmodel, &iter, cfgiter))
		return;
	do {
		sprintf(buf, "Channel %u", cnt++);
		gtk_tree_store_set(GTK_TREE_STORE (configmodel), &iter, 
				   LABEL_COL, buf, -1);
	} while (gtk_tree_model_iter_next(configmodel, &iter));
	
}

void renumber_channels(void)
{
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter_first(configmodel, &iter))
		return;
	do {
		renumber_onecfg(&iter);
	} while (gtk_tree_model_iter_next(configmodel, &iter));
}

/*    gtk_container_remove */
/* ---------------------------------------------------------------------- */

static void on_errorok_clicked(GtkButton *button, gpointer user_data)
{
	gtk_widget_hide(GTK_WIDGET(user_data));
	gtk_widget_destroy(GTK_WIDGET(user_data));
}

void error_dialog(const gchar *text)
{
	GtkWidget *dlg = create_errordialog();

	g_signal_connect(G_OBJECT(g_object_get_data(G_OBJECT(dlg), "errorok")), 
			 "clicked", G_CALLBACK(on_errorok_clicked), dlg);
	gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(dlg), "errorlabel")), text);
	gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(mainwindow));
	gtk_widget_show(dlg);
}

/* ---------------------------------------------------------------------- */

static void on_newconfigok_clicked(GtkButton *button, gpointer user_data)
{
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(g_object_get_data(G_OBJECT(user_data), "newconfigentry")));
	int ret;
	char buf[128];
	
	gtk_widget_hide(GTK_WIDGET(user_data));
	if (text[0]) {
		ret = xml_newconfig(text);
		if (ret) {
			snprintf(buf, sizeof(buf), "Duplicate name: \"%s\"\n", text);
			error_dialog(buf);
		} else {
			new_configuration(text);
		}
	}
	gtk_widget_destroy(GTK_WIDGET(user_data));
}

static void on_newconfigcancel_clicked(GtkButton *button, gpointer user_data)
{
	gtk_widget_hide(GTK_WIDGET(user_data));
	gtk_widget_destroy(GTK_WIDGET(user_data));
}

void on_newconfiguration_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *dlg = create_newconfigwindow();

	g_signal_connect(G_OBJECT(g_object_get_data(G_OBJECT(dlg), "newconfigok")), 
			 "clicked", G_CALLBACK(on_newconfigok_clicked), dlg);
	g_signal_connect(G_OBJECT(g_object_get_data(G_OBJECT(dlg), "newconfigcancel")), 
			 "clicked", G_CALLBACK(on_newconfigcancel_clicked), dlg);
	gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(mainwindow));
	gtk_widget_show(dlg);
}

/* ---------------------------------------------------------------------- */

void on_newchannel_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	const char *cfgname = NULL, *chname = NULL;

	cfgname = g_object_get_data(G_OBJECT(configmodel), "cfgname");
	if (!cfgname) {
		g_printerr("on_newchannel_activate: cfgname NULL\n");
		return;
	}
	chname = xml_newchannel(cfgname);
	if (!chname) {
		g_printerr("on_newchannel_activate: cannot create new channel\n");
		return;
	}
	new_channel(cfgname, chname);
	renumber_channels();
}


void on_deleteconfiguration_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	const char *cfgname = NULL;
	GtkTreeIter iter;

	cfgname = g_object_get_data(G_OBJECT(configmodel), "cfgname");
	if (!cfgname)
		return;
	if (findconfigitem(cfgname, &iter)) {
		g_print("delete cfg %s\n", cfgname);
		if (xml_deleteconfig(cfgname) == 0) {
			g_object_set_data(G_OBJECT(configmodel), "cfgname", NULL);
			free((char *)cfgname);
			gtk_tree_store_remove(GTK_TREE_STORE(configmodel), &iter);
		}
	}

}

void on_deletechannel_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	const char *cfgname = NULL, *chname = NULL;
	GtkTreeIter cfg_iter, iter;

	cfgname = g_object_get_data(G_OBJECT(configmodel), "cfgname");
	chname = g_object_get_data(G_OBJECT(configmodel), "chname");
	if (!cfgname || !chname)
		return;
	if (findconfigitem(cfgname, &cfg_iter) && findchannelitem(chname, &cfg_iter, &iter)) {
		if (xml_deletechannel(cfgname, chname) == 0) {
			g_object_set_data(G_OBJECT(configmodel), "chname", NULL);
			free((char *)chname);
			renumber_channels();
			gtk_tree_store_remove(GTK_TREE_STORE(configmodel), &iter);
		}
	}
}
