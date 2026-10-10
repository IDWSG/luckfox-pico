# SPDX-License-Identifier: GPL-2.0
# 纯 kbuild 风格：workflow 会走 "make -C kernel O=... M=... modules" 分支
obj-m += sample_drv.o

# 本地编译用（workflow 里不依赖这些变量，它自己传参）
KDIR ?= ../../source/kernel
KOUT ?= $(KDIR)/../objs_kernel
ARCH ?= arm
CROSS_COMPILE ?= arm-rockchip830-linux-uclibcgnueabihf-

module:
	$(MAKE) -C $(KDIR) O=$(abspath $(KOUT)) ARCH=$(ARCH) \
	        CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules

meterd: meterd.c sample_ioctl.h
	$(CROSS_COMPILE)gcc -O2 -Wall -I. -o $@ meterd.c

all: module meterd

clean:
	$(MAKE) -C $(KDIR) O=$(abspath $(KOUT)) ARCH=$(ARCH) \
	        CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) clean
	rm -f meterd

.PHONY: module meterd all clean