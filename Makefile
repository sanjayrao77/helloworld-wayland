CFLAGS=-g -O2 -Wall
all: helloworld

xdg-shell-protocol.c:
	wayland-scanner private-code < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml > xdg-shell-protocol.c

xdg-shell-client-protocol.h:
	wayland-scanner client-header < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml > xdg-shell-client-protocol.h

main.o: main.c xdg-shell-client-protocol.h
	gcc -o main.o -c main.c ${CFLAGS}

helloworld: main.o display_wayland.o mmapshm.o font.o xdg-shell-protocol.o
	gcc -o helloworld main.o display_wayland.o mmapshm.o font.o xdg-shell-protocol.o -lwayland-client

clean:
	rm -f helloworld *.o core xdg-shell-protocol.c xdg-shell-client-protocol.h

monitor: clean
	scp -pr * monitor:src/helloworld-wayland/
