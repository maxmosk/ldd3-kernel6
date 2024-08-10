CONFIG_MODULE_SIG=n

ifneq ($(KERNELRELEASE),)
	scullv-objs := impl/main.o impl/scullv_dev_common.o impl/scullv.o
    obj-m := scullv.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif
