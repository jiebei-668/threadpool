#include "threadpool_cpp.h"
#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

void testf(void *arg)
{
	int p = (int)(long)arg;
	return;
}
void fun(void *arg)
{
	int int_arg = *(int *)arg;
	printf("pthreadID[%ld] is working..., arg=%d\n", pthread_self(), int_arg);
	sleep(5);
	printf("pthreadID[%ld] is exiting...\n", pthread_self());
	return;
}
ThreadPool pool;
void handler(int sig)
{
	for(int i = 0; i < 5; i++)
	{
		pool.insertTask(fun, malloc(sizeof(int)));
	}
}
void handler_2(int sig)
{
	pool.destoryThreadPool();
}
int main(int argc, char* argv[])
{
	/*
	TaskQueue tq;
	cout << tq.getTaskQueueNum() << "\n";
	tq.insertTask(testf, (void*)(long)1);
	tq.insertTask(testf, (void*)(long)2);
	tq.insertTask(testf, (void*)(long)3);
	cout << tq.getTaskQueueNum() << "\n";
	auto t1 = tq.getTask();
	cout << tq.getTaskQueueNum() << "\n";
	auto t2 = tq.getTask();
	cout << tq.getTaskQueueNum() << "\n";
	auto t3 = tq.getTask();
	cout << tq.getTaskQueueNum() << "\n";
	*/
	pool.init(2);
	signal(15, handler);
	sleep(60);
	return 0;
}
