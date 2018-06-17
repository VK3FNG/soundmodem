/*****************************************************************************/

/*
 *      ptt.c  --  PTT signalling.
 *
 *      Copyright (C) 1999-2000, 2002
 *        Thomas Sailer (t.sailer@alumni.ethz.ch)
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
 *
 * Support for CM108 GPIO control of PTT by Andrew Errington ZL3AME May 2011
 * CM108/Hidraw detection by Thomas Sailer
 *
 */

/*****************************************************************************/

#define _GNU_SOURCE
#define _REENTRANT

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "modem.h"
#include "pttio.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

#include <stdio.h> 


#ifdef HAVE_SYS_IOCCOM_H
#include <sys/ioccom.h>
#endif

#ifdef HAVE_LINUX_PPDEV_H
#include <linux/ppdev.h>
#else
#include "ppdev.h"
#endif

#if defined HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#ifdef HAVE_LINUX_HIDRAW_H
#include <linux/hidraw.h>
#endif

/* ---------------------------------------------------------------------- */
struct modemparams pttparams[] = {
	{ "file", "PTT Driver", "Path name of the serial, parallel or USB HID port for outputting PTT", "none", MODEMPAR_COMBO, 
	  { c: { { "none", "/dev/ttyS0", "/dev/ttyS1", "/dev/parport0", "/dev/parport1","/dev/hidraw0","/dev/hidraw1" } } } },
#ifdef HAVE_LINUX_HIDRAW_H
	{ "gpio", "GPIO", "GPIO bit number on CM108 or compatible USB sound card", "0", MODEMPAR_COMBO,
	{ c: {{ "0","1","2","3","4","5","6","7"}}}},
#endif
#ifdef HAVE_LIBHAMLIB
	{ "hamlib_model", "Hamlib model", "Model number", "", MODEMPAR_STRING }, 
	{ "hamlib_params", "Rig configuration params", "Rig configuration params", "", MODEMPAR_STRING },
#endif

	{ NULL }
};

/* ---------------------------------------------------------------------- */

