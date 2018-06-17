/*
 * This file is derived from GLIB by Thomas Sailer, <sailer@ife.ee.ethz.ch>
 *
 *
 * GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static unsigned int printf_string_upper_bound (const char *format, va_list args)
{
	unsigned int len = 1;
  
	while (*format) {
		int long_int = 0;
		int extra_long = 0;
		char c;
      
		c = *format++;
      
		if (c == '%') {
			int done = 0;
	  
			while (*format && !done) {
				switch (*format++) {
					char *string_arg;
		  
				case '*':
					len += va_arg (args, int);
					break;
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					/* add specified format length, since it might exceed the
					 * size we assume it to have.
					 */
					format -= 1;
					len += strtol (format, (char**) &format, 10);
					break;
				case 'h':
					/* ignore short int flag, since all args have at least the
					 * same size as an int
					 */
					break;
				case 'l':
					if (long_int)
						extra_long = 1; /* linux specific */
					else
						long_int = 1;
					break;
				case 'q':
				case 'L':
					long_int = 1;
					extra_long = 1;
					break;
				case 's':
					string_arg = va_arg (args, char *);
					if (string_arg)
						len += strlen (string_arg);
					else {
						/* add enough padding to hold "(null)" identifier */
						len += 16;
					}
					done = 1;
					break;
				case 'd':
				case 'i':
				case 'o':
				case 'u':
				case 'x':
				case 'X':
					if (long_int)
						(void) va_arg (args, long);
					else
						(void) va_arg (args, int);
					len += extra_long ? 64 : 32;
					done = 1;
					break;
				case 'D':
				case 'O':
				case 'U':
					(void) va_arg (args, long);
					len += 32;
					done = 1;
					break;
				case 'e':
				case 'E':
				case 'f':
				case 'g':
					(void) va_arg (args, double);
					len += extra_long ? 64 : 32;
					done = 1;
					break;
				case 'c':
					(void) va_arg (args, int);
					len += 1;
					done = 1;
					break;
				case 'p':
				case 'n':
					(void) va_arg (args, void*);
					len += 32;
					done = 1;
					break;
				case '%':
					len += 1;
					done = 1;
					break;
				default:
					/* ignore unknow/invalid flags */
					break;
				}
			}
		}
		else
			len += 1;
	}
	return len;
}

/* WARNING: This does only work for i386 (plus a few others, but not eg. alpha) !!!! */
#define VA_COPY(x,y) (x) = (y)

static char *strdup_vprintf (const char *format, va_list args1)
{
	char *buffer;
	va_list args2;

	VA_COPY (args2, args1);
	buffer = malloc(printf_string_upper_bound (format, args1));
	if (buffer)
		vsprintf (buffer, format, args2);
	va_end (args2);
	return buffer;
}

int snprintf (char *str, size_t n, char const *fmt, ...)
{
	char *printed;
	va_list args;
	int len;

	va_start(args, fmt);
	printed = strdup_vprintf (fmt, args);
	va_end (args);
	if (!printed)
		return -1;
	len = strlen(printed);
	strncpy(str, printed, n);
	str[n-1] = '\0';
	free(printed);
	return len >= n ? -1 : len;
}

int vsnprintf (char *str, size_t n, char const *fmt, va_list args)
{
	char *printed;
	int len;
  
	printed = strdup_vprintf (fmt, args);
	if (!printed)
		return -1;
	len = strlen(printed);
	strncpy(str, printed, n);
	str[n-1] = '\0';
	free(printed);
	return len >= n ? -1 : len;
}
