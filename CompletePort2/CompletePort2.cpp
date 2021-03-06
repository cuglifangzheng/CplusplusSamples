// CompletePort2.cpp: 定义控制台应用程序的入口点。
//
#include "stdafx.h"

#include <iostream>
#include <WinSock2.h>
#include <windows.h>
using namespace std;

#pragma comment( lib, "Ws2_32.lib" )

#define DefaultPort 8888
#define DataBuffSize 5

typedef struct
{
	OVERLAPPED overlapped;
	WSABUF databuff;
	CHAR buffer[DataBuffSize];
	DWORD bytesSend;
	DWORD bytesRecv;
}PER_IO_OPERATEION_DATA, *LPPER_IO_OPERATION_DATA;

typedef struct
{
	SOCKET socket;
}PER_HANDLE_DATA, *LPPER_HANDLE_DATA;




DWORD WINAPI ServerWorkThread(LPVOID CompletionPortID);

void main()
{
	SOCKET acceptSocket;
	HANDLE completionPort;
	LPPER_HANDLE_DATA pHandleData;
	LPPER_IO_OPERATION_DATA pIoData;
	DWORD recvBytes;
	DWORD flags;

	WSADATA wsaData;
	DWORD ret;
	if (ret = WSAStartup(0x0202, &wsaData) != 0)
	{
		std::cout << "WSAStartup failed. Error:" << ret << std::endl;
		return;
	}

	completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (completionPort == NULL)
	{
		std::cout << "CreateIoCompletionPort failed. Error:" << GetLastError() << std::endl;
		return;
	}

	SYSTEM_INFO mySysInfo;
	GetSystemInfo(&mySysInfo);

	// 创建 2 * CPU核数 + 1 个线程
	DWORD threadID;
	for (DWORD i = 0; i < (mySysInfo.dwNumberOfProcessors * 2 + 1); ++i)
	{
		HANDLE threadHandle;
		threadHandle = CreateThread(NULL, 0, ServerWorkThread, completionPort, 0, &threadID);
		if (threadHandle == NULL)
		{
			std::cout << "CreateThread failed. Error:" << GetLastError() << std::endl;
			return;
		}

		CloseHandle(threadHandle);
	}

	// 启动一个监听socket
	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET)
	{
		std::cout << " WSASocket( listenSocket ) failed. Error:" << GetLastError() << std::endl;
		return;
	}

	SOCKADDR_IN internetAddr;
	internetAddr.sin_family = AF_INET;
	internetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	internetAddr.sin_port = htons(DefaultPort);

	// 绑定监听端口
	if (bind(listenSocket, (PSOCKADDR)&internetAddr, sizeof(internetAddr)) == SOCKET_ERROR)
	{
		std::cout << "Bind failed. Error:" << GetLastError() << std::endl;
		return;
	}

	if (listen(listenSocket, 5) == SOCKET_ERROR)
	{
		std::cout << "listen failed. Error:" << GetLastError() << std::endl;
		return;
	}

	// 开始死循环，处理数据
	while (1)
	{
		acceptSocket = WSAAccept(listenSocket, NULL, NULL, NULL, 0);
		if (acceptSocket == SOCKET_ERROR)
		{
			printf("WSAAccept failed. Error:%d", GetLastError());
			return;
		}

		pHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA));
		if (pHandleData == NULL)
		{
			printf("GlobalAlloc( HandleData ) failed. Error:%d", GetLastError());
			return;
		}

		pHandleData->socket = acceptSocket;
		if (CreateIoCompletionPort((HANDLE)acceptSocket, completionPort, (ULONG_PTR)pHandleData, 0) == NULL)
		{
			printf("CreateIoCompletionPort failed. Error:%d", GetLastError());
			return;
		}

		pIoData = (LPPER_IO_OPERATION_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_OPERATEION_DATA));
		if (pIoData == NULL)
		{
			printf("GlobalAlloc( IoData ) failed. Error:%d", GetLastError());
			return;
		}

		ZeroMemory(&(pIoData->overlapped), sizeof(pIoData->overlapped));
		pIoData->bytesSend = 0;
		pIoData->bytesRecv = 0;
		pIoData->databuff.len = DataBuffSize;
		pIoData->databuff.buf = pIoData->buffer;
		memset(pIoData->buffer, 0, DataBuffSize);

		flags = 0;
		if (WSARecv(acceptSocket, &(pIoData->databuff), 1, &recvBytes, &flags, &(pIoData->overlapped), NULL) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != ERROR_IO_PENDING)
			{
				printf("WSARecv() failed. Error: %d", GetLastError());
			}
			else
			{
				printf("WSARecv() io pending\n");
			}
		}
	}
}