int pttinit(struct pttio *state, const char *params[])
{
	const char *path = params[0];
	int fd;
	unsigned char x;
	unsigned int y = 0;
#ifdef HAVE_LINUX_HIDRAW_H
	struct hidraw_devinfo hiddevinfo;
	const char *gpio_pin = params[1];
#define PAR_HAMLIBMODEL  2
#define PAR_HAMLIBPARAMS 3
#else
#define PAR_HAMLIBMODEL  1
#define PAR_HAMLIBPARAMS 2
#endif

	state->mode = noport;
	state->gpio = 0;
	if (!path || !path[0] || !strcasecmp(path, "none"))
		return 0;
#ifdef HAVE_LIBHAMLIB
	const char *hamlib_model = params[PAR_HAMLIBMODEL];
	if (hamlib_model && hamlib_model[0]) {
		int my_rig_error = ~RIG_OK;
		char *hamlib_params = params[PAR_HAMLIBPARAMS] ? strdup(params[3]) : NULL;
		int rig_modl ;
		char *ptr_key = hamlib_params;

		logprintf(MLOG_INFO, "Hamlib: pttinit: path=%s model=%s params=%s\n",
			  path ? path : "NULL",
			  hamlib_model ? hamlib_model : "NULL",
			  hamlib_params ? hamlib_params : "NULL");
		if (1 != sscanf(hamlib_model, "%d", &rig_modl)) {
			logprintf(MLOG_ERROR, "Hamlib: Invalid model:\"%s\"\n", hamlib_model);
			goto the_end;
		}
		state->mode = hamlibport;
		state->u.rig_ptr = rig_init(rig_modl);
		if(state->u.rig_ptr == NULL) {
			logprintf(MLOG_ERROR, "Hamlib: rig_init model=%dn", rig_modl);
			goto the_end;
		}
		strncpy(state->u.rig_ptr->state.rigport.pathname, path, FILPATHLEN);

		logprintf(MLOG_INFO, "Hamlib: pttinit parsing %s\n", ptr_key ? ptr_key : "NULL" );

		while (ptr_key && *ptr_key != '\0') {
			char * ptr_val = strchr(ptr_key, '=');
			if (ptr_val) {
				*ptr_val++ = '\0';
			}

			char * ptr_key_next = ptr_val ? strchr(ptr_val, ',') : NULL ;
			if (ptr_key_next) {
				*ptr_key_next++ = '\0';
			}
			my_rig_error = rig_set_conf(state->u.rig_ptr,
						    rig_token_lookup(state->u.rig_ptr, ptr_key), ptr_val);
			if (my_rig_error != RIG_OK) {
				logprintf(MLOG_ERROR,
					  "Hamlib: rig_set_conf: %s=%s : %s\n",
					  ptr_key ? ptr_key : NULL,
					  ptr_val ? ptr_val : NULL,
					  rigerror(my_rig_error));
				goto the_end;
			}
			ptr_key = ptr_key_next;
		}
		my_rig_error = rig_open(state->u.rig_ptr);
		if (RIG_OK != my_rig_error) {
			logprintf(MLOG_ERROR, "Hamlib: rig_open: %s\n", rigerror(my_rig_error));
			goto the_end;
		}
		pttsetptt(state, 0);
		pttsetdcd(state, 0);
		my_rig_error = RIG_OK;
	the_end:
		free(hamlib_params);
        	return my_rig_error == RIG_OK ? 0 : -1 ;
	}
#endif

	logprintf(MLOG_INFO, "Opening PTT device \"%s\"\n", path);
	if ((fd = open(path, O_RDWR, 0)) < 0) {
		logprintf(MLOG_ERROR, "Cannot open PTT device \"%s\"\n", path);
		return -1;
	}
	if (!ioctl(fd, TIOCMBIC, &y)) {
		state->u.fd = fd;
		state->mode = serport;
	} else if (!ioctl(fd, PPCLAIM, 0) && !ioctl(fd, PPRDATA, &x)) {
		state->u.fd = fd;
		state->mode = parport;
#ifdef HAVE_LINUX_HIDRAW_H
	} else if (!ioctl(fd, HIDIOCGRAWINFO, &hiddevinfo) && hiddevinfo.vendor == 0x0d8c
		   && hiddevinfo.product >= 0x0008 && hiddevinfo.product <= 0x000f) {
		state->u.fd = fd;
		state->mode = cm108;
		state->gpio = strtoul(gpio_pin, NULL, 0); 
		logprintf(MLOG_INFO, "pttinit gpio bit number %d\n", state->gpio);
#endif
	} else {
		logprintf(MLOG_ERROR, "Device \"%s\" neither parport nor serport nor CM108\n", path);
		close(fd);
		return -1;
	}
	pttsetptt(state, 0);
	pttsetdcd(state, 0);
        return 0;
}

