
#ifndef _THREADPOOL_H
#define _THREADPOOL_H
typedef struct ThreadPool ThreadPool;
// 创建线程池并初始化
ThreadPool *threadPoolCreate(int min, int max, int queueSize);
// 给线程池添加任务
void threadPoolAdd(ThreadPool* pool, void (*funtion)(void *), void *arg);
// 获取线程池中活着的线程数量
int threadPoolLiveNum(ThreadPool* pool);
// 获取线程池中工作的线程数量
int threadPoolBusyNum(ThreadPool* pool);
// 销毁线程池
int threadPoolDestroy(ThreadPool* pool);

void* worker(void *arg);
void* manager(void *arg);
void threadExit(ThreadPool *pool);

#endif
