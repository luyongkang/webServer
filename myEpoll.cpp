#include "myEpoll.h"
#include "myTimer.h"

#include <cstring>
#include <iostream>

using std::cerr;
using std::endl;

const int LISTENQ = 1024;
epoll_event *events;

int myEpollInit()
{
	int epfd = epoll_create(LISTENQ + 1);
	if (epfd < 0)
	{
		cerr << getNowTime() << ": myEpollInit epoll_create error ---" << strerror(errno) << endl;
		exit(1);
	}

	events = new epoll_event[LISTENQ + 1];

	return epfd;
}

int myEpollAdd(int epfd,int fd,epoll_event* event)
{
	if(epoll_ctl(epfd,EPOLL_CTL_ADD,fd,event)<0)
	{
		cerr << getNowTime() << ": myEpollAdd epoll_ctl error ---" << strerror(errno) << endl;
		exit(1);
	}

	return 0;
}

int myEpollDel(int epfd,int fd)
{
	if(epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr)<0)
	{
		cerr << getNowTime() << ": myEpollDel epoll_ctl error ---" << strerror(errno) << endl;
		exit(1);
	}

	return 0;
}


int myEpollMod(int epfd,int fd,epoll_event* event)
{
	if(epoll_ctl(epfd,EPOLL_CTL_MOD,fd,event)<0)
	{
		cerr << getNowTime() << ": myEpollDel epoll_ctl error ---" << strerror(errno) << endl;
		exit(1);
	}

	return 0;
}


int myEpollWait(int epfd,epoll_event* evnets,int maxEvents,int timeout)
{
	int nfds = epoll_wait(epfd, events, maxEvents, timeout);
	if(nfds<0)
	{
		cerr << getNowTime() << ": myEpollWait epoll_wait error ---" << strerror(errno) << endl;
		exit(1);
	}

	return nfds;
}

void myEpollDestroy()
{
	delete[] events;
	return;
}