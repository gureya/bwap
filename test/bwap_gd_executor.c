/*
 * aapp_executor.c
 *
 *  Created on: Jun 4, 2018
 *      Author: GUREYA
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// A structure to hold the nodes information
typedef struct rec {
	int id;
	float weight;
	int count;
} RECORD;

int normalized = 0;

#define MAX_NODES 8
RECORD current_nodes_info[MAX_NODES]; // hold the nodes information ids and weights

void insert_file(float weights[MAX_NODES]) {
	int i;
	FILE *f = fopen("/home/dgureya/devs/unstickymem/config/weights_1w.txt", "w");
	if (f == NULL) {
		printf("Error opening file!\n");
		exit(1);
	}

	for (i = 0; i < 8; i++) {
		fprintf(f, "%.1f %d\n", weights[i], i);
	}

	fclose(f);
	return;
}

void aapp_executor_log(float weights[MAX_NODES], double d, double s, double t) {
	int i;
	FILE *f = fopen("gd_log.txt", "a");
	if (f == NULL) {
		printf("Error opening file!\n");
		exit(1);
	}

	for (i = 0; i < MAX_NODES; i++) {
		fprintf(f, "%.1f ", weights[i]);
	}
	fprintf(f, " - %.2f ", d);
	fprintf(f, " - %.2f ", s);
	fprintf(f, " - %.2f\n", t);

	fclose(f);
	return;
}

void aapp_executor_log_v1(int n, char message[]) {
	FILE *f = fopen("gd_log.txt", "a");
	if (f == NULL) {
		printf("Error opening file!\n");
		exit(1);
	}

	fprintf(f, "%s: %d\n", message, n);

	fclose(f);
	return;
}

double run_app(float weights[]) {
	double elapsed_time;

	//put the weights in file that can be read by the library
	//this should be done by an init function, fix this later
	insert_file(weights);

	//int i;
	time_t start, stop;

	time(&start);
	system("numactl --physcpubind=0-15 parsecmgmt -a run -p splash2x.ocean_ncp -i native -n 16");
	//system("export OMP_NUM_THREADS=8");
	//system("numactl --physcpubind=0-7 /home/dgureya/devs/NPB3.0-omp-C/bin/sp.B");
	time(&stop);

	elapsed_time = difftime(stop, start);
	sleep(1);
	return elapsed_time;
}

int find_minimum(float a[], int n) {
	int c, min, index;

	min = a[0];
	index = 0;

	for (c = 1; c < n; c++) {
		if (a[c] < min) {
			index = c;
			min = a[c];
		}
	}

	return index;
}

int main(void) {

	/* input weights - w0,w1,w2,w3,w4,w5,w6,w7 */
	int i, j, k, n;
	float initial_weights[MAX_NODES] = { 50, 50, 0, 0, 0, 0, 0, 0 }; //testing!

	double dim_time[MAX_NODES]; //neighbor completion times
	//double dim_time_test[MAX_NODES] = { 118, 122, 120, 119, 117, 118, 117, 117 };
	//double dim_time_test_1[MAX_NODES] = { 107, 114, 110, 108, 106, 108, 168, 107 };
	float dim_array[MAX_NODES]; //neighbor weights
	double dim_deravative[MAX_NODES]; //dimension derivatives
	int num_runs = 3; // number of runs
	double average_time; //
	double t_0; //time for the initial run

	double w_step = 7.0;
	double o_step = 1.0;

	double sum;

	/*for (k = 0; k < num_runs; k++) {
	 average_time += run_app(initial_weights);
	 }
	 //t_0 = run_app(initial_weights); //single run
	 t_0 = average_time / num_runs;
	 //log the time and weights
	 aapp_executor_log(initial_weights, t_0);*/

	int max_iter = 20;

	for (n = 0; n < max_iter; n++) {
		average_time = 0;
		char message1[] = "Iteration";
		aapp_executor_log_v1(n, message1);
		for (k = 0; k < num_runs; k++) {
			average_time += run_app(initial_weights);
		}
		//t_0 = run_app(initial_weights); //single run
		t_0 = average_time / num_runs;
		/*if (n == 0) {
		 t_0 = 121;
		 } else if (n == 1) {
		 t_0 = 108;
		 } else {

		 }*/
		//log the time and weights
		sum = 0;
		for (i = 0; i < MAX_NODES; i++) {
			sum += initial_weights[i];
		}
		aapp_executor_log(initial_weights, 0, sum, t_0);

		char message2[] = "Iteration Neighbors";
		aapp_executor_log_v1(n, message2);

		for (j = 0; j < MAX_NODES; j++) {
			average_time = 0;
			//get the next neighbor!
			for (i = 0; i < 8; i++) {
				if (i == j) {
					dim_array[i] = initial_weights[i] + w_step;
					if (dim_array[i] > 100)
						dim_array[i] = 100;
				} else {
					dim_array[i] = initial_weights[i] - o_step;
					if (dim_array[i] < 0) //fix the beyond zero bug
						dim_array[i] = 0;
				}
			}

			//normalize the weights of this neighbor
			sum = 0;
			for (i = 0; i < MAX_NODES; i++) {
				sum += dim_array[i];
				//printf("%.2f ", initial_weights[i]);
			}
			if (sum != 100) {
				for (i = 0; i < MAX_NODES; i++) {
					dim_array[i] = dim_array[i] / sum * 100;
				}
			}

			//get the time of this neighbor as average of three runs!
			for (k = 0; k < num_runs; k++) {
				average_time += run_app(dim_array);
				//printf("run%d: %.2f\n", k, average_time);
			}
			dim_time[j] = average_time / num_runs;
			/*if (n == 0) {
			 dim_time[j] = dim_time_test[j];
			 } else if (n == 1) {
			 dim_time[j] = dim_time_test_1[j];
			 } else {

			 }*/
			//dim_time[j] = run_app(dim_array); // single run!
			//printf("t%d: %.2f\n", j, dim_time[j]);
			//aapp_executor_log(dim_array, dim_time[j]);
			sum = 0;
			for (i = 0; i < MAX_NODES; i++) {
				sum += dim_array[i];
			}

			//get the dims deravative
			if (initial_weights[j] - dim_array[j] == 0) {
				dim_deravative[j] = 1;
			} else {
			dim_deravative[j] = ((t_0 - dim_time[j])
					/ (initial_weights[j] - dim_array[j]));
			}
			//printf("DIM%d: %.2f\n", j, dim_deravative[j]);
			aapp_executor_log(dim_array, dim_deravative[j], sum, dim_time[j]);
		}

		//determine the next point after getting all the time of the neighbors
		for (j = 0; j < MAX_NODES; j++) {
			initial_weights[j] = (initial_weights[j]
					- (w_step * dim_deravative[j]));
			//printf(" %.1f ", initial_weights[j]);
		}
		//printf("\n");

		//normalize the new weights
		//First eliminate any negatives
		int location = find_minimum(initial_weights, MAX_NODES);
		double minimum = initial_weights[location];
		//printf("Minimum: %.2f\n", minimum);
		if (minimum < 0) {
			minimum = minimum * (-1); //make it +ve and add
			//printf("+veMinimum: %.2f\n", minimum);
			for (i = 0; i < MAX_NODES; i++) {
				initial_weights[i] = initial_weights[i] + minimum;
			}
		}

		/*	for (j = 0; j < MAX_NODES; j++) {
		 printf(" %.1f ", initial_weights[j]);
		 }
		 printf("\n"); */

		sum = 0;
		for (i = 0; i < MAX_NODES; i++) {
			sum += initial_weights[i];
			//printf("%.2f ", initial_weights[i]);
		}
		if (sum != 100) {
			for (i = 0; i < MAX_NODES; i++) {
				initial_weights[i] = initial_weights[i] / sum * 100;
			}
		}

		/*for (j = 0; j < MAX_NODES; j++) {
		 printf(" %.1f ", initial_weights[j]);
		 }
		 printf("\n"); */

	}

	exit(1);
}
