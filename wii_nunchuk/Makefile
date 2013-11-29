obj-m := wii_nunchuk.o

# Build the Module for the Host system
#KDIR := /lib/modules/$(shell uname -r)/build 

BUILDROOT := $(HOME)/buildroot

# Build the Module for ARM/Raspberry Pi training. Don't forget to update the PATH accordingly
KDIR := $(BUILDROOT)/output/build/linux-1587f77/

PWD := $(shell pwd)

all:
	$(MAKE) ARCH=arm CROSS_COMPILE=$(BUILDROOT)/output/host/usr/bin/arm-buildroot-linux-uclibcgnueabi- -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

