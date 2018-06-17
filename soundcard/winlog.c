/*****************************************************************************/

/*
 *      winlog.c  --  Win32 Logging functions.
 *
 *      Copyright (C) 1999-2000  Thomas Sailer (sailer@ife.ee.ethz.ch)
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
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 */

/*****************************************************************************/

#define _GNU_SOURCE
#define _REENTRANT

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "soundio.h"

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <errno.h>

/* ---------------------------------------------------------------------- */

unsigned int log_verblevel = 0;
static HANDLE logh = NULL;

/* ---------------------------------------------------------------------- */

void logrelease(void)
{
	if (logh) {
                DeregisterEventSource(logh);
                logh = NULL;
        }
}

void loginit(unsigned int vl, unsigned int tosysl)
{
        logrelease();
	log_verblevel = vl;
	if (tosysl) {
                logh = RegisterEventSource(NULL, /* uses local computer */ "soundmodem");
                if (!logh)
                        OutputDebugString("Cannot open event log\n");
	}
}

void logvprintf(unsigned int level, const char *fmt, va_list args)
{
	static const WORD vltosev[] = { EVENTLOG_ERROR_TYPE, EVENTLOG_ERROR_TYPE,
                                        EVENTLOG_WARNING_TYPE, EVENTLOG_INFORMATION_TYPE };
        
        if (level <= log_verblevel) {
                char tmp[512];
                vsnprintf(tmp, sizeof(tmp), fmt, args);
                if (logh) {
                        if (!ReportEvent(logh,
                                         (level >= 4) ? EVENTLOG_INFORMATION_TYPE : vltosev[level],  /* event type */
                                         level,                /* category */
                                         /*MSG_ERR_EXIST*/0,        /* event identifier */  /* FIX ME */
                                         NULL,                 /* no user security identifier */
                                         1,                    /* one substitution string */
                                         0,                    /* no data */
                                         &tmp,                 /* pointer to string array */
                                         NULL)) {              /* pointer to data */
                                OutputDebugString("ReportEvent failed\n");
                                exit(1);
                        }
                } else {
                        OutputDebugString(tmp);        
                }
	}
        if (!level)
                exit(1);
}

void logprintf(unsigned int level, const char *fmt, ...)
{
        va_list args;

	va_start(args, fmt);
	logvprintf(level, fmt, args);
        va_end(args);
}

void logerr(unsigned int level, const char *st)
{
        logprintf(level, "%s: %s (%d)\n", st, strerror(errno), errno);
}
