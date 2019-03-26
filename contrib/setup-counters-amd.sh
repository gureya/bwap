#!/usr/bin/env bash

# enable kernel interface for x86 MSRs
#sudo modprobe msr

# configure the HW events to be monitored
likwid-perfctr -f -g CPU_CLOCKS_UNHALTED:PMC0,DISPATCH_STALLS:PMC1 ls