void pttsetptt(struct pttio *state, int pttx)
{
	unsigned char reg;

	if (!state)
		return;
	state->ptt = !!pttx;
	switch (state->mode) {
#ifdef HAVE_LIBHAMLIB
	case hamlibport:
	{
		if (!state->u.rig_ptr)
			return;
		logprintf(MLOG_INFO, "Hamlib: pttsetptt state=%d\n", state->ptt);
		ptt_t my_ptt ;
		int my_rig_error;
		if (state->ptt)
		    my_ptt = RIG_PTT_ON;
		else
		    my_ptt = RIG_PTT_OFF;
		my_rig_error = rig_set_ptt(state->u.rig_ptr, RIG_VFO_CURR, my_ptt);
		if(RIG_OK != my_rig_error) {
			logprintf(MLOG_ERROR, "Hamlib: rig_set_ptt %s\n", rigerror(my_rig_error) );
		}
		return;
	}	
#endif

	case serport:
	{
		if (state->u.fd == -1)
			return;
#if 0
		unsigned int y = TIOCM_RTS;
		ioctl(state->u.fd, state->ptt ? TIOCMBIS : TIOCMBIC, &y);
#else
		unsigned int y;
		ioctl(state->u.fd, TIOCMGET, &y);
		if (state->ptt)
		    y |= TIOCM_RTS;
		else
		    y &= ~TIOCM_RTS;
		ioctl(state->u.fd, TIOCMSET, &y);
#endif
		return;
	}

	case parport:
	{
		if (state->u.fd == -1)
			return;
		reg = state->ptt | (state->dcd << 1);
		ioctl(state->u.fd, PPWDATA, &reg);
		return;
	}

#ifdef HAVE_LINUX_HIDRAW_H
	case cm108:
	{
		// Build two packets for CM108 HID.  One turns a GPIO bit on.  The other turns it off.
		// Packet is 4 bytes, preceded by a 'report number' byte
		// 0x00 report number
		// Write data packet (from CM108 documentation)
		// byte 0: 00xx xxxx     Write GPIO
		// byte 1: xxxx dcba     GPIO3-0 output values (1=high)
		// byte 2: xxxx dcba     GPIO3-0 data-direction register (1=output)
		// byte 3: xxxx xxxx     SPDIF

		char out_rep[] = { 
			0x00, // report number
			// HID output report
			0x00,
			state->ptt ? (1 << (state->gpio)) : 0, // set GPIO
			1 << (state->gpio), // Data direction register (1=output)
			0x00
		};
		ssize_t nw;

		if (state->u.fd == -1)
			return;
		logprintf(MLOG_INFO, "pttsetptt gpio=%d\n", state->gpio);
		/* Can't do anything with DCD */
		logprintf(MLOG_INFO, "sm_CM108: pttsetptt state=%d\n", state->ptt);
		nw = write(state->u.fd, out_rep, sizeof(out_rep));
		if (nw < 0) {
			logprintf(MLOG_ERROR, "sm_CM108: write error\n");
		}
		return;
	}
#endif

	default:
		return;
	}
}

void pttsetdcd(struct pttio *state, int dcd)
{
	unsigned char reg;

	if (!state)
		return;
	state->dcd = !!dcd;
	switch (state->mode) {
#ifdef HAVE_LIBHAMLIB
	/* For Hamlib, it does not make sense to set DCD. */
	case hamlibport:
		return;
#endif

	case serport:
	{
		if (state->u.fd == -1)
			return;
#if 0
		unsigned int y = TIOCM_DTR;
		ioctl(state->u.fd, state->dcd ? TIOCMBIS : TIOCMBIC, &y);
#else
		unsigned int y;
		ioctl(state->u.fd, TIOCMGET, &y);
		if (state->dcd)
		    y |= TIOCM_DTR;
		else
		    y &= ~TIOCM_DTR;
		ioctl(state->u.fd, TIOCMSET, &y);
#endif
		return;
	}

	case parport:
	{
		if (state->u.fd == -1)
			return;
		reg = state->ptt | (state->dcd << 1);
		ioctl(state->u.fd, PPWDATA, &reg);
		return;
	}

#ifdef HAVE_LINUX_HIDRAW_H
	case cm108:
	{
		/* For CM108 it does not make sense to set DCD */
		return;
	}
#endif

	default:
		return;
	}
}

void pttrelease(struct pttio *state)
{
	if (!state)
		return;
	pttsetptt(state, 0);
	pttsetdcd(state, 0);
	switch (state->mode) {
#ifdef HAVE_LIBHAMLIB
	case hamlibport:
	{
		logprintf(MLOG_INFO, "Hamlib: pttrelease\n");
		int my_rig_error;
		my_rig_error = rig_close(state->u.rig_ptr);
		if(RIG_OK != my_rig_error) {
			logprintf(MLOG_ERROR, "Hamlib: rig_close: %s\n", rigerror(my_rig_error) );
		}
		my_rig_error = rig_cleanup(state->u.rig_ptr);
		if(RIG_OK != my_rig_error) {
			logprintf(MLOG_ERROR, "Hamlib: rig_cleanup: %s\n", rigerror(my_rig_error) );
		}
		break;
	}
#endif

	case serport:
	case parport:
	case cm108:
		close(state->u.fd);
		break;

	default:
		break;
	}
	state->mode = noport;
}
