CONFIG_MODULE_SIG = n
TARGET_MODULE := fibdrv_bn

obj-m := $(TARGET_MODULE).o
$(TARGET_MODULE)-objs := fibdrv.o\
						 bn_fib.o

ccflags-y := -std=gnu99 -Wno-declaration-after-statement

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied

USR := client\
	   expt00_checkvalues92\
	   expt01_userkernel\
	   expt02_times\
	   expt03_perf\
	   expt04_exactsol\
	   expt05bn_userkernel\
	   expt06bn_ktime\
	   expt07bn_perf\
	   fbn_debug

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

# Check Fibonacci values under F_100
check: all
	$(MAKE) unload
	$(MAKE) load
	sudo ./client > out
	$(MAKE) unload
	@diff -u out scripts/expected.txt && $(call pass)
	@scripts/verify.py

# For debugging big number operations
fbndebug: $(USR)
	$(MAKE) -C $(KDIR) M=$(PWD) modules KCFLAGS=-D_FBN_DEBUG
	$(MAKE) unload
	$(MAKE) load
	sudo ./fbn_debug
	$(MAKE) unload
	dmesg | grep fibdrv_debug | tail -n 60

# ./scripts/expt.sh <arg1>
# @arg1: which experiment (0-based)

# Test Fibonacci values under F_92
expt00: $(USR)
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(MAKE) unload
	$(MAKE) load
	./scripts/expt.sh 0
	$(MAKE) unload

# Test Fibonacci executing time in user space vs. kernel space
expt01: $(USR)
	$(MAKE) -C $(KDIR) M=$(PWD) modules KCFLAGS=-D_TEST_KTIME
	$(MAKE) unload
	$(MAKE) load
	./scripts/expt.sh 1
	$(MAKE) unload

# Test Fibonacci executing time in different methods
expt02: $(USR)
	$(MAKE) -C $(KDIR) M=$(PWD) modules KCFLAGS=-D_TEST_KTIME
	$(MAKE) unload
	$(MAKE) load
	./scripts/expt.sh 2
	$(MAKE) unload

# Test Fibonacci execution by perf
expt03: $(USR)
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(MAKE) unload
	$(MAKE) load
	sudo sh -c "taskset -c 7 perf stat -r 50 ./expt03_perf"
	$(MAKE) unload

# Test Fibonacci values computed from exact solution method
expt04: expt04_exactsol.c
	-mv data/00_checkvalues92_data.out data/00_checkvalues92_data.out.tmp 2> /dev/null
	-mv data/00_checkvalues92_pic.png data/00_checkvalues92_pic.png.tmp 2> /dev/null
	$(CC) -o expt04_exactsol $< -lm
	./expt04_exactsol
	gnuplot scripts/plot00_checkvalues92.gp
	mv -f data/00_checkvalues92_data.out data/04_exactsol_data.out
	mv -f data/00_checkvalues92_pic.png data/04_exactsol_pic.png
	-mv data/00_checkvalues92_data.out.tmp data/00_checkvalues92_data.out 2> /dev/null
	-mv data/00_checkvalues92_pic.png.tmp data/00_checkvalues92_pic.png 2> /dev/null

expt05: $(USR)
	$(MAKE) -C $(KDIR) M=$(PWD) modules KCFLAGS=-D_TEST_KTIME
	$(MAKE) unload
	$(MAKE) load
	./scripts/expt.sh 3
	$(MAKE) unload

# Test big number Fibonacci computation ktime
expt06: $(USR)
	$(MAKE) -C $(KDIR) M=$(PWD) modules KCFLAGS=-D_TEST_KTIME
	$(MAKE) unload
	$(MAKE) load
	./scripts/expt.sh 4
	$(MAKE) unload

# Use perf-events on big number Fibonacci computation
expt07: $(USR)
	$(MAKE) -C $(KDIR) M=$(PWD) modules KCFLAGS=-D_PERF_EVT
	$(MAKE) unload
	$(MAKE) load
	sudo sh -c "taskset -c 7 perf record -g ./expt07bn_perf"
	$(MAKE) unload

# Generate module.dep for loading symbols in perf-events report
loadsymbol:
	$(MAKE) -C $(KDIR) M=$(PWD) modules KCFLAGS=-D_PERF_EVT
	sudo mkdir -p /lib/modules/$(shell uname -r)/extra
	sudo cp -f $(TARGET_MODULE).ko /lib/modules/$(shell uname -r)/extra
	sudo depmod -a

.PHONY: loadsymbol clean load unload all

cscope_tags:
	@rm -f cscope.* tags
	@echo -n "Old cscope files, tags removed..\nNew cscope.. "
	@find . -name "*.[ch]" > cscope.files
	@cscope -Rbq
	@echo "done\nNew tags.. \c" -e
	@ctags -R -h=".c.h"
	@echo "done"
