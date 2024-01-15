/*
 * mmapshm.c - create shm and mmap it
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
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#define DEBUG
#include "common/conventions.h"

#include "mmapshm.h"

void clear_mmapshm(struct mmapshm *m) {
static struct mmapshm blank={.fd=-1,.ptr=MAP_FAILED};
*m=blank;
}

void deinit_mmapshm(struct mmapshm *m) {
if (m->ptr!=MAP_FAILED) {
	munmap(m->ptr,m->allocsize);
}
ifclose(m->fd);
}

int init_mmapshm(struct mmapshm *m, unsigned int requestsize) {
unsigned char *ptr=MAP_FAILED;
unsigned int size;
int pagesize;
int fd=-1;
char *name="/mmapshm.tmp";

pagesize=sysconf(_SC_PAGESIZE);
size=requestsize;
{
	unsigned int slop;
	slop=size%pagesize;
	if (slop) {
		size+=pagesize-slop;
	}
}

fd=shm_open(name,O_RDWR|O_CREAT|O_EXCL,0600);
if (fd<0) GOTOERROR;
if (shm_unlink(name)) GOTOERROR;
if (0>ftruncate(fd,size)) GOTOERROR;

ptr=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
if (ptr==MAP_FAILED) GOTOERROR;

m->ptr=ptr;
m->allocsize=size;
m->requestsize=requestsize;

m->fd=fd;
return 0;
error:
	ifclose(fd);
	return -1;
}
