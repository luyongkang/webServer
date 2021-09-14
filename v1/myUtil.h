#ifndef MYUTIL_H
#define MYUTIL_H

#include "myPthreadPool.h"

#include<sys/epoll.h>

#include<string>

void handleTimeOut();

int setSockNonBlock(int fd);

void handleEvents(int epollfd, int listenfd, epoll_event *events, int eventNum, const std::string &path, threadpool_t *p);

void acceptConnection(int listenfd, int epollfd, const std::string &PATH);

void myHandler(void *arg);

ssize_t readn(int fd, char *buf, size_t maxSize);

ssize_t writen(int fd, char *buf, size_t maxSize);
#endif