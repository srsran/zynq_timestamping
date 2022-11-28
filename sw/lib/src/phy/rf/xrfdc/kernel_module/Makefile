#
# Copyright 2013-2020 Software Radio Systems Limited
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the distribution.
#

KDIR = /home/srs/linux
PWD = $(shell pwd)
obj-m := srs_dma_driver.o
CFLAGS_srs_dma_driver += -DDEBUG
ccflags-y := -std=gnu99 -Wno-declaration-after-statement 

default:
		$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
		@rm -rf *.o *.cmd .*.flags *.mod.c *.order .srs_dma_driver.* .tmp_versions

distclean: clean
		@rm *.ko *.symvers
