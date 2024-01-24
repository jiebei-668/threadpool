### main.cpp
- c++版的线程池实现的入口函数。线程池最大任务数100，一个管理线程，若干个工作线程，最多有20个工作线程。管理线程可以根据存活工作线程数、忙的工作线程数和任务数量适当增减工作线程数。工作线程执行完后就退出。通过信号15可以一次向任务队列添加5个任务，通过信号2可以终止程序。详见threadpool.cpp threadpool_cpp.h
- Usage: ./maincpp killall -15 maincpp(添加任务） killall -2 maincpp（终止程序）
### threadpool.cpp threadpool_cpp.h
- c++实现的线程池类文件
### notice
- threadpool.cpp还留存bug，在void *ThreadPool::work(void *arg)中如果取出任务task线程跳转到对应函数执行时被取消，应该将task的arg（堆内存）释放，程序并没有做这一点
