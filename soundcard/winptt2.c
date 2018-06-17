/*****************************************************************************/

/*
 *      winptt2.c  --  Windows32 PTT signalling.
 *
 *      Copyright (C) 1999-2001
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
#include <stdlib.h>
#include <windows.h>
#include <winioctl.h>

/* ---------------------------------------------------------------------- */

struct modemparams pttparams[] = {
	{ "file", "PTT Driver", "Device name of the serial port for outputting PTT", "COM1", MODEMPAR_COMBO, 
	  { c: { { "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8" } } } },
	{ NULL }
};

/* ---------------------------------------------------------------------- */

int pttinit(struct pttio *state, const char *params[])
{
	const char *path = params[0];
	HANDLE h;
        DCB dcb;
        
	if (!path || !path[0])
		return 0;
        state->h = INVALID_HANDLE_VALUE;
        h = CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		logprintf(MLOG_ERROR, "Cannot open PTT device \"%s\"\n", path);
		return -1;
	}
        /* Check if it is a comm device */
        if (!GetCommState(h, &dcb)) {
                logprintf(MLOG_ERROR, "Device \"%s\" not a COM device\n", path);
                CloseHandle(h);
                return -1;
        }
        state->h = h;
	pttsetptt(state, 0);
	pttsetdcd(state, 0);
        return 0;
}

void pttsetptt(struct pttio *state, int pttx)
{
	if (!state || state->h == INVALID_HANDLE_VALUE)
		return;
	state->ptt = !!pttx;
        if (!EscapeCommFunction(state->h, state->ptt ? SETRTS : CLRRTS))
                logprintf(MLOG_ERROR, "EscapeCommFunction (ptt) Error 0x%lx\n", GetLastError());
}

void pttsetdcd(struct pttio *state, int dcd)
{
	if (!state || state->h == INVALID_HANDLE_VALUE)
		return;
	state->dcd = !!dcd;
        if (!EscapeCommFunction(state->h, state->dcd ? SETDTR : CLRDTR))
                logprintf(MLOG_ERROR, "EscapeCommFunction (dcd) Error 0x%lx\n", GetLastError());
}

void pttrelease(struct pttio *state)
{
	if (!state || state->h == INVALID_HANDLE_VALUE)
		return;
	pttsetptt(state, 0);
	pttsetdcd(state, 0);
	CloseHandle(state->h);
	state->h = INVALID_HANDLE_VALUE;
}
