# helloworld-wayland

## Overview

This is a "hello world" example for Wayland display systems. It checks every(?) error
but is otherwise meant to be mostly minimal. I couldn't find such an example so I
made my own.

I've tested this on my Raspberry Pi 4 running Wayfire but it might work on other
compositors.

## Building

You'll want a unix system with a Wayland compositor installed. You'll also need wayland-client
libraries and wayland build utilities. On my Raspberry Pi OS Bookworm-based system, I installed
the "wayland-protocols" and "libwayland-dev" packages to build it.

With everything installed, you should be able to just clone this and run "make".

Example:
```
git clone https://www.github.com/sanjayrao77/helloworld-wayland
cd helloworld-wayland
sudo apt get install wayland-protocols libwayland-dev
make
```

## Quick start

After it's compiled, you can run "./helloworld" to run the example.

It should create a 640x480 window and then print "Hello world!" character by character.

To quit, you can press any key while the window is focused.

## Error checking

Callbacks are common in libwayland-client. These callbacks are void and all the examples I saw make calls that
could error. In those examples, any errors would be ignored and that can be terrible design.

Also, the examples I saw played fast and loose with frame updating. It's tricky and I believe I've handled it
properly in this example. When the compositor doesn't want new frames, we should save updates and send them
after the compositor becomes ready.

## Minimal notes

This is not completely minimal.

It would be more minimal to destroy and create the "struct wl\_buffer" on every
use. But, that seems wasteful to me so I added wl\_buffer
reuse. Reuse can be tricky to do correctly because the compositor might hold on
to old wl\_buffer locks, requiring more than just waiting for an unlock. I
think this implementation is minimal for a reuse implementation and I wanted to
include it to warn against anything simpler.

## Screen tearing

Screen tearing is possible. I use the same actual memory for all buffers so
it's possible to overwrite an old buffer when the compositor isn't expecting
it.

If tearing is a problem for you, you should allocate separate memory for
each buffer. While you could use one shm/mmap/wl\_shm\_pool and partition it
across multiple buffers, in practice this is very hard to anticipate and you
could easily overallocate. I think the practical way is to bundle
shm/mmap/wl\_shm\_pool with a wl\_buffer and dynamically allocate them.

## Compared to X11

My similar helloworld for X11 is a third of the code and a tiny fraction of the memory/cpu.
Wayland has its advantages but so does X11.
