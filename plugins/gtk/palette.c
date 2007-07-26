/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/*
 *  port to ekg2:
 *  Copyright (C) 2007 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *			
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define GTK_DISABLE_DEPRECATED

/* fix includes */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "main.h"
#include "palette.h"

GdkColor colors[] = {
	/* colors for xtext */
	{0, 0x0000, 0x0000, 0x0000},	/* 17 black [30 V] */
	{0, 0xc3c3, 0x3b3b, 0x3b3b},	/* 20 red [31 V] */
	{0, 0x2a3d, 0x8ccc, 0x2a3d},	/* 19 green [32 V] */
	{0, 0xd999, 0xa6d3, 0x4147},	/* 24 yellow [33 V] */
	{0, 0x35c2, 0x35c2, 0xb332},	/* 18 blue [34 V] */
	{0, 0x8000, 0x2666, 0x7fff},	/* 22 purple [35] V */
	{0, 0x199a, 0x5555, 0x5555},	/* 26 aqua [36] V */
	{0, 0xcccc, 0xcccc, 0xcccc},	/* 16 white [37 V] */

	{0, 0x4c4c, 0x4c4c, 0x4c4c},	/* 30 grey [30b V] */
	{0, 0xc7c7, 0x3232, 0x3232},	/* 21 light red [31b V] */
	{0, 0x3d70, 0xcccc, 0x3d70},	/* 25 green [32b V] */
	{0, 0x6666, 0x3636, 0x1f1f},	/* 23 orange [33b V] */
	{0, 0x451e, 0x451e, 0xe666},	/* 28 blue [34b V] */
	{0, 0xb0b0, 0x3737, 0xb0b0},	/* 29 light purple */
	{0, 0x2eef, 0x8ccc, 0x74df},	/* 27 light aqua */
	{0, 0x9595, 0x9595, 0x9595},	/* 31 light grey */

	{0, 0xcccc, 0xcccc, 0xcccc},	/* 16 white */
	{0, 0x0000, 0x0000, 0x0000},	/* 17 black */
	{0, 0x35c2, 0x35c2, 0xb332},	/* 18 blue */
	{0, 0x2a3d, 0x8ccc, 0x2a3d},	/* 19 green */
	{0, 0xc3c3, 0x3b3b, 0x3b3b},	/* 20 red */
	{0, 0xc7c7, 0x3232, 0x3232},	/* 21 light red */
	{0, 0x8000, 0x2666, 0x7fff},	/* 22 purple */
	{0, 0x6666, 0x3636, 0x1f1f},	/* 23 orange */
	{0, 0xd999, 0xa6d3, 0x4147},	/* 24 yellow */
	{0, 0x3d70, 0xcccc, 0x3d70},	/* 25 green */
	{0, 0x199a, 0x5555, 0x5555},	/* 26 aqua */
	{0, 0x2eef, 0x8ccc, 0x74df},	/* 27 light aqua */
	{0, 0x451e, 0x451e, 0xe666},	/* 28 blue */
	{0, 0xb0b0, 0x3737, 0xb0b0},	/* 29 light purple */
	{0, 0x4c4c, 0x4c4c, 0x4c4c},	/* 30 grey */
	{0, 0x9595, 0x9595, 0x9595},	/* 31 light grey */

	{0, 0xffff, 0xffff, 0xffff},	/* 32 marktext Fore (white) */
	{0, 0x3535, 0x6e6e, 0xc1c1},	/* 33 marktext Back (blue) */
	{0, 0x0000, 0x0000, 0x0000},	/* 34 foreground (black) */
	{0, 0xf0f0, 0xf0f0, 0xf0f0},	/* 35 background (white) */
	{0, 0xcccc, 0x1010, 0x1010},	/* 36 marker line (red) */

	/* colors for GUI */
	{0, 0x9999, 0x0000, 0x0000},	/* 37 tab New Data (dark red) */
	{0, 0x0000, 0x0000, 0xffff},	/* 38 tab Nick Mentioned (blue) */
	{0, 0xffff, 0x0000, 0x0000},	/* 39 tab New Message (red) */
	{0, 0x9595, 0x9595, 0x9595},	/* 40 away user (grey) */
};

#define MAX_COL 40


void palette_alloc(GtkWidget *widget)
{
	int i;
	static int done_alloc = FALSE;
	GdkColormap *cmap;

	if (!done_alloc) {	/* don't do it again */
		done_alloc = TRUE;
		cmap = gtk_widget_get_colormap(widget);
		for (i = MAX_COL; i >= 0; i--)
			gdk_colormap_alloc_color(cmap, &colors[i], FALSE, TRUE);
	}
}