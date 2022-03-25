CONFIG_MODULE_SIG = n
TARGET_MODULE := fibdrv

obj-m := $(TARGET_MODULE).o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied

EXPT=expt01_userkernel\
	 expt02_vlafree

EXPT_PERF=expt03_vlafree_perf

all: $(GIT_HOOKS) client $(EXPT) $(EXPT_PERF)
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) client out $(EXPT)
load:
	sudo insmod $(TARGET_MODULE).ko
unload:
	sudo rmmod $(TARGET_MODULE) || true >/dev/null

client: client.c
	$(CC) -o $@ $^

$(EXPT): %: %.c
	$(CC) -o $@ $< -lm

$(EXPT_PERF): %: %.c
	$(CC) -o $@ $<

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

expt01: all
	$(MAKE) unload
	$(MAKE) load
	sudo ./scripts/expt.sh 0 0
	$(MAKE) unload

expt02: all
	$(MAKE) unload
	$(MAKE) load
	sudo ./scripts/expt.sh 1 0
	$(MAKE) unload

expt03: $(EXPT_PERF)
	$(MAKE) -C $(KDIR) M=$(PWD) modules EXTRA_FLAGS=-Dperf_test
	$(MAKE) unload
	$(MAKE) load
	sudo ./scripts/expt.sh 2 1 0
	sudo ./scripts/expt.sh 2 1 1
	sudo ./scripts/expt.sh 2 1 2
	$(MAKE) unload

cscope_tags:
	@rm -f cscope.* tags
	@echo -n "Old cscope files, tags removed..\nNew cscope.. "
	@find . -name "*.[ch]" > cscope.files
	@cscope -Rbq
	@echo "done\nNew tags.. \c" -e
	@ctags -R -h=".c.h"
	@echo "done"