DWORD WINAPI ServerWorkThread(LPVOID CompletionPortID)
{
	HANDLE complationPort = (HANDLE)CompletionPortID;
	DWORD bytesTransferred;
	LPPER_HANDLE_DATA pHandleData = NULL;
	LPPER_IO_OPERATION_DATA pIoData = NULL;
	DWORD sendBytes = 0;
	DWORD recvBytes = 0;
	DWORD flags;

	while (1)
	{
		if (GetQueuedCompletionStatus(complationPort, &bytesTransferred, (PULONG_PTR)&pHandleData, (LPOVERLAPPED *)&pIoData, INFINITE) == 0)
		{
			printf("GetQueuedCompletionStatus failed. Error:%d", GetLastError());
			return 0;
		}

		// 检查数据是否已经传输完了
		if (bytesTransferred == 0)
		{
			std::cout << " Start closing socket..." << std::endl;
			if (CloseHandle((HANDLE)pHandleData->socket) == SOCKET_ERROR)
			{
				printf("Close socket failed. Error:%d\n", GetLastError());
			}

			GlobalFree(pHandleData);
			GlobalFree(pIoData);
			continue;
		}

		// 检查管道里是否有数据
		if (pIoData->bytesRecv == 0)
		{
			printf("读事件\n");
			pIoData->bytesRecv = bytesTransferred;
			pIoData->bytesSend = 0;
		}
		else
		{
			printf("写事件\n");
			pIoData->bytesSend += bytesTransferred;
		}

		// 数据没有发完，继续发送
		if (pIoData->bytesRecv > pIoData->bytesSend)
		{
			ZeroMemory(&(pIoData->overlapped), sizeof(OVERLAPPED));
			pIoData->databuff.buf = pIoData->buffer + pIoData->bytesSend;
			pIoData->databuff.len = pIoData->bytesRecv - pIoData->bytesSend;

			// 发送数据出去
			if (WSASend(pHandleData->socket, &(pIoData->databuff), 1, &sendBytes, 0, &(pIoData->overlapped), NULL) == SOCKET_ERROR)
			{
				if (WSAGetLastError() != ERROR_IO_PENDING)
				{
					printf("WSASend() failed. Error:%d\n", GetLastError());
					continue;
				}
				else
				{
					printf("WSASend() failed. io pending. Error:%d\n", GetLastError());
					continue;
				}
			}

			printf("Send %s\n", pIoData->buffer);
		}
		else
		{
			pIoData->bytesRecv = 0;
			flags = 0;

			ZeroMemory(&(pIoData->overlapped), sizeof(OVERLAPPED));
			pIoData->databuff.len = DataBuffSize;
			pIoData->databuff.buf = pIoData->buffer;

			if (WSARecv(pHandleData->socket, &(pIoData->databuff), 1, &recvBytes, &flags, &(pIoData->overlapped), NULL) == SOCKET_ERROR)
			{
				if (WSAGetLastError() != ERROR_IO_PENDING)
				{
					printf("WSARecv() failed. Error:%d\n", GetLastError());
				}
				else
				{
					printf("WSARecv() io pending\n");
				}
			}
		}
	}
}
