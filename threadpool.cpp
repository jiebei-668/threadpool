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
		if(one.arg != NULL)
		{
			free(one.arg);
			one.arg = NULL;
		}
	}

}




// 向任务队列插入任务
// @param: 任务函数funtion 任务函数的参数arg
void ThreadPool::insertTask(void (*funtion)(void *), void *arg)
{
	pthread_mutex_lock(&this->mutex);
	while(this->taskQ.getMaxTaskQueueNum() == this->taskQ.getTaskQueueNum())
	{
		pthread_cond_wait(&this->notFull, &this->mutex);
	}
	this->taskQ.insertTask(funtion, arg);
	pthread_cond_broadcast(&this->notEmpty);
	printf("add one task... there're %d task waiting...\n", this->taskQ.getTaskQueueNum());
	pthread_mutex_unlock(&this->mutex);
	return;
}
// 初始化线程池，启动一个管理者线程和若干个工作线程
// @param: 初始工作线程数量
void ThreadPool::init(int init_work_thread_num)
{
	this->shutdown = false;
	// 初始化相关成员变量
	if( pthread_mutex_init(&this->mutex, NULL) || pthread_cond_init(&this->notFull, NULL) || pthread_cond_init(&this->notEmpty, NULL))
	{
		printf("ThreadPool::init() failed! Mutex or cond init() failed!\n");
		// 这里其实不合适调用析构函数，因为mutex和cond未初始化成功，析构函数里会拿到mutex以及销毁cond，这可能有问题
		this->~ThreadPool();
		exit(-1);
	}
	this->changeNum = 2;
	if(init_work_thread_num < minWorkThreadNum)
	{
		this->liveNum = minWorkThreadNum;
	}
	else if(init_work_thread_num > maxWorkThreadNum)
	{
		this->liveNum = maxWorkThreadNum;
	}
	else
	{
		this->liveNum = init_work_thread_num;
	}
	this->busyNum = 0;
	this->exitNum = 0;
	this->threadIDs.resize(this->maxWorkThreadNum+1, 0);
	// 启动一个管理者线程和 liveNum 个工作线程
	pthread_create(&this->threadIDs[0], NULL, manage, (void *)this);
	for(int i = 0; i < this->liveNum; i++)
	{
		pthread_create(&this->threadIDs[i+1], NULL, work, (void *)this);
	}
}
// 工作线程的线程清理函数
// 如果线程在等待条件变量时被取消（pthread_cancel），会有bug，互斥锁还不了
// 需要在线程清理函数完成这个释放互斥锁的工作，并在使用条件变量的函数中注册线程清理函数
// 释放mutex，防止取消有等待条件变量的线程时无法退出
// 注：应该将工作线程函数的参数释放掉，怎么实现？
// @param 强转为void *类型的ThreadPool *类型参数
void ThreadPool::worker_cleanup(void *arg)
{
	ThreadPool *pool = (ThreadPool *)arg;	
	pthread_mutex_unlock(&pool->mutex);
	printf("worker[%ld] exit...\n", pthread_self());
	return;
}
// 管理者线程函数
// 每5s管理一次，如果shutdown==true就退出
	// 当没有空闲工作线程busynum==livenum且任务数量大于0时，增加changeNum个新工作线程pool
