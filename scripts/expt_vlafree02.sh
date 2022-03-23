#!/bin/bash
#
# NEED root privilege
#

F_ASLR=/proc/sys/kernel/randomize_va_space
F_CPUFREQ=/sys/devices/system/cpu/cpu7/cpufreq/scaling_governor
F_TURBO=/sys/devices/system/cpu/intel_pstate/no_turbo
F_DSMPAFF=/proc/irq/default_smp_affinity

declare -a files
declare -a origs

cnt_f=-1

expt_start() {
	# turn off ASLR
	echo 0 > ${F_ASLR}
	# switch to performance mode
	echo performance > ${F_CPUFREQ}
	# turn off Intel turbo mode
	echo 1 > ${F_TURBO}
	# save the smp_affinity masks and modify them
	for file in `find /proc/irq -name "smp_affinity"`
	do
		files+=(${file})
		orig=`cat ${file}`
		origs+=(${orig})
		orig=0x${orig}
		new=$((${orig} & ~0x80))
		new=`printf '%02x' ${new}`
		cnt_f=$((${cnt_f} + 1))
		echo ${new} > ${files[${cnt_f}]}
	done
	echo 7f > ${F_DSMPAFF}
}

check_state() {
	NCHECK=$((${cnt_f} + 1 + 4))
	cnt_check=0

	if [ `cat ${F_ASLR}` = "2" ]; then
		echo "ASLR recovered"
		cnt_check=$((${cnt_check} + 1))
	fi
	if [ `cat ${F_CPUFREQ}` = "powersave" ]; then
		echo "CPU mode recovered"
		cnt_check=$((${cnt_check} + 1))
	fi
	if [ `cat ${F_TURBO}` = "0" ]; then
		echo "Turbo mode recovered"
		cnt_check=$((${cnt_check} + 1))
	fi
	if [ `cat ${F_DSMPAFF}` = "ff" ]; then
		echo "Default SMP IRQ affinity recovered"
		cnt_check=$((${cnt_check} + 1))
	fi
	for i in `seq 0 ${cnt_f}`; do
		if [ `cat ${files[${i}]}` = ${origs[$i]} ]; then
			echo "${files[${i}]} recovered"
			cnt_check=$((${cnt_check} + 1))
		else
			echo "${files[${i}]} recover failed"
		fi
	done

	echo "Recovered: ${cnt_check}/${NCHECK}"
}

expt_end() {
	# recover the states
	echo 2 > ${F_ASLR}
	echo powersave > ${F_CPUFREQ}
	echo 0 > ${F_TURBO}
	for i in `seq 0 ${cnt_f}`; do
		echo ${origs[${i}]} > ${files[${i}]}
	done
	echo ff > ${F_DSMPAFF}
}

expt() {
	expt_start
	taskset -c 7 ./expt_vlafree02
	expt_end
}

expt
#check_state
gnuplot ./scripts/plot_vlafree02.gp
chown zz4t:zz4t data/vlafree02-data.out data/vlafree02-pic.png
