#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_THREADS 10

static int a[MAX_THREADS] = { 0 };
static int global = 5;

void increment_a(int *a) {
	*a = *a + getpid();
}

void __do_work(void * arg)
{
	int tid = (long)arg;

	for (int i = 0; i < 10000; i++) {
		increment_a(&a[tid]);
	}
}

static void func_with_common_arg(int num, int *arg)
{
	if(num < *arg)
		printf("%p\n", arg);
}

void *work(void *arg) {

	printf("Thread %d starting\n", (int)arg);

	for(int i = 0; i < 100; i++) {
		func_with_common_arg(i, &global);
		__do_work(arg);
	}

	return 0;
}

int main(int argc, char **argv) {

	int ret;
	pthread_t threads[MAX_THREADS];

	for(int i = 0; i < MAX_THREADS; i++) {
		ret = pthread_create(&threads[i], NULL, work, (void*) (long)i);
		if(ret) {
			printf("pthread_create: %s", strerror(ret));
			exit(-1);
		}
	}

	for(int i = 0; i < MAX_THREADS; i++) {
		ret = pthread_join(threads[i], NULL);
		if(ret) {
			printf("pthread_join: %s", strerror(ret));
			exit(-1);
		}
	}

	for(int i = 0; i < MAX_THREADS; i++)
		printf("%d\n", a[i]);

    return 0;
}
