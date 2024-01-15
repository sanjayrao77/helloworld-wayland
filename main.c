/*
 * main.c - example for using wayland-client
 * Copyright (C) 2024 Sanjay Rao
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#define DEBUG
#include "common/conventions.h"
#include "mmapshm.h"
#include "font.h"
#include "display_wayland.h"

int main(int argc, char **argv) {
struct display display;
unsigned int wwidth=640,wheight=480;
int timeout=1;
char *message="Hello world!";
unsigned int xcursor=100;

clear_display(&display);

if (init_display(&display,"pictureframe",wwidth,wheight,NULL)) GOTOERROR;

while (1) {
	unsigned int keycode;
	int r;
	r=waitforkey_display(&keycode,&display,timeout);
	if (0>r) GOTOERROR;
	if (r) {
		fprintf(stderr,"Keycode: %u\n",keycode);
		break;
	}
	if (message) {
		unsigned int cwidth;
		(ignore)drawchar_display(&cwidth,&display,&mono_global,xcursor,240,*message);
		xcursor+=cwidth+1;
		message++;
		if (!*message) { message=NULL; timeout=10; }
	}
}
fputs("Quiting...\n",stderr);

deinit_display(&display);
return 0;
error:
	deinit_display(&display);
	return -1;
}
