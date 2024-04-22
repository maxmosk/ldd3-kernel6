CONFIG_MODULE_SIG=n

ifneq ($(KERNELRELEASE),)
	scull_pipe-objs := main.o fops.o
    obj-m := scull_pipe.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif