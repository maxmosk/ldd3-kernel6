CONFIG_MODULE_SIG=n

ifneq ($(KERNELRELEASE),)
	scullc-objs := impl/main.o impl/scullc_dev_common.o impl/scullc.o
    obj-m := scullc.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif
