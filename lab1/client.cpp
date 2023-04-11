#include<iostream>
#include<winsock2.h>
#include<cstring>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

using namespace std;

#pragma comment(lib,"ws2_32.lib")   //socket动态链接库

const int BUF_SIZE = 1024;
char buf2[64];

DWORD WINAPI recvMsgThread(LPVOID IpParameter);

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
	printf("%s [ GET  ] Input the IP of server\n",timei());
	char ip[20]={0};
	gets(ip);
	printf("%s [ GET  ] Input the Port of server\n",timei());
	int port;
	cin>>port;

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData); //主版本号为2，副版本号为2
	printf("%s [ OK   ] WSAStartup Complete!\n",timei());

	SOCKET sockClient = socket(AF_INET, SOCK_STREAM, 0);
	if(sockClient !=INVALID_SOCKET)
		printf("%s [ OK   ] Socket Created!\n",timei());
	
	//客户端
	SOCKADDR_IN cliAddr = { 0 };
	cliAddr.sin_family = AF_INET;
	cliAddr.sin_addr.s_addr = inet_addr("127.0.0.1");//IP地址
	cliAddr.sin_port = htons(12345);//端口号

	//服务端
	SOCKADDR_IN addrSrv = { 0 };
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_addr.S_un.S_addr = inet_addr(ip);
	addrSrv.sin_port = htons(port);

	if (connect(sockClient, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << timei()<<" [ INFO ] 链接出现错误，错误代码" << WSAGetLastError() << endl;
	}
	else
		printf("%s [ INFO ] Server connected succesfully!\n",timei());

	//创建接受消息线程
	CloseHandle(CreateThread(NULL, 0, recvMsgThread, (LPVOID)&sockClient, 0, 0));

	while (1)
	{
		char buf[BUF_SIZE] = { 0 };
		cin.getline(buf, sizeof(buf));
		if (strcmp(buf, "exit") == 0)
		{
			break;
		}
		send(sockClient, buf, sizeof(buf), 0);
	}

	closesocket(sockClient);
	WSACleanup();

}

DWORD WINAPI recvMsgThread(LPVOID Iparam)//接收消息的线程
{
	SOCKET cliSock = *(SOCKET*)Iparam;//获取客户端的SOCKET参数

	while (1)
	{
		char buffer[BUF_SIZE] = { 0 };
		int nrecv = recv(cliSock, buffer, sizeof(buffer), 0);//nrecv是接收到的字节数
		if (nrecv > 0)//如果接收到的字符数大于0
		{
			cout << buffer << endl;
		}
		else if (nrecv < 0)//如果接收到的字符数小于0就说明断开连接
		{
			printf("%s [ INFO ] Disconnect from server\n",timei());
			break;
		}
	}
	return 0;
}