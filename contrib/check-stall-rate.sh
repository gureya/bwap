#!/usr/bin/env bash
# This script will report the observed stall rates when executing
# a given program with different NUMA node allocations
#
# It will execute the program bound to all the CPU ranges specified
# with the `unstickymem` library set to SCAN mode.
# The library will periodically move pages between NUMA nodes and
# report the observed stall rates.
# The library will automatically terminate execution once finished.
#
# Example usage:
# /path/to/check-stall-rate.sh /path/to/myApp appArg1 appArg2
#
# To configure the CPU bindings, change the arrays below!

cpus=("0-6" "0-5" "0-4" "0-3" "0-2" "0-1" "0")

run_cmd () {
  echo ${red}$@${reset}
  $@
}

echo "Executable and args  ${@}"
echo "CPUs                 ${cpus[@]}"

red=`tput setaf 1`
reset=`tput sgr0`

for (( i=0; i<${#cpus[@]}; i++ ));
do
  echo -e "${red}Cores: ${cpus[$i]}${reset}"
	export UNSTICKYMEM_SCAN=1
  #run_cmd "/usr/bin/time -f%e ./autobench -c ${cpus[$i]}"
  run_cmd "/usr/bin/time -f%e numactl --physcpubind=${cpus[$i]} $@"
	unset UNSTICKYMEM_SCAN
done
