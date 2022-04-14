#!/bin/bash
#
# @(1st arg): a number, which specifies executing which experiment
#

F_ASLR=/proc/sys/kernel/randomize_va_space
F_CPUFREQ=/sys/devices/system/cpu/cpu7/cpufreq/scaling_governor
F_TURBO=/sys/devices/system/cpu/intel_pstate/no_turbo
F_DSMPAFF=/proc/irq/default_smp_affinity

user=`ls -l fibdrv.c | cut -d' ' -f3`
group=`ls -l fibdrv.c | cut -d' ' -f4`

declare -a files
declare -a origs

declare -a expts
expts+=(00_checkvalues92)
expts+=(01_userkernel)
expts+=(02_times)
expts+=(05bn_userkernel)
expts+=(06bn_ktimes)

which_expt=$1

cnt_f=-1

expt_start() {
	# turn off ASLR
	sudo sh -c "echo 0 > ${F_ASLR}"
	# switch to performance mode
	sudo sh -c "echo performance > ${F_CPUFREQ}"
	# turn off Intel turbo mode
	sudo sh -c "echo 1 > ${F_TURBO}"
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
		if [ ${cnt_f} != "0" ] && [ ${cnt_f} != "2" ]; then
			sudo sh -c "echo ${new} > ${files[${cnt_f}]}"
		fi
	done
	sudo sh -c "echo 7f > ${F_DSMPAFF}"
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
		if [ ${i} != "0" ] && [ ${i} != "2" ]; then
			if [ `cat ${files[${i}]}` = ${origs[$i]} ]; then
				echo "${files[${i}]} recovered"
				cnt_check=$((${cnt_check} + 1))
			else
				echo "${files[${i}]} recover failed"
			fi
		fi
	done

	echo "Recovered: ${cnt_check}/${NCHECK}"
}

expt_end() {
	# recover the states
	sudo sh -c "echo 2 > ${F_ASLR}"
	sudo sh -c "echo powersave > ${F_CPUFREQ}"
	sudo sh -c "echo 0 > ${F_TURBO}"
	for i in `seq 0 ${cnt_f}`; do
		if [ ${i} != "0" ] && [ ${i} != "2" ]; then
			sudo sh -c "echo ${origs[${i}]} > ${files[${i}]}"
		fi
	done
	sudo sh -c "echo ff > ${F_DSMPAFF}"
}

expt() {
	expt_start
	echo "Experiment start.."
	sudo sh -c "taskset -c 7 ./expt${expts[$which_expt]}"
	echo "Experiment end.."
	expt_end
}

expt
#check_state
if [ -f "./scripts/plot${expts[$which_expt]}.gp" ]; then
	gnuplot ./scripts/plot${expts[$which_expt]}.gp
fi
sudo sh -c "chown ${user}:${group} ./data/${expts[$which_expt]}_data.out"
