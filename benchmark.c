#include "simple_counter.h"
#include "safe_counter.h"
#include "approximate_counter.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

long get_time_nanos() {
	struct timespec ts;
	assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
	return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}

long get_num_cpus() {
	return sysconf(_SC_NPROCESSORS_ONLN);
}

void iteration_simple_counter(int n) {
	simple_counter_t c;
	simple_counter_init(&c);
	for (int i = 0; i < n; i++) {
		simple_counter_increment(&c);
	}
	assert(simple_counter_get(&c) == n);
}

typedef struct {
	int n;
	safe_counter_t *c;
} safe_counter_thread_arg_t;

void *safe_counter_thread(void *arg) {
	safe_counter_thread_arg_t *args = arg;
	int n = args->n;
	safe_counter_t *c = args->c;
	for (int i = 0; i < n; i++) {
		assert(safe_counter_increment(c) == 0);
	}
	return NULL;
}

void iteration_safe_counter(int n) {
	long num_cpus = get_num_cpus();
	safe_counter_t c;
	pthread_t threads[num_cpus];

	assert(safe_counter_init(&c) == 0);

	safe_counter_thread_arg_t args = {n, &c};
	for (int i = 0; i < num_cpus; i++) {
		assert(pthread_create(&threads[i], NULL, safe_counter_thread, (void *)(&args)) == 0);
	}
	
	for (int i = 0; i < num_cpus; i++) {
		assert(pthread_join(threads[i], NULL) == 0);
	}

	int result;
	assert(safe_counter_get(&c, &result) == 0);
	assert(result == num_cpus * n);
	assert(safe_counter_destroy(&c) == 0);
}

typedef struct {
	int n;
	int thread_id;
	approx_counter_t *c;
} approx_counter_thread_arg_t;

void *approx_counter_thread(void *arg) {
	approx_counter_thread_arg_t *args = arg;
	int n = args->n;
	int thread_id = args->thread_id;
	approx_counter_t *c = args->c;
	for (int i = 0; i < n; i++) {
		assert(approx_counter_update(c, thread_id, 1) == 0);
	}
	return NULL;
}

void iteration_approx_counter(int n) {
	long num_cpus = get_num_cpus();
	approx_counter_t c;
	pthread_t threads[num_cpus];
	approx_counter_thread_arg_t args[num_cpus];

	assert(approx_counter_init(&c, 1024, num_cpus) == 0);

	for (int i = 0; i < num_cpus; i++) {
		args[i].n = n;
		args[i].thread_id = i;
		args[i].c = &c;
		assert(pthread_create(&threads[i], NULL, approx_counter_thread, (void *)(&args[i])) == 0);
	}
	
	for (int i = 0; i < num_cpus; i++) {
		assert(pthread_join(threads[i], NULL) == 0);
	}

	int result;
	assert(approx_counter_flush(&c) == 0);
	assert(approx_counter_get(&c, &result) == 0);
	assert(result == num_cpus * n);
	assert(approx_counter_destroy(&c) == 0);
}

// run the 'iteration' function with parameter 'n', 'iterations' times with the name 'bench_name'
// measures the average time taken from running 'iteration(n)' 'iterations' times.
void benchmark(int iterations, int n, void (*iteration)(int), char *bench_name) {
	long totaltime = 0;
	long start_time;
	long end_time;
	printf("%s-%ld:\t", bench_name, get_num_cpus());
	// print to stdout now, don't wait for newline
	fflush(stdout);
	for (int i = 0; i < iterations; i++) {
		start_time = get_time_nanos();
		iteration(n);
		end_time = get_time_nanos();
		totaltime += end_time - start_time;
	}
	printf("%d iterations\taverage time: %ldns\n", iterations, totaltime/iterations);
}

int main(int argc, char *argv[]) {
	benchmark(5, 1000000, iteration_simple_counter, "benchmark_simple_counter");
	benchmark(5, 1000000, iteration_safe_counter, "benchmark_safe_counter");
	benchmark(5, 1000000, iteration_approx_counter, "benchmark_approx_counter");

	exit(EXIT_SUCCESS);
}