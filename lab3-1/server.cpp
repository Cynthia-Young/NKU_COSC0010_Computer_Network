#include <winsock2.h> 
#include<iostream>
#include<fstream>
#include<cstring>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include<ctime>
//#include <netinet/in.h>
#include <errno.h>
#include <WS2tcpip.h>
using namespace std;
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996)

#define SYN 1
#define SYN_ACK 2
#define ACK 4
#define FIN_ACK 8
#define PSH 16
#define NAK 32

#define JPG 1
#define TXT 2

const int BUFFER_SIZE = 8192;
const int WAIT_TIME = 100;//客户端等待事件的时间，单位ms
char buf[64];//用于时间戳
unsigned char lastAck = -1;

char* timei()
{
	time_t t;
	struct tm* tmp;

	time(&t);
	tmp = localtime(&t);
	strftime(buf, 64, "%Y-%m-%d %H:%M:%S", tmp);
	return buf;
}

struct HeadMsg {
	u_long dataLen;         //数据总长度
	u_short len;			// 数据长度，16位
	u_short checkSum;		// 校验和，16位
	unsigned char type;		// 消息类型
	unsigned char seq;		// 序列号，可以表示0-255
	unsigned char fileNum;  //文件号
	unsigned char fileTyp;
};

struct Package {
	HeadMsg hm;
	char data[8000];
};

struct RecvData {
	char* data;
	int dataLen;
	char fileNum;
};

// 校验和：每16位相加后取反，接收端校验时若结果为全0则为正确消息
u_short checkSumVerify(u_short* msg, int length) {
	int count = (length + 1) / 2;
	u_long checkSum = 0;//32bit
	while (count--) {
		checkSum += *msg++;
		if (checkSum & 0xffff0000) {
			checkSum &= 0xffff;
			checkSum++;
		}
	}
	return ~(checkSum & 0xffff);
}

bool SendPkg(Package p, SOCKET sockSrv, SOCKADDR_IN addrClient)
{
	char Type[10];
	switch (p.hm.type) {
	case 2: strcpy(Type, "SYN_ACK"); break;
	case 4: strcpy(Type, "ACK"); break;
	case 8: strcpy(Type, "FIN_ACK"); break;
	}
	// 发送消息
	while (sendto(sockSrv, (char*)&p, sizeof(p), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR)) == -1)
	{
		printf("%s [ ERR  ] Server: send [%s] ERROR:%s Seq=%d\n", timei(), Type, strerror(errno), p.hm.seq);

	}
	printf("%s [ INFO ] Server: [%s] Seq=%d\n", timei(), Type, p.hm.seq);
	if (!strcmp(Type, "ACK"))
		return true;
	// 开始计时
	clock_t start = clock();
	// 等待接收消息
	Package p1;
	int addrlen = sizeof(SOCKADDR);
	while (true) {
		if (recvfrom(sockSrv, (char*)&p1, sizeof(p1), 0, (SOCKADDR*)&addrClient, &addrlen) > 0 && clock() - start <= WAIT_TIME) {
			u_short ckSum = checkSumVerify((u_short*)&p1, sizeof(p1));
			// 收到消息需要验证消息类型、序列号和校验和
			if (p1.hm.type == ACK && ckSum == 0)
			{
				printf("%s [ GET  ] Server: receive [ACK] from Client\n", timei());
				return true;
			}
			else {
				SendPkg(p, sockSrv, addrClient);
				return true;
				// 差错重传并重新计时
			}
		}
		else {
			SendPkg(p, sockSrv, addrClient);
			return true;
			// 超时重传并重新计时
		}
	}
}

bool HandShake(SOCKET sockSrv, SOCKADDR_IN addrClient)
{
	Package p2;
	int len = sizeof(SOCKADDR);
	while (true)
	{
		if (recvfrom(sockSrv, (char*)&p2, sizeof(p2), 0, (SOCKADDR*)&addrClient, &len) > 0)
		{
			cout << "-----------------------------------CONNECTION-----------------------------------" << endl;
			int ck = checkSumVerify((u_short*)&p2, sizeof(p2));
			if (p2.hm.type == SYN && ck == 0)
			{
				printf("%s [ GET  ] Server: receive [SYN] from Client\n", timei());
				Package p3;
				p3.hm.type = SYN_ACK;
				p3.hm.seq = (lastAck + 1) % 256;
				lastAck = (lastAck + 2) % 256;
				p3.hm.checkSum = 0;
				p3.hm.checkSum = checkSumVerify((u_short*)&p3, sizeof(p3));
				SendPkg(p3, sockSrv, addrClient);
				break;
			}
			else
			{
				printf("%s [ ERR  ] Server: receive [SYN] ERROR\n", timei());
				return false;
			}
		}
	}
	return true;
}