// 当空闲工作线程超过6 busyNum+6 <= liveNum-exitNum，杀死changeNum个工作线程
// @param: 强转为void *类型的ThreadPool *类型的参数，指向线程池对象
void* ThreadPool::manage(void* arg)
{
	ThreadPool *pool = (ThreadPool *)arg;
	pthread_cleanup_push(pool->manager_cleanup, arg);
	// 分离线程，在退出时不必被join
	pthread_detach(pthread_self());
	while(!pool->shutdown)
	{
		// 每5s管理一次
		printf("manage...\n");
		sleep(5);

		// 当没有空闲线程busynum==livenum且任务数量大于5时，增加changeNum个新线程pool
		pthread_mutex_lock(&pool->mutex);
		if(pool->busyNum == pool->liveNum && pool->taskQ.getTaskQueueNum() > 0)
		{
			for(int i = 0; i < pool->changeNum && pool->liveNum < pool->maxWorkThreadNum; i++)
			{
				int pos = -1;
				// 位置0放管理者线程id
				for(int j = 1; j < pool->threadIDs.size(); j++)
				{
					if(pool->threadIDs[j] == 0) 
					{
						pos = j;
						break;
					}
				}
				pthread_create(&pool->threadIDs[pos], NULL, pool->work, arg);
				printf("add a new worker thread[%ld]...\n", pool->threadIDs[pos]);
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
// 管理者线程退出时的清理函数
// 释放mutex，防止取消有等待条件变量的线程时无法退出
// 需要对任务队列中的堆内存进行释放
void ThreadPool::manager_cleanup(void *arg)
{
	ThreadPool *pool = (ThreadPool *)arg;
	pthread_mutex_unlock(&pool->mutex);
	for(auto &task: pool->taskQ.dq)
	{
		if(task.arg != NULL)
		{
			free(task.arg);
			task.arg = NULL;
		}
	}
	printf("manager exit...\n");
}
void* ThreadPool::work(void* arg)
{
	ThreadPool *pool = (ThreadPool *)arg;
	pthread_cleanup_push(pool->worker_cleanup, arg);
	// 分离线程，在退出时不必被join
	pthread_detach(pthread_self());
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
		if(pool->liveNum > pool->minWorkThreadNum)
		{
			pool->liveNum--;
			pthread_mutex_unlock(&pool->mutex);
			pool->threadExit();
		}
	}
	// 当线程池关闭时，需要清除所有工作线程
	if(pool->shutdown)
	{
		pthread_mutex_unlock(&pool->mutex);
		pool->threadExit();
	}
	auto task = pool->taskQ.getTask();
	pool->busyNum++;
	printf("one task consumed[%ld]... there're %d task waiting\n", pthread_self(), pool->taskQ.getTaskQueueNum());
	pthread_cond_broadcast(&pool->notFull);
	pthread_mutex_unlock(&pool->mutex);
	task.funtion(task.arg);
	// 参数规定动态分配，运行完free
	if(task.arg != NULL)
	{
		free(task.arg);
		task.arg = NULL;
	}
	pthread_mutex_lock(&pool->mutex);
	pool->busyNum--;
	pool->liveNum--;
	pthread_mutex_unlock(&pool->mutex);
	pool->threadExit();
	pthread_cleanup_pop(1);
	return NULL;
}
// 线程退出时将自己的线程id从threadIDs中去除（清零）
void ThreadPool::threadExit()
{
	pthread_mutex_lock(&this->mutex);
	pthread_t id = pthread_self();
	for(int i = 1; i < this->maxWorkThreadNum+1; i++)
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
	liveNum = 0;
	busyNum = 0;
	exitNum = 0;
	shutdown = true;
}
ThreadPool::~ThreadPool()
{
	printf("threadpool is exiting...\n");
	//pthread_mutex_lock(&this->mutex);
	//this->shutdown = true;
	//// 设置退出标志，并唤醒所有等待notEmpty的线程，让其退出
	//pthread_cond_broadcast(&this->notEmpty);
	//pthread_mutex_unlock(&this->mutex);
	// 直接取消全部管理者和工作者线程
	for(auto &id: threadIDs)
	{
		if(id != 0)
		{
			pthread_cancel(id);
			id = 0;
		}
	}
	// 睡眠一段时间保证全部退出
	sleep(5);
	pthread_mutex_destroy(&this->mutex);
	pthread_cond_destroy(&this->notFull);
	pthread_cond_destroy(&this->notEmpty);
	printf("threadpool exiting ok...\n");
}
void ThreadPool::destoryThreadPool()
{
	this->~ThreadPool();
}
