#include "myPthreadPool.h"
#include "myTimer.h"

#include <iostream>
#include <cstring>

using std::cerr;
using std::endl;

const int THREADPOOL_INVALID = -1;
const int THREADPOOL_LOCK_FAILURE = -2;
const int THREADPOOL_QUEUE_FULL = -3;
const int THREADPOOL_SHUTDOWN = -4;
const int THREADPOOL_THREAD_FAILURE = -5;

const int MAX_THREADS = 1024;
const int MAX_QUEUES = 65535;

threadpool_t *myThreadpoolInit(int threadNum, int queueSize)
{
	threadpool_t *pool;

	if (threadNum <= 0 || threadNum > MAX_THREADS)
	{
		cerr << getNowTime() << ": myThreadpoolInit error ---threadNum error" << endl;
		exit(1);
	}

	if (queueSize <= 0 || queueSize > MAX_QUEUES)
	{
		cerr << getNowTime() << ": myThreadpoolInit error ---queueSize error" << endl;
		exit(1);
	}

	do
	{
		pool = new threadpool_t();
		if (pool == nullptr)
		{
			break;
		}

		//初始化pool
		pool->count = pool->head = pool->tail = 0;
		pool->queueSize = queueSize;
		pool->threadNum = threadNum;
		pool->shutdown = pool->started = 0;

		//分配
		pool->threads = new pthread_t[threadNum];
		pool->queue = new threadpool_task_t[queueSize];

		if (pool->threads == nullptr || pool->queue == nullptr)
		{
			break;
		}

		//初始化锁
		if (pthread_cond_init(&pool->notify, nullptr) != 0 || pthread_mutex_init(&pool->lock, nullptr) != 0)
		{
			break;
		}
		//创建工作线程
		for (int i = 0; i < threadNum; i++)
		{
			if (pthread_create(&pool->threads[i], nullptr, threadMain, static_cast<void *>(pool)) < 0)
			{
				cerr << getNowTime() << ": myThreadpoolInit pthread_create error ---" << strerror(errno) << endl;
				myThreadpoolDes(pool, 0);
				return nullptr;
			}
			pool->started++;
		}

		return pool;
	} while (false);

	if (pool != nullptr)
	{
		myThreadpoolFree(pool);
	}

	return nullptr;
}

int myThreadpoolAdd(threadpool_t *pool, void (*function)(void *), void *arg)
{
	int err = 0;
	if (pool == nullptr || function == nullptr)
	{
		return THREADPOOL_INVALID;
	}

	if (pthread_mutex_lock(&pool->lock) != 0)
	{
		return THREADPOOL_LOCK_FAILURE;
	}
	do
	{
		if (pool->queueSize == pool->count)
		{
			err = THREADPOOL_QUEUE_FULL;
			break;
		}

		if (pool->shutdown != 0)
		{
			err = THREADPOOL_SHUTDOWN;
			break;
		}

		pool->queue[pool->tail].function = function;
		pool->queue[pool->tail].argument = arg;
		pool->tail = (pool->tail + 1) % pool->queueSize;
		pool->count++;

		if (pthread_cond_signal(&pool->notify) != 0)
		{
			err = THREADPOOL_LOCK_FAILURE;
			break;
		}
	} while (false);

	if (pthread_mutex_unlock(&pool->lock) != 0)
	{
		err = THREADPOOL_LOCK_FAILURE;
	}

	return err;
}

int myThreadpoolDes(threadpool_t *pool, int flag)
{
	if (pool == nullptr)
	{
		return 0;
	}

	if (pthread_mutex_lock(&pool->lock) != 0)
	{
		return THREADPOOL_LOCK_FAILURE;
	}

	if (pool->shutdown != 0)
	{
		return THREADPOOL_LOCK_FAILURE;
	}

	pool->shutdown = flag ? gracefulShutdown : immediateShutdown;

	if (pthread_cond_broadcast(&pool->notify) != 0)
	{
		return THREADPOOL_LOCK_FAILURE;
	}

	if (pthread_mutex_unlock(&pool->lock) != 0)
	{
		return THREADPOOL_LOCK_FAILURE;
	}

	for (int i = 0; i < pool->threadNum; i++)
	{
		if (pthread_join(pool->threads[i], nullptr) != 0)
		{
			return THREADPOOL_THREAD_FAILURE;
		}
	}

	myThreadpoolFree(pool);

	return 0;
}

int myThreadpoolFree(threadpool_t *pool)
{
	if (pool == nullptr)
		return 0;
	if (pool->started != 0)
		return -1;
	pthread_mutex_destroy(&pool->lock);
	pthread_cond_destroy(&pool->notify);

	if (pool->threads != nullptr)
	{
		delete[] pool->threads;
	}

	if (pool->queue != nullptr)
	{
		delete[] pool->queue;
	}

	delete pool;

	return 0;
}

void *threadMain(void *arg)
{
	threadpool_t *pool = static_cast<threadpool_t *>(arg);
	threadpool_task_t task;

	while (true)
	{
		pthread_mutex_lock(&pool->lock);

		while (pool->count == 0 && pool->shutdown == 0)
		{
			pthread_cond_wait(&pool->notify, &pool->lock);
		}

		if (pool->shutdown == immediateShutdown || (pool->shutdown == gracefulShutdown && pool->count == 0))
		{
			break;
		}

		task.function = pool->queue[pool->head].function;
		task.argument = pool->queue[pool->head].argument;
		pool->head = (pool->head + 1) % pool->queueSize;
		pool->count--;
		pthread_mutex_unlock(&pool->lock);

		(*(task.function))(task.argument);
	}

	--pool->started;
	pthread_mutex_unlock(&pool->lock);
	pthread_exit(nullptr);
	return nullptr;
}