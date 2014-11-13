#
# Copyright (C) 2011, Texas Instruments, Incorporated - http://www.ti.com/
#
# SPDX-License-Identifier:	GPL-2.0+
#
ifndef CONFIG_SPL_BUILD
ALL-y	+= u-boot.img
endif

# Skip relocation as U-Boot cannot run on SDRAM for secure boot
LDFLAGS_u-boot := $(filter-out -pie, $(LDFLAGS_u-boot))
ALL-y := $(filter-out checkarmreloc, $(ALL-y))

# Added for handoff support
PLATFORM_RELFLAGS += -I$(TOPDIR)/board/$(BOARDDIR)

