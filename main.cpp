#include "myDaemonize.h"
#include "myTimer.h"
#include "myEpoll.h"
#include "myUtil.h"
#include "myPthreadPool.h"
#include "myHttpRequest.h"

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <arpa/inet.h>

using namespace std;

const int THREADPOOL_THREAD_NUM = 4;
const int QUEUE_SIZE = 65535;
const string PATH = "/";

int main()
{
	int listenfd;			  //监听socket
	int sockfd;				  //连接socket
	int epfd;				  //epoll描述符
	threadpool_t *threadPool; //线程池对象
	epoll_event event;		  //epoll监听事件
	int nfds;				  //epoll监听返回的数量
	httpRequest *req;
	//首先先将该进程初始化为守护进程
	daemonize();

	listenfd = initServer();
	if (setSockNonBlock(listenfd) != 0)
	{
		cerr << getNowTime() << ": setSockNonBlock err" << endl;
	}

	epfd = myEpollInit();

	threadPool = myThreadpoolInit(THREADPOOL_THREAD_NUM, QUEUE_SIZE);

	//写入listenfd的epoll_event
	req = new httpRequest();
	req->setFd(listenfd);
	event.events = EPOLLIN | EPOLLET;
	event.data.ptr = req;

	myEpollAdd(epfd, listenfd, &event);

	while (true)
	{
		nfds = myEpollWait(epfd, events, LISTENQ + 1, -1);
		if (nfds == 0)
		{
			continue;
		}

		//nfds!=0
		handleEvents(epfd, listenfd, events, nfds, PATH, threadPool);
		//剔除超时的连接
		handleTimeOut();
	}
	myEpollDestroy();
	return 0;
}
