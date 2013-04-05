TESTS = 0
DEBUG = 0
TARGET = pfabric
pfabric-objs := sch_pfab.o stats.o
obj-m += $(TARGET).o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
RM := rm -f
DEBUG_FLAGS = -g2

ifeq ($(DEBUG),1)
ccflags-y := -DDEBUG
endif

ifeq ($(TESTS),1)
	pfabric-objs += pfabric_tests.o
	ccflags-y += -DPFABRIC_TESTS
endif

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) $(CPPFLAGS) modules

clean:
	$(RM) *.ko *.mod.c *.o Module.symvers modules.order

