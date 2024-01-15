/*
 * display_wayland.c - wrapper for using wayland-client
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
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <poll.h>
#include <time.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#define DEBUG
#include "common/conventions.h"
#include "mmapshm.h"
#include "font.h"

#include "display_wayland.h"

void deinit_display(struct display *s) {
	if (s->wl_keyboard) (void)wl_keyboard_destroy(s->wl_keyboard);
	if (s->wl_seat) (void)wl_seat_destroy(s->wl_seat);
	if (s->cb) (void)wl_callback_destroy(s->cb);
	if (s->xdg_wm_base) (void)xdg_wm_base_destroy(s->xdg_wm_base);
	if (s->xdg_toplevel) (void)xdg_toplevel_destroy(s->xdg_toplevel);
	if (s->xdg_surface) (void)xdg_surface_destroy(s->xdg_surface);
	if (s->wl_surface) (void)wl_surface_destroy(s->wl_surface);
	{
		struct buffer_display *bd,*next;
		for (bd=s->buffers.first_inuse;bd;bd=next) {
			next=bd->next;
			(void)wl_buffer_destroy(bd->wl_buffer);
			free(bd);
		}
		for (bd=s->buffers.first_unused;bd;bd=next) {
			next=bd->next;
			(void)wl_buffer_destroy(bd->wl_buffer);
			free(bd);
		}
	}
	if (s->wl_shm_pool) (void)wl_shm_pool_destroy(s->wl_shm_pool);
	if (s->wl_shm) (void)wl_shm_destroy(s->wl_shm);
	if (s->wl_registry) (void)wl_registry_destroy(s->wl_registry);
	if (s->wl_compositor) (void)wl_compositor_destroy(s->wl_compositor);
	if (s->wl_display) (void)wl_display_disconnect(s->wl_display);
	deinit_mmapshm(&s->mmapshm);
}

static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
struct display *display=(struct display *)data;
struct buffer_display *bd,**plastbd;
plastbd=&display->buffers.first_inuse;
bd=display->buffers.first_inuse;
while (1) {
	if (!bd) break;
	if (bd->wl_buffer!=wl_buffer) {
		plastbd=&bd->next;
		bd=bd->next;
		continue;
	}
	*plastbd=bd->next;
	
	bd->next=display->buffers.first_unused;
	display->buffers.first_unused=bd;

	fputs("buffer released\n",stderr);
	return;
}

fputs("Unknown buffer released!\n",stderr);
}

static const struct wl_buffer_listener wl_buffer_listener = {
	.release=wl_buffer_release
};

static struct wl_buffer *grab_buffer(struct display *display) {
struct buffer_display *bd;
struct wl_buffer *wlb=NULL;

bd=display->buffers.first_unused;
if (bd) {
	display->buffers.first_unused=bd->next;
	bd->next=display->buffers.first_inuse;
	display->buffers.first_inuse=bd;
	return bd->wl_buffer;
}

bd=malloc(sizeof(struct buffer_display));
if (!bd) GOTOERROR;
wlb=wl_shm_pool_create_buffer(display->wl_shm_pool,0,display->width,display->height,display->stride,WL_SHM_FORMAT_XRGB8888);
if (!wlb) GOTOERROR;
if (wl_buffer_add_listener(wlb,&wl_buffer_listener,display)) GOTOERROR;
bd->wl_buffer=wlb;

bd->next=display->buffers.first_inuse;
display->buffers.first_inuse=bd;

return bd->wl_buffer;
error:
	if (bd) free(bd);
	if (wlb) wl_buffer_destroy(wlb);
	return NULL;
}

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
	(void)xdg_wm_base_pong(xdg_wm_base,serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping=xdg_wm_base_ping
};

static void wl_registry_global(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version) {
struct display *display=(struct display*)data;

if (!strcmp(interface,wl_compositor_interface.name)) {
	display->wl_compositor=wl_registry_bind(wl_registry,name,&wl_compositor_interface,5);
} else if (!strcmp(interface,wl_shm_interface.name)) {
	display->wl_shm=wl_registry_bind(wl_registry,name,&wl_shm_interface,1);
} else if (!strcmp(interface,wl_seat_interface.name)) {
	display->wl_seat=wl_registry_bind(wl_registry,name,&wl_seat_interface,1);
} else if (!strcmp(interface,xdg_wm_base_interface.name)) {
	display->xdg_wm_base=wl_registry_bind(wl_registry,name,&xdg_wm_base_interface,1);
}

// printf("interface: '%s', version: %d, name: %d\n",interface,version,name);
}

static void wl_registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {
// left blank
}

static const struct wl_registry_listener registry_listener = {
	.global=wl_registry_global,
	.global_remove=wl_registry_global_remove
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
struct display *display=(struct display *)data;
(void)xdg_surface_ack_configure(xdg_surface,serial);
display->isconfigured=1;
fputs("got surface configure\n",stderr);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure=xdg_surface_configure
};

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
struct display *display=(struct display*)data;
fputs("Got quit\n",stderr);
display->isquit=1;
}

static void noop() {
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure=noop,
	.close=xdg_toplevel_close
};

static const struct wl_callback_listener wl_surface_frame_listener;

static void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time) {
struct display *display=(struct display *)data;

(void)wl_callback_destroy(cb);
display->cb=NULL;

display->drawstatus.isready=1;
fputs("Frame is done\n",stderr);
}

static const struct wl_callback_listener wl_surface_frame_listener = {
	.done=wl_surface_frame_done
};

static void wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
struct display *display=(struct display *)data;
if (capabilities&WL_SEAT_CAPABILITY_KEYBOARD) {
	display->iskeyboard=1;
	fputs("Found keyboard\n",stderr);
} else {
	display->iskeyboard=0;
	if (display->wl_keyboard) {
		(void)wl_keyboard_destroy(display->wl_keyboard);
		display->wl_keyboard=NULL;
	}
}
}

static const struct wl_seat_listener wl_seat_listener = {
	.capabilities=wl_seat_capabilities
};

static void wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
struct display *display=(struct display *)data;
fprintf(stderr,"Key: serial:%u key:%u state:%u\n",serial,key,state);
if (state==1) {
	display->keycode=key;
}
}


static const struct wl_keyboard_listener wl_keyboard_listener = {
	.keymap=noop,
	.enter=noop,
	.leave=noop,
	.key=wl_keyboard_key,
	.modifiers=noop,
	.repeat_info=noop
};

void clear_display(struct display *display) {
static struct display blank;
*display=blank;
clear_mmapshm(&display->mmapshm);
}

int init_display(struct display *display, char *title, unsigned int width, unsigned int height, char *hint) {

display->width=width;
display->height=height;
display->stride=width*4;

display->wl_display=wl_display_connect(NULL);
if (!display->wl_display) GOTOERROR;
display->wl_fd=wl_display_get_fd(display->wl_display);
if (0>display->wl_fd) GOTOERROR;

if (init_mmapshm(&display->mmapshm,display->stride*display->height)) GOTOERROR;
memset(display->mmapshm.ptr,128,display->mmapshm.requestsize);

display->wl_registry=wl_display_get_registry(display->wl_display);
if (!display->wl_registry) GOTOERROR;

if (wl_registry_add_listener(display->wl_registry,&registry_listener,display)) GOTOERROR;
if (0>wl_display_roundtrip(display->wl_display)) GOTOERROR;

if (!display->wl_compositor) GOTOERROR;
if (!display->wl_shm) GOTOERROR;
if (!display->wl_seat) GOTOERROR;
if (!display->xdg_wm_base) GOTOERROR;

if (wl_seat_add_listener(display->wl_seat,&wl_seat_listener,display)) GOTOERROR;

if (0>wl_display_roundtrip(display->wl_display)) GOTOERROR;

if (display->iskeyboard) {
	fputs("looking for keyboard\n",stderr);
	display->wl_keyboard=wl_seat_get_keyboard(display->wl_seat);
	if (!display->wl_keyboard) GOTOERROR;
	if (wl_keyboard_add_listener(display->wl_keyboard,&wl_keyboard_listener,display)) GOTOERROR;
}

if (xdg_wm_base_add_listener(display->xdg_wm_base,&xdg_wm_base_listener,display)) GOTOERROR;

display->wl_shm_pool=wl_shm_create_pool(display->wl_shm,display->mmapshm.fd,display->mmapshm.requestsize);
if (!display->wl_shm_pool) GOTOERROR;

display->wl_surface=wl_compositor_create_surface(display->wl_compositor);
if (!display->wl_surface) GOTOERROR;

display->xdg_surface=xdg_wm_base_get_xdg_surface(display->xdg_wm_base,display->wl_surface);
if (!display->xdg_surface) GOTOERROR;

if (xdg_surface_add_listener(display->xdg_surface,&xdg_surface_listener,display)) GOTOERROR;

display->xdg_toplevel=xdg_surface_get_toplevel(display->xdg_surface);
(void)xdg_surface_set_window_geometry(display->xdg_surface,0,0,display->width,display->height);

if (xdg_toplevel_add_listener(display->xdg_toplevel,&xdg_toplevel_listener,display)) GOTOERROR;
if (title) (void)xdg_toplevel_set_title(display->xdg_toplevel,title);
(void)wl_surface_commit(display->wl_surface);

if (0>wl_display_roundtrip(display->wl_display)) GOTOERROR;

fputs("Waiting for configure\n",stderr);

while (1) {
	if (display->isconfigured) break;
	if (0>wl_display_dispatch(display->wl_display)) GOTOERROR;
}

display->cb=wl_surface_frame(display->wl_surface);
if (!display->cb) GOTOERROR;
if (wl_callback_add_listener(display->cb,&wl_surface_frame_listener,display)) GOTOERROR;
(void)wl_surface_damage_buffer(display->wl_surface,0,0,display->width,display->height);
{
	struct wl_buffer *wlb;
	wlb=grab_buffer(display);
	if (!wlb) GOTOERROR;
	(void)wl_surface_attach(display->wl_surface,wlb,0,0);
}
(void)wl_surface_commit(display->wl_surface);
// display->drawstatus.ispredrawn=0; display->drawstatus.isready=0; // redundant
if (0>wl_display_flush(display->wl_display)) GOTOERROR;

fputs("We should have a window now\n",stderr);

return 0;
error:
	return -1;
}

static int checkpredraw(struct display *display) {
if (display->drawstatus.ispredrawn && display->drawstatus.isready) {
	display->cb=wl_surface_frame(display->wl_surface);
	if (!display->cb) GOTOERROR;
	wl_callback_add_listener(display->cb,&wl_surface_frame_listener,display);
//		wl_surface_damage_buffer(display.wl_surface,0,0,UINT32_MAX,UINT32_MAX); // DON'T DO THIS!!
	(void)wl_surface_damage_buffer(display->wl_surface,0,0,display->width,display->height);
	{
		struct wl_buffer *wlb;
		wlb=grab_buffer(display);
		if (!wlb) GOTOERROR;
		(void)wl_surface_attach(display->wl_surface,wlb,0,0);
	}
	(void)wl_surface_commit(display->wl_surface);
	if (0>wl_display_flush(display->wl_display)) GOTOERROR;
	display->drawstatus.ispredrawn=0;
	display->drawstatus.isready=0;
}
return 0;
error:
	return -1;
}

int waitforkey_display(unsigned int *keycode_out, struct display *display, int seconds) {
struct pollfd pollfd;
time_t expires;

pollfd.fd=display->wl_fd;
pollfd.events=POLLIN;
expires=time(NULL)+seconds;

while (1) {
	time_t now;
	int r;

	now=time(NULL);
	if (expires <= now) break;

	if (display->isquit) {
		*keycode_out=0;
		return 1;
	}
	if (display->keycode) {
		*keycode_out=display->keycode;
		display->keycode=0;
		return 1;
	}
	if (checkpredraw(display)) GOTOERROR;
	r=poll(&pollfd,1,1000);
	if ((r==1)&&(pollfd.revents&POLLIN)) {
		if (0>wl_display_dispatch(display->wl_display)) GOTOERROR;
		continue;
	}
	if (r<0) GOTOERROR;
}

return 0;
error:
	return -1;
}

static inline void setpixel(struct display *display, unsigned int x, unsigned int y, unsigned char *bgra4) {
unsigned char *ptr;
if ((x>=display->width) || (y>=display->height)) {
	fprintf(stderr,"Clipped pixel: %u,%u\n",x,y);
	return;
}
ptr=display->mmapshm.ptr;
ptr+=y*display->stride;
ptr+=x*4;
memcpy(ptr,bgra4,4);
}

int drawchar_display(unsigned int *width_out, struct display *display, struct font *font,
		unsigned int x, unsigned int y, unsigned char uch) {
unsigned char bgra[4]={0xff,0xff,0xff,0xff};
unsigned int fw;
struct char_font *cf;
unsigned int cfw;
int lead;
unsigned int curx,curx_reset,cury;
unsigned int wleft,zleft;
unsigned char *zrle,*zrlelimit;

if (uch==' ') {
	*width_out=1+(font->width>>2);
	return 0;
}

cf=font->chars[uch];
if (!cf) cf=font->chars['.'];
if (!cf) {
	*width_out=0;
	return 0;
}

cfw=cf->width;
fw=font->width>>1;

lead=cf->left-fw;
if (lead<0) lead=0;

curx_reset=curx=x+lead;
cury=y+cf->top;

zrle=(unsigned char *)cf->zrle;

wleft=cfw;
zleft=0;
zrlelimit=zrle+cf->zrlelen;
while (1) {
	unsigned char z;
	if (zrle>=zrlelimit) {
		if (zrle!=zrlelimit) WHEREAMI;
		break;
	}
	if (zleft) {
		if (zleft>=wleft) {
			zleft-=wleft;
			wleft=cfw;
			curx=curx_reset;
			cury+=1;
			continue;
		}
		wleft-=zleft;
		curx+=zleft;
		zleft=0;
	}

	z=*zrle; zrle++;
	if (!z) {
		zleft=*zrle; zrle++;
		continue;
	}
	bgra[0]=z;
	bgra[1]=z;
	bgra[2]=z;
	setpixel(display,curx,cury,bgra);
	curx++;
	wleft--;
	if (!wleft) {
		curx=curx_reset;
		cury+=1;
		wleft=cfw;
	}
}

display->drawstatus.ispredrawn=1;
*width_out=lead+cf->width;
return 0;
}
