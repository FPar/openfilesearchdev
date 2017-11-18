obj-m += openfilesearchdev.o
CFLAGS_openfilesearchdev.o := -Wall

all: ioctl
	+make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

ioctl: openfilesearchdev.h ioctl.c
	gcc ioctl.c -o ioctl

clean:
	+make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f ioctl