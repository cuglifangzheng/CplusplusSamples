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
		printf("�ͻ��˹ر�����\n");
		close(conFd);
		return -1;
	}

	if (nRead < 0)
	{
		perror("read error");
		close(conFd);
		return -1;
	}
	
	write(conFd, buff, nRead);//��Ӧ�ͻ��� 
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
	//����ÿ�����̴򿪵�����ļ���
	rlimit rt;
	rt.rlim_cur = rt.rlim_max = MAX_POLL_SIZE;
	if (setrlimit(RLIMIT_NOFILE, &rt) == -1)
	{
		perror("setrlimit error");
		return -1;
	}

	// ��������socket
	int listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == -1) 
	{
		perror("can't create socket file");
		return -1;
	}

	// ���ö˿ڸ��ã����������ã�
	/*
	O_REUSEADDR��������һ��������������������������֪�˿ڣ���ʹ��ǰ�����Ľ��˶˿��������ǵı��ض˿ڵ������Դ��ڡ���ͨ������������������ʱ���֣�
	�������ô�ѡ���bindʱ������

	SO_REUSEADDR������ͬһ�˿�������ͬһ�������Ķ��ʵ����ֻҪÿ��ʵ������һ����ͬ�ı���IP��ַ���ɡ�����TCP��
	���Ǹ�������������������ͬIP��ַ����ͬ�˿ںŵĶ����������

	SO_REUSEADDR��������������ͬһ�˿ڵ�����׽ӿ��ϣ�ֻҪÿ������ָ����ͬ�ı���IP��ַ���ɡ���һ�㲻����TCP��������

	SO_REUSEADDR������ȫ�ظ������󣺵�һ��IP��ַ�Ͷ˿ڰ󶨵�ĳ���׽ӿ���ʱ���������IP��ַ�Ͷ˿�������һ���׽ӿ��ϡ�
	һ����˵��������Խ���֧�ֶಥ��ϵͳ�ϲ��У�����ֻ��UDP�׽ӿڶ��ԣ�TCP��֧�ֶಥ����
	*/
	int opt = 1;
	setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// �趨�˿ڷ�������Ҫ�������¼�����epollģ����
	if (setNonblocking(listenSock) < 0)
	{
		perror("set nonblocking failed");
		return -1;
	}
	// �󶨼����˿�
	sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(SERVER_PORT);
	if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == -1)
	{
		perror("bind failed");
		return -1;
	}

	// ��������
	if (listen(listenSock, LISTEN_SIZE) == -1)
	{
		perror("listen failed");
		return -1;
	}

	// ����epoll����
	// ����epoll���
	int epollFd = epoll_create(MAX_POLL_SIZE);
	// �Ѽ���������룬�����¼�ѭ��
	epoll_event ev = { 0 };
	ev.data.fd = listenSock;
	ev.events = EPOLLIN | EPOLLET; // ע��˴���ET��LTģʽ������
	if (epoll_ctl(epollFd, EPOLL_CTL_ADD, listenSock, &ev) < 0)
	{
		fprintf(stderr, "epoll set insertion error: fd=%d\n", listenSock);
		return -1;
	}
	// ��¼��ʱҪ�������¼�����
	int curEventsCounts = 1;
	printf("epoll�¼������������˿ڣ�%d, �����������%d, ���������У�%d", SERVER_PORT, MAX_POLL_SIZE, LISTEN_SIZE);

	epoll_event events[MAX_POLL_SIZE];
	sockaddr_in client;
	socklen_t clentLen = sizeof(client);
	for (;;)
	{
		// �ȴ��¼�����
		int connFd = epoll_wait(epollFd, events, curEventsCounts, -1);
		if (connFd == -1)
		{
			continue;
		}
		// �������е��¼�
		for (int i=0; i<connFd; i++)
		{
			if (events[i].data.fd == listenSock)
			{
				// ��һ�������¼�
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
				ev.events = EPOLLIN ; // ע��˴���ET��LTģʽ������
				if (epoll_ctl(epollFd, EPOLL_CTL_ADD, connFd, &ev) < 0)
				{
					fprintf(stderr, "add socket '%d' to epoll failed: %s\n", connFd, strerror(errno));
					return -1;
				}

				curEventsCounts++;

				continue;
			}

			// ����ͻ�������Ϊ���¼�
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