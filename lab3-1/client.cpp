#include <winsock2.h> 
#include<iostream>
#include<fstream>
#include<cstring>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <WS2tcpip.h>
#include<ctime>
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
unsigned char seq = 0; // 初始化8位序列号

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
	u_long dataLen;
	u_short len;			// 数据长度，16位 最长65535位 8191个字节
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

bool SendPkg(Package p, SOCKET sockClient, SOCKADDR_IN addrSrv)
{
	char Type[10];
	switch (p.hm.type) {
	case 1: strcpy(Type, "SYN"); break;
	case 4: strcpy(Type, "ACK"); break;
	case 8: strcpy(Type, "FIN_ACK"); break;
	case 16:strcpy(Type, "PSH"); break;
	}

	// 发送消息
	while (sendto(sockClient, (char*)&p, sizeof(p), 0, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == -1)
	{
		printf("%s [ ERR  ] Client: send [%s] ERROR:%s Seq=%d\n", timei(), Type, strerror(errno), p.hm.seq);

	}
	printf("%s [ INFO ] Client: [%s] Seq=%d\n", timei(), Type, p.hm.seq);

	if (!strcmp(Type, "ACK"))
		return true;
	// 开始计时
	clock_t start = clock();
	// 等待接收消息
	Package p1;
	int addrlen = sizeof(SOCKADDR);
	while (true) {
		if (recvfrom(sockClient, (char*)&p1, sizeof(p1), 0, (SOCKADDR*)&addrSrv, &addrlen) > 0 && clock() - start <= WAIT_TIME) {
			// 收到消息需要验证消息类型、序列号和校验和
			u_short ckSum = checkSumVerify((u_short*)&p1, sizeof(p1));
			if ((p1.hm.type == SYN_ACK && !strcmp(Type, "SYN")) && p1.hm.seq == seq && ckSum == 0)
			{
				printf("%s [ GET  ] Client: receive [SYN, ACK] from Server\n", timei());
				return true;
			}
			else if ((p1.hm.type == ACK && (!strcmp(Type, "FIN_ACK") || !strcmp(Type, "PSH"))) && p1.hm.seq == seq && ckSum == 0)
			{
				printf("%s [ GET  ] Client: receive [ACK] from Server\n", timei());
				return true;
			}
			else {
				SendPkg(p, sockClient, addrSrv);
				return true;
				// 差错重传并重新计时
			}
		}
		else {
			SendPkg(p, sockClient, addrSrv);
			return true;
			// 超时重传并重新计时
		}
	}
}

bool HandShake(SOCKET sockClient, SOCKADDR_IN addrSrv)
{
	char sendBuf[BUFFER_SIZE] = {};
	cin.getline(sendBuf, BUFFER_SIZE);
	if (strcmp(sendBuf, "connect") != 0)
		return false;
	cout << "-----------------------------------CONNECTION-----------------------------------" << endl;
	Package p1;
	p1.hm.type = SYN;
	p1.hm.seq = seq;
	p1.hm.checkSum = 0;
	p1.hm.checkSum = checkSumVerify((u_short*)&p1, sizeof(p1));
	int len = sizeof(SOCKADDR);
	SendPkg(p1, sockClient, addrSrv);
	seq = (seq + 1) % 256;
	p1.hm.type = ACK;
	p1.hm.seq = seq;
	p1.hm.checkSum = 0;
	p1.hm.checkSum = checkSumVerify((u_short*)&p1, sizeof(p1));

	if (sendto(sockClient, (char*)&p1, sizeof(p1), 0, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) != -1)
	{
		printf("%s [ INFO ] Client: [ACK] Seq=%d\n", timei(), seq);
		seq = (seq + 1) % 256;
		return true;
	}
	else
	{
		printf("%s [ ERR  ] Client: send [ACK] ERROR\n", timei());
		return false;
	}
}

bool WaveHand(SOCKET sockClient, SOCKADDR_IN addrSrv)
{
	/*char sendBuf[BUFFER_SIZE] = {};
	cin.getline(sendBuf, BUFFER_SIZE);
	if(strcmp(sendBuf,"disconnect")!=0)
		return false;*/
	cout << "----------------------------------DISCONNECTION---------------------------------" << endl;
	Package p1;
	p1.hm.type = FIN_ACK;
	p1.hm.seq = seq;
	p1.hm.checkSum = 0;
	p1.hm.checkSum = checkSumVerify((u_short*)&p1, sizeof(p1));
	int len = sizeof(SOCKADDR);

	SendPkg(p1, sockClient, addrSrv);
	Package p4;

	while (true)
	{
		if (recvfrom(sockClient, (char*)&p4, sizeof(p4), 0, (SOCKADDR*)&addrSrv, &len) > 0)
		{
			/*memcpy(&s1, recvBuf_s1, sizeof(s1));*/
			if (p4.hm.type == FIN_ACK)
			{
				printf("%s [ GET  ] Client: receive [FIN, ACK] from Server\n", timei());
				p4.hm.type = ACK;
				p4.hm.checkSum = 0;
				p4.hm.checkSum = checkSumVerify((u_short*)&p4, sizeof(p4));
				SendPkg(p4, sockClient, addrSrv);
				break;
			}
			else
			{
				printf("%s [ ERR  ] Server: receive [FIN, ACK] ERROR\n", timei());
				return false;
			}
		}
	}

	return true;
}

bool SendMsg(char* data, SOCKET sockClient, SOCKADDR_IN addrSrv, int dataLen, char fileNum)
{
	int sentLen = 0;
	for (int i = 0; i < dataLen / 8000 + 1; i++)
	{
		// 设置信息头
		Package p;
		p.hm.dataLen = dataLen;
		p.hm.seq = seq; //need to check
		p.hm.type = PSH;
		p.hm.checkSum = 0;
		p.hm.fileNum = fileNum;
		if (fileNum == '1' || fileNum == '2' || fileNum == '3')
			p.hm.fileTyp = JPG;
		else
			p.hm.fileTyp = TXT;

		if (i != dataLen / 8000)
			p.hm.len = 8000;
		else
			p.hm.len = dataLen % 8000;

		// data存放的是读入的二进制数据，sentLen是已发送的长度，作为分批次发送的偏移量
		memcpy(p.data, data + sentLen, p.hm.len); //把本个包的数据存进去
		sentLen += (int)p.hm.len; //发送长度加长
		// 计算校验和
		p.hm.checkSum = checkSumVerify((u_short*)&p, sizeof(p));
		SendPkg(p, sockClient, addrSrv);
		seq = (seq + 1) % 256;
	}
	return true;
}

void main()
{
	WSADATA wsaData; //可用socket详细信息
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	printf("%s [ OK   ] WSAStartup Complete!\n", timei());

	struct timeval timeo = { 20,0 };
	socklen_t lens = sizeof(timeo);

	SOCKET sockClient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	setsockopt(sockClient, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeo, lens);

	//服务端
	SOCKADDR_IN addrSrv = { 0 };
	addrSrv.sin_family = AF_INET;//用AF_INET表示TCP/IP协议。
	addrSrv.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//设置为本地回环地址
	addrSrv.sin_port = htons(4567);

	SOCKADDR_IN addrClient;

	int len = sizeof(SOCKADDR);

	if (HandShake(sockClient, addrSrv) == true)
	{
		printf("%s [ INFO ] Client: Connection Success\n", timei());
		cout << "------------------------------CONNECTION SUCCESSFUL------------------------------" << endl;
	}

	cout << "There are the files existing in the path.\n(1) 1.jpg\n(2) 2.jpg\n(3) 3.jpg\n(4)helloworld.txt\n";
	cout << "You can input the num '0' to quit\nPlease input the number of the file which you want to choose to send:\n";

	float seconds;
	float T;
	long long head, tail, freq;
	QueryPerformanceFrequency((LARGE_INTEGER*)&freq);

	while (1)
	{
		// 以二进制方式打开文件
		char Buf[BUFFER_SIZE] = {};
		cin.getline(Buf, BUFFER_SIZE);
		char* data;
		if (strcmp(Buf, "1") == 0 || strcmp(Buf, "2") == 0 || strcmp(Buf, "3") == 0 || strcmp(Buf, "4") == 0)
		{
			char file[100] = "..\\test\\";
			if (Buf[0] == '1' || Buf[0] == '2' || Buf[0] == '3')
				sprintf(file, "%s%c.jpg", file, Buf[0]);
			else
				sprintf(file, "%s%s", file, "helloworld.txt");
			ifstream in(file, ifstream::in | ios::binary);
			int dataLen = 0;
			if (!in)
			{
				printf("%s [ INFO ] Client: can't open the file! Please retry\n", timei());
				continue;
			}
			// 文件读取到data
			BYTE t = in.get();
			char* data = new char[100000000];
			memset(data, 0, sizeof(data));
			while (in)
			{
				data[dataLen++] = t;
				t = in.get();
			}
			in.close();
			printf("read over\n");
			//cout<<dataLen<<endl;
			QueryPerformanceCounter((LARGE_INTEGER*)&head);

			SendMsg(data, sockClient, addrSrv, dataLen, Buf[0]);
			QueryPerformanceCounter((LARGE_INTEGER*)&tail);
			T = (tail - head) * 1000.0 / freq;
			printf("%s [ INFO ] Client: Send Finish! transmission delay :%fms\n", timei(), T);
		}
		else if (strcmp(Buf, "0") == 0)
			break;
		else
		{
			printf("%s [ ERR  ] Client: Invalidate Input\n", timei());
		}
	}

	if (WaveHand(sockClient, addrSrv) == true)
	{
		printf("%s [ INFO ] Client: Disconnection Success\n", timei());
		cout << "-----------------------------DISCONNECTION SUCCESSFUL----------------------------" << endl;
	}

	closesocket(sockClient);

	WSACleanup();
}