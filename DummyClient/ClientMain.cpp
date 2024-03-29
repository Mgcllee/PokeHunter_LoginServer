#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include<iostream>
#include<string>
#include<winsock2.h>

#include "../IOCPServer/protocol.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

using namespace std;

constexpr int CLIENT_NUM = 50;

void ShowErrorMessage(string message)
{
	cout << "[오류발생]: " << message << '\n';

	cout << "ErrorCode: " << WSAGetLastError();

	system("pause");
	exit(1);
}

int main()
{
	SOCKET clientSocket[CLIENT_NUM];
	for (int i = 0; i < CLIENT_NUM; ++i) {
		WSADATA wsaData;

		char received[256];

		string sent;

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) // Winsock을초기화합니다.
			ShowErrorMessage("WSAStartup()");

		clientSocket[i] = socket(PF_INET, SOCK_STREAM, 0); // TCP 소켓을생성합니다.

		if (clientSocket[i] == INVALID_SOCKET)
			ShowErrorMessage("socket()");

		SOCKADDR_IN serverAddress;
		memset(&serverAddress, 0, sizeof(serverAddress));
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_addr.s_addr = inet_addr("172.31.176.1"); // 문자열IP를네트워크바이트형식으로
		serverAddress.sin_port = htons(PORT_NUM); // 2바이트정수네트워크바이트형식으로

		if (connect(clientSocket[i], (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
			ShowErrorMessage("connect()");

		cout << "[Client]\n";

		CS_LOGIN_PACK test_pack;
		test_pack.size = sizeof(CS_LOGIN_PACK);
		test_pack.type = CS_LOGIN;
		strcpy_s(test_pack.Token, "theEnd");
		send(clientSocket[i], (char*)&test_pack, test_pack.size, NULL);

	}

	for (int i = 0; i < CLIENT_NUM; ++i) {
		SC_LOGIN_SUCCESS_PACK* ok_pack{};
		recv(clientSocket[i], (char*)ok_pack, sizeof(SC_LOGIN_SUCCESS_PACK), NULL);
	}

	for (int i = 0; i < CLIENT_NUM; ++i) {
		closesocket(clientSocket[i]);
	}
	WSACleanup();
}