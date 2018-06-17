/*****************************************************************************/

/*
 *      openpty.c  -- openpty.
 *
 *      From glibc2, Copyright (C) Free Software Foundation
 *      Modified by Thomas Sailer, sailer@ife.ee.ethz.ch
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

#ifndef HAVE_OPENPTY

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <grp.h>
#include <errno.h>

int openpty(int *amaster, int *aslave, char *name, struct termios *termp, struct winsize *winp)
{
        char line[11];
        const char *cp1, *cp2;
        int master, slave, ttygid;
        struct group *gr;

        strcpy (line, "/dev/ptyXX");
	if (gr = getgrnam("tty"))
                ttygid = gr->gr_gid;
        else
                ttygid = -1;
        for (cp1 = "pqrs"; *cp1; cp1++) {
                line[8] = *cp1;
                for (cp2 = "0123456789abcdef"; *cp2; cp2++) {
                        line[9] = *cp2;
                        if ((master = open(line, O_RDWR, 0)) == -1) {
                                if (errno == ENOENT)
                                        return (-1);    /* out of ptys */
                        } else {
                                line[5] = 't';
                                (void)chown(line, getuid(), ttygid);
                                (void)chmod(line, S_IRUSR|S_IWUSR|S_IWGRP);
#ifdef HAVE_REVOKE
                                revoke(line);
#endif
                                if ((slave = open(line, O_RDWR, 0)) != -1) {
                                        *amaster = master;
                                        *aslave = slave;
                                        if (name)
                                                strcpy(name, line);
                                        if (termp)
                                                (void)tcsetattr(slave, TCSAFLUSH, termp);
                                        if (winp)
                                                (void)ioctl(slave, TIOCSWINSZ, (char *)winp);
                                        return 0;
                                }
                                (void)close(master);
                                line[5] = 'p';
                        }
                }
        }
        errno = ENOENT;   /* out of ptys */
        return -1;
}

#endif /* HAVE_OPENPTY */
