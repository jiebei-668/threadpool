#include "threadpool.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

void taskFunc(void* arg)
{
	int num = *(int *)arg;
	printf("thread[%d] is working, thread_id[%ld]\n", num, (long)pthread_self());
	sleep(1);
	return;
}

int main(int argc, char* argv[])
{
	ThreadPool* pool = threadPoolCreate(3, 10, 100);
	for(int i = 0; i < 100; i++)
	{
		int* p_num = (int*)malloc(sizeof(int));
		*p_num = i;
		threadPoolAdd(pool, taskFunc, (void *)p_num);
	}
	sleep(30);
	threadPoolDestroy(pool);
	return 0;
}