bool WaveHand(SOCKET sockSrv, SOCKADDR_IN addrClient, Package p2)
{
	int len = sizeof(SOCKADDR);

	u_short ckSum = checkSumVerify((u_short*)&p2, sizeof(p2));
	if (p2.hm.type == FIN_ACK && ckSum == 0)
	{
		printf("%s [ GET  ] Server: receive [FIN, ACK] from Client\n", timei());
		p2.hm.type = ACK;
		p2.hm.seq = (lastAck + 1) % 256;
		lastAck = (lastAck + 2) % 256;
		p2.hm.checkSum = 0;
		p2.hm.checkSum = checkSumVerify((u_short*)&p2, sizeof(p2));
		SendPkg(p2, sockSrv, addrClient);
	}
	else
	{
		printf("%s [ ERR  ] Server: receive [FIN, ACK] ERROR\n", timei());
		return false;
	}


	Package p3;
	p3.hm.type = FIN_ACK;
	p3.hm.checkSum = 0;
	p3.hm.checkSum = checkSumVerify((u_short*)&p2, sizeof(p2));
	SendPkg(p3, sockSrv, addrClient);

	return true;
}

RecvData RecvMsg(SOCKET sockSrv, SOCKADDR_IN addrClient)
{
	Package p1;
	int addrlen = sizeof(SOCKADDR);
	int totalLen = 0;
	RecvData rd;
	rd.data = new char[100000000];
	// 等待接收消息
	while (true) {

		// 收到消息需要验证校验和及序列号
		if (recvfrom(sockSrv, (char*)&p1, sizeof(p1), 0, (SOCKADDR*)&addrClient, &addrlen) > 0)
		{
			if (p1.hm.type == FIN_ACK)
			{
				cout << "----------------------------------DISCONNECTION---------------------------------" << endl;
				rd.fileNum = '0';
				WaveHand(sockSrv, addrClient, p1);
				break;
			}
			Package p2;
			p2.hm.fileTyp = p2.hm.fileNum = p2.hm.checkSum = p2.hm.dataLen = p2.hm.len = 0;

			int ck = !checkSumVerify((u_short*)&p1, sizeof(p1));
			if (p1.hm.seq == (lastAck + 1) % 256 && ck == 1)  //序列号问题

			{
				lastAck = (lastAck + 1) % 256;
				p2.hm.type = ACK;
				p2.hm.seq = lastAck;
				p2.hm.checkSum = checkSumVerify((u_short*)&p2, sizeof(p2));
				while (sendto(sockSrv, (char*)&p2, sizeof(p2), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR)) == -1)
					printf("%s [ ERR  ] Server: send [ACK] ERROR:%s Seq=%d\n", timei(), strerror(errno), p2.hm.seq);
				printf("%s [ INFO ] Server: [ACK] Seq=%d\n", timei(), p2.hm.seq);
				memcpy(rd.data + totalLen, (char*)&p1 + sizeof(p1.hm), p1.hm.len);
				totalLen += (int)p1.hm.len;
				rd.dataLen = p1.hm.dataLen;
				rd.fileNum = p1.hm.fileNum;
				if (totalLen == p1.hm.dataLen)
				{
					break;
				}
			}
			else {
				// 差错重传
				p2.hm.type = NAK;
				p2.hm.seq = lastAck;
				p2.hm.checkSum = checkSumVerify((u_short*)&p2, sizeof(p2));
				sendto(sockSrv, (char*)&p2, BUFFER_SIZE, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
			}
		}
	}
	return rd;
}

void main()
{
	WSADATA wsaData; //可用socket详细信息
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	printf("%s [ OK   ] WSAStartup Complete!\n", timei());

	struct timeval timeo = { 20,0 };
	socklen_t lens = sizeof(timeo);

	SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	setsockopt(sockSrv, SOL_SOCKET, SO_RCVTIMEO, (char*) & timeo, lens);

	//服务端
	SOCKADDR_IN addrSrv = { 0 };
	addrSrv.sin_family = AF_INET;//用AF_INET表示TCP/IP协议。
	addrSrv.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//设置为本地回环地址
	addrSrv.sin_port = htons(4567);
	printf("%s [ INFO ] Local Machine IP Address: 127.0.0.1\n", timei());

	if (bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) != SOCKET_ERROR)
		printf("%s [ OK   ] Bind Success!\n", timei());
	else
		printf("%s [ ERR  ] Bind Failure!\n", timei());
	SOCKADDR_IN addrClient = { 0 };

	int len = sizeof(SOCKADDR);


	if (HandShake(sockSrv, addrClient) == true)
	{
		printf("%s [ INFO ] Server: Connection Success\n", timei());
		cout << "------------------------------CONNECTION SUCCESSFUL------------------------------" << endl;
	}

	while (1)
	{
		RecvData rd;
		rd = RecvMsg(sockSrv, addrClient);
		if (rd.fileNum == '0')
			break;
		char file1[100] = ".\\file\\";
		if (rd.fileNum == '1' || rd.fileNum == '2' || rd.fileNum == '3')
			sprintf(file1, "%s%c.jpg", file1, rd.fileNum);
		else
			sprintf(file1, "%s%s", file1, "helloworld.txt");
		ofstream out(file1, ofstream::binary);
		for (int i = 0; i < rd.dataLen; i++)
		{
			out << rd.data[i];
		}
		out.close();
		printf("收到文件: %s\n", file1);
	}
	printf("%s [ INFO ] Server: Disconnection Success\n", timei());
	cout << "-----------------------------DISCONNECTION SUCCESSFUL----------------------------" << endl;

	closesocket(sockSrv);

	WSACleanup();
}