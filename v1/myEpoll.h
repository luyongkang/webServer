#ifndef MYEPOLL_H
#define MYEPOLL_H

#include <sys/epoll.h>

extern const int LISTENQ;
extern epoll_event *events;

//初始化epoll
int myEpollInit();
//epoll添加监听对象
int myEpollAdd(int epfd, int fd, epoll_event *event);
//epoll删除监听对象
int myEpollDel(int epfd, int fd);
//epoll修改监听对象
int myEpollMod(int epfd, int fd, epoll_event *event);

//epoll等待监听对象
int myEpollWait(int epfd, epoll_event *events, int maxEvents, int timeout);

//释放events
void myEpollDestroy();
#endif