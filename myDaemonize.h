#ifndef MYDEAMONIZE_H
#define MYDEAMONIZE_H

//初始化为守护进程
void daemonize(); 
//初始化服务器，返回监听socket
int initServer();

#endif