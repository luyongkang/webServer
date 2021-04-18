#ifndef MYPTHREADPOOL_H
#define MYPTHREADPOOL_H

#include <pthread.h>

typedef enum
{
	immediateShutdown = 1,
	gracefulShutdown = 2
} threadpool_shutdown_t;

typedef struct
{
	void (*function)(void *);
	void *argument;
} threadpool_task_t;


struct threadpool_t
{
	pthread_mutex_t lock;
	pthread_cond_t notify;
	pthread_t *threads;//线程队列
	threadpool_task_t *queue;//任务队列
	int threadNum; //线程数，即线程队列大小
	int queueSize; //任务队列大小
	int head;//当前需要处理的任务的index
	int tail;
	int count;//目前有的任务数
	int shutdown;//关闭命令，0为不关闭，immediateShutdown为立即关闭，gracefulShutdown为完成已有任务后关闭
	int started; //已经启动的线程数
};

//初始化线程池
threadpool_t *myThreadpoolInit(int threadNum, int queueSize);
//向线程池中添加任务
int myThreadpoolAdd(threadpool_t *pool, void (*function)(void *), void *argument);
//该函数关闭所有线程,并清除内存空间,flag为1则完成当前所有任务后再关闭线程，flag为0则立即关闭所有线程
int myThreadpoolDes(threadpool_t *pool, int flag);
//该函数释放线程池对象的内存空间
int myThreadpoolFree(threadpool_t *pool);
//线程主函数
void *threadMain(void *threadpool);

#endif