CONFIG_MODULE_SIG=n

CONFIG := $(PWD)/.config
include ${CONFIG}

$(info Timeout = ${CONFIG_SNULL_TIMEOUT})
	ccflags-y += "-D SNULL_TIMEOUT=$(CONFIG_SNULL_TIMEOUT)"

ifeq (${CONFIG_SNULL_NAPI},y)
$(info Use napi in snull)
	ccflags-y += "-D USE_NAPI=1"
else
$(info Don't use napi in snull)
	ccflags-y += "-D USE_NAPI=0"
endif

$(info Timeout = ${CONFIG_SNULL_LOCKUP})
	ccflags-y += "-D LOCKUP=$(CONFIG_SNULL_LOCKUP)"
$(info Timeout = ${CONFIG_POOL_SIZE})
	ccflags-y += "-D POOL_SIZE=$(CONFIG_POOL_SIZE)"

ifneq ($(KERNELRELEASE),)
	snull-objs := main.o snull_device.o
	obj-m := snull.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif