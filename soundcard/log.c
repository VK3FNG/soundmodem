/*****************************************************************************/

/*
 *      log.c  --  Logging functions.
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

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------- */

static unsigned int tosyslog = 0;
unsigned int log_verblevel = 0;

/* ---------------------------------------------------------------------- */

void logrelease(void)
{
#ifdef HAVE_CLOSELOG
	if (tosyslog)
		closelog();
#endif
	tosyslog = 0;
}

void loginit(unsigned int vl, unsigned int tosysl)
{
	log_verblevel = vl;
	tosyslog = 0;
#ifdef HAVE_OPENLOG
	if (tosysl) {
		openlog("soundmodem", LOG_PID, LOG_DAEMON);
		tosyslog = 1;
	}
#endif
}

void logvprintf(unsigned int level, const char *fmt, va_list args)
{
	static const int vltosev[] = { LOG_CRIT, LOG_ERR, LOG_WARNING, LOG_NOTICE };

	if (level <= log_verblevel) {
#ifdef HAVE_SYSLOG
		if (tosyslog) {
			char tmp[512];
			vsnprintf(tmp, sizeof(tmp), fmt, args);
			syslog((level >= 256) ? LOG_DEBUG : (level >= 4) ? LOG_INFO : vltosev[level], tmp);
		} else
#endif
		{
			fprintf(stderr, "sm[%lu]: ", (unsigned long)getpid());
			vfprintf(stderr, fmt, args);
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

