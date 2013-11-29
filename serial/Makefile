obj-m := feserial.o

BUILDROOT := $(HOME)/buildroot
KDIR := $(BUILDROOT)/output/build/linux-1587f77/
PWD := $(shell pwd)

all:
	$(MAKE) ARCH=arm CROSS_COMPILE=$(BUILDROOT)/output/host/usr/bin/arm-buildroot-linux-uclibcgnueabi- -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
