CONFIG_MODULE_SIG=n

ifneq ($(KERNELRELEASE),)
	sbull-objs := main.o sbull_device.o
    obj-m := sbull.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif