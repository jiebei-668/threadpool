/**************************************************************************
 * program: c++版的线程池实现。线程池最大任务数100，一个管理线程，若干个工作线程，最多有20个工作线程。管理线程可以根据存活工作线程数、忙的工作线程数和任务数量适当增减工作线程数。工作线程执行完后就退出。通过信号15可以一次向任务队列添加5个任务，通过信号2可以终止程序。详见threadpool.cpp threadpool_cpp.h
 * author: jiebei
 *************************************************************************/
#include "threadpool_cpp.h"
#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

// 可以自定义工作线程函数
void fun(void *arg)
{
	int int_arg = *(int *)arg;
	printf("pthreadID[%ld] is working..., arg=%d\n", pthread_self(), int_arg);
	sleep(5);
	printf("pthreadID[%ld] is exiting...\n", pthread_self());
	return;
}
ThreadPool pool;
// 信号15的处理函数，生产5个任务
void handler(int sig)
{
	for(int i = 0; i < 5; i++)
	{
		pool.insertTask(fun, malloc(sizeof(int)));
	}
}
// 信号2的处理函数，终止程序
void handler_2(int sig)
{
	signal(2, SIG_IGN);
	signal(15, SIG_IGN);
	exit(0);
}
int main(int argc, char* argv[])
{
	signal(15, handler);
	signal(2, handler_2);
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
	// 初始化线程池，初始2个工作线程
	pool.init(2);
	// 主线程必须在这里死循环不能退出
	while(true)
	{
	}
	return 0;
}
