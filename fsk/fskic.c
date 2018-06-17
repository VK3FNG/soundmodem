/*****************************************************************************/

/*
 *      fskic.c  --  DF9IC FSK Modem filter curves.
 *
 *      Copyright (C) 2003
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
 *
 */

/*****************************************************************************/

#include "modem.h"
#include "raisedcosine.h"
#include "fskic.h"

/* --------------------------------------------------------------------- */

double df9ic_rxfilter(double t)
{
	unsigned int i;
	double sum = 0;

	t *= FSKIC_RXOVER;
	t += 0.5 * sizeof(fskic_rxpulse) / sizeof(fskic_rxpulse[0]);
	for (i = 0; i < sizeof(fskic_rxpulse) / sizeof(fskic_rxpulse[0]); i++)
		sum += fskic_rxpulse[i] * sinc(i - t);
	return sum;
}

double df9ic_txfilter(double t)
{
	unsigned int i;
	double sum = 0;

	t *= FSKIC_TXOVER;
	t += 0.5 * sizeof(fskic_txpulse) / sizeof(fskic_txpulse[0]);
	for (i = 0; i < sizeof(fskic_txpulse) / sizeof(fskic_txpulse[0]); i++)
		sum += fskic_txpulse[i] * sinc(i - t);
	return sum;
}
