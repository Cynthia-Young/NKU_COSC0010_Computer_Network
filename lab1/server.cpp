#include <winsock2.h> 
#include<iostream>
#include<cstring>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

using namespace std;

#pragma comment(lib,"ws2_32.lib")

const int BUFFER_SIZE = 1024;
char buf2[64];//用于时间戳
const int WAIT_TIME = 10;//客户端等待事件的时间，单位ms
const int MAX = 5;//服务端最大链接数
int total = 0;//已经链接的客服端服务数

SOCKET cliSock[MAX];
SOCKADDR_IN cliAddr[MAX];
WSAEVENT cliEvent[MAX];//事件

DWORD WINAPI handlerRequest(LPVOID lparam);//服务器端处理线程

char* timei()
{
	time_t t;
    struct tm *tmp;

	time(&t);
    tmp = localtime(&t);
	strftime(buf2, 64, "%Y-%m-%d %H:%M:%S", tmp);
	return buf2;
}

void main()
{    
	printf("%s [ INFO ] Start Server Manager\n",timei());

	WSADATA wsaData; //可用socket详细信息
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	printf("%s [ OK   ] WSAStartup Complete!\n",timei());

	printf("%s [ INFO ] Local Machine IP Address: 127.0.0.1\n",timei());

	SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, 0);
	if(sockSrv !=INVALID_SOCKET)
		printf("%s [ OK   ] Socket Created!\n",timei());

	//服务端
	SOCKADDR_IN addrSrv = { 0 };
	addrSrv.sin_family = AF_INET;//用AF_INET表示TCP/IP协议。
	addrSrv.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//设置为本地回环地址
	addrSrv.sin_port = htons(819711);

	if(bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR))!= SOCKET_ERROR)
		printf("%s [ OK   ] Bind Success!\n",timei());

	WSAEVENT servEvent = WSACreateEvent();//创建一个事件对象
	WSAEventSelect(sockSrv, servEvent, FD_ALL_EVENTS);//绑定事件对象，并且监听所有事件

	cliSock[0] = sockSrv;
	cliEvent[0] = servEvent;

	CloseHandle(CreateThread(NULL, 0, handlerRequest, (LPVOID)&sockSrv, 0, 0));
	//CreateThread建立新的线程  线程终止运行后，线程对象仍然在系统中，必须通过CloseHandle函数来关闭该线程对象
	//不需要句柄所以直接关闭 这个时候，内核对象的引用计数不为0，线程不停。
	printf("%s [ INFO ] Broadcast thread create success!\n",timei());

	listen(sockSrv, 5);
	printf("%s [ INFO ] Start listening...\n",timei());

	while (1) {
		char contentBuf[BUFFER_SIZE] = { 0 };
		char sendBuf[BUFFER_SIZE] = { 0 };
		cin.getline(contentBuf, sizeof(contentBuf));
		sprintf(sendBuf, "[ 公告 ]%s", contentBuf);
		for (int j = 1; j <= total; j++)
		{
			send(cliSock[j], sendBuf, sizeof(sendBuf), 0);
		}
	}

	closesocket(sockSrv);

	WSACleanup();
}

DWORD WINAPI handlerRequest(LPVOID lparam)
{
	SOCKET sockSrv = *(SOCKET*)lparam;//LPVOID为空指针类型
	while (1) //不停执行
	{
		for (int i = 0; i < total + 1; i++)//i代表现在正在监听事件的终端
		{
			//对每一个终端，查看是否发生事件，等待WAIT_TIME毫秒
			int index = WSAWaitForMultipleEvents(1, &cliEvent[i], false, WAIT_TIME, 0);
			index -= WSA_WAIT_EVENT_0;//此时index为事件在事件数组中的位置

			if (index == WSA_WAIT_TIMEOUT || index == WSA_WAIT_FAILED)
			{
				continue;//如果出错或者超时，即跳过此终端
			}
			else if (index == 0)
			{
				WSANETWORKEVENTS networkEvents;
				WSAEnumNetworkEvents(cliSock[i], cliEvent[i], &networkEvents);//查看是什么事件

				//事件选择
				if (networkEvents.lNetworkEvents & FD_ACCEPT)
				{
					if (networkEvents.iErrorCode[FD_ACCEPT_BIT] != 0)
					{
						cout << "连接时产生错误，错误代码" << networkEvents.iErrorCode[FD_ACCEPT_BIT] << endl;
						continue;
					}
					if (total + 1 <= MAX)
					{
						int nextIndex = total + 1;
						int addrLen = sizeof(SOCKADDR);
						SOCKET newSock = accept(sockSrv, (SOCKADDR*)&cliAddr[nextIndex], &addrLen); 
						if (newSock !=INVALID_SOCKET)
						{
							printf("%s [ OK   ] Accept Success!\n",timei());
							cliSock[nextIndex] = newSock;

							WSAEVENT newEvent = WSACreateEvent();
							WSAEventSelect(cliSock[nextIndex], newEvent, FD_CLOSE | FD_READ | FD_WRITE);
							cliEvent[nextIndex] = newEvent;

							total++;//客户端连接数增加

							printf("%s [ JOIN ] user#%d just join, welcome!\n",timei(),nextIndex,inet_ntoa(cliAddr[nextIndex].sin_addr));
							//inet_ntoa() 将网络地址转换成“.”点隔的字符串格式

							char buf[BUFFER_SIZE];
							sprintf(buf,"%s [ JOIN ] welcome user#%d enter the chat room",timei(),nextIndex);

							for (int j = i; j <= total; j++)
							{
								send(cliSock[j], buf, sizeof(buf), 0);
							}
						}
					}

				}
				else if (networkEvents.lNetworkEvents & FD_CLOSE)//客户端被关闭，即断开连接
				{
					//i表示已关闭的客户端下标
					total--;
					printf("%s [ EXIT ] user#%d just exit the chat room\n",timei(),i,inet_ntoa(cliAddr[i].sin_addr));
					//释放这个客户端的资源
					closesocket(cliSock[i]);
					WSACloseEvent(cliEvent[i]);

					for (int j = i; j < total; j++)
					{
						cliSock[j] = cliSock[j + 1];
						cliEvent[j] = cliEvent[j + 1];
						cliAddr[j] = cliAddr[j + 1];
					}

					char buf[BUFFER_SIZE];
					sprintf(buf,"%s [ EXIT ] user#%d just exit the chat room\n",timei(),i);

					for (int j = 1; j <= total; j++)
					{
						send(cliSock[j], buf, sizeof(buf), 0);
					}
				}
				else if (networkEvents.lNetworkEvents & FD_READ)//接收到消息
				{

					char buffer[BUFFER_SIZE] = { 0 };//字符缓冲区，用于接收和发送消息
					char buffer2[BUFFER_SIZE] = { 0 };

					for (int j = 1; j <= total; j++)
					{
						int nrecv = recv(cliSock[j], buffer, sizeof(buffer), 0);//nrecv是接收到的字节数
						if (nrecv > 0)//如果接收到的字符数大于0
						{
							sprintf(buffer2,"%s [ RECV ] user#%d: %s",timei(),j,buffer);
							cout << buffer2 << endl;
							//在其他客户端显示（广播给其他客户端）
							for (int k = 1; k <= total; k++)
							{
								send(cliSock[k], buffer2, sizeof(buffer), 0);
							}
						}
					}
				}
			}
		}
	}
	return 0;
}