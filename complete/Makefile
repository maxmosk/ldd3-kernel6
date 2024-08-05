CONFIG_MODULE_SIG=n

ifneq ($(KERNELRELEASE),)
	complete-objs := main.o
    obj-m := complete.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif
