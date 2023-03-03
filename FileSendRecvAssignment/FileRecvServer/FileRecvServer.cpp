#include "..\Common.h"
#include <fstream>
#include <string>

#define SERVERPORT 9000
#define BUFSIZE    100'000

bool ErrorCheck(int retval);
bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf);
bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf, ofstream& file);

int main(int argc, char* argv[]) {
	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// ���� ����
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// ������ ��ſ� ����� ����
	SOCKET client_sock;
	struct sockaddr_in clientaddr;
	int addrlen;
	ULONG len; // ���� ���� ������
	char buf[BUFSIZE + 1]; // ���� ���� ������

	while (1) {
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (struct sockaddr*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}

		// ������ Ŭ���̾�Ʈ ���� ���
		char addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
		printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			addr, ntohs(clientaddr.sin_port));

		// Ŭ���̾�Ʈ�� ������ ���
		// 1. ���� ���� ���� �̸� �޾ƿ���
		RcevFixedVariable(client_sock, len, buf);
		string fileName(buf, (size_t)len);
		ofstream out{ fileName, ios::binary};

		// 2. ���� ������ �ޱ�
		RcevFixedVariable(client_sock, len, buf, out);

		// ���� �ݱ�
		closesocket(client_sock);
		printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			addr, ntohs(clientaddr.sin_port));
	}

	// ���� �ݱ�
	closesocket(listen_sock);

	// ���� ����
	WSACleanup();
	return 0;
}

bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf) {
	int retval;

	// ������ �ޱ�(���� ����)
	retval = recv(sock, (char*)&len, sizeof(ULONG), MSG_WAITALL);
	if (ErrorCheck(retval))
		return false;

	// ������ �ޱ�(���� ����)
	retval = recv(sock, buf, len, MSG_WAITALL);
	if (ErrorCheck(retval))
		return false;

	return true;
}

bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf, ofstream& file) {
	int retval;

	// ������ ũ�⸦ �޴´�.(���� ����)
	retval = recv(sock, (char*)&len, sizeof(ULONG), MSG_WAITALL);
	if (ErrorCheck(retval))
		return false;

	ULONG popSize = len;
	// ���� �����͸� �޴´�.(���� ����)
	while (true) {
		if (BUFSIZE < popSize) {
			retval = recv(sock, buf, BUFSIZE, MSG_WAITALL);
			if (ErrorCheck(retval))
				return false;
			file.write(buf, BUFSIZE);
			popSize -= BUFSIZE;
		}
		else {
			retval = recv(sock, buf, popSize, MSG_WAITALL);
			if (ErrorCheck(retval))
				return false;
			file.write(buf, popSize);
			popSize -= retval;
			break;
		}
	}

	return true;
}

bool ErrorCheck(int retval) {
	if (retval == SOCKET_ERROR) {
		err_display("recv()");
		return true;
	}
	else if (retval == 0)
		return true;

	return false;
}