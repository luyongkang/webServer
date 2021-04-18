#include "myTimer.h"

#include <string>
#include <sstream>
#include <queue>
#include <sys/time.h>
#include <time.h>

using std::priority_queue;
using std::queue;
using std::string;
using std::stringstream;

priority_queue<mytimer *, queue<mytimer *>, timerCmp> myTimerQueue;
pthread_mutex_t myTimerLock = PTHREAD_MUTEX_INITIALIZER;
const int TIME_TIMER_OUT = 500;

//返回当前时间函数
string getNowTime()
{
	time_t now = time(nullptr);
	tm *t = localtime(&now);

	stringstream ss;
	ss << t->tm_year + 1900 << "-" << t->tm_mon + 1 << "-" << t->tm_mday << " "
	   << t->tm_hour << ":" << t->tm_min << ":" << t->tm_sec;
	return ss.str();
}

mytimer::mytimer(httpRequest *_p, int timeout) : phttpRequest(_p), deleted(false)
{
	timeval nowtime;
	gettimeofday(&nowtime, nullptr);
	//转换为毫秒
	expiredTime = nowtime.tv_sec * 1000 + nowtime.tv_usec / 1000 + timeout;
}

mytimer::~mytimer()
{
	if(phttpRequest!=nullptr)
	{
		delete phttpRequest;
		phttpRequest = nullptr;
	}
}

void mytimer::update(int timeout)
{
	struct timeval now;
    gettimeofday(&now, NULL);
    expiredTime = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) + timeout;
}

bool mytimer::isvalid()
{
	struct timeval now;
    gettimeofday(&now, NULL);
    size_t temp = ((now.tv_sec * 1000) + (now.tv_usec / 1000));
    if (temp < expiredTime)
    {
        return true;
    }
    else
    {
        this->setDeleted();
        return false;
    }
}

size_t mytimer::getExpTime() const
{
	return expiredTime;
}

void mytimer::clearReq()
{
	phttpRequest = nullptr;
	setDeleted();
}

void mytimer::setDeleted()
{
	deleted = true;
}

bool mytimer::isDeleted() const
{
	return deleted;
}

bool timerCmp::operator()(const mytimer *a, const mytimer *b) const
{
	return a->getExpTime() > b->getExpTime();
}