#include <algorithm>
#include <deque>
#include <vector>
#include <pthread.h>
using namespace std;

class TaskQueue
{
public:
	struct Task
	{
		void (*funtion)(void*);
		void* arg;
	};
	deque<struct Task> dq;
	int maxTaskQueueNum = 100;
	// 获取maxTaskQueueNum的值
	int getMaxTaskQueueNum();
	// 获取当前队列中任务的数量
	int getTaskQueueNum();
	void insertTask(void (*funtion)(void *), void *arg);
	struct Task getTask();
	TaskQueue();
	~TaskQueue();
};

class ThreadPool
{
public:
	// 任务队列
	TaskQueue taskQ;
	// 最多有一个管理者线程和maxWorkThreadNum个工作线程
	int maxWorkThreadNum = 20;
	// 最少有一个管理者线程和minWorkThreadNum个工作线程
	int minWorkThreadNum = 2;
	// 大小为 maxWorkThreadNum+1，最多存放1个管理者线程和maxWorkThreadNum个工作线程
	vector<pthread_t> threadIDs;
	// 所有活着的工作线程，包括忙的工作线程和阻塞等待任务的工作线程
	int liveNum = 0;
	// 忙的工作线程
	int busyNum = 0;
	// 当空闲的工作线程过多时（大于6），将会杀死exitNum个空闲工作线程
	int exitNum = 0;
	// 管理者每次增加工作线程或是杀死工作线程时的数量
	int changeNum = 2;
	pthread_mutex_t mutex;
	pthread_cond_t notFull;
	pthread_cond_t notEmpty;
	// 当线程池要销毁时，置为true
	bool shutdown = true;

	// 向任务队列插入任务
	// @param: 任务函数funtion 任务函数的参数arg
	void insertTask(void (*funtion)(void *), void *arg);
	// 初始化线程池，启动一个管理者线程和若干个工作线程
	// @param: 初始工作线程数量
	void init(int init_work_thread_num);
	// 管理者线程函数
	// 每5s管理一次，如果shutdown==true就退出
	// 当没有空闲工作线程busynum==livenum且任务数量大于0时，增加changeNum个新工作线程pool
	// 当空闲工作线程超过6 busyNum+6 <= liveNum-exitNum，杀死changeNum个工作线程
	// @param: 强转为void *类型的ThreadPool *类型的参数，指向线程池对象
	static void* manage(void*);
	// 管理者线程退出时的清理函数
	// 释放mutex，防止取消有等待条件变量的线程时无法退出
	// 需要对任务队列中的堆内存进行释放
	static void manager_cleanup(void *arg);
	// 工作者线程函数
	// 首先等待有任务或线程退出的通知
	static void* work(void*);
	// 工作线程的线程清理函数
	// 如果线程在等待条件变量时被取消（pthread_cancel），会有bug，互斥锁还不了
	// 需要在线程清理函数完成这个释放互斥锁的工作，并在使用条件变量的函数中注册线程清理函数
	// 释放mutex，防止取消有等待条件变量的线程时无法退出
	// 注：应该将工作线程函数的参数释放掉，怎么实现？
	// @param 强转为void *类型的ThreadPool *类型参数
	static void worker_cleanup(void *);
	// 线程退出时将自己的线程id从threadIDs中去除（清零）
	void threadExit();
	ThreadPool();
	// 取消所有的管理线程和工作线程，销毁所有锁和条件变量
	~ThreadPool();
	// 调用析构函数
	void destoryThreadPool();
};
