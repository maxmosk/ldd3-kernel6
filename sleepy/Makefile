CONFIG_MODULE_SIG=n

ifneq ($(KERNELRELEASE),)
	sleepy-objs := main.o
    obj-m := sleepy.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif
