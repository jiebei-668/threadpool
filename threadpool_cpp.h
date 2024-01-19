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
	TaskQueue taskQ;
	void insertTask(void (*funtion)(void *), void *arg);

	// 最多有一个管理者线程和maxThreadNum个线程，实际上应写作 maxLiveThreadNum
	int maxThreadNum = 20;
	// 最少有一个管理者线程和minThreadNum个线程
	int minThreadNum = 2;
	// 大小为 maxThreadNum+1
	vector<pthread_t> threadIDs;
	// 所有活着的线程，包括忙的线程和阻塞等待任务的线程
	int liveNum;
	// 忙的线程
	int busyNum;
	// 当空闲的线程过多时（大于6），将会杀死exitNum个空闲线程
	int exitNum;
	// 管理者每次增加线程或是杀死线程时的数量
	int changeNum = 2;
	pthread_mutex_t mutex;
	pthread_cond_t notFull;
	pthread_cond_t notEmpty;
	// 当线程池要销毁时，置为true
	bool shutdown;
	void init(int init_thread_num);
	static void* manage(void*);
	static void* work(void*);
	void threadExit();
	ThreadPool();
	~ThreadPool();
	void destoryThreadPool();
};
