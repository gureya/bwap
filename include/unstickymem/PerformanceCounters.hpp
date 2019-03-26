#ifndef UNSTICKYMEM_HARDWARE_EVENTS
#define UNSTICKYMEM_HARDWARE_EVENTS

#include <cstdint>

#include <string>

namespace unstickymem {

void initialize_likwid();

// checks performance counters and computes stalls per second since last call
double get_stall_rate();  // via joao barreto's lib

double get_stall_rate_v2();  // via Like I Knew What I'm Doing (LIKWID Library!)
void stop_all_counters();  // Restarting it might have some issues if counters are not stopped!
double get_elapsed_stall_rate();  //get the elapsed stall rate

// samples stall rate multiple times and filters outliers
double get_average_stall_rate(size_t num_measurements,
                              useconds_t usec_between_measurements,
                              size_t num_outliers_to_filter);
double get_average_stall_rate2(size_t num_measurements,
                               useconds_t usec_between_measurements,
                               size_t num_outliers_to_filter);

//output stall rate to a log file
void unstickymem_log(double ratio, double sr);
void unstickymem_log(double ratio);
void unstickymem_log_v1(double sr, std::string mode);

// read time stamp counter
inline uint64_t readtsc(void);

// read performance monitor counter
inline uint64_t readpmc(int32_t n);

}  // namespace unstickymem

#endif  // UNSTICKYMEM_HARDWARE_EVENTS
