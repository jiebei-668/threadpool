#include "threadpool_cpp.h"
#include <deque>
#include <algorithm>
#include <pthread.h>
#include <iostream>
#include <unistd.h>


int TaskQueue::getTaskQueueNum()
{
	return this->dq.size();
}
int TaskQueue::getMaxTaskQueueNum()
{
	return this->maxTaskQueueNum;
}
void TaskQueue::insertTask(void (*funtion)(void *), void *arg)
{
	struct Task t = {funtion, arg};
	this->dq.push_back(t);
	return;
}
TaskQueue::Task TaskQueue::getTask()
{
	struct Task t = this->dq.front();
	this->dq.pop_front();
	return t;
}
TaskQueue::TaskQueue()
{

}
// 规定task的arg为动态分配内存，所以最后需要释放
TaskQueue::~TaskQueue()
{
	for(auto one: this->dq)
	{
		free(one.arg);
	}

}




void ThreadPool::insertTask(void (*funtion)(void *), void *arg)
{
	pthread_mutex_lock(&this->mutex);
	while(this->taskQ.getMaxTaskQueueNum() == this->taskQ.getTaskQueueNum())
	{
		pthread_cond_wait(&this->notFull, &this->mutex);
	}
	this->taskQ.insertTask(funtion, arg);
	pthread_cond_signal(&this->notEmpty);
	printf("add one task... there're %d task waiting...\n", this->taskQ.getTaskQueueNum());
	pthread_mutex_unlock(&this->mutex);
	return;
}
void ThreadPool::init(int init_thread_num)
{
	this->shutdown = false;
	// 初始化相关成员变量
	if( pthread_mutex_init(&this->mutex, NULL) || pthread_cond_init(&this->notFull, NULL) || pthread_cond_init(&this->notEmpty, NULL))
	{
		printf("ThreadPool::init() failed! mutex or cond init failed!\n");
		this->~ThreadPool();
		return;
	}
	this->changeNum = 2;
	this->liveNum = min(init_thread_num, minThreadNum);
	this->liveNum = min(this->liveNum, maxThreadNum);
	this->busyNum = 0;
	this->exitNum = 0;
	this->threadIDs.resize(this->maxThreadNum+1, 0);
	// 启动一个管理者线程和 inin_thread_num 个工作线程
	pthread_create(&this->threadIDs[0], NULL, manage, (void *)this);
	for(int i = 0; i < this->liveNum; i++)
	{
		pthread_create(&this->threadIDs[i+1], NULL, work, (void *)this);
	}
}
void ThreadPool::cleanup()
{
	pthread_mutex_unlock();
	return;
}
void* ThreadPool::manage(void* arg)
{
	ThreadPool *pool = (ThreadPool *)arg;
	pthread_cleanup_push(pool->cleanup, NULL);
	while(!pool->shutdown)
	{
		// 每5s管理一次
		sleep(5);
		printf("manage...\n");


		// 当没有空闲线程busynum==livenum且任务数量大于5时，增加changeNum个新线程pool
		pthread_mutex_lock(&pool->mutex);
		if(pool->busyNum == pool->liveNum && pool->taskQ.getTaskQueueNum() >= 5)
		{
			for(int i = 0; i < pool->changeNum && pool->liveNum < pool->maxThreadNum; i++)
			{
				int pos = -1;
				// 位置0放管理者线程id
				for(int j = 1; j < pool->threadIDs.size(); j++)
				{
					if(pool->threadIDs[j] != 0) 
					{
						pos = j;
						break;
					}
				}
				pthread_create(&pool->threadIDs[pos], NULL, pool->work, pool);
				pool->liveNum++;
			}
		}


		// 当空闲线程超过6 busyNum+6 <= liveNum-exitNum，杀死changeNum线程
		if(pool->busyNum+6 < pool->liveNum - pool->exitNum)
		{
			pool->exitNum += pool->changeNum;
			pthread_cond_broadcast(&pool->notEmpty);
			printf("free threads're too many, manager gonna kill %d thread...\n", pool->exitNum);
		}
		pthread_mutex_unlock(&pool->mutex);
	}
	pthread_cleanup_pop(1);
	return NULL;
}
void* ThreadPool::work(void* arg)
{
	ThreadPool *pool = (ThreadPool *)arg;
	pthread_cleanup_push(pool->cleanup, NULL);
	// 锁住线程从任务队列获取一个任务或者自杀	
	pthread_mutex_lock(&pool->mutex);

	while(pool->taskQ.getTaskQueueNum() == 0 && pool->exitNum == 0)
	{
		pthread_cond_wait(&pool->notEmpty, &pool->mutex);
	}
	// 优先判断是否需要自杀，因为获取任务的代码默认队列中有任务，直接取没判断，会不安全
	if(pool->exitNum > 0)
	{
		printf("thread[%ld] is killed by manager...\n", pthread_self());
		pool->exitNum--;
		// 在当前条件下一定满足这个条件
		if(pool->liveNum > pool->minThreadNum)
		{
			pool->liveNum--;
			pthread_mutex_unlock(&pool->mutex);
			pool->threadExit();
		}
	}
	if(pool->shutdown)
	{
			pthread_mutex_unlock(&pool->mutex);
		pool->threadExit();
	}
	auto task = pool->taskQ.getTask();
	pool->busyNum++;
	printf("one task consumed[%ld]... there're %d task waiting\n", pthread_self(), pool->taskQ.getTaskQueueNum());
	pthread_cond_signal(&pool->notFull);
	pthread_mutex_unlock(&pool->mutex);
	task.funtion(task.arg);
	// 参数规定动态分配，运行完free
	free(task.arg);
	pthread_mutex_lock(&pool->mutex);
	pool->busyNum--;
	pool->liveNum--;
	pthread_mutex_unlock(&pool->mutex);
	pthread_cleanup_pop(1);
	return NULL;
}
void ThreadPool::threadExit()
{
	pthread_mutex_lock(&this->mutex);
	pthread_t id = pthread_self();
	for(int i = 1; i < this->maxThreadNum+1; i++)
	{
		if(id == this->threadIDs[i]) 
		{
			this->threadIDs[i] = 0;
			break;
		}
	}
	pthread_mutex_unlock(&this->mutex);
	pthread_exit(NULL);
}
ThreadPool::ThreadPool()
{
}
ThreadPool::~ThreadPool()
{
	printf("threadpool is exiting...\n");
	pthread_mutex_lock(&this->mutex);
	this->shutdown = true;
	// 设置退出标志，并唤醒所有等待notEmpty的线程，让其退出
	pthread_cond_broadcast(&this->notEmpty);
	pthread_mutex_unlock(&this->mutex);
	// 睡眠一段时间保证全部退出
	sleep(30);
	pthread_join(this->threadIDs[0], NULL);
	pthread_mutex_destroy(&this->mutex);
	pthread_cond_destroy(&this->notFull);
	pthread_cond_destroy(&this->notEmpty);
	printf("threadpool exiting ok...\n");
}
void ThreadPool::destoryThreadPool()
{
	this->~ThreadPool();
}
