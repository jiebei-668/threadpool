#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


const int NUMBER = 2;

typedef struct Task
{
	void (*funtion)(void* arg);
	void* arg;
}Task;


struct ThreadPool
{
	Task* taskQ;
	int queueCapacity;	// 容量
	int queueSize;		// 任务队列大小
	int queueFront;		// 队头：取数据
	int queueTear;		// 队尾：放数据
	
	pthread_t managerID;
	pthread_t *threadIDs;
	int minNum;			// 最小线程数量
	int maxNum;			// 最大线程数量
	int busyNum;		// 忙的线程数量
	int liveNum;		// 活的线程数量
	int exitNum;		// 要销毁的线程数量
	pthread_mutex_t	mutexPool;	// 锁整个线程池
	pthread_mutex_t mutexBusy;	// 锁busyNum
	pthread_cond_t notEmpty;	// 判满
	pthread_cond_t notFull;		// 判空
	
	int shutdown;		// 1销毁线程池 0不销毁线程池
};

ThreadPool *threadPoolCreate(int min, int max, int queueSize)
{
	ThreadPool* pool = NULL;
	do
	{
		pool = (ThreadPool *)malloc(sizeof(ThreadPool));
		if(pool == NULL)
		{
			printf("malloc threadpool failed!\n");
			break;
		}

		// thread settings
		pool->threadIDs = (pthread_t *)malloc(sizeof(pthread_t)*max);
		if(pool->threadIDs == NULL)
		{
			printf("malloc threadIDs failed!\n");
			break;
		}
		memset(pool->threadIDs, 0, sizeof(pthread_t)*max);
		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->liveNum = min;
		pool->exitNum = 0;


		// mutex and cond
		if(pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0)
		{
			printf("mutex or cond failed!\n");
			break;
		}


		// task queue
		pool->taskQ = (Task *)malloc(sizeof(Task) * queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueTear = 0;

		pool->shutdown = 0;


		pthread_create(&pool->managerID, NULL, manager, pool);
		for(int i = 0; i < min; i++)
		{
			pthread_create(&pool->threadIDs[i], NULL, worker, pool);
		}
		return pool;

	}while(0);

	if(pool && pool->taskQ)
	{
		free(pool->taskQ);
	}
	if(pool && pool->threadIDs)
	{
		free(pool->threadIDs);
	}
	if(pool)
	{
		free(pool);
	}
	
	return NULL;
}


void* worker(void* arg)
{
	ThreadPool *pool = (ThreadPool *)arg;
	while(1)
	{
		pthread_mutex_lock(&pool->mutexPool);
		
		while(pool->queueSize == 0 && !pool->shutdown)
		{
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);
		}

		if(pool->exitNum > 0)
		{
			pool->exitNum--;
			if(pool->liveNum > pool->minNum)
			{
				pool->liveNum--;
				pthread_mutex_unlock(&pool->mutexPool);
				threadExit(pool);
			}
		}

		if(pool->shutdown)
		{
			pthread_mutex_unlock(&pool->mutexPool);
			threadExit(pool);
		}

		Task task;
		task.funtion = pool->taskQ[pool->queueFront].funtion;
		task.arg = pool->taskQ[pool->queueFront].arg;
		pool->queueFront = (pool->queueFront+1) % pool->queueCapacity;
		pool->queueSize--;
		pthread_cond_signal(&pool->notFull);
		pthread_mutex_unlock(&pool->mutexPool);

		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);
		printf("thread[%ld] is starting\n", pthread_self());
		task.funtion(task.arg);
		free(task.arg);
		task.arg = NULL;
		printf("thread[%ld] is ending\n", pthread_self());
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
	}
	return NULL;
}

void* manager(void* arg)
{
	ThreadPool *pool = (ThreadPool *)arg;
	while(!pool->shutdown)
	{
		sleep(3);

		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);


		// add thread
		// rule: queueSize > liveNum && liveNum < maxNum
		if(queueSize > liveNum && liveNum < pool->maxNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			int counter = 0;
			for(int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; i++)
			{
				if(pool->threadIDs[i] == 0)
				{
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					counter++;
					pool->liveNum++;
				}
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}

		// destory thread
		// rule: busyNum*2 < liveNum && liveNum > minNum
		if(busyNum*2 < liveNum && liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);

			for(int i = 0; i < NUMBER; i++)
			{
				pthread_cond_signal(&pool->notEmpty);
			}
		}
	}
	return NULL;
}

void threadExit(ThreadPool* pool)
{
	pthread_t tid = pthread_self();
	for(int i = 0; i < pool->maxNum; i++)
	{
		if(pool->threadIDs[i] == tid)
		{
			pool->threadIDs[i] = 0;
			printf("threadExit() called! thread[%ld] exit!\n", tid);
			break;
		}
	}
	pthread_exit(NULL);
}


void threadPoolAdd(ThreadPool* pool, void (*funtion)(void *), void *arg)
{
	pthread_mutex_lock(&pool->mutexPool);
	while(pool->queueSize == pool->queueCapacity && !pool->shutdown)
	{
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}

	if(pool->shutdown)
	{
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}

	// add task
	pool->taskQ[pool->queueTear].funtion = funtion;
	pool->taskQ[pool->queueTear].arg = arg;
	pool->queueTear = (pool->queueTear+1) % pool->queueCapacity;
	pool->queueSize++;

	pthread_cond_signal(&pool->notEmpty);
	pthread_mutex_unlock(&pool->mutexPool);
	
}
int threadPoolLiveNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexPool);
	int liveNum = pool->liveNum;
	pthread_mutex_unlock(&pool->mutexPool);
	return liveNum;
}
int threadPoolBusyNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexBusy);
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexBusy);
	return busyNum;
}
int threadPoolDestroy(ThreadPool* pool)
{
	if(pool == NULL)
	{
		return -1;
	}

	pool->shutdown = 1;
	pthread_join(pool->managerID, NULL);
	for(int i = 0; i < pool->liveNum; i++)
	{
		pthread_cond_signal(&pool->notEmpty);
	}

	if(pool->taskQ)
	{
		free(pool->taskQ);
	}
	if(pool->threadIDs)
	{
		free(pool->threadIDs);
	}
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_mutex_destroy(&pool->mutexPool);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);
	free(pool);
	pool = NULL;
}
