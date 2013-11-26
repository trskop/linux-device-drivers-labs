obj-m := feserial.o
#KDIR := /lib/modules/$(shell uname -r)/build
KDIR := /home/devel/linux
PWD := $(shell pwd)

all:
	$(MAKE) ARCH=arm CROSS_COMPILE=/home/devel/embedded/buildroot/output/host/usr/bin/arm-buildroot-linux-uclibcgnueabi- -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

