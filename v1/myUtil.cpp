#include "myUtil.h"
#include "myHttpRequest.h"
#include "myEpoll.h"
#include "myTimer.h"
#include "myPthreadPool.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

using std::string;

int setSockNonBlock(int fd)
{
	int flag = fcntl(fd, F_GETFL);
	if (flag == -1)
		return -1;
	flag = flag | O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flag) == -1)
	{
		return -1;
	}

	return 0;
}

void handleEvents(int epollfd, int listenfd, epoll_event *events, int eventNum, const string &path, threadpool_t *p)
{
	for (int i = 0; i < eventNum; i++)
	{
		httpRequest *request = static_cast<httpRequest *>(events[i].data.ptr);
		int fd = request->getFd(); //获取当前产生事件的文件描述符

		//如果发生事件的文件描述符是监听描述符,说明有新的连接
		if (fd == listenfd)
		{
			acceptConnection(listenfd, epollfd, path);
		}
		else
		{
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || !(events[i].events & EPOLLIN)) //发生了错误事件
			{
				delete request;
				continue;
			}

			//没发生错误事件，可以读出数据
			//加入线程池之前先将timer与request分开
			request->seperateTimer();
			myThreadpoolAdd(p, myHandler, events[i].data.ptr);
		}
	}
}

void acceptConnection(int listenfd, int epollfd, const string &path)
{
	int acceptfd = 0;
	while ((acceptfd = accept(listenfd, nullptr, nullptr)) < 0)
	{
		//将刚刚连接的socket设置为非阻塞
		if (setSockNonBlock(acceptfd) < 0)
		{
			return;
		}
		//初始化监听acceptfd的epoll_event
		epoll_event event;
		event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
		httpRequest *req = new httpRequest(epollfd, acceptfd, path);
		event.data.ptr = static_cast<void *>(req);
		//添加至epoll的监听列表中
		myEpollAdd(epollfd, acceptfd, &event);

		mytimer *ptime = new mytimer(req, TIME_TIMER_OUT);
		req->addTimer(ptime);
		pthread_mutex_lock(&myTimerLock);
		myTimerQueue.push(ptime);
		pthread_mutex_unlock(&myTimerLock);
	}
}

void myHandler(void *arg)
{
	httpRequest *request = static_cast<httpRequest *>(arg);
	request->handleRequest();
}

ssize_t readn(int fd, char *buf, size_t maxSize)
{
	int nleft = maxSize;
	int readn = 0;
	int sum = 0;

	while(nleft>0)
	{
		if((readn=read(fd,buf+sum,nleft))<0)
		{
			if(errno==EINTR)
			{
				readn = 0;
			}
			else if(errno==EAGAIN)
			{
				return sum;
			}
			else
			{
				return -1;
			}
		}
		else if(readn ==0)
		{
			break;
		}
		else
		{
			sum = sum + readn;
			nleft = nleft - readn;
		}
	}

	return sum;
}

ssize_t writen(int fd,char * buf,size_t maxSize )
{
	int nleft = maxSize;
	int writen = 0;
	int sum = 0;
	while(nleft>0)
	{
		if((writen=write(fd,buf+sum,nleft))<=0)
		{
			if(writen<0)
			{
				if(errno==EINTR||errno==EAGAIN)
				{
					writen = 0;
					continue;
				}
				else
				{
					return -1;
				}
			}
			else
			{
				nleft = nleft - writen;
				sum = sum + writen;
			}
		}
	}

	return sum;
}

void handleTimeOut()
{
    pthread_mutex_lock(&myTimerLock);
    while (!myTimerQueue.empty())
    {
        mytimer *ptimer_now = myTimerQueue.top();
        if (ptimer_now->isDeleted())
        {
            myTimerQueue.pop();
            delete ptimer_now;
        }
        else if (ptimer_now->isvalid() == false)
        {
            myTimerQueue.pop();
            delete ptimer_now;
        }
        else
        {
            break;
        }
    }
    pthread_mutex_unlock(&myTimerLock);
}