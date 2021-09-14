#include "myDaemonize.h"
#include "myTimer.h"

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <cstring>
#include <csignal>
#include <iostream>

using std::cerr;
using std::endl;

/*
	守护进程的步骤：清空屏蔽字umask，创建子进程，调用setsid生成新的会话，修改当前目录，关闭无用文件描述符，处理sigchld信号
*/
void daemonize()//该函数不仅初始化守护进程，还处理一些信号
{
	rlimit rl;
	int pid;
	struct sigaction sa;
	int fd0, fd1, fd2;

	umask(0);					   //给予创建文件全部权限;
	getrlimit(RLIMIT_NOFILE, &rl); //获取该进程最多能打开的文件数

	if ((pid = fork()) < 0)
	{
		cerr << "first fork() fail ---" << strerror(errno) << endl;
		exit(0);
	}
	else if (pid != 0)
	{
		//this is parent
		exit(-1);
	}

	//this is children
	setsid(); //将复制出来的进程变为新的会话组的领头进程，但是该进程依旧可以重新打开终端，所以再次fork可以使该进程无法打开终端

	//由于关闭会话组的领头进程，会对同一会话组内的其他进程发送sighup信号导致其他进程也关闭,所以这里对sighup信号进行忽略处理
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &sa, nullptr) < 0)
	{
		cerr << "sigaction sighub err ---" << strerror(errno) << endl;
		exit(1);
	}

	if ((pid = fork()) < 0)
	{
		cerr << "second fork() fail ---" << strerror(errno) << endl;
		exit(1);
	}
	else if (pid != 0)
	{
		//this is parent
		exit(0);
	}

	//this is children
	//由于该进程不是会话组组长所以无法打开终端了
	if (chdir("/home/luyongkang/web") < 0)
	{
		cerr << "change directory error ---" << strerror(errno) << endl;
		exit(1);
	}

	if (rl.rlim_max == RLIM_INFINITY)
	{
		rl.rlim_max = 1024;
	}

	for (int i = 0; i < rl.rlim_max; i++)
	{
		close(i);
	}
	fd0 = open("/dev/null", O_RDWR);
	fd1 = open("/home/luyongkang/web/log/outlog.txt", O_CREAT | O_RDWR | O_APPEND, 0777);
	fd2 = open("/home/luyongkang/web/log/errlog.txt", O_CREAT | O_RDWR | O_APPEND, 0777);

	//signal(SIGCHLD, SIG_IGN);
	//sigchld信号为子进程结束时向父进程发送的信号，如果不设置则子进程会变成僵死进程
	if(sigaction(SIGCHLD,&sa,nullptr)<0)
	{
		cerr << getNowTime() << ": sigaction sigchld err ---" << strerror(errno) << endl;
		exit(1);
	}


	//sigpipe信号，会在对端已经close，但是本端依旧发送了消息时产生
	if(sigaction(SIGPIPE,&sa,nullptr)<0)
	{
		cerr << getNowTime() << ": sigaction sigpipe err ---" << strerror(errno) << endl;
		exit(1);
	}

	return;
}

int initServer()
{
	int socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(socketfd==-1)
	{
		cerr << getNowTime() << ": initServer bind err ---" << strerror(errno) << endl;
		exit(1);
	}
	sockaddr_in addr;
	int err;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8888);
	//inet_pton(AF_INET, "192.168.1.100", &addr.sin_addr);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(socketfd, (sockaddr *)&addr, sizeof(addr)) < 0)
	{
		cerr << getNowTime() << ": initServer bind err ---" << strerror(errno) << endl;
		exit(1);
	}

	err = listen(socketfd, 10);
	if (err != 0)
	{
		cerr << getNowTime() << ": initServer listen err ---" << strerror(errno) << endl;
		exit(1);
	}

	return socketfd;
}
