CONFIG_MODULE_SIG=n

CONFIG := $(PWD)/.config
include ${CONFIG}

ifeq (${CONFIG_BIO_BASED_MODE},y)
$(info Build sbull in bio based mode)
	ccflags-y += "-D BIO_BASED_SBULL"
else
$(info Build sbull in request based mode)
endif

ifeq (${CONFIG_BLOCK_DEVICE_INFO},y)
$(info Build sbull with additional info)
	ccflags-y += "-D PRINT_INFO"
endif

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