CONFIG_MODULE_SIG=n

ifneq ($(KERNELRELEASE),)
	scull_full-objs := impl/main.o impl/scull_dev_common.o impl/scull.o impl/scull_pipe.o impl/scull_single.o impl/scull_uid.o impl/scull_wuid.o impl/scull_priv.o
    obj-m := scull_full.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif
