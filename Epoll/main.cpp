#include <cstdio>
#include <sys/resource.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define MAX_POLL_SIZE 10
#define MAX_MSG_LEN 5
#define LISTEN_SIZE 1000
#define SERVER_PORT 6000

int handleMsg(int conFd)
{
	char nRead = 0;
	char buff[MAX_MSG_LEN] = { 0 };
	nRead = read(conFd, buff, MAX_MSG_LEN);
	if (nRead == 0)
	{
		printf("客户端关闭连接\n");
		close(conFd);
		return -1;
	}

	if (nRead < 0)
	{
		perror("read error");
		close(conFd);
		return -1;
	}
	
	write(conFd, buff, nRead);//响应客户端 
	return 0;
}

int setNonblocking(int socktFd)
{
	if (fcntl(socktFd, F_SETFL, fcntl(socktFd, F_GETFD, 0) | O_NONBLOCK) == -1) {
		return -1;
	}
	return 0;
}

int doEpollService()
{
	//设置每个进程打开的最大文件数
	rlimit rt;
	rt.rlim_cur = rt.rlim_max = MAX_POLL_SIZE;
	if (setrlimit(RLIMIT_NOFILE, &rt) == -1)
	{
		perror("setrlimit error");
		return -1;
	}

	// 创建监听socket
	int listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == -1) 
	{
		perror("can't create socket file");
		return -1;
	}

	// 设置端口复用，有以下作用：
	/*
	O_REUSEADDR允许启动一个监听服务器并捆绑其众所周知端口，即使以前建立的将此端口用做他们的本地端口的连接仍存在。这通常是重启监听服务器时出现，
	若不设置此选项，则bind时将出错。

	SO_REUSEADDR允许在同一端口上启动同一服务器的多个实例，只要每个实例捆绑一个不同的本地IP地址即可。对于TCP，
	我们根本不可能启动捆绑相同IP地址和相同端口号的多个服务器。

	SO_REUSEADDR允许单个进程捆绑同一端口到多个套接口上，只要每个捆绑指定不同的本地IP地址即可。这一般不用于TCP服务器。

	SO_REUSEADDR允许完全重复的捆绑：当一个IP地址和端口绑定到某个套接口上时，还允许此IP地址和端口捆绑到另一个套接口上。
	一般来说，这个特性仅在支持多播的系统上才有，而且只对UDP套接口而言（TCP不支持多播）。
	*/
	int opt = 1;
	setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// 设定端口非阻塞，要将连接事件仍如epoll模型中
	if (setNonblocking(listenSock) < 0)
	{
		perror("set nonblocking failed");
		return -1;
	}
	// 绑定监听端口
	sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(SERVER_PORT);
	if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == -1)
	{
		perror("bind failed");
		return -1;
	}

	// 开启监听
	if (listen(listenSock, LISTEN_SIZE) == -1)
	{
		perror("listen failed");
		return -1;
	}

	// 进入epoll设置
	// 创建epoll句柄
	int epollFd = epoll_create(MAX_POLL_SIZE);
	// 把监听句柄加入，进入事件循环
	epoll_event ev = { 0 };
	ev.data.fd = listenSock;
	ev.events = EPOLLIN | EPOLLET; // 注意此处的ET和LT模式的区别
	if (epoll_ctl(epollFd, EPOLL_CTL_ADD, listenSock, &ev) < 0)
	{
		fprintf(stderr, "epoll set insertion error: fd=%d\n", listenSock);
		return -1;
	}
	// 记录此时要监听的事件个数
	int curEventsCounts = 1;
	printf("epoll事件开启，监听端口：%d, 最大连接数：%d, 最大监听队列：%d", SERVER_PORT, MAX_POLL_SIZE, LISTEN_SIZE);

	epoll_event events[MAX_POLL_SIZE];
	sockaddr_in client;
	socklen_t clentLen = sizeof(client);
	for (;;)
	{
		// 等待事件发生
		int connFd = epoll_wait(epollFd, events, curEventsCounts, -1);
		if (connFd == -1)
		{
			continue;
		}
		// 处理所有的事件
		for (int i=0; i<connFd; i++)
		{
			if (events[i].data.fd == listenSock)
			{
				// 是一个监听事件
				int connFd = accept(listenSock, (sockaddr*)&client, &clentLen);
				if (connFd == -1)
				{
					continue;
				}

				if (curEventsCounts >= MAX_POLL_SIZE) {
					fprintf(stderr, "too many connection, more than %d\n", MAX_POLL_SIZE);
					close(connFd);
					continue;
				}

				if (setNonblocking(connFd) == -1)
				{
					close(connFd);
					continue;
				}
				ev.data.fd = connFd;
				ev.events = EPOLLIN ; // 注意此处的ET和LT模式的区别
				if (epoll_ctl(epollFd, EPOLL_CTL_ADD, connFd, &ev) < 0)
				{
					fprintf(stderr, "add socket '%d' to epoll failed: %s\n", connFd, strerror(errno));
					return -1;
				}

				curEventsCounts++;

				continue;
			}

			// 处理客户端请求，为读事件
			if (handleMsg(events[i].data.fd) == -1)
			{
				epoll_ctl(epollFd, EPOLL_CTL_DEL, events[i].data.fd, &ev);
				curEventsCounts--;
			}
		}

	}
	return 0;
}


int main()
{
    return doEpollService();
}