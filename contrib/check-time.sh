#!/usr/bin/env bash
#
# This script will report the time taken to execute a program
# given a CPU binding and a local NUMA node allocation ratio
#
# It will execute the program with all the (CPU,ratio) combinations
# and report its elapsed time
#
# Example usage:
# /path/to/check-time.sh /path/to/myApp appArg1 appArg2
#
# To configure the CPUs/ratios, change the arrays below!

cpus=("0-6")
#cpus=("0-6" "0-5" "0-4" "0-3" "0-2" "0-1" "0")
ratios=("0.25" "0.30" "0.35" "0.40" "0.45" "0.50" "0.55" "0.60" "0.65" "0.70" "0.75" "0.80" "0.85" "0.90" "0.95" "1.00")

run_cmd () {
  echo ${red}$@${reset}
  $@ > /dev/null
}

echo "Executable and args  ${@}"
echo "CPUs                 ${cpus[@]}"
echo "Ratios               ${ratios[@]}"

red=`tput setaf 1`
reset=`tput sgr0`

for (( i=0; i<${#cpus[@]}; i++ ));
do
	for (( j=0; j<${#ratios[@]}; j++ ));
  do
	  echo -e "${red}Cores: ${cpus[$i]}\tratio: ${ratios[$j]}${reset}"
    export UNSTICKYMEM_FIXED_RATIO=${ratios[$j]}
    echo -e "${red}UNSTICKYMEM_FIXED_RATIO=${UNSTICKYMEM_FIXED_RATIO}${reset}"
	  #run_cmd "/usr/bin/time -f%e /home/dgureya/adaptive_bw_bench/bandwidth_bench_with_unstickymem -c ${cpus[$i]}"
	  run_cmd "/usr/bin/time -f%e numactl --physcpubind=${cpus[$i]} $@"
    unset UNSTICKYMEM_FIXED_RATIO
  done
done
