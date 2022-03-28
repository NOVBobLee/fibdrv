CONFIG_MODULE_SIG = n
TARGET_MODULE := fibdrv

obj-m := $(TARGET_MODULE).o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied

USR := client\
	   expt00_checkvalues_92\
	   expt01_userkernel\
	   expt02_vlafree\
	   expt03_vlafree_perf\
	   expt04_exactsol

all: $(GIT_HOOKS) $(USR)
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) $(USR) out
load:
	sudo insmod $(TARGET_MODULE).ko
unload:
	sudo rmmod $(TARGET_MODULE) || true >/dev/null

$(USR): %: %.c
	$(CC) -o $@ $< -lm

PRINTF = env printf
PASS_COLOR = \e[32;01m
NO_COLOR = \e[0m
pass = $(PRINTF) "$(PASS_COLOR)$1 Passed [-]$(NO_COLOR)\n"

check: all
	$(MAKE) unload
	$(MAKE) load
	sudo ./client > out
	$(MAKE) unload
	@diff -u out scripts/expected.txt && $(call pass)
	@scripts/verify.py

# expt.sh arg1 arg2 arg3
# arg1: which experiment (0-based)
# arg2: is perf ?
# arg3: experiment arg

mycheck92: all
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(MAKE) unload
	$(MAKE) load
	sudo ./expt00_checkvalues_92 4
	$(MAKE) unload

expt01: all
	$(MAKE) -C $(KDIR) M=$(PWD) modules KCFLAGS=-D__TEST_KTIME
	$(MAKE) unload
	$(MAKE) load
	sudo ./scripts/expt.sh 0 0
	$(MAKE) unload

expt02: all
	$(MAKE) -C $(KDIR) M=$(PWD) modules KCFLAGS=-D__TEST_KTIME
	$(MAKE) unload
	$(MAKE) load
	sudo ./scripts/expt.sh 1 0
	$(MAKE) unload

expt03: all
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(MAKE) unload
	$(MAKE) load
	sudo ./scripts/expt.sh 2 1 0
	sudo ./scripts/expt.sh 2 1 1
	sudo ./scripts/expt.sh 2 1 2
	$(MAKE) unload

expt04: expt04_exactsol.c
	$(CC) -o expt04_exactsol expt04_exactsol.c -lm
	./expt04_exactsol
	gnuplot scripts/plot04_exactsol.gp

cscope_tags:
	@rm -f cscope.* tags
	@echo -n "Old cscope files, tags removed..\nNew cscope.. "
	@find . -name "*.[ch]" > cscope.files
	@cscope -Rbq
	@echo "done\nNew tags.. \c" -e
	@ctags -R -h=".c.h"
	@echo "done"
