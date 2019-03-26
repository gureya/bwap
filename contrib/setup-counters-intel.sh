#!/usr/bin/env bash

# enable kernel interface for x86 MSRs
sudo modprobe msr

# configure the HW events to be monitored
sudo likwid-perfctr -f -g CPU_CLOCK_UNHALTED_THREAD_P:PMC0,RESOURCE_STALLS_ANY:PMC1 ls
