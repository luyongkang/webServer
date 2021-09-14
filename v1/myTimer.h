#ifndef MYTIMER_H
#define MYTIMER_H

#include <string>
#include <queue>
#include <pthread.h>

class mytimer;
class timerCmp;
class httpRequest;

extern std::priority_queue<mytimer *, std::deque<mytimer *>, timerCmp> myTimerQueue;
extern pthread_mutex_t myTimerLock;
extern const int TIME_TIMER_OUT;

//获取当前时间函数
std::string getNowTime();

class mytimer
{
public:
	bool deleted;
	size_t expiredTime;
	httpRequest *phttpRequest;

	mytimer(httpRequest *_p, int timeout);
	~mytimer();
	void update(int timeout);
	bool isvalid();
	void clearReq();
	void setDeleted();
	bool isDeleted() const;
	size_t getExpTime() const;
};

class timerCmp
{
public:
	bool operator()(const mytimer *a, const mytimer *b) const;
};

#endif